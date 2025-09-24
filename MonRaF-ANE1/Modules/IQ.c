/**
 * @file IQ.c
 * @brief Implementación de funciones relacionadas con la lectura de archivos CS8, manejo de bandas y creación de vectores IQ.
 * 
 * Este archivo contiene la implementación de funciones utilizadas para trabajar con archivos CS8, cargar bandas de frecuencias y construir vectores de datos complejos (IQ).
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <complex.h>

#include "IQ.h"

int8_t* read_CS8(uint8_t file_sample, size_t* file_size)
{
    char file_path[4];
    memset(file_path, 0, 4);
    sprintf(file_path, "%d", file_sample);
    
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        printf("Error: Unable to open file %s\n", file_path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    *file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (*file_size == 0) {
        printf("Error: File is empty.\n");
        fclose(file);
        return NULL;
    }

    int8_t *raw_data = (int8_t *)malloc(*file_size);
    if (!raw_data) {
        printf("Error: Unable to allocate memory for raw data\n");
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(raw_data, sizeof(int8_t), *file_size, file);
    if (read_size != *file_size) {
        printf("Error: Incomplete read. Expected %zu bytes but got %zu\n", *file_size, read_size);
        free(raw_data);
        fclose(file);
        return NULL;
    }

    fclose(file);
    return raw_data;
}


void delete_CS8(uint8_t file_sample)
{
    char file_path[20];
    memset(file_path, 0, 20);
    sprintf(file_path, "Samples/%d", file_sample);

    if (remove(file_path) != 0) {
        fprintf(stderr, "Error al eliminar el archivo Sample %s.\n", file_path);
    } else {
        printf("Archivo Sample %s eliminado correctamente.\n", file_path);
    }
}

void delete_JSON(uint8_t file_json)
{
    char file_path[20];
    memset(file_path, 0, 20);
    sprintf(file_path, "JSON/%d", file_json);

    if (remove(file_path) != 0) {
        fprintf(stderr, "Error al eliminar el archivo JSON %s.\n", file_path);
    } else {
        printf("Archivo JSON %s eliminado correctamente.\n", file_path);
    }
}

int load_bands(uint8_t bands, double* frequencies, double* bandwidths)
{
    char temp_buffer[MAX_BAND_SIZE];
    char *token;
    int num_rows = 0;
    int i = 0;
    const char *file_band = NULL;

    switch (bands) {
        case VHF1: file_band = "bands/VHF1.csv"; break;
        case VHF2: file_band = "bands/VHF2.csv"; break;
        case VHF3: file_band = "bands/VHF3.csv"; break;
        case VHF4: file_band = "bands/VHF4.csv"; break;
        case UHF1: file_band = "bands/UHF1.csv"; break;
        case UHF1_2: file_band = "bands/UHF1_2.csv"; break;
        case UHF1_3: file_band = "bands/UHF1_3.csv"; break;
        case UHF1_4: file_band = "bands/UHF1_4.csv"; break;
        case UHF2_1: file_band = "bands/UHF2_1.csv"; break;
        case UHF2_2: file_band = "bands/UHF2_2.csv"; break;
        case UHF2_3: file_band = "bands/UHF2_3.csv"; break;
        case UHF2_4: file_band = "bands/UHF2_4.csv"; break;
        case UHF2_5: file_band = "bands/UHF2_5.csv"; break;
        case UHF2_6: file_band = "bands/UHF2_6.csv"; break;
        case UHF2_7: file_band = "bands/UHF2_7.csv"; break;
        case UHF2_8: file_band = "bands/UHF2_8.csv"; break;
        case UHF2_9: file_band = "bands/UHF2_9.csv"; break;
        case UHF2_10: file_band = "bands/UHF2_10.csv"; break;
        case UHF2_11: file_band = "bands/UHF2_11.csv"; break;
        case UHF2_12: file_band = "bands/UHF2_12.csv"; break;
        case UHF2_13: file_band = "bands/UHF2_13.csv"; break;
        case UHF3: file_band = "bands/UHF3.csv"; break;
        case UHF3_1: file_band = "bands/UHF3_1.csv"; break;
        case UHF3_2: file_band = "bands/UHF3_2.csv"; break;
        case UHF3_3: file_band = "bands/UHF3_3.csv"; break;
        case UHF3_4: file_band = "bands/UHF3_4.csv"; break;
        case UHF3_5: file_band = "bands/UHF3_5.csv"; break;
        case SHF1: file_band = "bands/SHF1.csv"; break;
        case SHF2: file_band = "bands/SHF2.csv"; break;
        case SHF2_2: file_band = "bands/SHF2_2.csv"; break;
        case SHF2_3: file_band = "bands/SHF2_3.csv"; break;
        case SHF2_4: file_band = "bands/SHF2_4.csv"; break;
        case SHF2_5: file_band = "bands/SHF2_5.csv"; break;
        case SHF2_6: file_band = "bands/SHF2_6.csv"; break;
        case SHF2_7: file_band = "bands/SHF2.csv"; break;
        
        default: return 0;
    }
    
    FILE *file = fopen(file_band, "r");
    if (!file) {
        printf("Error: Unable to open file %s\n", file_band);
        return 0;
    }

    while (fgets(temp_buffer, MAX_BAND_SIZE, file) != NULL) {
        num_rows++;
    }

    fseek(file, 0, SEEK_SET);
    fgets(temp_buffer, MAX_BAND_SIZE, file);

    while (fgets(temp_buffer, MAX_BAND_SIZE, file) != NULL) {
        token = strtok(temp_buffer, ",");
        frequencies[i] = atof(token);
        token = strtok(NULL, "\n");
        bandwidths[i] = atof(token);
        i++;
    }

    fclose(file);
    return num_rows;
}

uint16_t load_bands_tdt(char* channel, char* city, int *modulation)
{
    char temp_buffer[MAX_BAND_SIZE];
    char *token;
    int num_rows = 0;
    int i = 0;    
    char file_band[20];
    char canal[10];
    char frequencia[10];
    char modulacion[10];
    sprintf(file_band, "Ciudades/%s.csv", city); //Aca se debe ajustar

    FILE *file = fopen(file_band, "r");
    if (!file) {
        printf("Error: Unable to open file %s\n", file_band);
        return 0;
    }

    while (fgets(temp_buffer, MAX_BAND_SIZE, file) != NULL) {
        num_rows++;
    }

    fseek(file, 0, SEEK_SET);
    fgets(temp_buffer, MAX_BAND_SIZE, file);

    while (fgets(temp_buffer, MAX_BAND_SIZE, file) != NULL) {
        token = strtok(temp_buffer, ",");
        sprintf(canal, "%s", token);
        token = strtok(NULL, ",");
        sprintf(frequencia, "%s", token);
        token = strtok(NULL, "\n");
        sprintf(modulacion, "%s", token);

        if(!strcmp(canal, channel)) {
            break;
        }
    }

    *modulation = atoi(modulacion);

    fclose(file);
    return atoi(frequencia);
}

complex double* Vector_BIN(int8_t *rawVector, size_t length, size_t* num_samples)
{
    *num_samples = length / 2;

    complex double* complex_samples = malloc(*num_samples * sizeof(complex double));
    if (!complex_samples) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < *num_samples; i++) {
        complex_samples[i] = CMPLX(rawVector[2 * i], rawVector[2 * i + 1]);
    }

    return complex_samples;
}