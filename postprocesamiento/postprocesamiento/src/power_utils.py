from __future__ import annotations

from typing import Optional

import numpy as np

from .spectrum_frame import SpectrumFrame


def channel_power_dbm_uniform_bins(
    frame: SpectrumFrame,
    *,
    fc_hz: Optional[float] = None,
    bw_hz: Optional[float] = None,
) -> float:
    """Potencia integrada en dBm asumiendo amplitudes en dBm/Hz.

    Por qué existe:
      - `measure_channel_power()` integra con `np.trapz(..., x=freq)`.
      - En algunos espectros (sobre todo spans muy cortos con pocos bins),
        se han observado resultados no finitos (p.ej. -inf) por situaciones
        numéricas raras.

    Qué hace distinto:
      - Integra por suma discreta usando el `bin_hz` del frame (o un fallback
        robusto basado en la mediana de |df| si bin_hz no está disponible).
      - Esto evita integrales negativas/NaN por ejes no estrictamente
        monótonos o acumulación de cancelaciones.

    Args:
        frame: SpectrumFrame con `amplitudes_dbm` en dBm/Hz y `freq_hz`.
        fc_hz: frecuencia central para recortar (opcional).
        bw_hz: ancho de banda para recortar (opcional).

    Returns:
        Potencia en dBm (float). Si no hay datos válidos, retorna -inf.
    """

    f = np.asarray(frame.freq_hz, dtype=float)
    if f.ndim != 1:
        f = f.reshape(-1)

    p_w_per_hz = frame.to_power_w()  # W/Hz, siempre >= 0 para dBm finitos
    p_w_per_hz = np.asarray(p_w_per_hz, dtype=float)

    if fc_hz is not None and bw_hz is not None:
        lo = float(fc_hz) - float(bw_hz) / 2.0
        hi = float(fc_hz) + float(bw_hz) / 2.0
        m = (f >= lo) & (f <= hi)
        f = f[m]
        p_w_per_hz = p_w_per_hz[m]

    if f.size == 0 or p_w_per_hz.size == 0:
        return float("-inf")

    # Limpiar NaN/inf por robustez (si llegaran a aparecer)
    finite = np.isfinite(f) & np.isfinite(p_w_per_hz)
    f = f[finite]
    p_w_per_hz = p_w_per_hz[finite]
    if f.size == 0:
        return float("-inf")

    # Estimar el ancho de bin
    bin_hz = getattr(frame, "bin_hz", None)
    try:
        bin_hz = float(bin_hz) if bin_hz is not None else float("nan")
    except Exception:
        bin_hz = float("nan")

    if not np.isfinite(bin_hz) or bin_hz <= 0.0:
        if f.size >= 2:
            df = np.diff(f)
            df_abs = np.abs(df[np.isfinite(df)])
            bin_hz = float(np.median(df_abs)) if df_abs.size else float("nan")
        else:
            bin_hz = float("nan")

    if not np.isfinite(bin_hz) or bin_hz <= 0.0:
        # No se puede integrar correctamente
        return float("-inf")

    # Integración discreta: sum(p_w_per_hz) * bin_hz
    p_w = float(np.sum(p_w_per_hz) * bin_hz)

    if not np.isfinite(p_w) or p_w <= 0.0:
        # Último intento: integrar sobre eje ordenado (si hay datos)
        if f.size >= 2:
            order = np.argsort(f)
            p_w_try = float(np.trapz(p_w_per_hz[order], f[order]))
            if np.isfinite(p_w_try) and p_w_try > 0.0:
                p_w = p_w_try

    if not np.isfinite(p_w) or p_w <= 0.0:
        return float("-inf")

    return 10.0 * float(np.log10(p_w / 1e-3))
