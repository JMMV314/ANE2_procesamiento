/**
 * @file parameters_tdt_rni.c
 * @brief Función para analizar señales RNI y generar un archivo JSON con resultados.
 *
 * Esta función calcula métricas relacionadas con la radiación electromagnética, 
 * como la intensidad de campo eléctrico (V/m) y el porcentaje del límite ocupado, 
 * guardándolos en un archivo JSON.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <complex.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "cJSON.h"
#include "tdt_functions.h"


void parameter_tdt_rni(int modulation, uint64_t central_freq, uint8_t file_sample) {
    size_t num_samples;

    char cs8_path[100];

    snprintf(cs8_path, sizeof(cs8_path), "%d", file_sample);

    complex double* IQ_data = cargar_cs8(cs8_path, &num_samples);

    printf("Total samples: %lu\r\n", num_samples);

    char timer0[9];
    time_t rawtime;
    struct tm * timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timer0, sizeof(timer0), "%X", timeinfo);
    
    double mer_value = 0.0, ber_value = 0.0, c_n_value = 0.0, signal_power_value;

    analyze_signal(central_freq, modulation, IQ_data, num_samples, &mer_value, &ber_value, &c_n_value, &signal_power_value);

    double v_m = sqrt((signal_power_value/1000)*377);

    double v_max=28;

    cJSON *json_array = cJSON_CreateArray();
    cJSON *json_item = cJSON_CreateObject();

    cJSON_AddStringToObject(json_item, "time", timer0);
    cJSON_AddNumberToObject(json_item, "freq", central_freq); // Quitar
    cJSON_AddNumberToObject(json_item, "V/m", v_m);
    cJSON_AddNumberToObject(json_item, "limite ocupado", v_m/v_max*100);

    cJSON_AddItemToArray(json_array, json_item);

    char *json_string = cJSON_Print(json_array);

    char filename[20];
    snprintf(filename, sizeof(filename), "parameters_tdt.json");

    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        printf("Error al abrir el archivo para escribir.\n");
        cJSON_Delete(json_array);
        free(json_string);
        return;
    }
    fprintf(file, "%s", json_string);
    fclose(file);
    
    cJSON_Delete(json_array);
    free(json_string);
    free(IQ_data);
    return EXIT_SUCCESS;
}