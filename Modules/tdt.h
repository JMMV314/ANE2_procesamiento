/**
 * @file tdt.h
 * @brief Declaraciones de funciones y definiciones para el análisis de señales TDT.
 *
 * Este archivo contiene las declaraciones de funciones y estructuras necesarias
 * para procesar señales IQ y calcular parámetros clave en sistemas de transmisión
 * digital terrestre (TDT).
 */

#ifndef TDT_H
#define TDT_H

#include <stdint.h>
#include <complex.h>
#include "cJSON.h"
#include "cs8_to_iq.h"
#include "tdt_functions.h"
#include "../Drivers/bacn_RTI.h"
/**
 * @brief Procesa señales IQ para calcular parámetros clave de transmisión digital terrestre.
 * 
 * Esta función carga datos de señal IQ, analiza los parámetros relevantes para una transmisión TDT
 * basada en la modulación especificada, y genera un archivo JSON con los resultados.
 * 
 * @param modulation Tipo de modulación (ej., QPSK, QAM16, etc.).
 * @param central_freq Frecuencia central de la señal (en Hz).
 * @param file_sample Identificador del archivo que contiene los datos de señal IQ en formato CS8.
 * 
 * @return Código de salida `EXIT_SUCCESS` si la operación se realiza con éxito.
 * 
 * @note Genera un archivo JSON llamado `parameters_tdt.json` con los resultados.
 */
void parameter_tdt(st_server *s_server, int modulation, uint64_t central_freq, uint8_t file_sample, char* channel);

#endif // TDT_H