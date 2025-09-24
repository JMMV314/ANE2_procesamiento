/**
 * @file tdt.c
 * @brief Análisis de señales TDT y generación de parámetros clave.
 *
 * Este archivo implementa la función `parameter_tdt`, que analiza señales IQ para calcular
 * parámetros de transmisión digital terrestre (TDT) como potencia, relación portadora/ruido,
 * MER, BER y otros atributos del sistema de modulación.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <complex.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "moda.h"
#include "cJSON.h"
#include "IQ.h"
#include "tdt_functions.h"
#include "welch.h"
#include "cs8_to_iq.h"
#include <math.h>
#include <time.h>
#include <unistd.h>
#include "../Drivers/bacn_RTI.h"
#include "tdt.h"

/**
 * @brief Procesa señales IQ para calcular parámetros clave de transmisión digital terrestre.
 * 
 * Esta función carga datos de señal IQ, analiza los parámetros relevantes para una transmisión TDT
 * basada en la modulación especificada, y genera un archivo JSON con los resultados.
 * 
 * @param modulation Tipo de modulación (ej., QPSK, QAM16, etc.).
 * @param central_freq Frecuencia central de la señal (en Hz).
 * @param file_sample Identificador del archivo que contiene los datos de señal IQ en formato CS8.
 * 
 * @return Código de salida `EXIT_SUCCESS` si la operación se realiza con éxito.
 * 
 * @note Genera un archivo JSON llamado `parameters_tdt.json` con los resultados.
 */

extern bool program;

