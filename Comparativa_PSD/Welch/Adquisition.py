import ctypes
import os
import subprocess
import numpy as np
import matplotlib.pyplot as plt

# =========================================================
# 1. DEFINICIÓN DE ESTRUCTURAS C EN PYTHON (MAPPING)
# =========================================================

# Enum para WindowType (según datatypes.h)
HAMMING_TYPE = 0
HANN_TYPE = 1
RECTANGULAR_TYPE = 2
BLACKMAN_TYPE = 3

class SignalIQ(ctypes.Structure):
    _fields_ = [
        ("signal_iq", ctypes.c_void_p), # Puntero a complex double (opaque para Python)
        ("n_signal", ctypes.c_size_t)
    ]

class PsdConfig(ctypes.Structure):
    _fields_ = [
        ("window_type", ctypes.c_int),   # Enum
        ("sample_rate", ctypes.c_double),
        ("nperseg", ctypes.c_int),
        ("noverlap", ctypes.c_int)
    ]

# Cargar la librería compilada
# Asegúrate de que libpsd.so esté en la misma carpeta o ruta completa
lib_path = os.path.abspath("/home/jmmv/ANE2/Prueba_PSD/libs/libpsd.so")
try:
    lib = ctypes.CDLL(lib_path)
except OSError:
    print(f"❌ Error: No se pudo cargar {lib_path}. ¿Ejecutaste el comando gcc?")
    exit(1)

# Configurar argumentos y tipos de retorno de las funciones C

# signal_iq_t* load_iq_from_buffer(const int8_t* buffer, size_t buffer_size)
lib.load_iq_from_buffer.argtypes = [ctypes.POINTER(ctypes.c_int8), ctypes.c_size_t]
lib.load_iq_from_buffer.restype = ctypes.POINTER(SignalIQ)

# void execute_welch_psd(signal_iq_t*, PsdConfig_t*, double* f_out, double* p_out)
lib.execute_welch_psd.argtypes = [
    ctypes.POINTER(SignalIQ),
    ctypes.POINTER(PsdConfig),
    ctypes.POINTER(ctypes.c_double), # f_out array
    ctypes.POINTER(ctypes.c_double)  # p_out array
]

# int scale_psd(double* psd, int nperseg, const char* scale_str)
lib.scale_psd.argtypes = [
    ctypes.POINTER(ctypes.c_double),
    ctypes.c_int,
    ctypes.c_char_p
]

# void free_signal_iq(signal_iq_t* signal)
lib.free_signal_iq.argtypes = [ctypes.POINTER(SignalIQ)]

# =========================================================
# 2. FUNCIONES DE UTILIDAD (ADQUISICIÓN Y PROCESAMIENTO)
# =========================================================

def adquirir_hackrf(config):
    # 1. FORZAR NUEVA CAPTURA: Si el archivo existe, lo borramos.
    if os.path.exists(config['nombre_archivo']):
        print(f"[HW] Borrando archivo antiguo para aplicar nuevas ganancias...")
        os.remove(config['nombre_archivo'])

    print(f"\n[HW] Capturando {config['nombre_archivo']} | LNA:{config['lna']} VGA:{config['vga']}...")
    
    cmd = [
        "hackrf_transfer",
        "-r", config['nombre_archivo'],
        "-f", str(config['frecuencia_central']),
        "-s", str(config['ancho_banda']),
        "-n", str(config['num_muestras']),
        "-l", str(config['lna']), # LNA Gain (0-40, steps of 8)
        "-g", str(config['vga']), # VGA Gain (0-62, steps of 2)
        "-a", str(config['amp'])  # Amp (0 or 1)
    ]
    
    # Ejecutar hackrf_transfer
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    if result.returncode != 0:
        print(f"❌ Error en hackrf_transfer: {result.stderr}")
        return False
        
    print("✅ Captura completada.")
    return True

