from __future__ import annotations

from typing import Any, Dict, Optional, Tuple
import math
import re
import unicodedata

import numpy as np
import pandas as pd


def _to_float_series(series: pd.Series) -> pd.Series:
    """Convierte una serie a float de forma robusta.

    Soporta formatos comunes en exportaciones:
      - Decimal con coma: "88,9"
      - Miles con punto y decimal con coma: "1.234,56"
      - Espacios / NBSP / texto sucio
    """
    s = series.astype(str)
    s = s.str.strip().str.replace("\u00A0", " ", regex=False)

    # Dejar solo caracteres típicos de números (incluye signos y separadores)
    # (no borremos la coma/punto aún)
    s = s.str.replace(r"[^0-9,\.\-\+]", "", regex=True)

    def _norm_num(x: str) -> str:
        x = (x or "").strip()
        if not x:
            return ""
        # Si tiene '.' y ',' asumimos formato 1.234,56 => quitar miles '.' y cambiar ',' por '.'
        if "," in x and "." in x:
            x = x.replace(".", "")
            x = x.replace(",", ".")
            return x
        # Si solo tiene ',' lo tomamos como decimal
        if "," in x and "." not in x:
            return x.replace(",", ".")
        return x

    s = s.map(_norm_num)
    return pd.to_numeric(s, errors="coerce")


def _norm_dane(x: Any) -> str:
    """Normaliza código DANE.

    - Conserva ceros a la izquierda.
    - Extrae solo dígitos.
    - Si tiene menos de 5 dígitos, rellena con ceros a la izquierda.
    """
    if x is None:
        return ""
    digits = re.sub(r"\D", "", str(x))
    if not digits:
        return ""
    if len(digits) < 5:
        digits = digits.zfill(5)
    return digits


def _norm_text(s: Any) -> str:
    """Normaliza texto: uppercase, sin tildes, espacios colapsados."""
    if s is None:
        return ""
    s = str(s).strip().upper()
    s = unicodedata.normalize("NFKD", s)
    s = "".join(ch for ch in s if not unicodedata.combining(ch))
    # normaliza separadores raros
    s = s.replace("\u00A0", " ")
    s = " ".join(s.split())
    return s


def _read_licencias_csv(path: str) -> pd.DataFrame:
    """
    Lee licencias robustamente:
    - soporta separador ';' o ',' (algunas bases vienen en uno u otro)
    - preferimos engine C (más estable en archivos grandes)
    """
    # 1) intentar con ';' (muy común en exportaciones ANE)
    try:
        df = pd.read_csv(path, sep=";", encoding="utf-8", on_bad_lines="skip")
    except Exception:
        # Fallback de encoding típico en Windows
        df = pd.read_csv(path, sep=";", encoding="latin-1", on_bad_lines="skip")

    # 2) si quedó como una sola columna, probablemente es ','
    if df.shape[1] == 1:
        try:
            df = pd.read_csv(path, sep=",", encoding="utf-8", on_bad_lines="skip")
        except Exception:
            df = pd.read_csv(path, sep=",", encoding="latin-1", on_bad_lines="skip")
    # Si viene BOM o nombres raros:
    df.columns = [c.strip().replace("\ufeff", "") for c in df.columns]
    return df


def _si_no(x: Any) -> str:
    """Normaliza a 'SI'/'NO' (sin tildes ni caracteres raros)."""
    if x is None:
        return "NO"
    s = str(x).strip().upper()
    # Normalizaciones comunes
    s = (
        s.replace("SÍ", "SI")
        .replace("SÍ", "SI")
        .replace("SÝ", "SI")
        .replace("YES", "SI")
        .replace("TRUE", "SI")
    )
    if s in ("SI", "S", "1"):
        return "SI"
    return "NO"


