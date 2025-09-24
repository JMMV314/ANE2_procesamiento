#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <complex.h>
#include "bacn_RF.h"
#include "cs8_to_iq.h"
#include "capture.h"

// Stub necesario por bacn_RF
int central_freq[10] = {100};
void switch_ANTENNA(bool RF) {
    (void)RF;
    printf("[stub] switch_ANTENNA llamado (ignorado)\n");
}

int capture_signal(long samples_to_xfer_max, uint64_t central_frequency_mhz) {
    transceiver_mode_t mode = TRANSCEIVER_MODE_RX;
    uint16_t lna_gain = 0;
    uint16_t vga_gain = 0;
    uint16_t centralFrec_TDT = 200;
    bool is_second_sample = false;

    mkdir("Samples", 0777);

    printf("▶ Capturando en %lu MHz (LNA=%u, VGA=%u, N=%ld)...\n",
           central_frequency_mhz, lna_gain, vga_gain, samples_to_xfer_max);

    int r = getSamples(central_frequency_mhz, samples_to_xfer_max,
                       mode, lna_gain, vga_gain,
                       centralFrec_TDT, is_second_sample);

    if (r != 0) {
        fprintf(stderr, "❌ getSamples devolvió %d\n", r);
        return 1;
    }

    printf("✅ Captura terminada. Archivo CS8 en Samples/0\n");
    return 0;
}

complex double* convert_cs8(const char* filename, size_t* N) {
    complex double* x = cargar_cs8(filename, N);
    if (!x) {
        fprintf(stderr, "❌ Error cargando %s\n", filename);
        return NULL;
    }
    printf("📥 %zu muestras cargadas desde %s\n", *N, filename);
    return x;
}

