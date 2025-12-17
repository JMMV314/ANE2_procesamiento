import os
import subprocess
from pathlib import Path

def adquirir_hackrf_cs8(
    ruta_salida,
    frecuencia_central_hz,
    sample_rate_hz,
    num_muestras,
    lna=16,
    vga=0,
    amp=0,
    sobrescribir=True,
):
    """
    Captura IQ con HackRF y guarda en formato CS8:
    - I/Q intercalado: I0,Q0,I1,Q1,...
    - cada componente es int8 (signed 8-bit)
    """

    ruta_salida = Path(ruta_salida)

    # Crear carpeta si no existe
    ruta_salida.parent.mkdir(parents=True, exist_ok=True)

    # Borrar si existe
    if ruta_salida.exists():
        if sobrescribir:
            ruta_salida.unlink()
        else:
            raise FileExistsError(f"El archivo ya existe: {ruta_salida}")

    cmd = [
        "hackrf_transfer",
        "-r", str(ruta_salida),                 # salida CS8
        "-f", str(int(frecuencia_central_hz)),  # Hz
        "-s", str(int(sample_rate_hz)),         # Hz (sample rate)
        "-n", str(int(num_muestras)),           # # muestras complejas
        "-l", str(int(lna)),                    # LNA gain (0-40, step 8)
        "-g", str(int(vga)),                    # VGA gain (0-62, step 2)
        "-a", str(int(amp)),                    # amp (0/1)
    ]

    print("[HW] Ejecutando:", " ".join(cmd))
    result = subprocess.run(cmd, capture_output=True, text=True)

    if result.returncode != 0:
        raise RuntimeError(
            "Fallo hackrf_transfer:\n"
            f"STDERR:\n{result.stderr}\n"
            f"STDOUT:\n{result.stdout}\n"
        )

    # Validación simple
    size_bytes = ruta_salida.stat().st_size
    print(f"✅ Captura OK -> {ruta_salida} ({size_bytes} bytes, CS8)")
    return str(ruta_salida)


if __name__ == "__main__":
    cfg = {
        "ruta_salida": "/home/gcpds/Desktop/Procesamiento_ANE2/ANE2_procesamiento/Adquisicion/Adquisition",
        "frecuencia_central_hz": int(86.23e6),
        "sample_rate_hz": int(20e6),
        "num_muestras": int(2e6),
        "lna": 20,
        "vga": 0,
        "amp": 0,
    }

    adquirir_hackrf_cs8(**cfg)
