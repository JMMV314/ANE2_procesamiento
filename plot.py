import pandas as pd
import matplotlib.pyplot as plt
import os

# ==========================================
# CONFIGURACIÓN: Lista de archivos a graficar
# ==========================================
FILES_TO_PLOT = [
    "/home/gcpds/Desktop/Procesamiento_ANE2/ANE2_procesamiento/extraccion/keysight_20251217_153846.csv",
    "/home/gcpds/Desktop/Procesamiento_ANE2/ANE2_procesamiento/extraccion/keysight_20251217_153756.csv"
    # "ruta/a/otra_carpeta/resultado_3.csv",
    # Agrega tantas rutas como necesites...
]

def plot_multiple_psd(file_list):
    plt.figure(figsize=(12, 7))
    
    files_plotted = 0
    
    for file_path in file_list:
        try:
            # Verificar si el archivo existe antes de intentar leerlo
            if not os.path.exists(file_path):
                print(f"[ADVERTENCIA] El archivo no existe: {file_path}")
                continue

            # Leer el CSV
            df = pd.read_csv(file_path, comment="#")
            
            # Obtener nombres de columnas dinámicamente
            # Asumimos estructura: [Frecuencia, Potencia]
            freq_col = df.columns[0]
            power_col = df.columns[1] 

            # Extraer el nombre del archivo para la leyenda (sin la ruta completa)
            label_name = os.path.basename(file_path)

            # Graficar
            # Convertimos Hz a MHz para facilitar la lectura (df[freq_col] / 1e6)
            plt.plot(df[freq_col] / 1e6, df[power_col], linewidth=1, label=label_name, alpha=0.8)
            
            print(f"[OK] Cargado: {label_name}")
            files_plotted += 1

        except Exception as e:
            print(f"[ERROR] Falló al leer {file_path}: {e}")

    # Configuración final del gráfico
    if files_plotted > 0:
        plt.title('Comparación de PSD (Welch)')
        plt.xlabel('Frecuencia (MHz)')
        plt.ylabel('Potencia') # La unidad depende de tu configuración en C (dBm, dBuV, etc)
        
        plt.grid(True, which='major', linestyle='-', linewidth=0.7, alpha=0.7)
        plt.grid(True, which='minor', linestyle='--', linewidth=0.4, alpha=0.5)
        plt.minorticks_on()
        
        # Mostrar leyenda para identificar cada archivo
        plt.legend(loc='best')
        
        plt.tight_layout()
        plt.show()
    else:
        print("No se pudo graficar ningún archivo.")

if __name__ == "__main__":
    # Si se pasan argumentos por consola, úsalos; si no, usa la lista hardcodeada
    import sys
    if len(sys.argv) > 1:
        plot_multiple_psd(sys.argv[1:])
    else:
        plot_multiple_psd(FILES_TO_PLOT)