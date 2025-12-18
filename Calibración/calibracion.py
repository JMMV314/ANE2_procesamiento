import numpy as np
import matplotlib.pyplot as plt
from scipy import signal
import os
import glob
import pandas as pd
from datetime import datetime
import sys
sys.path.append('Calibración/extraccion')
import extractor


# --- CONFIGURACIÓN KEYSIGHT ---
cfg_keysight = {
    "ip": "192.168.46.113",
    "ruta_salida": "Calibración/extraccion/Samples",
    
    "frecuencia_central_hz": int(86.23e6),
    "span_hz": int(20e6),
    "puntos": 1001,
    "rbw_hz": int(47000) #None
}

# --- CONFIGURACIÓN HACKRF ---
cfg_hackrf = {
    "ruta_salida": "Calibración/extraccion/Samples",
    "frecuencia_central_hz": int(86.23e6),
    "sample_rate_hz": int(20e6),
    "num_muestras": int(2e6),
    "lna_gain": 20,
    "vga_gain": 0,
    "amp_enable": 0
}




# ==========================================
# CONFIGURACIÓN 
# ==========================================
CFG = {
    # AJUSTA TUS RUTAS AQUÍ
    "ruta_keysight": "Calibración/extraccion/Samples",
    "ruta_hackrf": "Calibración/extraccion/Samples",
    
    # IMPORTANTE: Deben ser los mismos valores que usaste en la captura
    "center_freq": 86.23e6, 
    "sample_rate": 20e6,
    
    "nperseg": 2048, 
    "alinear_graficas":False,
}

def encontrar_archivo_mas_reciente(directorio, extension):
    patron = os.path.join(directorio, f"*{extension}")
    archivos = glob.glob(patron)
    if not archivos: return None
    return max(archivos, key=os.path.getmtime)

def leer_csv_keysight(ruta_archivo):
    print(f" -> Leyendo Keysight: {os.path.basename(ruta_archivo)}")
    try:
        df = pd.read_csv(ruta_archivo, comment='#', header=0)
        df.columns = [c.strip() for c in df.columns]
        return df['Frecuencia_Hz'].values, df['Amplitud_dBm'].values
    except Exception as e:
        print(f"⚠️ Fallback Numpy: {e}")
        data = np.loadtxt(ruta_archivo, delimiter=',', comments='#', skiprows=1)
        return data[:, 0], data[:, 1]

def procesar_hackrf_cs8(ruta_archivo, fs, center_freq, nperseg=2048):
    print(f" -> Procesando HackRF: {os.path.basename(ruta_archivo)}")
    
    # 1. Leer y Normalizar
    raw_data = np.fromfile(ruta_archivo, dtype=np.int8)
    iq_data = (raw_data[0::2] + 1j * raw_data[1::2]) / 128.0
    iq_data = iq_data - np.mean(iq_data) # Quitar DC

    # 2. Welch (sin onesided para señal compleja)
    _, psd_welch = signal.welch(iq_data, fs=fs, window='hann', nperseg=nperseg, return_onesided=False, scaling='spectrum')

    # 3. Reordenar (FFT Shift)
    psd_shifted = np.fft.fftshift(psd_welch)
    
    # 4. Construir Eje X Manualmente (Corrección de frecuencia inicial)
    num_points = len(psd_shifted)
    freqs_baseband = np.fft.fftshift(np.fft.fftfreq(num_points, d=1/fs))
    freqs_real_hz = freqs_baseband + center_freq

    # 5. Convertir a dB
    psd_db = 10 * np.log10(psd_shifted + 1e-12)

    return freqs_real_hz, psd_db

def guardar_csv_hackrf(ruta_origen, freqs, psd_db):
    nombre_base = os.path.basename(ruta_origen).replace('.cs8', '_psd_welch.csv')
    ruta_destino = os.path.join(os.path.dirname(ruta_origen), nombre_base)
    
    df = pd.DataFrame({'Frecuencia_Hz': freqs, 'PSD_dB': psd_db})
    df.to_csv(ruta_destino, index=False)
    print(f"✅ CSV Guardado: {nombre_base}")

def plot_comparacion(k_freq, k_amp, h_freq, h_amp, usar_offset=False):
    plt.figure(figsize=(12, 6))
    
    # 1. Plot Keysight (Siempre igual)
    plt.plot(k_freq / 1e6, k_amp, 'g', label='Keysight (Ref)', alpha=0.9, linewidth=1.5)
    
    # 2. Plot HackRF (Condicional)
    if usar_offset:
        # Calcular offset basado en la media de amplitud
        offset = np.mean(k_amp) - np.mean(h_amp)
        datos_plot = h_amp + offset
        label_plot = f'HackRF (Ajustado {offset:.1f} dB)'
        color_plot = 'b--'
        print(f"ℹ️ Gráfica alineada. Offset aplicado: {offset:.2f} dB")
    else:
        # Datos crudos
        datos_plot = h_amp
        label_plot = 'HackRF (Raw dB)'
        color_plot = 'b'
        print("ℹ️ Gráfica sin alinear (Valores crudos).")

    plt.plot(h_freq / 1e6, datos_plot, color_plot, label=label_plot, alpha=0.8, linewidth=1)

    plt.title("Comparación Espectral: Keysight vs HackRF")
    plt.xlabel("Frecuencia (MHz)")
    plt.ylabel("Amplitud / PSD (dB)")
    plt.legend()
    plt.grid(True, which='both', linestyle='--', alpha=0.5)
    
    # Ajustar zoom al span común
    f_min = max(np.min(k_freq), np.min(h_freq)) / 1e6
    f_max = min(np.max(k_freq), np.max(h_freq)) / 1e6
    plt.xlim(f_min, f_max)
    
    plt.tight_layout()
    plt.show()

# ==========================================
# MAIN
# ==========================================
if __name__ == "__main__":
    extractor.adquisicion_simultanea(cfg_keysight, cfg_hackrf)
    # Buscar archivos recientes
    f_k = encontrar_archivo_mas_reciente(CFG["ruta_keysight"], ".csv")
    f_h = encontrar_archivo_mas_reciente(CFG["ruta_hackrf"], ".cs8")

    if f_k and f_h:
        # Procesar
        kf, ka = leer_csv_keysight(f_k)
        hf, ha = procesar_hackrf_cs8(f_h, CFG["sample_rate"], CFG["center_freq"], CFG["nperseg"])
        
        # Guardar CSV intermedio del HackRF
        guardar_csv_hackrf(f_h, hf, ha)
        
        # Graficar con la opción seleccionada
        plot_comparacion(kf, ka, hf, ha, usar_offset=CFG["alinear_graficas"])
    else:
        print("❌ No se encontraron archivos recientes en las carpetas especificadas.")