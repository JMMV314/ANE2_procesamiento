#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>
#include <sys/select.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "bacn_LTE.h"

uint32_t TimeOut = 0;
int8_t Response_Status, CRLF_COUNT = 0, Data_Count;
volatile uint8_t OBDCount = 0, GPSCount = 0;

char RESPONSE_BUFFER[UART_BUFFER_SIZE];

bool uart_run = false;
bool GPSRDY = false;
extern bool uart_open;
extern char Latitude[13];
extern char Longitude[13];

void Read_Response(void)
{
    char CRLF_BUF[2];
    char CRLF_FOUND;
    uint32_t TimeCount = 0, ResponseBufferLength;

    while(1)
    {
        if(TimeCount >= (DEFAULT_TIMEOUT+TimeOut))
        {
            CRLF_COUNT = 0; TimeOut = 0;
            Response_Status = LTE_RESPONSE_TIMEOUT;
            return;
        }

        if(Response_Status == LTE_RESPONSE_STARTING)
        {
            CRLF_FOUND = 0;
            memset(CRLF_BUF, 0, 2);
            Response_Status = LTE_RESPONSE_WAITING;
        }
        ResponseBufferLength = strlen(RESPONSE_BUFFER);
        if (ResponseBufferLength)
        {
            usleep(1000);

            TimeCount++;
            if (ResponseBufferLength==strlen(RESPONSE_BUFFER))
            {
                uint16_t i;
                for (i=0; i<ResponseBufferLength; i++)
                {
                    memmove(CRLF_BUF, CRLF_BUF + 1, 1);
                    CRLF_BUF[1] = RESPONSE_BUFFER[i];
                    if(!strncmp(CRLF_BUF, "\r\n", 2))
                    {
                        if(++CRLF_FOUND == (DEFAULT_CRLF_COUNT+CRLF_COUNT))
                        {
                            CRLF_COUNT = 0; TimeOut = 0;
                            Response_Status = LTE_RESPONSE_FINISHED;
                            return;
                        }
                    }
                }
                CRLF_FOUND = 0;
            }
        }
        usleep(1000);
        TimeCount++;
    }
}

void Start_Read_Response(void)
{
    Response_Status = LTE_RESPONSE_STARTING;
    do {
        Read_Response();
    } while(Response_Status == LTE_RESPONSE_WAITING);

}

bool WaitForExpectedResponse(const char* ExpectedResponse)
{   
    while(!GPSRDY);
    GPSRDY = false;
    Start_Read_Response();                      /* First read response */

    if((Response_Status != LTE_RESPONSE_TIMEOUT) && (strstr(RESPONSE_BUFFER, ExpectedResponse) != NULL))
        return true;                            /* Return true for success */

    return false;                               /* Else return false */
}

bool SendATandExpectResponse(st_uart *s_uart, const char* ATCommand, const char* ExpectedResponse)
{
    LTE_SendString(s_uart, ATCommand);            /* Send AT command to LTE */
    return WaitForExpectedResponse(ExpectedResponse);
}

void LTE_SendString(st_uart *s_uart, const char *data)
{
    char dataLTE[20];
    
    memset(dataLTE, 0, sizeof(dataLTE));
    sprintf(dataLTE, "<%s>", data);
    write(s_uart->serial_fd, dataLTE, strlen(dataLTE));   
}

bool LTE_Start(st_uart *s_uart)
{
    uint8_t i;

    for (i=0; i<5; i++)
    {
        if(SendATandExpectResponse(s_uart, "ATE0\r","OK"))
            return true;
    }

    return false;
}

bool GPS_ON(st_uart *s_uart)
{
    //LTE_SendString(s_uart, "AT+CGNSPWR=1\r");
    //return WaitForExpectedResponse("OK");
    LTE_SendString(s_uart, "AT+SGNSCMD=0\r");
    WaitForExpectedResponse("OK");
    LTE_SendString(s_uart, "AT+SGNSCFG=\"THRESHOLD\",10\r");
    WaitForExpectedResponse("OK");
    LTE_SendString(s_uart, "AT+SGNSCFG=\"OUTURC\",1\r");
    WaitForExpectedResponse("OK");
    LTE_SendString(s_uart, "AT+SGNSCFG=\"EXTRAINFO\",1\r");
    WaitForExpectedResponse("OK");
    LTE_SendString(s_uart, "AT+SGNSCMD=2,10000,0,3\r");
    return WaitForExpectedResponse("OK");
    
}

