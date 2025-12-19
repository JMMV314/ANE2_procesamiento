import pyvisa
import numpy as np
import threading
import subprocess
import time
import os
from pathlib import Path
from datetime import datetime

# ==========================================
# 1. CLASE CONTROLADOR KEYSIGHT (ACTUALIZADA CON RBW)
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

    def setup(self, freq, span, points=1001, rbw=None):
        """
        Configura frecuencia, span, puntos y RBW.
        Si rbw es None, lo configura en AUTO.
        """
        if self.inst is None: return
        
        # Comandos básicos
        self.inst.write("*CLS")
        self.inst.write(":FORM ASC")
        self.inst.write(f":SENSe:FREQuency:CENTer {freq}")
        self.inst.write(f":SENSe:FREQuency:SPAN {span}")
        self.inst.write(f":SENSe:SWEep:POINts {points}")
        
        # --- NUEVA LÓGICA RBW ---
        if rbw is not None and rbw > 0:
            self.inst.write(f":SENSe:BANDwidth:RESolution {rbw}")
        else:
            self.inst.write(":SENSe:BANDwidth:RESolution:AUTO ON")

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
        freq = cfg["frecuencia_central_hz"]
        span = cfg["span_hz"]
        freqs = np.linspace(freq - span/2, freq + span/2, len(trace))
        
        filename = f"keysight_{timestamp}.csv"
        full_path = os.path.join(ruta_carpeta, filename)
        
        try:
            with open(full_path, "w", encoding="utf-8") as f:
                f.write(f"# Timestamp: {timestamp}\n")
                # Al iterar cfg, el RBW se guardará automáticamente en el header
                for key, val in cfg.items():
                    f.write(f"# {key}: {val}\n")
                f.write("Frecuencia_Hz,Amplitud_dBm\n")
                for f_hz, amp in zip(freqs, trace):
                    f.write(f"{f_hz:.2f},{amp:.6f}\n")
            print(f"✅ [Keysight] CSV guardado en: {full_path}")
        except Exception as e:
            print(f"❌ [Keysight] Error guardando archivo: {e}")
    else:
        print("❌ [Keysight] Falló la captura de datos")

# ==========================================
# 3. HILO: TAREA HACKRF (Guarda CS8)
# ==========================================
def tarea_hackrf(timestamp, cfg):
    ruta_carpeta = Path(cfg["ruta_salida"])
    filename = f"hackrf_{timestamp}.cs8"
    ruta_completa = ruta_carpeta / filename
    
    try:
        cmd = [
            "hackrf_transfer",
            "-r", str(ruta_completa),
            "-f", str(int(cfg["frecuencia_central_hz"])),
            "-s", str(int(cfg["sample_rate_hz"])),
            "-n", str(int(cfg["num_muestras"])),
            "-l", str(int(cfg["lna_gain"])),
            "-g", str(int(cfg["vga_gain"])),
            "-a", str(int(cfg["amp_enable"])),
        ]
        
        print(f" -> [Hilo HackRF] Grabando {cfg['num_muestras']} muestras...")
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode == 0:
            size = ruta_completa.stat().st_size
            print(f"✅ [HackRF] CS8 guardado en: {ruta_completa} ({size} bytes)")
        else:
            print(f"❌ [HackRF] Error: {result.stderr}")
            
    except FileNotFoundError:
        print("❌ [HackRF] Error: No se encuentra 'hackrf_transfer'")

# ==========================================
# 4. ORQUESTADOR PRINCIPAL
# ==========================================
def adquisicion_simultanea(cfg_k, cfg_h):
    os.makedirs(cfg_k["ruta_salida"], exist_ok=True)
    os.makedirs(cfg_h["ruta_salida"], exist_ok=True)
    
    timestamp_comun = datetime.now().strftime("%Y%m%d_%H%M%S")
    
    print(f"\n--- Iniciando: {timestamp_comun} ---")

    ks = KeysightController(cfg_k["ip"], 10000)
    if ks.connect() != 0:
        return

    try:
        # Configuración incluye RBW
        print("\n[1/3] Configurando Keysight (Freq, Span, RBW)...")
        ks.setup(
            freq=cfg_k["frecuencia_central_hz"], 
            span=cfg_k["span_hz"], 
            points=cfg_k["puntos"],
            rbw=cfg_k.get("rbw_hz") # <--- AQUÍ SE PASA EL NUEVO PARÁMETRO
        )
        
        # Nota: Si el RBW es muy bajo (ej. 1kHz), el barrido será lento.
        # Aumentamos un poco la espera por seguridad.
        wait_time = 3 
        print(f"[2/3] Esperando estabilización ({wait_time}s)...")
        time.sleep(wait_time) 
        
        print("[3/3] ¡Disparando adquisiciones simultáneas!")
        
        t_hack = threading.Thread(target=tarea_hackrf, args=(timestamp_comun, cfg_h))
        t_keys = threading.Thread(target=tarea_keysight, args=(ks, timestamp_comun, cfg_k))
        
        t_hack.start()
        t_keys.start()
        
        t_hack.join()
        t_keys.join()
        print("\n--- Finalizado ---")

    finally:
        ks.disconnect()

# ==========================================
# EJECUCIÓN
# ==========================================
if __name__ == "__main__":
    
    # --- CONFIGURACIÓN KEYSIGHT ---
    cfg_keysight = {
        "ip": "192.168.46.113",
        "ruta_salida": "Calibracion/extraccion/Samples",
        
        "frecuencia_central_hz": int(86.23e6),
        "span_hz": int(20e6),
        "puntos": 1001,
        
        # --- NUEVO PARÁMETRO RBW ---
        # Pon el valor deseado en Hz. Ej: 100e3 (100 kHz), 30e3 (30 kHz).
        # Si pones None, se usará modo Auto.
        "rbw_hz": int(100e3) 
    }

    # --- CONFIGURACIÓN HACKRF ---
    cfg_hackrf = {
        "ruta_salida": "Calibracion/extraccion/Samples",
        "frecuencia_central_hz": int(86.23e6),
        "sample_rate_hz": int(20e6),
        "num_muestras": int(2e6),
        "lna_gain": 32,
        "vga_gain": 20,
        "amp_enable": 0
    }

    adquisicion_simultanea(cfg_keysight, cfg_hackrf)