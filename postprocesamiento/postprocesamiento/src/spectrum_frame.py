import numpy as np
from typing import Optional

class SpectrumFrame:
    """
    Representa una captura espectral (traza de analizador o simulación).

    Args:
        amplitudes_dbm: Amplitudes espectrales en dBm (puede ser lista o np.ndarray).
        f_start_hz: Frecuencia inicial del espectro (Hz).
        f_stop_hz: Frecuencia final del espectro (Hz).
        freq_hz: (Opcional) Eje de frecuencias (Hz). Si no se pasa, se genera automáticamente.
        bin_hz: (Opcional) Resolución espectral (Hz). Si no se pasa, se calcula.
    """

    def __init__(
        self,
        amplitudes_dbm,
        f_start_hz: float,
        f_stop_hz: float,
        freq_hz: Optional[np.ndarray] = None, #Vector que contiene las frecuencias
        bin_hz: Optional[float] = None,
    ):
        # --- Conversión a numpy ---
        self.amplitudes_dbm = np.asarray(amplitudes_dbm, dtype=float)
        self.f_start_hz = float(f_start_hz)
        self.f_stop_hz = float(f_stop_hz)

        N = len(self.amplitudes_dbm)

        # --- Construcción o validación del eje de frecuencia ---
        if freq_hz is not None:
            self.freq_hz = np.asarray(freq_hz, dtype=float)
            if len(self.freq_hz) != N:
                raise ValueError("freq_hz debe tener el mismo tamaño que amplitudes_dbm.")
        else:
            self.freq_hz = np.linspace(self.f_start_hz, self.f_stop_hz, N)

        # --- Cálculo o verificación de la resolución espectral ---
        if bin_hz is not None:
            self.bin_hz = float(bin_hz)
        else:
            self.bin_hz = float((self.freq_hz[-1] - self.freq_hz[0]) / max(1, N - 1))

        # --- Validaciones ---
        if self.f_stop_hz <= self.f_start_hz:
            raise ValueError("f_stop_hz debe ser mayor que f_start_hz.")
        if N < 2:
            raise ValueError("Debe haber al menos 2 muestras espectrales.")

    # --- Métodos útiles ---
    def get_freq_axis(self) -> np.ndarray:
        """Devuelve el eje de frecuencias en Hz."""
        return self.freq_hz

    def get_bin_width(self) -> float:
        """Devuelve la resolución espectral (Hz/bin)."""
        return self.bin_hz

    def to_power_w(self) -> np.ndarray:
        """Convierte amplitudes de dBm a W/Hz."""
        return 10 ** ((self.amplitudes_dbm - 30.0) / 10.0)

    def normalize(self, ref_dbm: Optional[float] = None) -> np.ndarray:
        """Normaliza el espectro en dB relativo."""
        if ref_dbm is None:
            ref_dbm = np.max(self.amplitudes_dbm)
        return self.amplitudes_dbm - ref_dbm

    def __repr__(self):
        return (
            f"SpectrumFrame(N={len(self.amplitudes_dbm)}, "
            f"f_start_hz={self.f_start_hz:.2f}, "
            f"f_stop_hz={self.f_stop_hz:.2f}, "
            f"bin_hz={self.bin_hz:.2f})"
        )