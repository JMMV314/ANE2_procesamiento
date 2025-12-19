/**
 * @file main.c
 * @brief Versión con DEBUGGING y CORRECCIÓN DE DC SPIKE integrada.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <math.h> // Necesario para la corrección si se agregan logs, o validaciones
#include "libs/psd.h"
#include "libs/datatypes.h"

// --- CONFIGURACIÓN ---
#define SAMPLE_RATE     20000000.0  // 20 MHz
#define CENTER_FREQ     107715000.0 // 148 MHz
#define N_FFT           4096/(1)
#define N_OVERLAP       N_FFT*(50/100)         
#define WINDOW_TYPE     HANN_TYPE
#define TARGET_UNIT     "dBm"       
#define DC_SPIKE_WIDTH  0       // Ancho del pico a corregir (en bins)

/**
 * Corrige el pico DC en una PSD mediante extrapolación lineal inversa.
 * Debe llamarse antes de convertir a dB.
 * Asume que el componente DC está en el índice 0 (salida estándar FFT).
 */
/**
 * Corrige el pico DC cuando este se encuentra en el centro del espectro (FFT Shifted).
 * Realiza una interpolación lineal entre los puntos válidos a la izquierda y derecha del pico.
 *
 * @param psd Array de PSD (Potencia lineal).
 * @param length Longitud del array (debe ser par, ej: 4096).
 * @param half_width Cuántos bins a cada lado del centro están corruptos. 
 * Ej: Si half_width=2, se corrigen: centro, centro-1, centro-2, centro+1, centro+2.
 */
void correct_centered_dc_spike(double* psd, size_t length, int half_width) {
    if (psd == NULL || length == 0 || half_width < 1) return;

    size_t center = length / 2;
    
    // Índices de los puntos "limpios" (anclas)
    // Aseguramos que no se salgan de los límites del array
    if (center <= (size_t)half_width || center + half_width >= length - 1) return;

    size_t left_idx = center - half_width - 1;
    size_t right_idx = center + half_width + 1;

    double y_left = psd[left_idx];
    double y_right = psd[right_idx];

    // Calculamos la pendiente de la línea que une ambos puntos
    // Distancia total en el eje X entre las anclas
    double run = (double)(right_idx - left_idx);
    double rise = y_right - y_left;
    double slope = rise / run;

    // Rellenamos el hueco (interpolamos)
    for (size_t i = left_idx + 1; i < right_idx; i++) {
        double dist_from_left = (double)(i - left_idx);
        psd[i] = y_left + (slope * dist_from_left);
    }
    
    printf("[DEBUG] DC Spike corregido en el centro (idx %zu, radio %d bins).\n", center, half_width);
}

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

    // VALIDACIÓN IMPORTANTE
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
    execute_welch_psd(signal, &config, freq_out, psd_out);
    printf("[DEBUG] execute_welch_psd finalizó correctamente.\n");

    // --- NUEVO: CORRECCIÓN DE DC SPIKE ---
    // Se aplica AQUÍ porque psd_out aún está en potencia lineal (antes de scale_psd).
    // Asumimos que la librería entrega DC en el índice 0 (estándar FFT).
    printf("[DEBUG] Aplicando corrección de DC Spike (interpolación en frecuencia)...\n");
    correct_centered_dc_spike(psd_out, N_FFT, DC_SPIKE_WIDTH);
    // -------------------------------------

    // 6. Escalar (Convertir a dBm, etc)
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
        fflush(csv); 
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