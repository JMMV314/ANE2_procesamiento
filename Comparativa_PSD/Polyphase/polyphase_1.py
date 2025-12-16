import os
import subprocess
import numpy as np
import matplotlib.pyplot as plt
from scipy import signal
import time

# =========================================================
# 1. CLASE POLYPHASE FILTER BANK (VERSIÓN CPU OPTIMIZADA)
# =========================================================

class PolyphaseFilterBank:
    """
    Implementación del Banco de Filtros Polifásicos (PFB) usando solo CPU (NumPy).
    Utiliza operaciones vectorizadas para calcular la PSD eficientemente.
    """
    def __init__(self, num_channels=1024, window='kaiser'):
        self.M = num_channels
        
        # 1. Diseño del Filtro Prototipo (Ventana)
        # Para un PFB, diseñamos un filtro paso bajo y lo usamos como ventana
        # Esto reduce el "leakage" espectral mucho mejor que una FFT directa.
        if window == 'kaiser':
            # Beta 6.0 da buena atenuación (~60dB)
            self.window = signal.kaiser(self.M, beta=6.0)
        elif window == 'hamming':
            self.window = signal.hamming(self.M)
        else:
            self.window = signal.blackman(self.M)
            
        # Normalización de energía de la ventana
        self.window = self.window.astype(np.float32)
        scale = np.sum(self.window**2)
        self.window /= np.sqrt(scale)

    def process_data(self, samples):
        """
        Calcula la Densidad Espectral de Potencia (PSD) promediada.
        """
        # 1. Truncar datos para que sean múltiplo exacto del número de canales (M)
        n_samples = len(samples)
        num_blocks = n_samples // self.M
        trunc_len = num_blocks * self.M
        samples = samples[:trunc_len]
        
        # 2. Reshape (Vectorización)
        # Transformamos el array 1D en una matriz (Bloques x Canales)
        # Esto nos permite aplicar la FFT a todos los bloques simultáneamente.
        matrix = samples.reshape(num_blocks, self.M)
        
        # 3. Aplicar Ventana (Polyphase broadcasting)
        # Multiplicamos cada bloque por la ventana diseñada
        windowed_matrix = matrix * self.window
        
        # 4. FFT Masiva (Batch processing)
        # Calculamos la FFT sobre el eje de las filas (axis=1)
        spectrum = np.fft.fft(windowed_matrix, axis=1)
        
        # Shift para centrar la frecuencia 0 en el medio
        spectrum = np.fft.fftshift(spectrum, axes=1)
        
        # 5. Calcular Potencia
        # Potencia = |Magnitud|^2
        power_matrix = np.abs(spectrum) ** 2
        
        # 6. Promedio (Average PSD)
        # Promediamos todos los bloques temporales para obtener una sola gráfica espectral limpia
        avg_psd = np.mean(power_matrix, axis=0)
        
        return avg_psd

# =========================================================
# 2. FUNCIONES DE UTILIDAD
# =========================================================

def adquirir_hackrf(config):
    # Si el archivo existe, lo borramos para asegurar datos frescos
    if os.path.exists(config['nombre_archivo']):
        print(f"[HW] Borrando archivo antiguo...")
        try:
            os.remove(config['nombre_archivo'])
        except OSError:
            pass

    print(f"\n[HW] Capturando {config['nombre_archivo']}...")
    
    cmd = [
        "hackrf_transfer", 
        "-r", config['nombre_archivo'],
        "-f", str(config['frecuencia_central']), 
        "-s", str(config['ancho_banda']),
        "-n", str(config['num_muestras']), 
        "-l", str(config['lna']),
        "-g", str(config['vga']), 
        "-a", str(config['amp'])
    ]
    
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    if result.returncode != 0:
        print(f"❌ Error en hackrf_transfer: {result.stderr}")
        return False
        
    print("✅ Captura completada.")
    return True

