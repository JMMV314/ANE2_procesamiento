from __future__ import annotations

from typing import Any, Dict, List, Tuple, Union, Optional

import numpy as np
import pandas as pd

from src.payload_parser import frame_from_payload
from src.spectrum_frame import SpectrumFrame
from src.spectral_analysis import (
    detect_peak_bins,
    find_emission_span,
    slice_spectrum_frame,
    measure_emission_parameters,
    estimate_noise_floor,
    adaptive_threshold,
)
from src.power_utils import channel_power_dbm_uniform_bins
from src.calibration_io import comparar_parametros

PayloadInput = Union[Dict[str, Any], List[Any]]  # dict legacy o lista [json,picos,cumplimiento]


def unpack_input(inp: PayloadInput) -> Tuple[Dict[str, Any], List[float], int]:
    """Normaliza la entrada.

    Formatos aceptados:
      1) [frame_json, picos_list, cumplimiento]
      2) { ...frame_json..., "picos": [...], "cumplimiento": 0/1 }
    """
    if isinstance(inp, list):
        if len(inp) != 3:
            raise ValueError("Entrada tipo lista debe ser exactamente [json, picos, cumplimiento].")

        frame_json = inp[0]
        picos_raw = inp[1]
        cumplimiento_raw = inp[2]

        if not isinstance(frame_json, dict):
            raise TypeError("El primer elemento debe ser un dict con Pxx/start_freq_hz/end_freq_hz.")

        if picos_raw is None:
            picos_list: List[float] = []
        else:
            if not isinstance(picos_raw, list):
                raise TypeError("El segundo elemento (picos) debe ser una lista o null.")
            picos_list = [float(x) for x in picos_raw]

        try:
            cumplimiento = int(cumplimiento_raw)
        except Exception:
            cumplimiento = 0

        return frame_json, picos_list, cumplimiento

    if isinstance(inp, dict):
        frame_json = inp
        picos_raw = inp.get("picos", [])
        cumplimiento_raw = inp.get("cumplimiento", 0)

        if picos_raw is None:
            picos_list = []
        else:
            if not isinstance(picos_raw, list):
                raise TypeError("La clave 'picos' debe ser lista o null.")
            picos_list = [float(x) for x in picos_raw]

        try:
            cumplimiento = int(cumplimiento_raw)
        except Exception:
            cumplimiento = 0

        return frame_json, picos_list, cumplimiento

    raise TypeError("Entrada inválida. Debe ser dict o lista [json, picos, cumplimiento].")


def route_mode(picos: List[float], cumplimiento: int) -> str:
    """Reglas de enrutamiento:

    - Si llegan picos: modo 'peaks' (NO hace cumplimiento)
    - Si no llegan picos y cumplimiento==1: modo 'compliance'
    - Si no llegan picos y cumplimiento==0: modo 'all_emissions'
    """
    if len(picos) > 0:
        return "peaks"
    if cumplimiento == 1:
        return "compliance"
    return "all_emissions"


def apply_gain_correction(frame: SpectrumFrame, corr_csv_path: str) -> SpectrumFrame:
    freqs = np.asarray(frame.freq_hz, dtype=float)
    amps = np.asarray(frame.amplitudes_dbm, dtype=float)

    df_corr = pd.read_csv(corr_csv_path)
    df_corr.columns = [c.strip().replace("\ufeff", "") for c in df_corr.columns]

    freq_corr_axis = df_corr["Frecuencia (MHz)"].values.astype(float) * 1e6
    gain_corr_values = df_corr["Error (dB)"].values.astype(float)

    correction_interpolated = np.interp(freqs, freq_corr_axis, gain_corr_values)
    amps_corr = amps + correction_interpolated

    return SpectrumFrame(
        amplitudes_dbm=amps_corr,
        f_start_hz=float(freqs[0]),
        f_stop_hz=float(freqs[-1]),
        freq_hz=freqs,
        bin_hz=frame.bin_hz,
    )


def pico_to_hz(p: float) -> float:
    """Convierte un pico.

    - Si |p| < 1e6 asume que viene en MHz.
    - Si no, asume Hz.
    """
    if abs(p) < 1e6:
        return float(p) * 1e6
    return float(p)


def nearest_bin(freq_axis_hz: np.ndarray, f_hz: float) -> int:
    return int(np.argmin(np.abs(freq_axis_hz - f_hz)))


def match_margin_hz(pico_hz: float) -> float:
    """Margen para matchear un pico solicitado con un pico detectado.

    Requisito: "30% del pico o 100 kHz, lo que sea mínimo".
    => margin = min(0.30 * |pico_hz|, 100_000)

    Nota: para frecuencias típicas (MHz), casi siempre será 100 kHz.
    """
    return float(min(0.30 * abs(float(pico_hz)), 100_000.0))