void parameter_tdt(st_server *s_server, int modulation, uint64_t central_freq, uint8_t file_sample,  char* channel) {
    size_t num_samples;
    int c_f = central_freq;

    const char* modulation_type;

    switch (modulation) {
        case 64:
            modulation_type = "64-QAM";
            break;
        case 16:
            modulation_type = "16-QAM";
            break;
    }

    int nperseg = 4096;
    size_t psd_size = nperseg;
    char Longitude[13];

    struct tm time1;
    struct tm time2;

    time_t startTime, stopTime;

    char file_sample_str[100];
    sprintf(file_sample_str, "Samples/%d", file_sample); 

     
    double* Pxx = NULL;
    double* f = NULL;
    Pxx = (double*) malloc(psd_size * sizeof(double));
    f = (double*) malloc(psd_size * sizeof(double));

    complex double* IQ_data = cargar_cs8(file_sample_str, &num_samples);

    printf("Total samples: %lu\r\n", num_samples);
    delete_CS8(file_sample);
    delete_JSON(file_sample);

    char timer0[17];
    time_t rawtime;
    struct tm * timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    int presence;
    int N_f=nperseg;
    strftime(timer0, sizeof(timer0), "%Y-%m-%dT%H:%M", timeinfo);

    
    
    double mer_value = 0.0, ber_value = 0.0, c_n_value = 0.0, signal_power_value;

    analyze_signal(central_freq, modulation, IQ_data, num_samples, &mer_value, &ber_value, &c_n_value, &signal_power_value);
    welch_psd_complex(IQ_data, num_samples, 6500000, nperseg, 0, f, Pxx);
    free(IQ_data);
       //real_time();
    if (nperseg % 2 != 0) {
        printf("La longitud del vector debe ser par.\n");
        return;
    }


    // -----------Acomodar potencias de welch-----------
    int half = nperseg / 2;
    double* temp = (double*)malloc(nperseg * sizeof(double));
    double* temp1 = (double*)malloc(4096 * sizeof(double));

    for (int i = 0; i < half; i++) {
        temp[i] = Pxx[half + i];
        
    }


    for (int i = 0; i < half; i++) {
        temp[half + i] = Pxx[i];
    }


    for (int i = 0; i < nperseg; i++) {
        Pxx[i] = temp[i];
    } 


    free(temp);
    free(temp1);

    // save_to_file(f, Pxx, nperseg, "data.csv");

    for (int i = 0; i < N_f; i++) {
        f[i] = (f[i] + central_freq) / 1e6;
    }

    int index = nperseg/2 ;
    int count = nperseg*0.002;
    int b=index-(count+5);
    int a=index;

    for (int i=0; i<count; i++)
    {   
        b=b-3;
        Pxx[a]=Pxx[b];
        a=a-1;
        
        
    }
    a=index;
    for (int i=0; i<count; i++)
    {   
        Pxx[a]=Pxx[b];
        a=a+1;
        b=b+2;
        
    }

    char FlowTdt[5];
    char FhighTdt[5];
    int Flow = (central_freq/1e6) - 3;
    int Fhigh = (central_freq/1e6) + 3;
    sprintf(FlowTdt, "%d", Flow);
    sprintf(FhighTdt, "%d", Fhigh);
        
// Crear el objeto JSON raíz
    cJSON *json_root = cJSON_CreateObject();

    // Agregar la fecha y hora al objeto JSON
    cJSON_AddStringToObject(json_root, "datetime", timer0);
    cJSON_AddStringToObject(json_root, "fmin", FlowTdt);
    cJSON_AddStringToObject(json_root, "fmax", FhighTdt);
    cJSON_AddStringToObject(json_root, "measure", "RMTDT");
    cJSON_AddStringToObject(json_root, "units", "MHz");
    cJSON_AddStringToObject(json_root, "channel", channel);
    cJSON_AddStringToObject(json_root, "band", "UHF");

    cJSON *json_vectors = cJSON_CreateObject();
    char tempu[50];
    // Agregar los vectores Pxx y f al objeto JSON
    cJSON *json_Pxx_array = cJSON_CreateArray();
    for (int i = 0; i < 4096; i++) {
        memset(tempu, 0, sizeof(tempu));
        sprintf(tempu, "%0.3f", 10*log10(Pxx[i]));
        cJSON_AddItemToArray(json_Pxx_array, cJSON_CreateNumber(atof(tempu)));
        //cJSON_AddItemToArray(json_Pxx_array, cJSON_CreateNumber(10*log10(constante*Pxx1[i])));
    }
    cJSON_AddItemToObject(json_vectors, "Pxx", json_Pxx_array);

    cJSON *json_f_array = cJSON_CreateArray();
    for (int i = 0; i < 4096; i++) {
        memset(tempu, 0, sizeof(tempu));
        sprintf(tempu, "%0.3f", f[i]);
        cJSON_AddItemToArray(json_f_array, cJSON_CreateNumber(atof(tempu)));
        //cJSON_AddItemToArray(json_f_array, cJSON_CreateNumber(f1[i]));
    }
    cJSON_AddItemToObject(json_vectors, "f", json_f_array);

    cJSON_AddItemToObject(json_root, "vectors", json_vectors);

    // Crear el arreglo de parámetros
    cJSON *json_params = cJSON_CreateObject();
    cJSON *json_params_array = cJSON_CreateArray();

    cJSON_AddNumberToObject(json_params, "freq", central_freq / 1e6);
    cJSON_AddNumberToObject(json_params, "power", 10 * log10(signal_power_value));
    cJSON_AddNumberToObject(json_params, "C/N", c_n_value);
    cJSON_AddNumberToObject(json_params, "MER", mer_value);
    cJSON_AddNumberToObject(json_params, "BER", ber_value);
    cJSON_AddStringToObject(json_params, "modulation", modulation_type);
    cJSON_AddStringToObject(json_params, "rate hp", "2/3");
    cJSON_AddStringToObject(json_params, "guard", "1/8");
    cJSON_AddNumberToObject(json_params, "segment length", 1024);
    cJSON_AddNumberToObject(json_params, "fs", 20000000);
    cJSON_AddStringToObject(json_params, "window", "Hamming");
    cJSON_AddNumberToObject(json_params, "bandwidth", 6500000);
    cJSON_AddNumberToObject(json_params, "overlap", 0);

    cJSON_AddItemToArray(json_params_array, json_params);

    //write(s_server->conf_fd, dataServer, strlen(dataServer));
    cJSON_AddItemToObject(json_root, "params", json_params_array);

    cJSON *json_data = cJSON_CreateObject();

    cJSON_AddItemToObject(json_data, "data", json_root);

    char *json_string = cJSON_Print(json_data);

    char filename[20];
    memset(filename, 0, 20);
    sprintf(filename, "JSON/%d", file_sample);

    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        printf("Error al abrir el archivo para escribir.\n");
        cJSON_Delete(json_root);
        free(json_string);
        free(f);
        free(Pxx);
        free(IQ_data);
        return;
    }
    fprintf(file, "%s", json_string);
    fclose(file);
#include "welch.h"
    char dataServer[10];
    memset(dataServer, 0, sizeof(dataServer));
    if(program) {
        sprintf(dataServer, "{data:{}}");
    } else {
        sprintf(dataServer, "{dataStreaming:{}}");
    }
    write(s_server->conf_fd, dataServer, strlen(dataServer));

    
    cJSON_Delete(json_root);
    free(json_string);
    free(f);
    free(Pxx);
}