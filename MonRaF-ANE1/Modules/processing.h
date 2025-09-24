#ifndef PROCESSING_H
#define PROCESSING_H

#include <complex.h>
#include <stddef.h>

void remove_dc(complex double* x, size_t N);
void compute_welch_psd(complex double* x, size_t N, double fs,
                       int segment_length, double overlap,
                       double* f, double* Pxx_dB);

#endif
