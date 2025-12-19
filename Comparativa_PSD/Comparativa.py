import time
import os
import subprocess
import ctypes
import numpy as np
import matplotlib.pyplot as plt
from scipy import signal

# =========================================================
# CONFIGURACIÓN GENERAL
# =========================================================
LIB_PSD_PATH = os.path.abspath("/home/gcpds/Desktop/Procesamiento_ANE2/ANE2_procesamiento/Comparativa_PSD/Welch/Algoritmo_welch_c/libs/libpsd.so") # AJUSTA ESTA RUTA SI ES NECESARIO
#FILENAME_BIN = "/home/gcpds/Desktop/Procesamiento_ANE2/ANE2_procesamiento/Comparativa_PSD/Polyphase98MHz_FM"
FILENAME_BIN = "/home/gcpds/Desktop/Procesamiento_ANE2/ANE2_procesamiento/Adquisicion/Adquisition"

# =========================================================
# 1. WRAPPER PARA WELCH (C/CTYPES)
# =========================================================
# Definiciones CTypes
HAMMING_TYPE = 0
class SignalIQ(ctypes.Structure):
    _fields_ = [("signal_iq", ctypes.c_void_p), ("n_signal", ctypes.c_size_t)]

class PsdConfig(ctypes.Structure):
    _fields_ = [("window_type", ctypes.c_int), ("sample_rate", ctypes.c_double),
                ("nperseg", ctypes.c_int), ("noverlap", ctypes.c_int)]

class WelchEngine:
    def __init__(self, lib_path):
        self.lib = None
        try:
            self.lib = ctypes.CDLL(lib_path)
            self._setup_types()
            print(f"[Welch] Librería C cargada correctamente.")
        except OSError:
            print(f"❌ [Welch] Error: No se pudo cargar {lib_path}")

    def _setup_types(self):
        self.lib.load_iq_from_buffer.argtypes = [ctypes.POINTER(ctypes.c_int8), ctypes.c_size_t]
        self.lib.load_iq_from_buffer.restype = ctypes.POINTER(SignalIQ)
        self.lib.execute_welch_psd.argtypes = [ctypes.POINTER(SignalIQ), ctypes.POINTER(PsdConfig),
                                               ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double)]
        self.lib.free_signal_iq.argtypes = [ctypes.POINTER(SignalIQ)]
        self.lib.scale_psd.argtypes = [ctypes.POINTER(ctypes.c_double), ctypes.c_int, ctypes.c_char_p]

    def process(self, raw_bytes, fs, fc, nfft=4096):
        if not self.lib: return None, None
        
        buffer_len = len(raw_bytes)
        c_buffer = (ctypes.c_int8 * buffer_len).from_buffer_copy(raw_bytes)
        
        # Carga
        sig_ptr = self.lib.load_iq_from_buffer(c_buffer, buffer_len)
        
        # Config
        cfg = PsdConfig()
        cfg.window_type = HAMMING_TYPE
        cfg.sample_rate = float(fs)
        cfg.nperseg = nfft
        cfg.noverlap = int(nfft * 0.5)
        
        f_out = np.zeros(nfft, dtype=np.float64)
        p_out = np.zeros(nfft, dtype=np.float64)
        
        # Ejecución
        self.lib.execute_welch_psd(sig_ptr, ctypes.byref(cfg), 
                                   f_out.ctypes.data_as(ctypes.POINTER(ctypes.c_double)), 
                                   p_out.ctypes.data_as(ctypes.POINTER(ctypes.c_double)))
        
        # Escalar
        self.lib.scale_psd(p_out.ctypes.data_as(ctypes.POINTER(ctypes.c_double)), nfft, b"dBm")
        self.lib.free_signal_iq(sig_ptr)
        
        return f_out + fc, p_out

# =========================================================
# 2. WRAPPER PARA POLYPHASE (PYTHON PURE)
# =========================================================
class RealPolyphaseFilterBank:
    def __init__(self, num_channels, taps_per_channel=8):
        self.M = num_channels
        self.K = taps_per_channel
        self.L = self.M * self.K
        # Diseño simplificado para el benchmark (filtro Kaiser)
        h = signal.firwin(self.L, cutoff=1/self.M, window=('kaiser', 8.6))
        h = h / np.sqrt(np.sum(h**2))
        self.h_poly = np.reshape(h, (self.K, self.M))

    def process(self, iq_data):
        n_blocks = (len(iq_data) - self.L) // self.M
        if n_blocks <= 0: return np.zeros(self.M)
        
        psd_acc = np.zeros(self.M, dtype=np.float64)
        
        # Bucle crítico (donde Python suele sufrir vs C)
        for b in range(n_blocks):
            x_block = iq_data[b*self.M : b*self.M + self.L]
            X = np.reshape(x_block, (self.K, self.M))
            y = np.sum(X * self.h_poly, axis=0) # Filtrado
            Y = np.fft.fftshift(np.fft.fft(y))
            psd_acc += np.abs(Y)**2
            
        return psd_acc / n_blocks

def run_polyphase(raw_bytes, fs, fc, n_channels=1024):
    # Conversión IQ en Python (costosa)
    raw = np.frombuffer(raw_bytes, dtype=np.int8).astype(np.float32)
    iq = (raw[0::2] + 1j*raw[1::2]) / 128.0
    
    pfb = RealPolyphaseFilterBank(n_channels, taps_per_channel=8)
    psd = pfb.process(iq)
    
    # Escalamiento aproximado a dBm para comparar visualmente
    psd_w_hz = psd / (fs * n_channels)
    psd_dbm = 10 * np.log10(psd_w_hz + 1e-20) + 30 + 10 * np.log10(fs/n_channels)
    
    freqs = np.fft.fftshift(np.fft.fftfreq(n_channels, d=1/fs)) + fc
    return freqs, psd_dbm

