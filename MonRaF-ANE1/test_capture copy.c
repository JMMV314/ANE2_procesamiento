#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <complex.h>    // para complex double
#include <math.h>       // para log10
#include <sys/stat.h>
#include <sys/types.h>

// --- headers de tus módulos ---
#include "Modules/bacn_RF.h"
#include "Modules/cs8_to_iq.h"
#include "Modules/welch.h"
#include "Modules/save_to_file.h"

long samples_to_xfer_max = 20000000; // valor por defecto (20M)---------------------


// --- STUBS requeridos por bacn_RF.c ---
int central_freq[10] = {100}; // se rellena en getSamples()

void switch_ANTENNA(bool RF) {
    (void)RF;
    printf("[stub] switch_ANTENNA llamado (ignorado)\n");
}
// -------------------------------------

// --- Función auxiliar: fftshift para arrays double ---
void fftshift_double(double* a, int M) {
    int h = M / 2;
    for (int i = 0; i < h; i++) {
        double tmp = a[i];
        a[i] = a[i + h];
        a[i + h] = tmp;
    }
}

void fftshift_pair(double* f, double* P, int M) {
    fftshift_double(f, M);
    fftshift_double(P, M);
}
// -----------------------------------------------------

void capture(long num_samples){

    // ================================
    // 0) CONFIGURAR Nº DE MUESTRAS
    // ================================
    // Esta variable está definida en bacn_RF.c y declarada como extern en bacn_RF.h
    // Cambia aquí según lo que necesites:
    // samples_to_xfer_max = 200000;    // 200k
    // samples_to_xfer_max = 2000000;   // 2M
    // samples_to_xfer_max = 20000000;  // 20M
    samples_to_xfer_max = num_samples;  // 20M


    // ================================
    // 1) PARÁMETROS DE CAPTURA
    // ================================
    uint8_t  bands       = 0;                  // nº de bandas a usar
    transceiver_mode_t mode = TRANSCEIVER_MODE_RX; // modo RX
    uint16_t lna_gain    = 32;                 // Ganancia LNA (0–40 dB)
    uint16_t vga_gain    = 32;                 // Ganancia VGA (0–62 dB)
    uint16_t centralFrec = 200;                // Frecuencia central (MHz)
    bool     is_second_sample = false;         // false = primera captura

    // Asegurar que exista la carpeta
    mkdir("Samples", 0777);

    printf("▶ Capturando en %u MHz (LNA=%u, VGA=%u, N=%ld)...\n",
           centralFrec, lna_gain, vga_gain, samples_to_xfer_max);

    int r = getSamples(bands, mode, lna_gain, vga_gain,
                       centralFrec, is_second_sample);
    if (r != 0) {
        fprintf(stderr, "❌ getSamples devolvió %d\n", r);
        return 1;
    }

    printf("✅ Captura terminada. Archivo CS8 en Samples/0\n");

    // ================================
    // 2) CONVERTIR CS8 → IQ
    // ================================
    size_t N;
    complex double* x = cargar_cs8("Samples/0", &N);
    if (!x) {
        fprintf(stderr, "❌ Error cargando Samples/0\n");
        return 1;
    }
    printf("📥 %zu muestras cargadas desde Samples/0\n", N);

    // --- eliminar DC (quitar pico central) ---
    complex double mean = 0.0;
    for (size_t i = 0; i < N; i++) mean += x[i];
    mean /= (double)N;
    for (size_t i = 0; i < N; i++) x[i] -= mean;

    // ================================
    // 3) CÁLCULO DE PSD (Welch)
    // ================================
    // ⚠️ IMPORTANTE: fs debe coincidir con DEFAULT_SAMPLE_RATE_HZ en bacn_RF.c
    double fs = 20000000;             // Hz (ejemplo: 4 MHz)
    int segment_length = 4096;   // tamaño de ventana
    double overlap = 0.75;       // 75% de solapamiento

    // reservar arrays de salida
    double* f   = malloc(segment_length * sizeof(double));
    double* Pxx = malloc(segment_length * sizeof(double));
    if (!f || !Pxx) {
        fprintf(stderr, "❌ Error al reservar memoria para PSD\n");
        free(x);
        return 1;
    }

    // calcular PSD
    welch_psd_complex(x, N, fs, segment_length, overlap, f, Pxx);

    // --- centrar espectro (fftshift) ---
    fftshift_pair(f, Pxx, segment_length);

    // --- convertir a dB ---
    double *Pxx_dB = malloc(segment_length * sizeof(double));
    if (!Pxx_dB) {
        fprintf(stderr, "❌ Error al reservar memoria para Pxx_dB\n");
        free(x); free(f); free(Pxx);
        return 1;
    }
    const double eps = 1e-15;
    for (int i = 0; i < segment_length; i++)
        Pxx_dB[i] = 10.0 * log10(Pxx[i] + eps);

    // ================================
    // 4) GUARDAR RESULTADO A CSV
    // ================================
    mkdir("Outputs", 0777);
    save_to_file(f, Pxx, segment_length, "Outputs/resultado_psd.csv");
    save_to_file(f, Pxx_dB, segment_length, "Outputs/resultado_psd_db.csv");
    printf("💾 PSD guardada en resultado_psd.csv (lineal) y resultado_psd_db.csv (dB)\n");

    // ================================
    // 5) LIMPIEZA
    // ================================
    free(x);
    free(f);
    free(Pxx);
    free(Pxx_dB);

    // return 0;
}