def procesar_con_pfb(filename, config_hw):
    """
    Lee el binario, convierte a IQ y procesa con PFB (CPU).
    """
    # 1. Leer archivo binario
    try:
        raw_data = np.fromfile(filename, dtype=np.int8)
    except FileNotFoundError:
        print("❌ Error: Archivo no encontrado.")
        return None, None

    print(f"[PFB] Archivo cargado: {len(raw_data)/1e6:.1f} MB. Convirtiendo a Complex...")
    
    # 2. Conversión a Complex64
    raw_data = raw_data.astype(np.float32)
    i = raw_data[0::2]
    q = raw_data[1::2]
    complex_iq = (i + 1j * q) / 127.5 # Normalizar -1 a 1
    
    # 3. Instanciar PFB
    # NUM_CHANNELS define la resolución de la FFT (ancho de cada bin)
    NUM_CHANNELS = 1024 
    pfb = PolyphaseFilterBank(num_channels=NUM_CHANNELS, window='kaiser')
    
    # 4. Procesar
    print(f"[PFB] Ejecutando FFT vectorizada en CPU ({NUM_CHANNELS} canales)...")
    t0 = time.time()
    
    psd_linear = pfb.process_data(complex_iq)
    
    t1 = time.time()
    print(f"[PFB] Procesamiento completado en {t1-t0:.3f} segundos.")
    
    # 5. Escalar a dB
    # Añadimos un pequeño epsilon (1e-12) para evitar log(0)
    psd_db = 10 * np.log10(psd_linear + 1e-12)
    
    # Offset de calibración visual (ajustar según necesidad)
    psd_db -= 30 

    # 6. Ejes de Frecuencia
    fs = config_hw['ancho_banda']
    fc = config_hw['frecuencia_central']
    
    # Crear array de frecuencias centradas
    freqs_rel = np.fft.fftshift(np.fft.fftfreq(NUM_CHANNELS, d=1/fs))
    freqs_abs = freqs_rel + fc
    
    return freqs_abs, psd_db

def plot_espectro_completo(freqs, psd, config):
    """
    Grafica el resultado
    """
    fc_mhz = config['frecuencia_central'] / 1e6
    bw_mhz = config['ancho_banda'] / 1e6
    f_axis_mhz = freqs / 1e6

    plt.style.use('dark_background')
    fig, ax = plt.subplots(figsize=(12, 7))
    
    # Color cian neón para la señal
    ax.plot(f_axis_mhz, psd, color='#00ff99', linewidth=0.8, label=f'PSD (PFB {len(psd)} bins)')
    
    # --- Metadatos en caja ---
    info_text = (
        f"$\\mathbf{{Parámetros de HW}}$\n"
        f"Fc: {fc_mhz:.1f} MHz\n"
        f"BW: {bw_mhz:.1f} MHz\n"
        f"Muestras: {config['num_muestras']/1e6:.1f} M\n"
        f"LNA: {config['lna']} dB | VGA: {config['vga']} dB"
    )
    props = dict(boxstyle='round', facecolor='#222222', alpha=0.8, edgecolor='white')
    ax.text(0.02, 0.95, info_text, transform=ax.transAxes, fontsize=10,
            verticalalignment='top', bbox=props, color='white', fontfamily='monospace')

    # --- Etiquetas y Picos ---
    ax.set_title(f"Espectro de Radiofrecuencia - PFB Analysis", fontsize=14, pad=15)
    ax.set_xlabel("Frecuencia (MHz)")
    ax.set_ylabel("Magnitud (dB)")
    ax.grid(True, which='both', linestyle='--', linewidth=0.5, alpha=0.3)
    
    # Zoom dinámico en Y
    ax.set_ylim(np.min(psd) - 5, np.max(psd) + 15)
    ax.set_xlim(np.min(f_axis_mhz), np.max(f_axis_mhz))

    # Anotación del pico máximo
    max_idx = np.argmax(psd)
    max_freq = f_axis_mhz[max_idx]
    max_pow = psd[max_idx]
    ax.annotate(f'Max: {max_freq:.2f} MHz\n{max_pow:.1f} dB',
                xy=(max_freq, max_pow), xytext=(max_freq, max_pow+10),
                arrowprops=dict(facecolor='yellow', shrink=0.05, width=1, headwidth=5),
                color='yellow', ha='center', fontsize=9)

    plt.tight_layout()
    plt.show()

# =========================================================
# BLOQUE MAIN
# =========================================================

mis_capturas = [
    {
        "nombre_archivo": "/home/jmmv/ANE2/polyphase/98MHz_FM",
        "frecuencia_central": 98*1000000, 
        "ancho_banda": 20*1000000, 
        "num_muestras": 20*1000000, 
        "lna": 40, 
        "vga": 62, 
        "amp": 0
    }
]

if __name__ == "__main__":
    for cfg in mis_capturas:
        if adquirir_hackrf(cfg):
            f, p = procesar_con_pfb(cfg['nombre_archivo'], cfg)
            
            if f is not None:
                plot_espectro_completo(f, p, cfg)