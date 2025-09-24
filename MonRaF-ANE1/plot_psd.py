import pandas as pd
import matplotlib.pyplot as plt

# ===========================
# Pregunta al usuario
# ===========================
opcion = input("¿Quieres graficar 3 espectros (escribe '3') o solo 1 espectro (escribe '1')?: ")

if opcion == "3":
    # ===========================
    # Pedir rutas de 3 archivos CSV
    # ===========================
    file1 = input("Escribe la ruta del primer CSV (ej: 200k.csv): ")
    file2 = input("Escribe la ruta del segundo CSV (ej: 2M.csv): ")
    file3 = input("Escribe la ruta del tercer CSV (ej: 20M.csv): ")

    # Cargar cada CSV
    df1 = pd.read_csv(file1)
    df2 = pd.read_csv(file2)
    df3 = pd.read_csv(file3)

    # Graficar
    plt.figure(figsize=(10, 6))
    plt.plot(df1.iloc[:, 0], df1.iloc[:, 1], label=file1)
    plt.plot(df2.iloc[:, 0], df2.iloc[:, 1], label=file2)
    plt.plot(df3.iloc[:, 0], df3.iloc[:, 1], label=file3)
    plt.xlabel("Frecuencia (Hz)")
    plt.ylabel("PSD")
    plt.title("Comparación de 3 espectros")
    plt.legend()
    plt.grid(True)
    plt.show()

elif opcion == "1":
    # ===========================
    # Pedir ruta de 1 archivo CSV
    # ===========================
    file1 = input("Escribe la ruta del CSV: ")

    df = pd.read_csv(file1)

    # Graficar
    plt.figure(figsize=(10, 6))
    plt.plot(df.iloc[:, 0], df.iloc[:, 1], label=file1)
    plt.xlabel("Frecuencia (Hz)")
    plt.ylabel("PSD")
    plt.title("Espectro")
    plt.legend()
    plt.grid(True)
    plt.show()

else:
    print("⚠️ Opción no válida. Escribe '3' o '1'.")