bool GPS_OFF(st_uart *s_uart)
{
    //LTE_SendString(s_uart, "AT+CGNSPWR=0\r");
    //return WaitForExpectedResponse("OK");
    LTE_SendString(s_uart, "AT+SGNSCMD=0\r");
    return WaitForExpectedResponse("OK");
}



bool start_GPS_location(st_uart *s_uart, bool mode)
{
    GPSCommand GPSData;
    bool ready;
    bool fixData = false;
    uint8_t Satelite;
    float Accuaracy;
    
    if(mode) 
    {
        const char g[6] = ":,\r\n";
        LTE_SendString(s_uart, "AT+CGNSINF\r");
        ready = WaitForExpectedResponse("+CGNSINF:");

        //char RESPONSE_BUFFER_Inpt[100]= "+CGNSINF:1,1,20191024051848.000,31.221946,121.355565,3.417,0.00,,0,,1.4,1.7,0.9,,6,,12.4,12.0";
        if(ready) {
            printf("%s\n", RESPONSE_BUFFER);            
            char RESPONSE_BUFFER_IN[2*strlen(RESPONSE_BUFFER)+1];

            int i, j = 0;
            for(i = 0; i < strlen(RESPONSE_BUFFER); i++) {
                RESPONSE_BUFFER_IN[j++] = RESPONSE_BUFFER[i];
                if(RESPONSE_BUFFER[i] == ',' && RESPONSE_BUFFER[i+1]== ',')
                    RESPONSE_BUFFER_IN[j++] = ' ';    
            }

            RESPONSE_BUFFER_IN[j] = '\0';

            char *token = strtok(RESPONSE_BUFFER_IN, g);
            char *proto = "+CGNSINF";
            char *valid = "1";
            
            if (token != NULL) {
                GPSData.Message_ID = token;
            }

            if(strcmp(GPSData.Message_ID, proto)) {
                return false;
            }

            token = strtok(NULL, g);
            if (token != NULL) {
                GPSData.Status = token;
            }
            token = strtok(NULL, g);
            if (token != NULL) {
                GPSData.FixStatus = token;
            }
            token = strtok(NULL, g);
            if (token != NULL) {
                GPSData.UTC_Time = token;                
            }
            token = strtok(NULL, g);
            if (token != NULL) {
                GPSData.Latitude = token;                
            }
            token = strtok(NULL, g);
            if (token != NULL) {
                GPSData.Longitude = token;               
            }
            token = strtok(NULL, g);
            if (token != NULL) {
                GPSData.Altitude = token;                
            }
            token = strtok(NULL, g);
            if (token != NULL) {
                GPSData.Speed = token;                
            }
            token = strtok(NULL, g);
            if (token != NULL) {
                GPSData.Course = token;                
            }
            token = strtok(NULL, g);
            if (token != NULL) {
                GPSData.FixMode = token;                
            }
            token = strtok(NULL, g);
            if (token != NULL) {
                GPSData.HDOP = token;               
            }   
            token = strtok(NULL, g);
            if (token != NULL) {
                GPSData.PDOP = token;               
            }   
            token = strtok(NULL, g);
            if (token != NULL) {
                GPSData.VDOP = token;                
            }
            token = strtok(NULL, g);
            if (token != NULL) {
                GPSData.Satelites = token;
                Satelite = atoi(GPSData.Satelites);                
            }
            token = strtok(NULL, g);
            if (token != NULL) {
                GPSData.HPA = token;                
            } 
            token = strtok(NULL, g);
            if (token != NULL) {
                GPSData.VPA = token;                
            }        

            if(Satelite < 6) {
                memset(Latitude, 0, sizeof(Latitude));
                memset(Longitude, 0, sizeof(Longitude));
                return false;
                
            } else {                
                sprintf(Latitude, "%s", GPSData.Latitude);
                sprintf(Longitude, "%s", GPSData.Longitude);
                return true;
            }
        } else {
            return false;
        }
    } else {
        const char s[6] = " ,\r\n";
        //LTE_SendString(s_uart, "AT+SGNSCMD=1,0\r");
        //WaitForExpectedResponse("OK");
        ready = WaitForExpectedResponse("+SGNSCMD:");

        if(ready) {
            printf("%s\n", RESPONSE_BUFFER);
            
            char *token = strtok(RESPONSE_BUFFER, s);
            char *proto = "+SGNSCMD:";
            
            if (token != NULL) {
                GPSData.Message_ID = token;
            }

            if(strcmp(GPSData.Message_ID, proto)) {
                return false;
            }

            token = strtok(NULL, s);
            if (token != NULL) {
                GPSData.Status = token;
            }
            token = strtok(NULL, s);
            if (token != NULL) {
                GPSData.UTC_Date = token;
            }
            token = strtok(NULL, s);
            if (token != NULL) {
                GPSData.UTC_Time = token;
            }
            token = strtok(NULL, s);
            if (token != NULL) {
                GPSData.Satelites = token;
                Satelite = atoi(GPSData.Satelites); 
            }                
            token = strtok(NULL, s);
            if (token != NULL) {
                GPSData.Latitude = token;
            }
            token = strtok(NULL, s);
            if (token != NULL) {
                GPSData.Longitude = token;
            }
            token = strtok(NULL, s);
            if (token != NULL) {
                GPSData.Accuaracy = token;
                Accuaracy = atof(GPSData.Accuaracy); 
            }

            if(Satelite < 4) {
                memset(Latitude, 0, sizeof(Latitude));
                memset(Longitude, 0, sizeof(Longitude));
                return false;
                
            } else {                
                sprintf(Latitude, "%s", GPSData.Latitude);
                sprintf(Longitude, "%s", GPSData.Longitude);
                return true;
            }           
        } else {
            return false;
        }
    }
}