def is_ruido_por_umbral(
    frame: SpectrumFrame,
    idx: int,
    n_sigma: float = 1.51,
    min_snr_db: float = 0.5,
) -> Tuple[bool, float, float, float]:
    """Heurística para decidir si ese bin es ruido (umbral).

    Devuelve: (es_ruido, amp_dbm, nf_dbm, thr_seed_dbm)
    """
    y = np.asarray(frame.amplitudes_dbm, dtype=float)
    kernel = np.array([0.25, 0.5, 0.25], dtype=float)
    y_smooth = np.convolve(y, kernel, mode="same")

    nf = float(estimate_noise_floor(frame))
    try:
        thr_seed = float(adaptive_threshold(frame, n_sigma=n_sigma))
    except Exception:
        thr_seed = nf + 2.0

    amp = float(y[idx])
    cond1 = y_smooth[idx] <= thr_seed
    cond2 = (amp - nf) < float(min_snr_db)
    return (cond1 or cond2), amp, nf, thr_seed


def _safe_power_dbm(value: Any) -> Optional[float]:
    """Evita -inf/inf en la salida. NO inventa potencia.

    - Si es finito: devuelve float
    - Si es +-inf / NaN / error: devuelve None
    """
    try:
        v = float(value)
    except Exception:
        return None
    return v if np.isfinite(v) else None


def _match_licencia(
    *,
    fc_mhz: float,
    bw_khz: float,
    power_dbm: Optional[float],
    licencia_csv_path: Optional[str],
    dane_filtro: Optional[str] = None,
    municipio_filtro: Optional[str] = None,
    tolerancia_freq_mhz: float,
) -> Dict[str, Any]:
    """Wrapper para comparar_parametros con valores None-safe."""
    if not licencia_csv_path:
        return {"Licencia": None}

    # Evita falsos positivos: si la base contiene múltiples entidades (municipio o
    # código DANE) y no se especifica ningún filtro, NO hacemos matching.
    if dane_filtro is None and municipio_filtro is None:
        return {"Licencia": None, "reason": "filtro_no_especificado"}

    try:
        comp = comparar_parametros(
            f_medida=float(fc_mhz),
            bw_medido=float(bw_khz),
            p_medida=(float(power_dbm) if power_dbm is not None else 0.0),
            ruta_csv=licencia_csv_path,
            tolerancia_freq=float(tolerancia_freq_mhz),
            dane_filtro=dane_filtro,
            municipio_filtro=municipio_filtro,
        )
        # comparar_parametros ya devuelve claves como fc_nominal_MHz, delta_f_MHz, etc.
        return comp
    except Exception:
        return {"Licencia": "NO"}


