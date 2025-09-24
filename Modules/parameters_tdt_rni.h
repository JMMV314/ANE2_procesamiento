/**
 * @file parameters_tdt_rni.h
 * @brief Definción de función para analizar señales RNI y generar un archivo JSON con resultados.
 *
 * Este archivo contiene la definición de la función `parameter_tdt_rni` que calcula métricas relacionadas con la radiación electromagnética, 
 * como la intensidad de campo eléctrico (V/m) y el porcentaje del límite ocupado, 
 * guardándolos en un archivo JSON.
 */

#ifndef PARAMETER_H
#define PARAMETER_H

#include <stdint.h>
#include <complex.h>

/**
 * @brief Analiza datos de señales RNI y guarda los resultados en un archivo JSON.
 *
 * @param modulation Esquema de modulación utilizado (por ejemplo, QAM, PSK).
 * @param central_freq Frecuencia central de la señal (en Hz).
 * @param file_sample Identificador del archivo de muestra CS8 que contiene los datos IQ.
 *
 * La función procesa datos IQ para calcular métricas como:
 * - Intensidad del campo eléctrico \( V/m \).
 * - Porcentaje del límite permitido para la intensidad del campo.
 *
 * Los resultados se almacenan en un archivo JSON llamado `parameters_tdt.json`.
 * 
 * Si ocurre un error al abrir el archivo, muestra un mensaje de error.
 */

void parameter_tdt_rni(int threshold, double* canalization, double* bandwidth, int canalization_length, uint64_t central_freq, uint8_t file_sample);

#endif // PARAMETER_H
