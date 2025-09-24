/**
 * @file tdt_functions.h
 * @brief Definición de funciones para el análisis y procesamiento de señales IQ para parámetros clave.
 *
 * Este archivo incluye funciones para calcular parámetros como C/N, MER, y BER
 * usando técnicas de procesamiento de señales, como el método de Welch, y 
 * genera resultados relacionados con modulación digital.
 */

#ifndef ANALYZE_SIGNAL_H
#define ANALYZE_SIGNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>

#define M_PI 3.14159265358979323846

/**
 * @brief Calcula la relación portadora/ruido (C/N) y la potencia de señal.
 *
 * @param Pxx Densidad espectral de potencia (PSD) calculada.
 * @param f Vector de frecuencias asociadas a la PSD.
 * @param f_low Índice inferior del rango de frecuencia de interés.
 * @param f_high Índice superior del rango de frecuencia de interés.
 * @return Relación C/N (en dB) y potencia de señal.
 */

double c_n(double* Pxx, double* f, int f_low, int f_high);

/**
 * @brief Calcula la Modulation Error Ratio (MER).
 *
 * @param f_low Índice inferior del rango de frecuencia de interés.
 * @param f_high Índice superior del rango de frecuencia de interés.
 * @param Pxx Densidad espectral de potencia (PSD) calculada.
 * @param N Longitud del vector PSD.
 * @return Valor de MER en dB.
 */

double mer(int f_low, int f_high, double* Pxx, int N);

/**
 * @brief Analiza señales IQ para calcular parámetros como MER, BER y C/N.
 *
 * @param frecuencia Frecuencia central de la señal (en Hz).
 * @param modulation Tipo de modulación (ej., 64-QAM, 16-QAM).
 * @param data Datos IQ complejos.
 * @param data_len Número de muestras en los datos IQ.
 * @param mer_value Referencia para almacenar el valor calculado de MER.
 * @param ber_value Referencia para almacenar el valor calculado de BER.
 * @param c_n_value Referencia para almacenar el valor calculado de C/N.
 * @param signal_power Referencia para almacenar la potencia de la señal.
 */

void analyze_signal(double frecuencia, int modulation, complex double* data, size_t data_len, double* mer_value, double* ber_value, double* c_n_value, double* signal_power);

/**
 * @brief Realiza un desplazamiento FFT para reordenar el espectro.
 *
 * @param input Vector de entrada.
 * @param output Vector de salida con el espectro desplazado.
 * @param N Tamaño del vector.
 */

void fftshift(double* input, double* output, int N);

/**
 * @brief Genera un vector de valores espaciados linealmente.
 *
 * @param start Valor inicial.
 * @param end Valor final.
 * @param num Número de valores a generar.
 * @param array Arreglo donde se almacenan los valores generados.
 */

void linspace(double start, double end, int num, double* array);

/**
 * @brief Encuentra el índice del valor más cercano a uno dado en un array.
 *
 * @param array Vector de entrada.
 * @param length Longitud del vector.
 * @param value Valor a buscar.
 * @return Índice del valor más cercano.
 */

int argmin_abs_difference(double* array, int length, double value);

/**
 * @brief Calcula la integral aproximada usando la regla del trapecio.
 *
 * @param y Vector de valores de la función.
 * @param x Vector de valores de la variable independiente.
 * @param start Índice inicial para la integración.
 * @param end Índice final para la integración.
 * @return Integral aproximada.
 */

double trapz(double* y, double* x, int start, int end);

/**
 * @brief Calcula la mediana de un subrango en un array.
 *
 * @param array Vector de entrada.
 * @param start Índice inicial del rango.
 * @param end Índice final del rango.
 * @return Mediana del rango especificado.
 */

double median(double* array, int start, int end);

/**
 * @brief Comparador para ordenar valores de tipo `double`.
 *
 * @param a Primer valor a comparar.
 * @param b Segundo valor a comparar.
 * @return -1 si `a < b`, 1 si `a > b`, 0 si son iguales.
 */

int compare_doubles(const void* a, const void* b);

/**
 * @brief Función base para la función de error complementaria.
 *
 * @param t Valor de entrada.
 * @return Resultado de la función integrando `exp(-t^2)`.
 */

static double integrand(double t);

/**
 * @brief Calcula la función de error complementaria (erfc) manualmente.
 *
 * @param x Valor de entrada.
 * @param num_steps Número de pasos en la aproximación.
 * @return Valor aproximado de `erfc(x)`.
 */

double erfc_manual(double x, int num_steps);

/**
 * @brief Calcula la función Q para un valor dado.
 *
 * @param x Valor de entrada.
 * @return Valor de la función Q.
 */

double Q(double x);

/**
 * @brief Calcula la BER basada en el SNR y la modulación.
 *
 * @param SNR Relación señal-ruido (en dB).
 * @param M Orden de la modulación (ej., 16 para 16-QAM).
 * @return BER estimada.
 */

double calculate_BER_from_snr(double SNR, float M);

#endif // ANALYZE_SIGNAL_H