def process_input(
    inp: PayloadInput,
    corr_csv_path: Optional[str] = None,
    licencia_csv_path: Optional[str] = None,
    dane_filtro: Optional[str] = None,
    municipio_filtro: Optional[str] = None,  # compatibilidad hacia atrás
) -> Dict[str, Any]:
    frame_json, picos, cumplimiento = unpack_input(inp)
    mode = route_mode(picos, cumplimiento)

    # En modo compliance, el matching de licencias es obligatorio
    # y requiere un filtro (preferiblemente código DANE).
    if mode == "compliance" and licencia_csv_path and (dane_filtro is None and municipio_filtro is None):
        raise ValueError("Para mode=compliance debes pasar --dane (o --municipio legacy) cuando usas --lic")

    out: Dict[str, Any] = {
        "mode": mode,
        "cumplimiento": cumplimiento,
        "picos_count": len(picos),
        "picos": picos,
        "results": [],
        "num_emissions": 0,
        "correction_applied": bool(corr_csv_path),
    }

    if "timestamp" in frame_json:
        out["timestamp"] = frame_json["timestamp"]
    if "mac" in frame_json:
        out["mac"] = frame_json["mac"]

    frame = frame_from_payload(frame_json)
    if corr_csv_path:
        frame = apply_gain_correction(frame, corr_csv_path)

    freq_axis = np.asarray(frame.freq_hz, dtype=float)

    # =========================
    # MODO all_emissions
    # =========================
    if mode == "all_emissions":
        detected_bins = [int(b) for b in detect_peak_bins(frame)]
        results: List[Dict[str, Any]] = []

        for pk in detected_bins:
            fc_hz_seed = float(freq_axis[pk])

            L, R = find_emission_span(frame, pk, margin_db=0.0)
            sub = slice_spectrum_frame(frame, L, R)
            params = measure_emission_parameters(sub, fc=fc_hz_seed, xdb=3.0, obw_percent=99.0)

            fc_mhz = float(params["fc_hz"]) / 1e6
            bw_khz = float(params["bandwidth_xdb_hz"]) / 1e3
            p_dbm = _safe_power_dbm(channel_power_dbm_uniform_bins(sub))

            row: Dict[str, Any] = {
                "nearest_bin": int(pk),
                "status": "emision",
                "fc_hz": float(params["fc_hz"]),
                "fc_mhz": fc_mhz,
                "bw_hz": float(params["bandwidth_xdb_hz"]),
                "bw_khz": bw_khz,
                "power_dbm": p_dbm,
            }

            # NOTA: en modo "all_emissions" no cruzamos con licencias.
            # La idea es que este modo solo liste emisiones encontradas y sus métricas.

            results.append(row)

        out["results"] = results
        out["num_emissions"] = len(results)
        return out

    # =========================
    # MODO peaks
    # =========================
    if mode == "peaks":
        detected_bins = [int(b) for b in detect_peak_bins(frame)]
        detected_freqs = [float(freq_axis[b]) for b in detected_bins] if detected_bins else []

        results: List[Dict[str, Any]] = []

        for p in picos:
            req_hz = pico_to_hz(p)
            req_idx = nearest_bin(freq_axis, req_hz)

            margin = match_margin_hz(req_hz)

            # match con pico detectado más cercano
            if detected_bins:
                deltas = [abs(f - req_hz) for f in detected_freqs]
                best_i = int(np.argmin(deltas))
                best_bin = int(detected_bins[best_i])
                best_f = float(detected_freqs[best_i])
                best_delta = float(deltas[best_i])
            else:
                best_bin = None
                best_f = None
                best_delta = float("inf")

            # Si no hay match cercano => ruido
            if best_f is None or best_delta > margin:
                results.append({
                    "requested_pico": p,
                    "requested_pico_hz": float(req_hz),
                    "nearest_bin": int(req_idx),
                    "fc_hz": float(freq_axis[req_idx]),
                    "fc_mhz": float(freq_axis[req_idx]) / 1e6,
                    "status": "ruido",
                    "reason": "no_hay_emision_cercana",
                    "match_margin_hz": float(margin),
                    "matched_peak_hz": (float(best_f) if best_f is not None else None),
                    "delta_match_hz": (float(best_delta) if np.isfinite(best_delta) else None),
                    "Licencia": None,
                })
                continue

            # Si hay match, ahora validamos por umbral
            es_ruido, amp_dbm, nf_dbm, thr_seed_dbm = is_ruido_por_umbral(frame, best_bin)

            if es_ruido:
                results.append({
                    "requested_pico": p,
                    "requested_pico_hz": float(req_hz),
                    "match_margin_hz": float(margin),
                    "matched_peak_hz": float(best_f),
                    "delta_match_hz": float(best_delta),
                    "nearest_bin": int(best_bin),
                    "fc_hz": float(best_f),
                    "fc_mhz": float(best_f) / 1e6,
                    "status": "ruido",
                    "reason": "por_umbral",
                    "amp_dbm": float(amp_dbm),
                    "nf_dbm": float(nf_dbm),
                    "thr_seed_dbm": float(thr_seed_dbm),
                    "Licencia": None,
                })
                continue

            # Medición de esa emisión
            L, R = find_emission_span(frame, best_bin, margin_db=0.0)
            sub = slice_spectrum_frame(frame, L, R)
            params = measure_emission_parameters(sub, fc=float(best_f), xdb=3.0, obw_percent=99.0)

            fc_mhz = float(params["fc_hz"]) / 1e6
            bw_khz = float(params["bandwidth_xdb_hz"]) / 1e3
            p_dbm = _safe_power_dbm(channel_power_dbm_uniform_bins(sub))

            row: Dict[str, Any] = {
                "requested_pico": p,
                "requested_pico_hz": float(req_hz),
                "match_margin_hz": float(margin),
                "matched_peak_hz": float(best_f),
                "delta_match_hz": float(best_delta),
                "nearest_bin": int(best_bin),
                "status": "emision",
                "fc_hz": float(params["fc_hz"]),
                "fc_mhz": fc_mhz,
                "bw_hz": float(params["bandwidth_xdb_hz"]),
                "bw_khz": bw_khz,
                "power_dbm": p_dbm,
            }

            comp = _match_licencia(
                fc_mhz=fc_mhz,
                bw_khz=bw_khz,
                power_dbm=p_dbm,
                licencia_csv_path=licencia_csv_path,
                dane_filtro=dane_filtro,
                municipio_filtro=municipio_filtro,
                tolerancia_freq_mhz=0.1,  # ±100 kHz
            )

            if comp.get("Licencia") is not None:
                row["Licencia"] = comp.get("Licencia", "NO")
                row["fc_nominal_MHz"] = comp.get("fc_nominal_MHz")
                row["delta_f_MHz"] = comp.get("delta_f_MHz")
                row["bw_nominal_kHz"] = comp.get("bw_nominal_kHz")
                row["delta_bw_kHz"] = comp.get("delta_bw_kHz")
                row["p_nominal_dBm"] = comp.get("p_nominal_dBm")
                row["delta_p_dB"] = None if p_dbm is None else comp.get("delta_p_dB")

            results.append(row)

        out["results"] = results
        out["num_emissions"] = len(results)
        return out

    # =========================
    # MODO compliance
    # =========================
    if mode == "compliance":
        if not licencia_csv_path:
            raise ValueError("Para modo compliance debes pasar --lic (ruta al CSV de licencias).")

        # Reglas de cumplimiento (ajusta según tu reglamentación):
        # - FC: debe estar dentro de ±FC_MARGIN_MHZ del nominal.
        # - BW: BW medido puede exceder el nominal por hasta +BW_MARGIN_KHZ.
        FC_MARGIN_MHZ = 0.1
        BW_MARGIN_KHZ = 10.0

        detected_bins = [int(b) for b in detect_peak_bins(frame)]
        table: List[Dict[str, Any]] = []

        for pk in detected_bins:
            fc_seed = float(freq_axis[pk])
            L, R = find_emission_span(frame, pk, margin_db=0.0)
            sub = slice_spectrum_frame(frame, L, R)
            params = measure_emission_parameters(sub, fc=fc_seed, xdb=3.0, obw_percent=99.0)

            fc_medida_MHz = float(params["fc_hz"]) / 1e6
            bw_medido_kHz = float(params["bandwidth_xdb_hz"]) / 1e3
            p_medida_dBm = _safe_power_dbm(channel_power_dbm_uniform_bins(sub))

            comp = _match_licencia(
                fc_mhz=fc_medida_MHz,
                bw_khz=bw_medido_kHz,
                power_dbm=p_medida_dBm,
                licencia_csv_path=licencia_csv_path,
                dane_filtro=dane_filtro,
                municipio_filtro=municipio_filtro,
                tolerancia_freq_mhz=FC_MARGIN_MHZ,
            )

            fc_nominal_MHz = comp.get("fc_nominal_MHz", None)
            bw_nominal_kHz = comp.get("bw_nominal_kHz", None)
            p_nominal_dBm = comp.get("p_nominal_dBm", None)

            # Delta BW (si comparar_parametros no lo trae, lo calculamos)
            delta_bw_kHz = comp.get("delta_bw_kHz", None)
            if delta_bw_kHz is None and bw_nominal_kHz is not None:
                try:
                    delta_bw_kHz = float(bw_medido_kHz) - float(bw_nominal_kHz)
                except Exception:
                    delta_bw_kHz = None

            # Cumplimiento FC/BW (solo tiene sentido si existe licencia/nominal)
            lic_match = str(comp.get("Licencia", "NO") or "NO").upper()

            delta_f_MHz = comp.get("delta_f_MHz", None)

            cumple_fc = None
            cumple_bw = None

            if lic_match == "SI" and fc_nominal_MHz is not None and delta_f_MHz is not None:
                try:
                    cumple_fc = "SI" if abs(float(delta_f_MHz)) <= FC_MARGIN_MHZ else "NO"
                except Exception:
                    cumple_fc = None

            # BW: delta_bw <= +BW_MARGIN_KHZ (si es negativo también cumple)
            if lic_match == "SI" and bw_nominal_kHz is not None and delta_bw_kHz is not None:
                try:
                    cumple_bw = "SI" if float(delta_bw_kHz) <= BW_MARGIN_KHZ else "NO"
                except Exception:
                    cumple_bw = None

            # Licencia final: SI solo si (cumple_fc y cumple_bw) son SI
            licencia_final = "NO"
            if lic_match == "SI" and (cumple_fc == "SI") and (cumple_bw == "SI"):
                licencia_final = "SI"

            # Delta potencia: si potencia medida es None, delta también None
            delta_p_dB = None if p_medida_dBm is None else comp.get("delta_p_dB", None)

            row = {
                "fc_medida_MHz": fc_medida_MHz,
                "fc_nominal_MHz": fc_nominal_MHz,
                "delta_f_MHz": delta_f_MHz,
                "bw_medido_kHz": bw_medido_kHz,
                "bw_nominal_kHz": bw_nominal_kHz,
                "delta_bw_kHz": delta_bw_kHz,
                "p_medida_dBm": p_medida_dBm,
                "p_nominal_dBm": p_nominal_dBm,
                "delta_p_dB": delta_p_dB,
                "Cumple_FC": cumple_fc,
                "Cumple_BW": cumple_bw,
                "Licencia": licencia_final,
            }
            table.append(row)

        out["results"] = table
        out["num_emissions"] = len(table)
        return out

    return out