def procesar_con_c(filename, config_hw):
    """
    Lee el archivo binario, lo envía a C y recibe los arrays de PSD.
    """
    # 1. Leer archivo binario en Python (bytes)
    try:
        with open(filename, "rb") as f:
            raw_bytes = f.read()
    except FileNotFoundError:
        return None, None

    buffer_len = len(raw_bytes)
    # Crear buffer compatible con C
    c_buffer = (ctypes.c_int8 * buffer_len).from_buffer_copy(raw_bytes)

    # 2. Cargar IQ (Llamada a C)
    print("[C-LIB] Cargando IQ y convirtiendo a Complex...")
    signal_ptr = lib.load_iq_from_buffer(c_buffer, buffer_len)

    if not signal_ptr:
        print("Error: load_iq_from_buffer retornó NULL")
        return None, None

    # 3. Configurar Welch
    nfft = 4096
    psd_cfg = PsdConfig()
    psd_cfg.window_type = HAMMING_TYPE     # Tu enum C
    psd_cfg.sample_rate = float(config_hw['ancho_banda'])
    psd_cfg.nperseg = nfft
    psd_cfg.noverlap = int(nfft * 0.5)

    # 4. Preparar arrays de salida (Python reserva la memoria, C la llena)
    # Numpy facilita esto: creamos arrays vacíos de double
    f_out = np.zeros(nfft, dtype=np.float64)
    p_out = np.zeros(nfft, dtype=np.float64)

    # Obtener punteros a los datos de numpy para pasarlos a C
    f_ptr = f_out.ctypes.data_as(ctypes.POINTER(ctypes.c_double))
    p_ptr = p_out.ctypes.data_as(ctypes.POINTER(ctypes.c_double))

    # 5. Ejecutar Welch (Llamada a C)
    print("[C-LIB] Ejecutando FFTW Welch...")
    lib.execute_welch_psd(signal_ptr, ctypes.byref(psd_cfg), f_ptr, p_ptr)

    # 6. Escalar (Llamada a C)
    # Nota: Tu código C tiene correct_centered_dc_spike en main.c, no en psd.c
    # Si quieres corrección DC, agrégala a psd.c o hazla aquí en Python antes de escalar.
    # Aquí llamaremos directo a scale como pediste.
    print("[C-LIB] Escalando a dBm...")
    unit = b"dBm" # char*
    lib.scale_psd(p_ptr, nfft, unit)

    # 7. Limpiar memoria C
    lib.free_signal_iq(signal_ptr)

    # Ajustar frecuencia absoluta (C devuelve relativa -Fs/2 a Fs/2)
    f_absolute = f_out + config_hw['frecuencia_central']

    return f_absolute, p_out

def plot_espectro_completo(freqs, psd, config):
    """
    Grafica el espectro e incrusta los metadatos de adquisición en un recuadro.
    """
    # Convertir a unidades legibles
    fc_mhz = config['frecuencia_central'] / 1e6
    bw_mhz = config['ancho_banda'] / 1e6
    f_axis_mhz = freqs / 1e6

    # Configuración de estilo "Hacker/Dark"
    plt.style.use('dark_background')
    fig, ax = plt.subplots(figsize=(12, 7))
    
    # Trazar la señal
    ax.plot(f_axis_mhz, psd, color='#00ff99', linewidth=0.8, label='PSD (Welch C-Lib)')
    
    # --- AÑADIR METADATOS ---
    # Creamos un string con saltos de línea
    info_text = (
        f"$\mathbf{{Parámetros de HW}}$\n"
        f"Fc: {fc_mhz:.1f} MHz\n"
        f"BW: {bw_mhz:.1f} MHz\n"
        f"LNA: {config['lna']} dB\n"
        f"VGA: {config['vga']} dB\n"
        f"Amp: {'ON' if config['amp'] else 'OFF'}"
    )

    # Propiedades de la caja de texto (Semintransparente para ver trazos detrás)
    props = dict(boxstyle='round', facecolor='#222222', alpha=0.8, edgecolor='white')

    # Colocar texto en coordenadas relativas (0.02, 0.95 es esquina superior izquierda)
    ax.text(0.02, 0.95, info_text, transform=ax.transAxes, fontsize=10,
            verticalalignment='top', bbox=props, color='white', fontfamily='monospace')

    # --- ETIQUETAS Y DETALLES ---
    ax.set_title(f"Análisis Espectro FM - HackerRF One", fontsize=14, pad=15)
    ax.set_xlabel("Frecuencia (MHz)")
    ax.set_ylabel("Potencia (dBm)")
    
    # Grid sutil
    ax.grid(True, which='both', linestyle='--', linewidth=0.5, alpha=0.3)
    
    # Límites dinámicos (ajusta el Y para que se vea bien la señal)
    ax.set_ylim(min(psd) - 5, max(psd) + 10)
    ax.set_xlim(min(f_axis_mhz), max(f_axis_mhz))

    # Identificar el pico máximo automáticamente (Opcional, muy útil)
    max_idx = np.argmax(psd)
    max_freq = f_axis_mhz[max_idx]
    max_pow = psd[max_idx]
    
    ax.annotate(f'Peak: {max_freq:.2f} MHz\n{max_pow:.1f} dBm',
                 xy=(max_freq, max_pow), xytext=(max_freq, max_pow+10),
                 arrowprops=dict(facecolor='yellow', shrink=0.05, width=1, headwidth=5),
                 color='yellow', ha='center')

    plt.tight_layout()
    plt.show()

