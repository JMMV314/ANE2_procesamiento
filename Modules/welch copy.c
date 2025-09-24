/**
 * @file welch.c
 * @brief Compute the Power Spectral Density (PSD) of a complex signal and generate its frequency bins.
 *
 * Implements Welch’s method: splits the signal into overlapping segments,
 * applies a Hamming window, performs FFT on each segment, averages the periodograms,
 * and outputs PSD values with associated frequencies.
 */

#include <stdio.h>
#include <stdlib.h>
#include <complex.h>
#include <fftw3.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>

#include "welch.h"

#define PI 3.14159265358979323846

/**
 * @brief Generate a Hamming window.
 *
 * @param window         Array to fill with window coefficients (length = segment_length).
 * @param segment_length Number of samples in each segment/window.
 */
void generate_hamming_window(double* window, int segment_length) {
    for (int n = 0; n < segment_length; n++) {
        window[n] = 0.54 - 0.46 * cos((2.0 * PI * n) / (segment_length - 1));
    }
}

/**
 * @brief Compute the PSD of a complex signal using Welch’s method.
 *
 * Splits the input into overlapping segments, windows each segment,
 * executes the FFT, accumulates and averages the spectral power,
 * and fills output arrays with PSD values and corresponding frequencies.
 *
 * @param signal         Pointer to input complex signal array (length = N_signal).
 * @param N_signal       Total number of samples in the input signal.
 * @param fs             Sampling rate in Hz.
 * @param segment_length Number of samples per segment.
 * @param overlap        Fractional overlap between segments (0 ≤ overlap < 1).
 * @param f_out          Output array for frequency bins (length = segment_length).
 * @param P_welch_out    Output array for PSD values (length = segment_length).
 */
void welch_psd_complex(complex double* signal, size_t N_signal, double fs, 
                       int segment_length, double overlap, 
                       double* f_out, double* P_welch_out) {
    int step = (int)(segment_length * (1.0 - overlap));
    int K = ((int)N_signal - segment_length) / step + 1;
    size_t psd_size = segment_length;

    /* Allocate and prepare the window */
    double window[segment_length];
    generate_hamming_window(window, segment_length);

    /* Compute window normalization factor */
    double U = 0.0;
    for (int i = 0; i < segment_length; i++) {
        U += window[i] * window[i];
    }
    U /= segment_length;

    /* Allocate FFT input/output buffers and plan */
    complex double* segment = fftw_alloc_complex(segment_length);
    complex double* X_k     = fftw_alloc_complex(segment_length);
    fftw_plan plan = fftw_plan_dft_1d(segment_length, segment, X_k, FFTW_FORWARD, FFTW_ESTIMATE);

    /* Initialize PSD accumulator */
    memset(P_welch_out, 0, psd_size * sizeof(double));

    /* Loop over each segment */
    for (int k = 0; k < K; k++) {
        int start = k * step;

        /* Apply window to the current segment */
        for (int i = 0; i < segment_length; i++) {
            segment[i] = signal[start + i] * window[i];
        }

        /* Perform FFT */
        fftw_execute(plan);

        /* Accumulate spectral power */
        for (size_t i = 0; i < psd_size; i++) {
            double mag = cabs(X_k[i]);
            P_welch_out[i] += (mag * mag) / (fs * U);
        }
    }

    /* Average over all segments */
    for (size_t i = 0; i < psd_size; i++) {
        P_welch_out[i] /= K;
    }

    /* Generate frequency bins (from –fs/2 to +fs/2) */
    double df = fs / segment_length;
    for (size_t i = 0; i < psd_size; i++) {
        f_out[i] = -fs / 2 + i * df;
    }

    printf("[welch] PSD computation complete.\n");

    /* Clean up FFT resources */
    fftw_destroy_plan(plan);
    fftw_free(segment);
    fftw_free(X_k);
}

