#!/bin/bash

# --- CONFIGURACIÓN DE RUTAS ---
# Archivo de entrada (.cs8)
INPUT_FILE="Muestras_reales/148"

# Archivo donde el C guardará el resultado (.csv)
OUTPUT_FILE="out_debug.csv"

# Archivo de referencia para comparar en la gráfica (opcional)
REF_FILE="Muestras_reales/107115000.csv"

# Nombre del ejecutable C
EXE_NAME="psd_calc"

echo "========================================"
echo "1. COMPILANDO EL CÓDIGO C..."
echo "========================================"

# Compila el código (ajusta las rutas de libs si es necesario)
gcc main.c libs/psd.c -o $EXE_NAME -I. -lfftw3 -lm -lcjson

# Verificamos si la compilación fue exitosa
if [ $? -ne 0 ]; then
    echo "[ERROR] La compilación falló."
    exit 1
fi

echo "========================================"
echo "2. EJECUTANDO EL CÁLCULO PSD..."
echo "========================================"

# Ejecuta el programa C pasando los argumentos
./$EXE_NAME "$INPUT_FILE" "$OUTPUT_FILE"

# Verificamos si el programa C corrió bien
if [ $? -ne 0 ]; then
    echo "[ERROR] El programa C falló."
    exit 1
fi

echo "========================================"
echo "3. GRAFICANDO RESULTADOS CON PYTHON..."
echo "========================================"

# Ejecuta Python pasándole el archivo generado Y la referencia
# Tu script de Python ya soporta recibir argumentos por consola
python3 plot.py "$OUTPUT_FILE" "$REF_FILE"

echo "========================================"
echo "PROCESO TERMINADO."
echo "========================================"