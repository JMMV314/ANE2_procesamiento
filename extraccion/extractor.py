import pyvisa
import numpy as np
import threading
import subprocess
import time
import os
from pathlib import Path
from datetime import datetime

# ==========================================
# 1. CLASE CONTROLADOR KEYSIGHT
# ==========================================
class KeysightController:
    def __init__(self, ip, timeout_connection) -> None:
        self.ip = ip
        self.timeout_connection = timeout_connection
        self.visa_addr = f"TCPIP::{self.ip}::INSTR"
        try:
            self.rm = pyvisa.ResourceManager()
        except:
            self.rm = pyvisa.ResourceManager('@py')
        self.inst = None

    def connect(self):
        try:
            self.inst = self.rm.open_resource(self.visa_addr)
            self.inst.timeout = self.timeout_connection
            return 0
        except Exception as e:
            print(f"❌ [Keysight] Error conexión: {e}")
            return 1

    def setup(self, freq, span, points=1001):
        """Configura frecuencia y span antes de la captura"""
        if self.inst is None: return
        self.inst.write("*CLS")
        self.inst.write(":FORM ASC")
        self.inst.write(f":SENSe:FREQuency:CENTer {freq}")
        self.inst.write(f":SENSe:FREQuency:SPAN {span}")
        self.inst.write(f":SENSe:SWEep:POINts {points}")

    def capture_trace(self):
        """Solicita los datos de la traza actual"""
        if self.inst is None: return []
        try:
            return np.array(self.inst.query_ascii_values(":TRACe:DATA? TRACE1"))
        except Exception as e:
            print(f"❌ [Keysight] Error captura: {e}")
            return []

    def disconnect(self):
        try:
            if self.inst: self.inst.close()
            if self.rm: self.rm.close()
        except: pass

# ==========================================
# 2. HILO: TAREA KEYSIGHT (Guarda CSV)
# ==========================================
def tarea_keysight(keysight_obj, timestamp, cfg):
    ruta_carpeta = cfg["ruta_salida"]
    
    print(" -> [Hilo Keysight] Solicitando traza...")
    trace = keysight_obj.capture_trace()
    
    if len(trace) > 0:
        # Calcular eje de frecuencias
        freq = cfg["frecuencia_central_hz"]
        span = cfg["span_hz"]
        freqs = np.linspace(freq - span/2, freq + span/2, len(trace))
        
        # Nombre del archivo
        filename = f"keysight_{timestamp}.csv"
        full_path = os.path.join(ruta_carpeta, filename)
        
        # Guardar CSV
        try:
            with open(full_path, "w", encoding="utf-8") as f:
                f.write(f"# Timestamp: {timestamp}\n")
                f.write(f"# Config: {cfg}\n")
                f.write("Frecuencia_Hz,Amplitud_dBm\n")
                for f_hz, amp in zip(freqs, trace):
                    f.write(f"{f_hz:.2f},{amp:.6f}\n")
            print(f"✅ [Keysight] CSV guardado en: {full_path}")
        except Exception as e:
            print(f"❌ [Keysight] Error guardando archivo: {e}")
    else:
        print("❌ [Keysight] Falló la captura de datos (traza vacía)")

# ==========================================
# 3. HILO: TAREA HACKRF (Guarda CS8)
# ==========================================
def tarea_hackrf(timestamp, cfg):
    ruta_carpeta = Path(cfg["ruta_salida"])
    filename = f"hackrf_{timestamp}.cs8"
    ruta_completa = ruta_carpeta / filename
    
    # Comprobar si existe hackrf_transfer
    try:
        # Construcción del comando con los parámetros del cfg
        cmd = [
            "hackrf_transfer",
            "-r", str(ruta_completa),               # Ruta de salida
            "-f", str(int(cfg["frecuencia_central_hz"])), # Frecuencia
            "-s", str(int(cfg["sample_rate_hz"])),        # Tasa de muestreo
            "-n", str(int(cfg["num_muestras"])),          # Número de muestras
            "-l", str(int(cfg["lna_gain"])),              # Ganancia LNA
            "-g", str(int(cfg["vga_gain"])),              # Ganancia VGA
            "-a", str(int(cfg["amp_enable"])),            # Amplificador (0/1)
        ]
        
        print(f" -> [Hilo HackRF] Iniciando grabación ({cfg['num_muestras']} muestras)...")
        # Ejecutar comando
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode == 0:
            size = ruta_completa.stat().st_size
            print(f"✅ [HackRF] CS8 guardado en: {ruta_completa} ({size} bytes)")
        else:
            print(f"❌ [HackRF] Error: {result.stderr}")
            
    except FileNotFoundError:
        print("❌ [HackRF] Error: No se encuentra el comando 'hackrf_transfer'. ¿Está instalado?")

