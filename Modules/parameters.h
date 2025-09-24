/**
 * @file parameters.h
 * @brief Definción de la función que calcula parámetros a partir de señales IQ utilizando el método de Welch y análisis de canales.
 * 
 * Este archivo contiene la función `parameter` que realiza análisis espectral 
 * y cálculo de parámetros para un conjunto de canales a partir de una señal IQ.
 */

#ifndef PARAMETER_ANALYSIS_H
#define PARAMETER_ANALYSIS_H

#include <stdint.h>
#include "../Drivers/bacn_RTI.h"

/**
 * @brief Realiza análisis espectral y genera parámetros para canales específicos.
 * 
 * Esta función procesa una señal IQ, calcula su densidad espectral de potencia (PSD) utilizando el método de Welch,
 * y analiza los canales especificados para calcular parámetros como potencia, SNR y presencia de señales.
 * 
 * @param threshold Umbral de potencia máxima para determinar la presencia de una señal.
 * @param canalization Arreglo de frecuencias centrales de los canales a analizar.
 * @param bandwidth Ancho de banda de cada canal correspondiente a `canalization`.
 * @param canalization_length Número de canales a analizar.
 * @param central_freq Frecuencia central de la señal (en Hz).
 * @param file_sample Archivo de entrada que contiene la señal IQ en formato CS8.
 */

void parameter(st_server *s_server, int threshold, double* canalisation, double* bandwidth, int canalisation_length, uint64_t central_freq, uint8_t file_sample, char* banda, char* Flow, char* Fhigh);

#endif // PARAMETER_ANALYSIS_H