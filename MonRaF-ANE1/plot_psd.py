import matplotlib.pyplot as plt
import numpy as np
import csv

# Cambia esta variable a True para superponer, False para subplots
superponer = False  # <--- Cambia aquí

csv_files = [
    ("Outputs/98_20M.csv", "PSD 98_20M"),
    ("Outputs/resultado_psd_db.csv", "PSD 20M"),
    # ("Outputs/103_20M.csv", "PSD 103_20M"),
    ("Outputs/108_20.csv", "PSD 108_20M")
]

datos = []
for csv_file, label in csv_files:
    f = []
    Pxx_db = []
    try:
        with open(csv_file, newline='') as csvfile:
            reader = csv.reader(csvfile)
            next(reader, None)  # saltar cabecera
            for row in reader:
                if len(row) < 2:
                    continue
                f.append(float(row[0]))
                Pxx_db.append(float(row[1]))
        datos.append((f, Pxx_db, label))
    except FileNotFoundError:
        datos.append((None, None, f"No encontrado\n{label}"))

if superponer:
    plt.figure(figsize=(8, 5))
    # Definir los desfases para cada curva
    offsets = [0.0, 5e6, 10e6]
    for idx, (f, Pxx_db, label) in enumerate(datos):
        if f is not None:
            f_offset = [x + offsets[idx] for x in f]
            plt.plot(f_offset, Pxx_db, label=label)
    plt.xlabel('Frecuencia (Hz)')
    plt.ylabel('PSD (dB)')
    plt.title('PSD superpuestas (desfasadas)')
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.show()
else:
    fig, axs = plt.subplots(1, 3, figsize=(15, 5), sharey=True)
    for idx, (f, Pxx_db, label) in enumerate(datos):
        if f is not None:
            axs[idx].plot(f, Pxx_db)
        axs[idx].set_title(label)
        axs[idx].set_xlabel('Frecuencia (Hz)')
        axs[idx].grid(True)
    axs[0].set_ylabel('PSD (dB)')
    plt.tight_layout()
    plt.show()
