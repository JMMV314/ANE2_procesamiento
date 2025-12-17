import pyvisa
import numpy as np
import matplotlib.pyplot as plt
import time

class KeysightController:
    # CORRECCIÓN 1: Doble guion bajo init
    def __init__(self, ip, timeout_connection) -> None:
        self.ip = ip
        self.timeout_connection = timeout_connection
        self.visa_addr = f"TCPIP::{self.ip}::INSTR"
        # CORRECCIÓN 2: Quitamos '@py' para usar el driver por defecto (NI/Keysight)
        # Si prefieres usar pyvisa-py, devuélvelo a ('@py')
        try:
            self.rm = pyvisa.ResourceManager() 
        except:
            self.rm = pyvisa.ResourceManager('@py')

        self.inst = None

    def _init(self):
        return self.rm.open_resource(self.visa_addr)

    def connect(self):
        try:
            self.inst = self._init()
            self.inst.timeout = self.timeout_connection
            # Opcional: Aumentar timeout del instrumento si el barrido es lento
        except Exception as e:
            print(f"Failed to connect to instrument: {e}")
            self.inst = None
            return 1
        return 0
    
    def whoami(self):
        if self.inst is None:
            print("No instrument connected.")
            return
        print(f"Connected to: {self.inst.query('*IDN?').strip()}")
    # ... (whoami se mantiene igual) ...

    # CORRECCIÓN 3: Añadimos 'span' a la configuración
    def setup(self, freq, span):
        if self.inst is None:
            print("No instrument connected.")
            return
        self.inst.write("*CLS")
        self.inst.write(":FORM ASC")
        self.inst.write(f":SENSe:FREQuency:CENTer {freq}")
        # Enviamos el comando de SPAN al equipo
        self.inst.write(f":SENSe:FREQuency:SPAN {span}") 

    def configure_advanced(self, rbw=None, ref_level=None, points=None):
        """
        Configura parámetros avanzados si se especifican.
        """
        if self.inst is None:
            return

        # 1. Configurar Reference Level (Amplitud tope)
        if ref_level is not None:
            # Ej: self.keysight.configure_advanced(ref_level=0)
            self.inst.write(f":DISPlay:WINDow:TRACe:Y:RLEVel {ref_level}")

        # 2. Configurar RBW (Resolución)
        if rbw is not None:
            # Ej: self.keysight.configure_advanced(rbw=1000) # 1 kHz
            self.inst.write(f":SENSe:BANDwidth:RESolution {rbw}")
        else:
            # Si no se especifica, dejar en automático
            self.inst.write(":SENSe:BANDwidth:RESolution:AUTO ON")

        # 3. Puntos de barrido (Resolución eje X)
        if points is not None:
            # Ej: 461, 1001, etc.
            self.inst.write(f":SENSe:SWEep:POINts {points}")
            
    def set_trace_mode(self, mode="WRITe"):
        """
        Cambia el modo de la traza: WRITe, MAXHold, MINHold, AVERage
        """
        if self.inst is None: return
        # Nota: Algunos modelos viejos usan :TRACe1:MODE
        # Modelos nuevos (serie X): :TRACe1:TYPE
        try:
            self.inst.write(f":TRACe:TYPE {mode}")
        except:
            print(f"Comando de traza no aceptado para modo {mode}")

    def capture_trace(self):
        if self.inst is None:
            print("No instrument connected.")
            return []
        # Pedimos la data
        try:
            raw_data = self.inst.query_ascii_values(":TRACe:DATA? TRACE1")
            return np.array(raw_data)
        except Exception as e:
            print(f"Error reading trace: {e}")
            return []

    def disconnect(self):
        try:
            if self.inst is not None:
                self.inst.close()
            if self.rm is not None:
                self.rm.close() # Cerrar el Resource Manager también es buena práctica
        except Exception as e:
            print(f"Failed to disconnect: {e}")
            return 1
        return 0
    
def acquire_sample_keysigh(keysight, freq, span):
    # Pasamos freq Y span a la configuración
    keysight.setup(freq, span)
    keysight.configure_advanced(rbw=47000, points= 4096)  
    keysight.set_trace_mode("AVErage")
    
    # Pequeña espera para asegurar que el equipo aplicó la config y barrió
    time.sleep(2) 
    
    trace = keysight.capture_trace()
    
    if len(trace) == 0:
        return [], []

    # Crear eje X
    freqs_hz = np.linspace(freq - span/2, freq + span/2, len(trace))
    return freqs_hz, trace

# --- PARAMETERS ---
INSTR_IP = "192.168.46.113" # ¡Asegúrate que esta sea la IP real de tu equipo!
CENTER_FREQ_HZ = 98e6 
SPAN_HZ = 20e6

keysight = KeysightController(INSTR_IP, 10000)

if keysight.connect() == 0:
    keysight.whoami() # Verificar conexión
    f, trace = acquire_sample_keysigh(keysight, CENTER_FREQ_HZ, SPAN_HZ)

    if len(f) > 0:
        # --- VISUALIZATION ---
        plt.figure(figsize=(10, 5))
        plt.plot(f / 1e6, trace)
        plt.title(f"Spectrum Capture at {CENTER_FREQ_HZ/1e6} MHz")
        plt.xlabel("Frequency (MHz)")
        plt.ylabel("Amplitude (dBm)")
        plt.grid(True)
        plt.show()
    
    keysight.disconnect()
else:
    print("No se pudo conectar al equipo.")