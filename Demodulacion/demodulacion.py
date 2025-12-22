import sys
import socket
import struct
import numpy as np
from PyQt5 import QtWidgets, QtCore
import pyqtgraph as pg

# Configuración idéntica al C
UDP_IP = "127.0.0.1"
UDP_PORT = 9999
FFT_SIZE = 1024
# El tamaño esperado del paquete en bytes: 1024 floats * 4 bytes/float
EXPECTED_BYTES = FFT_SIZE * 4 

class UDPViewer(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Visor de Espectro UDP (Desde C)")
        self.resize(800, 400)

        # 1. Configurar Socket UDP (No bloqueante)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((UDP_IP, UDP_PORT))
        self.sock.setblocking(False) # Importante para que no congele la GUI

        # 2. GUI
        self.plot_widget = pg.PlotWidget()
        self.setCentralWidget(self.plot_widget)
        self.plot_widget.setYRange(-120, 0) # Rango dB típico
        self.plot_widget.setLabel('left', 'Magnitud (dB)')
        self.plot_widget.showGrid(x=True, y=True, alpha=0.3)
        self.curve = self.plot_widget.plot(pen='c')

        # 3. Timer ultrarrápido para revisar la red
        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.check_network)
        # Revisar cada 10ms (100 veces por segundo)
        self.timer.start(10) 
        
        print(f"Escuchando UDP en {UDP_IP}:{UDP_PORT}...")

    def check_network(self):
        try:
            # Intentamos leer el paquete más reciente
            # Buffer size un poco más grande por si acaso (4096 bytes mínimo)
            data, addr = self.sock.recvfrom(EXPECTED_BYTES + 100)
            
            if len(data) == EXPECTED_BYTES:
                # Desempaquetar binario C (floats) a Numpy
                # 'f' significa float (4 bytes), multiplicado por FFT_SIZE
                format_str = str(FFT_SIZE) + 'f'
                fft_data = struct.unpack(format_str, data)
                
                # Convertir a array numpy
                arr = np.array(fft_data)
                
                # FFT Shift (Mover el centro 0Hz al medio del gráfico)
                arr_shifted = np.fft.fftshift(arr)
                
                # Actualizar gráfico
                self.curve.setData(arr_shifted)
                
        except BlockingIOError:
            # No llegaron datos en esta vuelta, no pasa nada.
            pass
        except Exception as e:
            print(f"Error de red: {e}")

    def closeEvent(self, event):
        self.sock.close()

if __name__ == '__main__':
    # Necesitas: pip install pyqtgraph PyQt5 numpy
    app = QtWidgets.QApplication(sys.argv)
    window = UDPViewer()
    window.show()
    sys.exit(app.exec_())