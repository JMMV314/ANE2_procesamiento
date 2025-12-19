import numpy as np
import matplotlib.pyplot as plt
from scipy import signal
import os
import glob
import pandas as pd
import time
import sys

# Añade la ruta donde tienes tu extractor
sys.path.append('Calibracion/extraccion')
sys.path.append('Comparativa_PSD/polyphase')

try:
    import extractor
except ImportError:
    print("⚠️ No se encontró el módulo 'extractor'. Asegúrate de que la ruta sea correcta o comenta la línea de adquisición.")

try:
    import polyphase
except ImportError:
    print("⚠️ No se encontró el módulo 'Polyphase'. Asegúrate de que la ruta sea correcta o comenta la línea de adquisición.")

'''# ==========================================
# 1. CLASE POLYPHASE FILTER BANK (PFB)
# ==========================================
#class RealPolyphaseFilterBank:
#    """
#    Implementación de PFB para canalización y estimación espectral.
#    Reduce el 'spectral leakage' comparado con una FFT directa o Welch simple.
#    """
#    def __init__(self, num_channels=1024, taps_per_channel=8, window='kaiser'):
#        self.M = num_channels
#        self.K = taps_per_channel
#        self.L = self.M * self.K  # longitud total del FIR
#
#        # --- Diseño del filtro prototipo ---
#        if window == 'kaiser':
#            beta = 8.6  # ~80 dB de atenuación
#            h = signal.firwin(self.L, cutoff=1.0/self.M, window=('kaiser', beta))
#        else:
#            h = signal.firwin(self.L, cutoff=1.0/self.M, window=window)
#
#        # Normalización de energía
#        h = h / np.sqrt(np.sum(h**2))
#
#        # --- Descomposición polifásica ---
#        # h_p[p, m] = h[p + m*M]
#        self.h_poly = np.reshape(h, (self.K, self.M))
#
#    def process(self, x):
#        """
#        Procesa IQ complejos y devuelve PSD promediada.
#        """
#        # Número de bloques FFT posibles
#        n_blocks = (len(x) - self.L) // self.M
#        if n_blocks <= 0:
#            raise ValueError(f"No hay suficientes muestras ({len(x)}) para el PFB (Req: >{self.L})")
#
#        psd_acc = np.zeros(self.M, dtype=np.float64)
#
#        # Procesamiento por bloques
#        # (Se podría vectorizar más, pero este bucle es claro y funcional)
#        for b in range(n_blocks):
#            # Extraer segmento de tamaño L (se solapan implícitamente por el avance de M)
#            x_block = x[b*self.M : b*self.M + self.L]
#
#            # Matriz polifásica de datos
#            X = np.reshape(x_block, (self.K, self.M))
#
#            # Filtrado polifásico (producto punto + suma sobre los taps)
#            y = np.sum(X * self.h_poly, axis=0)
#
#            # FFT sobre la salida del filtro
#            Y = np.fft.fftshift(np.fft.fft(y))
#            
#            # Acumular potencia
#            psd_acc += np.abs(Y)**2
#
#        psd_avg = psd_acc / n_blocks
#        return psd_avg
#
## ==========================================
## 2. FUNCIONES AUXILIARES DE PROCESAMIENTO
## ==========================================
#
#def corregir_respuesta_filtro(psd):
#    """
#    Aproximación simple: Asume que el ruido debería ser plano.
#    Calcula la tendencia del piso de ruido y la invierte.
#    """
#    # Filtro de mediana para estimar el piso de ruido ignorando picos estrechos
#    noise_floor_shape = signal.medfilt(psd, kernel_size=101)
#    
#    center_val = np.median(noise_floor_shape)
#    correction_curve = center_val - noise_floor_shape
#    
#    return psd + correction_curve
#
#def procesar_hackrf_pfb(ruta_archivo, fs, center_freq, num_channels=1024, aplicar_correccion=False):
#    print(f" -> Procesando HackRF (PFB): {os.path.basename(ruta_archivo)}")
#    
#    # 1. Leer y Normalizar
#    raw_data = np.fromfile(ruta_archivo, dtype=np.int8)
#    # Convertir a complejo (I + jQ) y normalizar rango int8
#    iq_data = (raw_data[0::2] + 1j * raw_data[1::2]) / 128.0
#    
#    # 2. Instanciar y ejecutar PFB
#    # Usamos 'num_channels' como equivalente a 'nperseg' para definir resolución
#    pfb = RealPolyphaseFilterBank(num_channels=num_channels, taps_per_channel=8)
#    
#    t0 = time.time()
#    psd_raw = pfb.process(iq_data)
#    print(f"    [PFB] Procesado en {time.time()-t0:.3f} s")
#
#    # 3. Conversión de Unidades
#    # PSD física (W/Hz) aproximada
#    psd_w_hz = psd_raw / (fs * num_channels)
#
#    # Conversión a dBm/Hz
#    psd_dbm_hz = 10 * np.log10(psd_w_hz + 1e-18) + 30
#
#    # Conversión a dBm por bin (para comparar con Analizador de Espectro)
#    bin_bw = fs / num_channels
#    psd_dbm_bin = psd_dbm_hz + 10 * np.log10(bin_bw)
#
#    # 4. Eje de Frecuencias
#    freqs = np.fft.fftshift(np.fft.fftfreq(num_channels, d=1/fs)) + center_freq
#
#    # 5. Corrección opcional (aplanar respuesta del filtro)
#    if aplicar_correccion:
#        psd_final = corregir_respuesta_filtro(psd_dbm_bin)
#        print("    [Info] Corrección de filtro aplicada.")
#    else:
#        psd_final = psd_dbm_bin
#
#    return freqs, psd_final
'''