# =========================================================
# 3. UTILIDADES DE ADQUISICIÓN Y PLOT
# =========================================================
def adquirir_hardware(config):
    # Borrar archivo anterior si existe
    if os.path.exists(config['filename']): 
        os.remove(config['filename'])
        
    print(f"[HW] HackRF -> LNA:{config['lna']} | VGA:{config['vga']} | AMP:{config['amp']}")
    
    cmd = [
        "hackrf_transfer", 
        "-r", config['filename'], 
        "-f", str(config['fc']),
        "-s", str(config['bw']), 
        "-n", str(config['samples']),
        # AQUI ESTA EL CAMBIO: Leemos del config
        "-l", str(config['lna']),  # LNA Gain (0-40, pasos de 8)
        "-g", str(config['vga']),  # VGA Gain (0-62, pasos de 2)
        "-a", str(config['amp'])   # Amp (0 = OFF, 1 = ON)
    ]
    
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    if result.returncode != 0:
        print(f"❌ Error HackRF: {result.stderr}")
        return False
        
    return os.path.exists(config['filename'])

def plot_comparativa(results, titulo):
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 10))
    
    # Plot 1: Espectros
    ax1.set_title(f"Comparativa Espectral: {titulo}")
    for res in results:
        ax1.plot(res['freqs']/1e6, res['psd'], label=f"{res['name']} ({res['time']:.4f}s)", alpha=0.8, linewidth=1)
    ax1.legend()
    ax1.set_ylabel("Potencia (dBm aprox)")
    ax1.set_xlabel("Frecuencia (MHz)")
    ax1.grid(True, alpha=0.3)
    
    # Plot 2: Barras de Tiempo
    names = [r['name'] for r in results]
    times = [r['time'] for r in results]
    colors = ['#00ff99', '#ff9900'] # Verde Welch, Naranja PFB
    
    bars = ax2.bar(names, times, color=colors)
    ax2.set_ylabel("Tiempo de Procesamiento (s)")
    ax2.set_title("Diferencia de Rendimiento")
    
    # Poner valor encima de barra
    for bar in bars:
        height = bar.get_height()
        ax2.text(bar.get_x() + bar.get_width()/2., height,
                 f'{height:.4f} s', ha='center', va='bottom')
    
    plt.tight_layout()
    plt.show()

# =========================================================
# 4. MAIN - ESCENARIOS DE PRUEBA
# =========================================================

if __name__ == "__main__":
    
    welch_engine = WelchEngine(LIB_PSD_PATH)
    
    # CONFIGURACIÓN BASE DE FRECUENCIA
    base_config = {
        "fc": int(98e6),     # 98 MHz
        "bw": int(20e6),     # 20 MHz
        "filename": FILENAME_BIN,
        "samples": int(20e6), # 4 Millones de muestras
        "nfft": 4096        # FFT Size
    }

    # DEFINIR LOS ESCENARIOS DE GANANCIA A COMPARAR
    # Aquí puedes jugar con los valores que quieras probar
    escenarios_ganancia = [
        {"etiqueta": "Sin ganancia",    "lna": 0,  "vga": 0,  "amp": 1},
        {"etiqueta": "Ganancia Máxima", "lna": 40, "vga": 62, "amp": 1},
        {"etiqueta": "Ganancia 1",      "lna": 0,  "vga": 62, "amp": 1},
        {"etiqueta": "Ganancia 2",      "lna": 40, "vga": 0,  "amp": 1},
        {"etiqueta": "Ganancia opt",    "lna": 20, "vga": 0,  "amp": 1},
    ]

    for setup in escenarios_ganancia:
        print(f"\n--- Probando: {setup['etiqueta']} ---")
        
        # 1. Actualizar configuración con los valores de este escenario
        #    Unimos el diccionario base con los cambios de ganancia
        current_cfg = {**base_config, **setup} 
        
        ## 2. Adquirir
        #if not adquirir_hardware(current_cfg):
        #    continue
            
        # 3. Cargar datos
        with open(FILENAME_BIN, "rb") as f:
            raw_bytes = f.read()
            
        resultados = []

        # --- WELCH ---
        t0 = time.time()
        f_w, p_w = welch_engine.process(raw_bytes, current_cfg['bw'], current_cfg['fc'], nfft=current_cfg['nfft'])
        dt_w = time.time() - t0
        resultados.append({"name": "Welch (C)", "time": dt_w, "freqs": f_w, "psd": p_w})

        # --- POLYPHASE ---
        t0 = time.time()
        f_p, p_p = run_polyphase(raw_bytes, current_cfg['bw'], current_cfg['fc'], n_channels=current_cfg['nfft'])
        dt_p = time.time() - t0
        resultados.append({"name": "Polyphase (Py)", "time": dt_p, "freqs": f_p, "psd": p_p})
        
        # 4. Graficar
        # Verás cómo cambia el piso de ruido y la altura de los picos según la ganancia
        plot_comparativa(resultados, f"Comparación - {setup['etiqueta']}")

    print("\nBenchmark finalizado.")