int8_t init_usart(st_uart *s_uart)
{    
    struct termios tty;

    s_uart->serial_fd = -1;
    s_uart->serial_fd = open(SERIAL_DEV, O_RDWR | O_NOCTTY | O_NDELAY);
    if (s_uart->serial_fd == -1)
    {
        printf ("Error : open serial device: %s\r\n",SERIAL_DEV);
        perror("OPEN"); 
        return -1;       
    }

    tcgetattr(s_uart->serial_fd, &tty);
    tty.c_cflag = B115200 | CS8 | CLOCAL | CREAD;	
    tty.c_iflag = IGNPAR;
    tty.c_oflag = 0;
    tty.c_lflag = 0;
    tcflush(s_uart->serial_fd, TCIFLUSH);

    if( tcsetattr(s_uart->serial_fd, TCSANOW, &tty) < 0)
    {
        printf("ERROR :  Setup serial failed\r\n");
        return -1;
    }

    uart_run = true;

    if (pthread_create(&s_uart->th_recv, NULL, &LTEIntHandler, (void *)(s_uart)) != 0)
    {
        printf("ERROR : initial thread receive serial failed\r\n");
        return -1;
    }

    return 0;
}

void close_usart(st_uart *s_uart)
{   
    uart_run = false;
    close(s_uart->serial_fd);
}

void* LTEIntHandler(void *arg)
{
    st_uart *s_uart = (st_uart *)arg;
    fd_set rset;
    struct timeval tv;
    int32_t count = 0;
    uint8_t i = 0;


    while(uart_run)
    {
        FD_ZERO(&rset);
        FD_SET(s_uart->serial_fd, &rset);
        tv.tv_sec = 30;
        tv.tv_usec = 0;

        count = select(s_uart->serial_fd + 1, &rset, NULL, NULL, &tv);

        if(count > 0)
        {            
            memset(RESPONSE_BUFFER, 0, UART_BUFFER_SIZE); 
            usleep(800000);           
            s_uart->recv_buff_cnt = read(s_uart->serial_fd, &RESPONSE_BUFFER, UART_BUFFER_SIZE); 
            GPSRDY = true;           
        }
        else
        {
            if(s_uart->serial_fd < 0)
            {
                //close serial
                uart_run = false;
                close(s_uart->serial_fd);                
                printf("UART close 2\r\n");                 
            }
        }
    }
    uart_open = true;
    printf("UART close\r\n"); 
    pthread_exit(NULL);       
}
