/**
 * @file save_to_file.h
 * @brief Definición de función para guardar datos en un archivo CSV.
 *
 * Esta función permite almacenar datos de frecuencias y densidades espectrales de potencia (PSD)
 * en un archivo CSV con formato adecuado para análisis posterior.
 */

#ifndef SAVE_TO_FILE_H
#define SAVE_TO_FILE_H

#include <stdio.h>

/**
 * @brief Guarda los valores de frecuencia y PSD en un archivo CSV.
 * 
 * Esta función toma los arreglos de frecuencias `f` y PSD `Pxx`, junto con su longitud,
 * y guarda cada par de valores en un archivo en formato CSV.
 *
 * @param f Arreglo de frecuencias.
 * @param Pxx Arreglo de densidad espectral de potencia (PSD).
 * @param length Longitud de los arreglos `f` y `Pxx`.
 * @param filename Nombre del archivo donde se guardarán los datos.
 */
void save_to_file(double* f, double* Pxx, int length, const char* filename);

#endif // SAVE_TO_FILE_H