# ==========================================
# 3. CONFIGURACIÓN Y UTILIDADES
# ==========================================

# --- CONFIGURACIÓN DE ADQUISICIÓN ---
fc=int(98.7e6)

cfg_keysight = {
    "ip": "192.168.46.113",
    "ruta_salida": "Calibracion/extraccion/Samples",
    "frecuencia_central_hz": fc,
    "span_hz": int(20e6),
    "puntos": 4096,
    "rbw_hz": int(47000)
}

cfg_hackrf = {
    "ruta_salida": "Calibracion/extraccion/Samples",
    "frecuencia_central_hz": fc,
    "sample_rate_hz": int(20e6),
    "num_muestras": int(20e6),
    "lna_gain": 30,
    "vga_gain": 0,
    "amp_enable": 0
}

# --- CONFIGURACIÓN DE PROCESAMIENTO ---
CFG = {
    "ruta_keysight": "Calibracion/extraccion/Samples",
    "ruta_hackrf": "Calibracion/extraccion/Samples",
    
    "center_freq": fc, 
    "sample_rate": 20e6,
    
    # En PFB, esto define el número de canales (bins FFT)
    "nperseg": 4096, 
    
    "alinear_graficas": False,       # True: Calcula offset y alinea visualmente
    "corregir_filtro_pfb": False     # True: Aplica aplanado del piso de ruido
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

def plot_comparacion(k_freq, k_amp, h_freq, h_amp, usar_offset=False):
    plt.figure(figsize=(12, 6))
    
    # 1. Plot Keysight
    plt.plot(k_freq / 1e6, k_amp, 'g', label='Keysight (Ref)', alpha=0.9, linewidth=1.5)
    
    # 2. Plot HackRF
    if usar_offset:
        offset = np.mean(k_amp) - np.mean(h_amp)
        datos_plot = h_amp + offset
        label_plot = f'HackRF PFB (Ajustado {offset:.1f} dB)'
        estilo = 'b--'
    else:
        datos_plot = h_amp
        label_plot = 'HackRF PFB (dBm estimado)'
        estilo = 'b'

    plt.plot(h_freq / 1e6, datos_plot, estilo, label=label_plot, alpha=0.8, linewidth=1)

    plt.title("Comparación Espectral: Keysight vs HackRF (Método PFB)")
    plt.xlabel("Frecuencia (MHz)")
    plt.ylabel("Potencia (dBm)")
    plt.legend()
    plt.grid(True, which='both', linestyle='--', alpha=0.5)
    
    f_min = max(np.min(k_freq), np.min(h_freq)) / 1e6
    f_max = min(np.max(k_freq), np.max(h_freq)) / 1e6
    plt.xlim(f_min, f_max)
    
    plt.tight_layout()
    plt.show()

# ==========================================
# MAIN
# ==========================================
if __name__ == "__main__":
    # 1. Adquisición (Si el módulo extractor existe y funciona)
    if 'extractor' in sys.modules:
        try:
            print("--- Iniciando Adquisición ---")
            extractor.adquisicion_simultanea(cfg_keysight, cfg_hackrf)
        except Exception as e:
            print(f"❌ Error durante la adquisición: {e}")
    else:
        print("⚠️ Saltando adquisición (módulo no cargado). Usando archivos existentes.")

    # 2. Búsqueda de archivos
    f_k = encontrar_archivo_mas_reciente(CFG["ruta_keysight"], ".csv")
    f_h = encontrar_archivo_mas_reciente(CFG["ruta_hackrf"], ".cs8")

    if f_k and f_h:
        # 3. Procesamiento
        kf, ka = leer_csv_keysight(f_k)
        
        # AQUI ES DONDE USAMOS EL NUEVO MÉTODO PFB
        hf, ha = polyphase.procesar_hackrf_pfb(
            f_h, 
            fs=CFG["sample_rate"], 
            center_freq=CFG["center_freq"], 
            num_channels=CFG["nperseg"],
            aplicar_correccion=CFG["corregir_filtro_pfb"]
        )
        
        # 4. Guardar CSV intermedio
        nombre_base = os.path.basename(f_h).replace('.cs8', '_psd_pfb.csv')
        ruta_csv_pfb = os.path.join(os.path.dirname(f_h), nombre_base)
        df_out = pd.DataFrame({'Frecuencia_Hz': hf, 'PSD_dBm': ha})
        df_out.to_csv(ruta_csv_pfb, index=False)
        print(f"✅ CSV PFB Guardado: {ruta_csv_pfb}")
        
        # 5. Graficar
        plot_comparacion(kf, ka, hf, ha, usar_offset=CFG["alinear_graficas"])
    else:
        print("❌ No se encontraron archivos recientes en las carpetas especificadas.")