def _power_to_dbm(p: float, unit: str) -> float:
    """Convierte potencia (varias unidades) a dBm.

    Soporta: dBm, dBW, W, kW, mW, uW (y variantes en texto).
    Si llega "dB" (ambigua), se asume dBm para mantener la impresión en dBm.
    """
    u = _norm_text(unit)
    if not np.isfinite(p):
        return float("nan")

    # ya está en dBm
    if u in ("DBM", "DB", "DBM."):
        return float(p)

    # dBW -> dBm
    if u in ("DBW", "DBW."):
        return float(p) + 30.0

    # potencia lineal -> dBm
    # Normalizamos algunas variantes
    if u in ("MW", "M W", "MILLIWATT", "MILLIWATTS"):
        if p <= 0:
            return float("nan")
        # mW a W: p/1000
        return 10.0 * math.log10(p)  # 10*log10(mW) ya está referido a 1mW

    if u in ("UW", "U W", "MICROWATT", "MICROWATTS"):
        if p <= 0:
            return float("nan")
        # uW -> mW: p/1000
        return 10.0 * math.log10(p) - 30.0

    if u in ("W", "WATTS", "WATT"):
        if p <= 0:
            return float("nan")
        return 10.0 * math.log10(p) + 30.0

    if u in ("KW", "K W", "KILOWATT", "KILOWATTS"):
        if p <= 0:
            return float("nan")
        return 10.0 * math.log10(p * 1000.0) + 30.0

    if u in ("MWATT", "MEGAWATT", "MEGAWATTS"):
        if p <= 0:
            return float("nan")
        return 10.0 * math.log10(p * 1e6) + 30.0

    # si no reconoce, intenta como W si es positiva
    if p > 0:
        return 10.0 * math.log10(p) + 30.0
    return float("nan")


