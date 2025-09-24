#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <complex.h>
#include "Modules/capture.h"
#include "Modules/processing.h"
#include "Modules/storage.h"

int main(int argc, char *argv[]) {
    long samples = (argc > 1) ? strtol(argv[1], NULL, 10) : 20000000;
    uint64_t freq = (argc > 2) ? strtol(argv[2], NULL, 10) : 98;

    if (capture_signal(samples, freq) != 0) return 1;

    size_t N;
    complex double* x = convert_cs8("Samples/0", &N);
    if (!x) return 1;

    remove_dc(x, N);

    int segment_length = 4096;
    double fs = 20000000;
    double overlap = 0.75;

    double* f = malloc(segment_length * sizeof(double));
    double* Pxx_dB = malloc(segment_length * sizeof(double));

    compute_welch_psd(x, N, fs, segment_length, overlap, f, Pxx_dB);
    save_psd_to_csv(f, Pxx_dB, segment_length, "Outputs/resultado_psd_db.csv");

    free(x); free(f); free(Pxx_dB);
    return 0;
}