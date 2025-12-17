from __future__ import annotations

from typing import Any, Dict, Sequence

import numpy as np

from .spectrum_frame import SpectrumFrame


def _get_first(payload: Dict[str, Any], keys: Sequence[str]) -> Any:
    """Return the first non-null key found in payload."""
    for k in keys:
        if k in payload and payload[k] is not None:
            return payload[k]
    return None


def frame_from_payload(payload: Dict[str, Any]) -> SpectrumFrame:
    """
    Build a SpectrumFrame from a JSON-like payload.

    Expected (common) keys:
      - Pxx: list[float] amplitudes/power in dBm (or dB scale)
      - start_freq_hz: float
      - end_freq_hz: float

    It also tolerates a few alternative key names.
    """
    pxx = _get_first(payload, ["Pxx", "pxx", "PSD", "psd", "amplitudes_dbm", "amplitudes"])
    if pxx is None:
        raise KeyError("Payload no contiene 'Pxx' (ni alias compatibles).")

    f_start = _get_first(payload, ["start_freq_hz", "f_start_hz", "start_hz", "f_start", "start_freq"])
    f_stop = _get_first(payload, ["end_freq_hz", "f_stop_hz", "stop_hz", "f_stop", "end_freq", "stop_freq"])
    if f_start is None or f_stop is None:
        raise KeyError("Payload debe incluir 'start_freq_hz' y 'end_freq_hz' (o alias compatibles).")

    amps = np.asarray(pxx, dtype=float).reshape(-1)
    if amps.size < 4:
        raise ValueError("Pxx demasiado corto para análisis espectral.")

    return SpectrumFrame(
        amplitudes_dbm=amps,
        f_start_hz=float(f_start),
        f_stop_hz=float(f_stop),
    )