/**
 * @file tdt_functions.c
 * @brief Análisis y procesamiento de señales IQ para parámetros clave.
 *
 * Este archivo incluye funciones para calcular parámetros como C/N, MER, y BER
 * usando técnicas de procesamiento de señales, como el método de Welch, y 
 * genera resultados relacionados con modulación digital.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>

#include "save_to_file.h"
#include "welch.h"
#include "tdt_functions.h"
#include "find_closest_index.h"
#include "parameters.h"

#define M_PI 3.14159265358979323846
#define PI 3.14159265358979323846


double c_n(double* Pxx, double* f, int f_low, int f_high) {
    double signal_power = median(Pxx, f_low, f_high);
    double n = Pxx[0];
    double c_n_ratio = 10.0 * log10(signal_power / n);
    return c_n_ratio, signal_power;
}


double mer(int f_low, int f_high, double* Pxx, int N) {
    // Inicializar los valores máximo y mínimo en el array completo
    Pxx[513]= Pxx[510];
    Pxx[512]= Pxx[510];
    Pxx[511]= Pxx[510];
    double max_power = Pxx[0];
    double min_power = Pxx[0];
    int a;

    // Buscar la potencia máxima y mínima en todo el array Pxx
    for (int i = 1; i < N; i++) {
        if (Pxx[i] > max_power) {
            max_power = Pxx[i];
            a=i;
        }
        if (Pxx[i] < min_power) {
            min_power = Pxx[i];
            
        }
    }
    // Calcular MER en dB como la relación entre la potencia máxima y mínima
    return 10.0 * log10(max_power / min_power);
}



void analyze_signal(double frecuencia, int modulation, complex double* data, size_t data_len, double* mer_value, double* ber_value, double* c_n_value, double* signal_power) {
    int segment_length = 4096;
    double fs = 6500000;
    //char window[10] = 'Hamming';
    double bandwidth = 6500000;
    double overlap = 0.0;

    double* f1 = (double*)malloc(segment_length * sizeof(double));
    double* Pxx1 = (double*)malloc(segment_length * sizeof(double));

    // Verificación de asignaciones de memoria en un solo bloque
    if (!f1 || !Pxx1) {
        fprintf(stderr, "Memory allocation failed.\n");
        free(f1);
        free(Pxx1);
        return; // Usa return para manejar errores sin exit
    }
    // Calcular PSD usando Welch
    welch_psd_complex(data, data_len, fs, segment_length, overlap, f1, Pxx1);

    //-----------Acomodar potencias de welch-----------
    int half = segment_length / 2;
    double* temp = (double*)malloc(segment_length * sizeof(double));

    for (int i = 0; i < half; i++) {
        temp[i] = Pxx1[half + i];
    }


    for (int i = 0; i < half; i++) {
        temp[half + i] = Pxx1[i];
    }

    for (int i = 0; i < segment_length; i++) {
        Pxx1[i] = temp[i];
    }

    free(temp);

    for (int i = 0; i < segment_length; i++) {
        f1[i] = (f1[i] + frecuencia) / 1000000;
    }
    int index = segment_length/2 ;
    int count = segment_length*0.002;
    int b=index-(count+5);
    int a=index;

    for (int i=0; i<count; i++)
    {   
        b=b-3;
        Pxx1[a]=Pxx1[b];
        a=a-1;
        
        
    }
    a=index;
    for (int i=0; i<count; i++)
    {   
        Pxx1[a]=Pxx1[b];
        a=a+1;
        b=b+2;
        
    }

    

    double fc = frecuencia;
    int f_low = find_closest_index(f1, segment_length/2, fc - 3);
    int f_high = find_closest_index(f1, segment_length/2, fc + 3);

    f_low = 40;
    f_high = 950;

    // Verificación de índices válidos para evitar accesos fuera de los límites
    if (f_low < 0 || f_high >= segment_length || f_low >= f_high) {
        fprintf(stderr, "Invalid frequency indices.\n");
        free(f1);
        free(Pxx1);
        return;
    }

    // Calcular MER, BER y C/N
    *mer_value = mer(f_low, f_high , Pxx1, 1024);

    float MOD= modulation;
    *ber_value = calculate_BER_from_snr(*mer_value, MOD);
   
    *c_n_value, *signal_power = c_n(Pxx1, f1, f_low, f_high);

    // Liberar memoria
    free(f1);
    free(Pxx1);
}


void fftshift(double* input, double* output, int N) {
    int k = N / 2;
    for (int i = 0; i < N; i++) {
        output[i] = input[(i + k) % N];
    }
}


void linspace(double start, double end, int num, double* array) {
    double delta = (end - start) / (num - 1);
    for (int i = 0; i < num; i++) {
        array[i] = start + i * delta;
    }
}


int argmin_abs_difference(double* array, int length, double value) {
    int argmin = 0;
    double min_diff = fabs(array[0] - value);
    for (int i = 1; i < length; i++) {
        double diff = fabs(array[i] - value);
        if (diff < min_diff) {
            min_diff = diff;
            argmin = i;
        }
    }
    return argmin;
}


double trapz(double* y, double* x, int start, int end) {
    double integral = 0.0;
    for (int i = start; i < end - 1; i++) {
        double dx = x[i + 1] - x[i];
        integral += 0.5 * (y[i] + y[i + 1]) * dx;
    }
    return integral;
}


/**double median(double* array, int start, int end) {
    int length = end - start;
    double* temp = (double*)malloc(length * sizeof(double));
    if (temp == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < length; i++) {
        temp[i] = array[start + i];
    }
    qsort(temp, length, sizeof(double), compare_doubles);

    double med;
    if (length % 2 == 0) {
        med = 0.5 * (temp[length / 2 - 1] + temp[length / 2]);
    } else {
        med = temp[length / 2];
    }
    free(temp);
    return med;
}
*/

int compare_doubles(const void* a, const void* b) {
    double x = *(const double*)a;
    double y = *(const double*)b;
    if (x < y) return -1;
    else if (x > y) return 1;
    else return 0;
}


static double integrand(double t) {
    return exp(-t * t);
}

double erfc_manual(double x, int num_steps) {
    if (x > 10) return 0.0;  // Limitar x para evitar inestabilidad

    double step_size = 10.0 / num_steps;
    double integral_approx = 0.0;

    for (int i = 0; i < num_steps; i++) {
        double t1 = x + i * step_size;
        double t2 = x + (i + 1) * step_size;
        integral_approx += 0.5 * (integrand(t1) + integrand(t2)) * step_size;
    }

    return (2.0 / sqrt(PI)) * integral_approx;
}

double Q(double x) {
    return 0.5 * erfc_manual(x / sqrt(2.0), 10000);
}


double calculate_BER_from_snr(double SNR, float M) {
    if (SNR <= 0 || M <= 1) {
        fprintf(stderr, "SNR y M deben ser positivos.\n");
        return NAN;
    }

    return (4.0 / log2(M)) * (1.0 - 1.0 / sqrt(M)) * Q(sqrt((3.0 * log2(M) / (M - 1.0)) * SNR));
}