# ==========================================
# 4. ORQUESTADOR PRINCIPAL
# ==========================================
def adquisicion_simultanea(cfg_k, cfg_h):
    # 1. Crear carpetas si no existen (son rutas diferentes)
    os.makedirs(cfg_k["ruta_salida"], exist_ok=True)
    os.makedirs(cfg_h["ruta_salida"], exist_ok=True)
    
    # 2. Generar Timestamp único para sincronizar nombres
    timestamp_comun = datetime.now().strftime("%Y%m%d_%H%M%S")
    
    print(f"\n--- Iniciando Proceso: {timestamp_comun} ---")
    print(f"📁 Ruta Keysight: {cfg_k['ruta_salida']}")
    print(f"📁 Ruta HackRF:   {cfg_h['ruta_salida']}")

    # 3. Conectar y Configurar Keysight (Fase Previa)
    ks = KeysightController(cfg_k["ip"], 10000)
    if ks.connect() != 0:
        print("Abortando: No hay conexión con el Keysight.")
        return

    try:
        print("\n[1/3] Configurando Keysight...")
        ks.setup(cfg_k["frecuencia_central_hz"], cfg_k["span_hz"], cfg_k["puntos"])
        
        # Tiempo de estabilización del barrido del Keysight
        print("[2/3] Esperando estabilización (2s)...")
        time.sleep(2) 
        
        print("[3/3] ¡Disparando adquisiciones simultáneas!")
        
        # 4. Lanzar Hilos
        # Hilo HackRF
        t_hack = threading.Thread(target=tarea_hackrf, args=(timestamp_comun, cfg_h))
        # Hilo Keysight
        t_keys = threading.Thread(target=tarea_keysight, args=(ks, timestamp_comun, cfg_k))
        
        t_hack.start()
        t_keys.start()
        
        # Esperar a que terminen
        t_hack.join()
        t_keys.join()
        
        print("\n--- Adquisición Finalizada con Éxito ---")

    finally:
        ks.disconnect()

# ==========================================
# CONFIGURACIÓN Y EJECUCIÓN
# ==========================================
if __name__ == "__main__":
    
    # --- CONFIGURACIÓN KEYSIGHT (CSV) ---
    cfg_keysight = {
        "ip": "192.168.46.113",
        # RUTA ESPECÍFICA PARA EL CSV
        "ruta_salida": "/home/gcpds/Desktop/Procesamiento_ANE2/ANE2_procesamiento/extraccion",
        
        "frecuencia_central_hz": int(98e6),
        "span_hz": int(20e6),
        "puntos": 4096
    }

    # --- CONFIGURACIÓN HACKRF (CS8) ---
    cfg_hackrf = {
        # RUTA ESPECÍFICA PARA EL IQ (CS8)
        "ruta_salida": "/home/gcpds/Desktop/Procesamiento_ANE2/ANE2_procesamiento/extraccion",
        
        "frecuencia_central_hz": int(98e6),
        "sample_rate_hz": int(20e6),   # Frecuencia de muestreo
        "num_muestras": int(20e6),      # Cantidad de muestras (2M @ 20MHz = 0.1s)
        
        # Ganancias Configurables:
        "lna_gain": 0,    # (0-40, saltos de 8)
        "vga_gain": 0,    # (0-62, saltos de 2)
        "amp_enable": 0    # (0 = Off, 1 = On)
    }

    # Ejecutar
    adquisicion_simultanea(cfg_keysight, cfg_hackrf)