// Helper function for advanced DC spike correction using second acquisition
bool DC_spike_correction(double* psd1, double* f1, double* psd2, double* f2) {

    int length1 = sizeof(psd1);
    int length2 = sizeof(psd2);


    const int correction_width = 50;

    if (psd1 == NULL || f1 == NULL || 
        psd2 == NULL || f2 == NULL) {
        return false;
    }

    double center_freq1 =  f1[(int)(length1/2)]; 
    double center_freq2 =  f2[(int)(length2/2)]; 
    
    // Find the center frequency index in both arrays
    int center_index1 = -1;
    int center_index2 = -1;
    
    // Find the closest index to the center frequency in both arrays
    double min_diff1 = 1e9;
    double min_diff2 = 1e9;
    
    for (int i = 0; i < length1; i++) {
        double diff = fabs(f1[i] - center_freq1);
        if (diff < min_diff1) {
            min_diff1 = diff;
            center_index1 = i;
        }
    }
    
    for (int i = 0; i < length2; i++) {
        double diff = fabs(f2[i] - center_freq2);
        if (diff < min_diff2) {
            min_diff2 = diff;
            center_index2 = i;
        }
    }
    
    if (center_index1 < 0 || center_index2 < 0) {
        return false;
    }
    
    // Calculate the frequency step in both arrays
    double freq_step1 = (length1 > 1) ? fabs(f1[1] - f1[0]) : 0;
    double freq_step2 = (length2 > 1) ? fabs(f2[1] - f2[0]) : 0;
    
    if (freq_step1 <= 0 || freq_step2 <= 0) {
        return false;
    }
    
    // Calculate the number of points to correct on each side
    int points_to_correct = correction_width;
    
    // Replace the DC spike region in psd1 with corresponding values from psd2
    for (int i = -points_to_correct; i <= points_to_correct; i++) {
        int idx1 = center_index1 + i;
        
        if (idx1 >= 0 && idx1 < length1) {
            // Calculate the absolute frequency for this point
            double freq = f1[idx1];
            
            // Find the closest frequency in the second array
            int closest_idx2 = -1;
            double min_freq_diff = 1e9;
            
            for (int j = 0; j < length2; j++) {
                double diff = fabs(f2[j] - freq);
                if (diff < min_freq_diff) {
                    min_freq_diff = diff;
                    closest_idx2 = j;
                }
            }
            
            if (closest_idx2 >= 0 && closest_idx2 < length2) {
                // Aplicar un factor de corrección para ajustar los valores de magnitud
                // Calculamos la diferencia promedio entre los valores cercanos no afectados por el DC spike
                double correction_factor = 0.0;
                int num_samples = 0;
                
                // Tomamos muestras fuera de la región del DC spike para calcular el factor de corrección
                int sample_range = 10; // Número de muestras a considerar
                int start_sample = points_to_correct + 5; // Comenzamos justo después de la región del DC spike
                
                for (int k = 0; k < sample_range; k++) {
                    int sample_idx1 = center_index1 + start_sample + k;
                    if (sample_idx1 >= 0 && sample_idx1 < length1) {
                        // Encontrar la frecuencia correspondiente en el segundo array
                        double sample_freq = f1[sample_idx1];
                        int sample_idx2 = -1;
                        double min_sample_diff = 1e9;
                        
                        for (int j = 0; j < length2; j++) {
                            double diff = fabs(f2[j] - sample_freq);
                            if (diff < min_sample_diff) {
                                min_sample_diff = diff;
                                sample_idx2 = j;
                            }
                        }
                        
                        if (sample_idx2 >= 0 && sample_idx2 < length2) {
                            // Calcular la diferencia entre los valores de PSD
                            // Usamos valores logarítmicos para una mejor comparación
                            double psd1_db = 10.0 * log10(psd1[sample_idx1]);
                            double psd2_db = 10.0 * log10(psd2[sample_idx2]);
                            correction_factor += (psd1_db - psd2_db);
                            num_samples++;
                        }
                    }
                }
                
                // Calcular el factor de corrección promedio
                if (num_samples > 0) {
                    correction_factor /= num_samples;
                }
                
                // Aplicar el factor de corrección al valor de PSD2 antes de reemplazar
                double psd2_db = 10.0 * log10(psd2[closest_idx2]);
                double corrected_psd_db = psd2_db + correction_factor;
                double corrected_psd = pow(10.0, corrected_psd_db / 10.0);
                
                // Reemplazar el valor en psd1 con el valor corregido de psd2
                psd1[idx1] = corrected_psd;
            }
        }
    }
    
    return true;
}