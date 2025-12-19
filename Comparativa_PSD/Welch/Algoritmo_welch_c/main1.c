/**
 * @file main.c
 * @brief Versión con DEBUGGING para detectar por qué falla el guardado.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h> // Para ver códigos de error
#include "libs/psd.h"
#include "libs/datatypes.h"

// --- CONFIGURACIÓN ---
#define SAMPLE_RATE     20000000.0  // 20 MHz
#define CENTER_FREQ     148000000.0 // 148 MHz
#define N_FFT           4096/(2)
#define N_OVERLAP       N_FFT*(50/100)         
#define WINDOW_TYPE     HANN_TYPE
#define TARGET_UNIT     "dBm"      

int8_t* read_file_content(const char* filename, size_t* size_out) {
    printf("[DEBUG] Abriendo archivo: %s\n", filename);
    FILE* f = fopen(filename, "rb");
    if (!f) {
        perror("[ERROR] No se pudo abrir el archivo de entrada");
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        printf("[ERROR] El archivo está vacío o tiene tamaño inválido: %ld bytes\n", size);
        fclose(f);
        return NULL;
    }

    printf("[DEBUG] Tamaño del archivo: %ld bytes\n", size);

    int8_t* buffer = (int8_t*)malloc(size);
    if (!buffer) {
        printf("[ERROR] Falló malloc para el buffer de entrada.\n");
        fclose(f);
        return NULL;
    }

    size_t read_bytes = fread(buffer, 1, size, f);
    *size_out = read_bytes;
    fclose(f);
    
    printf("[DEBUG] Lectura de archivo completada.\n");
    return buffer;
}

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0); 
    setvbuf(stderr, NULL, _IONBF, 0);

    if (argc < 3) {
        printf("Uso: %s <input_file.cs8> <output_file.csv>\n", argv[0]);
        return 1;
    }

    const char* input_file = argv[1];
    const char* output_file = argv[2];

    printf("--- INICIO DEBUG ---\n");

    // 1. Cargar archivo
    size_t file_size = 0;
    int8_t* raw_buffer = read_file_content(input_file, &file_size);
    if (!raw_buffer) return 1;

    // VALIDACIÓN IMPORTANTE: ¿Hay suficientes datos para el Welch?
    // Necesitamos al menos N_FFT muestras complejas (N_FFT * 2 bytes en int8)
    if (file_size < (N_FFT * 2)) {
        printf("[ERROR] El archivo es muy pequeño (%zu bytes) para un N_FFT de %d.\n", file_size, N_FFT);
        printf("        Se necesitan al menos %d bytes.\n", N_FFT * 2);
        free(raw_buffer);
        return 1;
    }

    // 2. Convertir a IQ
    printf("[DEBUG] Convirtiendo a IQ...\n");
    signal_iq_t* signal = load_iq_from_buffer(raw_buffer, file_size);
    free(raw_buffer);

    if (!signal || !signal->signal_iq) {
        printf("[ERROR] Falló load_iq_from_buffer (retornó NULL).\n");
        return 1;
    }
    printf("[DEBUG] Señal IQ creada. Muestras: %zu\n", signal->n_signal);

    // 3. Configuración
    PsdConfig_t config;
    config.sample_rate = SAMPLE_RATE;
    config.nperseg = N_FFT;
    config.noverlap = N_OVERLAP;
    config.window_type = WINDOW_TYPE;

    // 4. Malloc salida
    printf("[DEBUG] Reservando memoria para salida...\n");
    double* freq_out = (double*)malloc(N_FFT * sizeof(double));
    double* psd_out  = (double*)malloc(N_FFT * sizeof(double));

    if (!freq_out || !psd_out) {
        printf("[ERROR] Falló malloc para arrays de salida.\n");
        free_signal_iq(signal);
        return 1;
    }

    // 5. Ejecutar Welch
    printf("[DEBUG] Ejecutando execute_welch_psd...\n");
    // Verificar si fftw falla internamente (segfault común si no se linkea bien)
    execute_welch_psd(signal, &config, freq_out, psd_out);
    printf("[DEBUG] execute_welch_psd finalizó correctamente.\n");

    // 6. Escalar
    printf("[DEBUG] Escalando datos...\n");
    scale_psd(psd_out, N_FFT, TARGET_UNIT);

    // 7. Guardar CSV
    printf("[DEBUG] Intentando crear archivo CSV: %s\n", output_file);
    FILE* csv = fopen(output_file, "w");
    if (!csv) {
        perror("[ERROR] No se pudo crear el archivo CSV");
        printf("        Verifica que la carpeta exista y tengas permisos.\n");
    } else {
        fprintf(csv, "Frequency_Hz,Power_%s\n", TARGET_UNIT);
        for (int i = 0; i < N_FFT; i++) {
            double rf_freq = freq_out[i] + CENTER_FREQ;
            if (fprintf(csv, "%.2f,%.4f\n", rf_freq, psd_out[i]) < 0) {
                 perror("[ERROR] Fallo escribiendo linea en CSV");
                 break;
            }
        }
        fflush(csv); // Forzar escritura en disco
        fclose(csv);
        printf("[EXITO] Archivo CSV guardado: %s\n", output_file);
    }

    // Limpieza
    free(freq_out);
    free(psd_out);
    free_signal_iq(signal);
    
    printf("--- FIN ---\n");
    return 0;
}