import csv

def corregir_respuesta_filtro(psd):
    """
    Aproximación simple: Asume que el ruido debería ser plano.
    Calcula la tendencia del piso de ruido y la invierte.
    """
    import scipy.signal
    
    # 1. Usar un filtro de mediana para encontrar el "piso" ignorando los picos (estaciones)
    # El kernel debe ser lo bastante ancho para ignorar señales finas
    noise_floor_shape = scipy.signal.medfilt(psd, kernel_size=101)
    
    # 2. Calcular cuánto se desvía el piso del promedio central
    center_val = np.median(noise_floor_shape)
    correction_curve = center_val - noise_floor_shape
    
    # 3. Aplicar corrección
    return psd + correction_curve

# Usar en tu main:
# f, p = procesar_con_c(...)
# p_corregido = corregir_respuesta_filtro(p)
# plot_espectro_completo(f, p_corregido, cfg)

def guardar_csv(nombre_csv_deseado, freqs, psd, config):
    """
    Guarda los datos en el nombre de archivo especificado por el usuario.
    """
    # Asegurar extensión .csv
    if not nombre_csv_deseado.endswith('.csv'):
        nombre_csv_deseado += '.csv'
    
    print(f"[IO] Guardando datos en: {nombre_csv_deseado}")
    
    try:
        with open(nombre_csv_deseado, mode='w', newline='') as file:
            writer = csv.writer(file)
            
            # Metadatos
            writer.writerow([f"# Origen Binario: {config['nombre_archivo']}"])
            writer.writerow([f"# Frecuencia Central: {config['frecuencia_central']} Hz"])
            writer.writerow([f"# Ancho de Banda: {config['ancho_banda']} Hz"])
            writer.writerow([f"# Ganancias: LNA={config['lna']} | VGA={config['vga']}"])
            writer.writerow([]) 
            
            # Datos
            writer.writerow(["Frequency_Hz", "Power_dBm"])
            for f, p in zip(freqs, psd):
                writer.writerow([f"{f:.2f}", f"{p:.4f}"])
                
        print(f"✅ CSV '{nombre_csv_deseado}' guardado correctamente.")
        
    except Exception as e:
        print(f"❌ Error guardando CSV: {e}")

# =========================================================
# BLOQUE MAIN
# =========================================================

mis_capturas = [
    {
        "nombre_archivo": "fm_full_dataset.bin",
        "nombre_csv": "/home/jmmv/ANE2/Comparativa_PSD/Welch/Resultados/resultado_prueba_1.csv",
        "frecuencia_central": 86.23*1000000,  # 103.7 MHz
        "ancho_banda": 20*1000000,         # 20 MHz
        "num_muestras": 2*1000000,        # ~2 segundos (suficiente para Welch)
        "lna": 20,                       # Ganancia LNA un poco más alta para ver detalles
        "vga": 0, 
        "amp": 0
    }
]


if __name__ == "__main__":
    import time

    t0=time.time()
    correction=False  # Activar corrección de respuesta de filtro

    for cfg in mis_capturas:
        # 1. Adquirir (Llamando a tu función existente)
        if adquirir_hackrf(cfg):
            t1= time.time()
            # 2. Procesar (Llamando a tu función C wrapper existente)
            f, p = procesar_con_c(cfg['nombre_archivo'], cfg)
            p_corregido = corregir_respuesta_filtro(p)
            
            if f is not None:
                # 3. GUARDAR CSV (Usando el nombre configurado)
                # Verifica si la clave existe, si no, usa un default
                nombre_salida = cfg.get('nombre_csv', 'salida_default.csv')
                guardar_csv(nombre_salida, f, p, cfg)
                
                t2=time.time()
                print(f"[TIME] Tiempo total de adquisición: {t1-t0:.2f} segundos.")
                print(f"[TIME] Tiempo total de procesamiento: {t2-t1:.2f} segundos.")

                # 4. Graficar
                if not correction:
                    plot_espectro_completo(f, p, cfg)
                else:
                    plot_espectro_completo(f, p_corregido, cfg)