int main(void) {
    // ================================
    // 0) CONFIGURAR Nº DE MUESTRAS
    // ================================
    // Esta variable está definida en bacn_RF.c y declarada como extern en bacn_RF.h
    // Cambia aquí según lo que necesites:
    // samples_to_xfer_max = 200000;    // 200k
    samples_to_xfer_max = 2000000;   // 2M
    // samples_to_xfer_max = 20000000;  // 20M

    // ================================
    // 1) PARÁMETROS DE CAPTURA
    // ================================
    uint8_t  bands       = 0;                  // nº de bandas a usar
    transceiver_mode_t mode = TRANSCEIVER_MODE_RX; // modo RX
    uint16_t lna_gain    = 32;                 // Ganancia LNA (0–40 dB)
    uint16_t vga_gain    = 32;                 // Ganancia VGA (0–62 dB)
    uint16_t centralFrec = 200;                // Frecuencia central (MHz)
    bool     is_second_sample = false;         // false = primera captura

    // Asegurar que exista la carpeta
    mkdir("Samples", 0777);

    printf("▶ Capturando en %u MHz (LNA=%u, VGA=%u, N=%ld)...\n",
           centralFrec, lna_gain, vga_gain, samples_to_xfer_max);

    int r = getSamples(bands, mode, lna_gain, vga_gain,
                       centralFrec, is_second_sample);
    if (r != 0) {
        fprintf(stderr, "❌ getSamples devolvió %d\n", r);
        return 1;
    }

    printf("✅ Captura terminada. Archivo CS8 en Samples/0\n");

    // ================================
    // 2) CONVERTIR CS8 → IQ
    // ================================
    size_t N;
    complex double* x = cargar_cs8("Samples/0", &N);
    if (!x) {
        fprintf(stderr, "❌ Error cargando Samples/0\n");
        return 1;
    }
    printf("📥 %zu muestras cargadas desde Samples/0\n", N);

    // --- eliminar DC (quitar pico central) ---
    complex double mean = 0.0;
    for (size_t i = 0; i < N; i++) mean += x[i];
    mean /= (double)N;
    for (size_t i = 0; i < N; i++) x[i] -= mean;

    // ================================
    // 3) CÁLCULO DE PSD (Welch)
    // ================================
    // ⚠️ IMPORTANTE: fs debe coincidir con DEFAULT_SAMPLE_RATE_HZ en bacn_RF.c
    double fs = 20000000;             // Hz (ejemplo: 4 MHz)
    int segment_length = 4096;   // tamaño de ventana
    double overlap = 0.75;       // 75% de solapamiento

    // reservar arrays de salida
    double* f   = malloc(segment_length * sizeof(double));
    double* Pxx = malloc(segment_length * sizeof(double));
    if (!f || !Pxx) {
        fprintf(stderr, "❌ Error al reservar memoria para PSD\n");
        free(x);
        return 1;
    }

    // calcular PSD
    welch_psd_complex(x, N, fs, segment_length, overlap, f, Pxx);

    // --- centrar espectro (fftshift) ---
    fftshift_pair(f, Pxx, segment_length);

    // --- convertir a dB ---
    double *Pxx_dB = malloc(segment_length * sizeof(double));
    if (!Pxx_dB) {
        fprintf(stderr, "❌ Error al reservar memoria para Pxx_dB\n");
        free(x); free(f); free(Pxx);
        return 1;
    }
    const double eps = 1e-15;
    for (int i = 0; i < segment_length; i++)
        Pxx_dB[i] = 10.0 * log10(Pxx[i] + eps);

    // ================================
    // 4) GUARDAR RESULTADO A CSV
    // ================================
    mkdir("Outputs", 0777);
    save_to_file(f, Pxx, segment_length, "Outputs/resultado_psd.csv");
    save_to_file(f, Pxx_dB, segment_length, "Outputs/resultado_psd_db.csv");
    printf("💾 PSD guardada en resultado_psd.csv (lineal) y resultado_psd_db.csv (dB)\n");

    // ================================
    // 5) LIMPIEZA
    // ================================
    free(x);
    free(f);
    free(Pxx);
    free(Pxx_dB);

    return 0;
}