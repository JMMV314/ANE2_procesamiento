
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#include "Modules/bacn_RF.h"
#include "Modules/IQ.h"
#include "Modules/parameters.h"
#include "Modules/tdt.h"
#include "Modules/parameters_rni.h"
#include "Modules/welch.h"
#include "Drivers/bacn_gpio.h"
#include "Drivers/bacn_LTE.h"
#include "Drivers/bacn_RTI.h"


st_uart USART0;
st_server SERVER0;

bool uart_open = false;
bool client_open = false;
bool GPSData = false;

char Latitude[13];
char Longitude[13];

time_t startTime, stopTime, programTime;

uint8_t bands = 0;
char banda[13];
char Flow[13];
char Fhigh[13];
char Tcity[13];
char Tchan[13];
char t_start[20];
char t_stop[20];
uint8_t net;
uint8_t getData = 0;
int64_t central_freq[3]; //No se le asigna nada

bool rfhack = false;
bool program = false;
//uint8_t busy = 0;
uint8_t times_up = 1;
uint8_t times_sample = 0;

static transceiver_mode_t transceiver_mode = TRANSCEIVER_MODE_RX;

double canalisation[2000];
double bandwidth[2000];
uint8_t qam[100];

int main(void)
{
	time_t t;   
    int bands_length; 
    int totalSamples;

	// Convert to local time and store in struct tm
    struct tm *currentTime;

    system("clear");
    system("sudo poff rnet");
    system("sudo systemctl stop monraf-client"); 
    
    /*
    
	// Check if module LTE is ON
	if(status_LTE()) {               //#----------Descomentar desde aqui-------------#
		printf("LTE module is ON\r\n");
	} else {
    	power_ON_LTE();
	}

	if(init_usart(&USART0) != 0)
    {
        printf("Error : uart open failed\r\n");
        return -1;
    }

    printf("LTE module ready\r\n");

    while(!LTE_Start(&USART0));
    printf("LTE response OK\n");

    GPS_ON(&USART0);
    printf("Wait for a valid location\r\n");
    while(!GPSData)
    {
        if(start_GPS_location(&USART0, false))
        {
            printf("GPS location done: %s, %s\r\n", Latitude, Longitude);
            GPSData = true;
        } else {            
            sleep(1);
        }
        
    }

    GPS_OFF(&USART0);
    close_usart(&USART0);
    printf("GPS power OFF\r\n");

    while(!uart_open);
    uart_open = false;

    printf("Turn on mobile data\r\n");
    system("sudo pon rnet");                     //#----------Descomentar hasta aqui-------------#
    sleep(5);
    */
   
    if(init_server(&SERVER0) != 0)
    {
        printf("Error : server open failed\r\n");
        return -1;
    }

    printf("RTI module ready\r\n");
    
    memset(Latitude, 0, sizeof(Latitude));
    sprintf(Latitude, "%s", "5.053265");
    memset(Longitude, 0, sizeof(Longitude));
    sprintf(Longitude, "%s", "-75.510462");

    char path_zero_sample[256];
    char path_one_sample[256];
    
    
       
    t = time(NULL);
    currentTime = localtime(&t);
    printf("Time start: %02d:%02d\n", currentTime->tm_min, currentTime->tm_sec);
    times_sample = currentTime->tm_min + times_up;

	while(1)
    {
        if(client_open) {
            if(rfhack) {
                switch(getData) {
                    case 0:
                        sendLocation(&SERVER0, Latitude, Longitude);
                        getData = 9;
                    break;
                    case 1:
                        //printf("Bands: %d\r\n", bands);
                        bands_length = load_bands(bands, canalisation, bandwidth);
                        printf("Bands length: %d\r\n", bands_length);

                        totalSamples = getSamples(bands, TRANSCEIVER_MODE_RX, 0, 0, 0, true);
                        printf("Total files: %d\r\n", totalSamples);
                        
                        snprintf(path_zero_sample, sizeof(path_zero_sample), "%s0", "Samples/");
                        snprintf(path_one_sample, sizeof(path_one_sample), "%s1", "Samples/");
                        rename(path_zero_sample, path_one_sample);

                        totalSamples = getSamples(bands, TRANSCEIVER_MODE_RX, 0, 0, 0, false);
                        printf("Total files: %d\r\n", totalSamples);                            

                        parameter(&SERVER0, -30, canalisation, bandwidth, bands_length, central_freq[0], 0, banda, Flow, Fhigh);
                    break;
                    case 2:
                        printf("Channel: %s\r\n", Tchan);
                        int Tmodu = 0;
                        uint16_t centralFrec = load_bands_tdt(Tchan, Tcity, &Tmodu);
                        printf("central frequency: %lu, Channel: %s, modulation: %d\r\n", centralFrec, Tchan, Tmodu);

                        totalSamples = getSamples(bands, TRANSCEIVER_MODE_RX, 0, 0, 0, true);
                        printf("Total files: %d\r\n", totalSamples);
                        
                        snprintf(path_zero_sample, sizeof(path_zero_sample), "%s0", "Samples/");
                        snprintf(path_one_sample, sizeof(path_one_sample), "%s1", "Samples/");
                        rename(path_zero_sample, path_one_sample);

                        totalSamples = getSamples(bands, TRANSCEIVER_MODE_RX, 0, 0, 0, false);
                        printf("Total files: %d\r\n", totalSamples);

                        uint64_t c_f=centralFrec*1000000;
                        printf("frecuencia central %d", c_f);
                        parameter_tdt(&SERVER0, Tmodu, c_f, 0, Tchan);
                    break;
                    case 3:
                        printf("Bands: %d\r\n", bands);
                        bands_length = load_bands(bands, canalisation, bandwidth);
                        printf("Bands length: %d\r\n", bands_length);

                        totalSamples = getSamples(bands, TRANSCEIVER_MODE_RX, 0, 0, 0, true);
                        printf("Total files: %d\r\n", totalSamples);
                        
                        snprintf(path_zero_sample, sizeof(path_zero_sample), "%s0", "Samples/");
                        snprintf(path_one_sample, sizeof(path_one_sample), "%s1", "Samples/");
                        rename(path_zero_sample, path_one_sample);

                        totalSamples = getSamples(bands, TRANSCEIVER_MODE_RX, 0, 0, 0, false);
                        printf("Total files: %d\r\n", totalSamples);
                        
                        parameter_rni(&SERVER0, 0.0005, canalisation, bandwidth, bands_length, central_freq[0], 0, banda, Flow, Fhigh);
                    break;
                    case 9:

                    break;
                    case 10:
                        printf("MonRaF Stoped\r\n");
                        getData = 9;
                    break;
                }              

                t = time(NULL);
                currentTime = localtime(&t);
                printf("Time inside: %02d:%02d\n", currentTime->tm_hour, currentTime->tm_min);
                if(currentTime->tm_min == 59) {
                    times_sample = 00;
                } else {
                    times_sample = currentTime->tm_min + times_up;
                }                
                rfhack = false;
            } else {
                t = time(NULL);
                currentTime = localtime(&t);

                if(program) {
                    programTime = mktime(currentTime);
                    if(programTime >= startTime && programTime <= stopTime) {
                        printf("Time Program : %02d:%02d\n", currentTime->tm_hour, currentTime->tm_min);
                        if(times_sample == currentTime->tm_min) {
                            rfhack = true;
                        }
                        //getData = 8;
                    } else if(programTime >= stopTime) {
                        printf("Time Program stop : %02d:%02d\n", currentTime->tm_hour, currentTime->tm_min);
                        program = false;
                        getData = 10;
                    }else {
                        printf("Time program wait : %02d:%02d\n", currentTime->tm_hour, currentTime->tm_min);
                        if(currentTime->tm_min == 59) {
                            times_sample = 00;
                        } else {
                            times_sample = currentTime->tm_min + times_up;
                        }
                    }

                } else {
                    printf("Time wait : %02d:%02d\n", currentTime->tm_hour, currentTime->tm_min);
                    if(times_sample == currentTime->tm_min) {
                        rfhack = true;
                    }
                }               

                sleep(10);
            }
        } 
    }

    return EXIT_SUCCESS;
}
