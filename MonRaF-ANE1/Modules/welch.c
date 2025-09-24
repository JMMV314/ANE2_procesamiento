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
                       double* f_out, double* P_welch_out) 
{
    // Convertimos overlap fraccional a muestras
    int noverlap = (int)(segment_length * overlap);
    if (noverlap >= segment_length) {
        fprintf(stderr, "Error: overlap demasiado grande.\n");
        return;
    }

    int nperseg = segment_length;
    int nfft = segment_length;  // mantenemos FFT igual al segmento (se puede extender)
    int step = nperseg - noverlap;
    if (step <= 0) {
        fprintf(stderr, "Error: Overlap results in a non-positive step size.\n");
        return;
    }

    int k_segments = (N_signal - noverlap) / step;
    if (k_segments <= 0) {
        fprintf(stderr, "Error: Signal is too short for the given segment and overlap settings.\n");
        return;
    }

    // Ventana Hamming (manteniendo comportamiento del código 1)
    double window[nperseg];
    for (int n = 0; n < nperseg; n++) {
        window[n] = 0.54 - 0.46 * cos((2.0 * PI * n) / (nperseg - 1));
    }

    // Factor de normalización U
    double u_norm = 0.0;
    for (int i = 0; i < nperseg; i++) {
        u_norm += window[i] * window[i];
    }
    u_norm /= nperseg;

    // Buffers FFTW
    complex double* segment = fftw_alloc_complex(nfft);
    complex double* x_k_fft = fftw_alloc_complex(nfft);
    fftw_plan plan = fftw_plan_dft_1d(nfft, segment, x_k_fft, FFTW_FORWARD, FFTW_ESTIMATE);

    // Inicializar acumulador PSD
    memset(P_welch_out, 0, nfft * sizeof(double));

    // Loop principal por segmentos
    for (int k = 0; k < k_segments; k++) {
        int start_index = k * step;

        // Aplicar ventana
        for (int i = 0; i < nperseg; i++) {
            segment[i] = signal[start_index + i] * window[i];
        }
        // Zero padding si nfft > nperseg
        for (int i = nperseg; i < nfft; i++) {
            segment[i] = 0.0;
        }

        // FFT
        fftw_execute(plan);

        // Acumular |X[k]|^2
        for (int i = 0; i < nfft; i++) {
            double mag = cabs(x_k_fft[i]);
            P_welch_out[i] += (mag * mag);
        }
    }

    // Promediar y escalar
    double scale = 1.0 / (fs * u_norm * k_segments * nperseg);
    for (int i = 0; i < nfft; i++) {
        P_welch_out[i] *= scale;
    }

    // --- fftshift para centrar en [-fs/2, fs/2] ---
    int half = nfft / 2;
    for (int i = 0; i < half; i++) {
        double tmp = P_welch_out[i];
        P_welch_out[i] = P_welch_out[i + half];
        P_welch_out[i + half] = tmp;
    }

    // Frecuencias asociadas
    double df = fs / nfft;
    for (int i = 0; i < nfft; i++) {
        f_out[i] = -fs / 2.0 + i * df;
    }

    printf("[welch] PSD computation complete.\n");

    // Liberar recursos
    fftw_destroy_plan(plan);
    fftw_free(segment);
    fftw_free(x_k_fft);
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