def comparar_parametros(
    f_medida: float,                 # MHz
    bw_medido: float,                # kHz
    p_medida: float,                 # dBm
    ruta_csv: str,
    tolerancia_freq: float = 0.1,    # MHz
    dane_filtro: Optional[str] = None,
    municipio_filtro: Optional[str] = None,  # compatibilidad hacia atrás
) -> Dict[str, Any]:
    """
    Encuentra la licencia que mejor calza por frecuencia (principal),
    opcionalmente filtrando por *codigo_dane* (nuevo) o por municipio (legado).

    Devuelve:
      - Licencia: "SI" o "NO"
      - fc_nominal_MHz, bw_nominal_kHz, p_nominal_dBm
      - delta_f_MHz, delta_bw_kHz, delta_p_dB
    """
    df = _read_licencias_csv(ruta_csv)

    # Validación mínima de columnas esperadas
    expected_base = {"frecuencia", "ancho_de_banda", "unidad_ancho_de_banda", "potencia", "unidad_potencia"}
    missing_base = [c for c in expected_base if c not in set(df.columns)]
    if missing_base:
        raise ValueError(
            f"CSV licencias no tiene columnas esperadas. Faltan: {missing_base}. Columnas: {list(df.columns)}"
        )

    has_dane = "codigo_dane" in set(df.columns)
    has_mun = "municipio" in set(df.columns)
    if not (has_dane or has_mun):
        raise ValueError(
            "CSV licencias debe contener columna 'codigo_dane' (nuevo) o 'municipio' (legado). "
            f"Columnas: {list(df.columns)}"
        )

    df2 = df.copy()

    # Filtro por DANE (preferido)
    dane_req = str(dane_filtro).strip() if dane_filtro is not None else ""
    if dane_req:
        if not has_dane:
            raise ValueError("Se recibió dane_filtro pero el CSV no tiene columna 'codigo_dane'.")

        def _norm_dane_str(x: Any) -> str:
            """Normaliza un código DANE a sólo dígitos, sin perder ceros a la izquierda."""
            if x is None:
                return ""
            s = str(x).strip()
            # quita .0 si viene como 11001.0
            if re.fullmatch(r"\d+\.0", s):
                s = s[:-2]
            digits = re.sub(r"\D", "", s)
            if not digits:
                return ""
            # Municipios son 5 dígitos; algunos archivos traen 4 (sin cero inicial)
            if len(digits) < 5:
                digits = digits.zfill(5)
            return digits

        df2["_dane_norm"] = df2["codigo_dane"].apply(_norm_dane_str)
        dane_req_n = _norm_dane_str(dane_req)
        # Si el usuario pasa el municipio (5 dígitos), acepta también códigos extendidos que empiecen por esos 5.
        if len(dane_req_n) == 5:
            df2 = df2[df2["_dane_norm"].str.startswith(dane_req_n, na=False)].copy()
        else:
            df2 = df2[df2["_dane_norm"] == dane_req_n].copy()

    # Filtro por municipio (legado)
    mun_req = _norm_text(municipio_filtro) if municipio_filtro else ""
    if (not dane_req) and mun_req:
        if not has_mun:
            raise ValueError("Se recibió municipio_filtro pero el CSV no tiene columna 'municipio'.")
        df2["_mun_norm"] = df2["municipio"].apply(_norm_text)
        df2 = df2[df2["_mun_norm"] == mun_req].copy()
        # si por algún motivo el municipio viene con dobles espacios o variantes,
        # al menos intenta contención suave:
        if df2.empty:
            df2 = df[df["municipio"].apply(_norm_text).str.contains(mun_req, na=False)].copy()

    # Frecuencia nominal (MHz)
    # Ojo: en muchas exportaciones viene como "88,9" (coma decimal) y pd.to_numeric lo vuelve NaN.
    fc_nom = _to_float_series(df2["frecuencia"]).astype(float)
    df2["_fc_nom_mhz"] = fc_nom

    # Candidatos por frecuencia
    df2 = df2[np.isfinite(df2["_fc_nom_mhz"])]
    if df2.empty:
        return {"Licencia": "NO"}

    df2["_delta_f_mhz"] = df2["_fc_nom_mhz"] - float(f_medida)
    dfcand = df2[np.abs(df2["_delta_f_mhz"]) <= float(tolerancia_freq)].copy()
    if dfcand.empty:
        return {"Licencia": "NO"}

    # BW nominal (kHz) (si existe)
    bw_nom = _to_float_series(dfcand["ancho_de_banda"]).astype(float)
    dfcand["_bw_nom_khz"] = bw_nom

    # Potencia nominal a dBm
    p_nom = _to_float_series(dfcand["potencia"]).astype(float)
    dfcand["_p_nom_dbm"] = [
        _power_to_dbm(float(p), str(u))
        for p, u in zip(p_nom.values, dfcand["unidad_potencia"].values)
    ]

    # Score: prioriza |delta_f| (lo más importante).
    # Si hay empate, usa BW más cercano (si está disponible).
    dfcand["_score"] = np.abs(dfcand["_delta_f_mhz"])
    if np.any(np.isfinite(dfcand["_bw_nom_khz"].values)):
        dfcand["_score"] = dfcand["_score"] + 1e-3 * np.abs(dfcand["_bw_nom_khz"] - float(bw_medido)).fillna(0.0)

    best = dfcand.sort_values("_score", ascending=True).iloc[0]

    fc_nominal_MHz = float(best["_fc_nom_mhz"])
    bw_nominal_kHz = float(best["_bw_nom_khz"]) if np.isfinite(best["_bw_nom_khz"]) else None
    p_nominal_dBm = float(best["_p_nom_dbm"]) if np.isfinite(best["_p_nom_dbm"]) else None

    delta_f_MHz = float(f_medida - fc_nominal_MHz)
    delta_bw_kHz = (float(bw_medido) - bw_nominal_kHz) if bw_nominal_kHz is not None else None
    delta_p_dB = (float(p_medida) - p_nominal_dBm) if p_nominal_dBm is not None else None

    return {
        "Licencia": "SI",
        "fc_nominal_MHz": fc_nominal_MHz,
        "bw_nominal_kHz": bw_nominal_kHz,
        "p_nominal_dBm": p_nominal_dBm,
        "delta_f_MHz": delta_f_MHz,
        "delta_bw_kHz": delta_bw_kHz,
        "delta_p_dB": delta_p_dB,
    }