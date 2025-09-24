/**
 * @file parameters_rni.h
 * @brief Definción de funciones que calculan parámetros para niveles de radiación electromagnética (RNI).
 * 
 * Este archivo contiene la definición de la función `parameter_rni`, que analiza señales IQ 
 * para calcular parámetros relacionados con radiación electromagnética, incluyendo densidad espectral de potencia (PSD),
 * intensidad de campo eléctrico (V/m) y límite ocupado.
 */

#ifndef PARAMETER_H
#define PARAMETER_H

#include <stdint.h>
#include <complex.h>
#include "../Drivers/bacn_RTI.h"

/**
 * @brief Realiza análisis espectral y calcula parámetros relacionados con radiación electromagnética.
 * 
 * Esta función procesa una señal IQ para calcular su PSD mediante el método de Welch, analiza canales
 * definidos, y genera parámetros como intensidad de campo eléctrico (V/m) y porcentaje del límite ocupado.
 * 
 * @param threshold Umbral de potencia máxima para determinar la presencia de una señal.
 * @param canalization Arreglo de frecuencias centrales de los canales a analizar.
 * @param bandwidth Ancho de banda de cada canal correspondiente a `canalization`.
 * @param canalization_length Número de canales a analizar.
 * @param central_freq Frecuencia central de la señal (en Hz).
 * @param file_sample Archivo de entrada que contiene la señal IQ en formato CS8.
 * 
 * @note Genera un archivo JSON con los resultados para cada canal analizado.
 */

void parameter_rni(st_server *s_server, int threshold, double* canalisation, double* bandwidth, int canalisation_length, uint64_t central_freq, uint8_t file_sample, char* banda, char* Flow, char* Fhigh);

#endif // PARAMETER_H
