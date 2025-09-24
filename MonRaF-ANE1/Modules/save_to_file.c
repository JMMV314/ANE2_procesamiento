/**
 * @file save_to_file.c
 * @brief Función para guardar datos en un archivo CSV.
 *
 * Esta función permite almacenar datos de frecuencias y densidades espectrales de potencia (PSD)
 * en un archivo CSV con formato adecuado para análisis posterior.
 */

#include <stdio.h>


void save_to_file(double* f, double* Pxx, int length, const char* filename) {
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        printf("Error al abrir el archivo para escribir.\n");
        return;
    }

    fprintf(file, "Frecuencia (Hz),PSD\n");  // Encabezado de columnas

    for (int i = 0; i < length; i++) {
        fprintf(file, "%f,%.15f\n", f[i], Pxx[i]);  // Escribe frecuencia y PSD en cada fila
    }

    fclose(file);
    printf("Datos guardados en %s\n", filename);
}