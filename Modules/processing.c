#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "welch.h"
#include "processing.h"

void remove_dc(complex double* x, size_t N) {
    complex double mean = 0.0;
    for (size_t i = 0; i < N; i++) mean += x[i];
    mean /= (double)N;
    for (size_t i = 0; i < N; i++) x[i] -= mean;
}

void compute_welch_psd(complex double* x, size_t N, double fs,
                       int segment_length, double overlap,
                       double* f, double* Pxx_dB) {

    double* Pxx = malloc(segment_length * sizeof(double));
    if (!Pxx) {
        fprintf(stderr, "❌ Error al reservar memoria para Pxx\n");
        return;
    }

    welch_psd_complex(x, N, fs, segment_length, overlap, f, Pxx);

    const double eps = 1e-15;
    for (int i = 0; i < segment_length; i++) {
        Pxx_dB[i] = 10.0 * log10(Pxx[i] + eps);
    }

    free(Pxx);
}
