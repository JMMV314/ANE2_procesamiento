
#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include "bacn_RTI.h"
#include "../Modules/cJSON.h"
#include "../Modules/IQ.h"

bool server_run = false;
extern bool client_open;

struct sockaddr_in servaddr, cli; 

struct tm time1;
struct tm time2;

extern time_t startTime, stopTime;

char serialID[10];

extern uint8_t bands;
extern char banda[13];
extern char Flow[13];
extern char Fhigh[13];
extern char Tcity[13];
extern char Tchan[13];
extern char t_start[20];
extern char t_stop[20];
extern uint8_t net;

extern uint8_t getData;
extern bool rfhack;
extern bool program;
//extern uint8_t busy;

char SERVER_BUFFER[SERVER_BUFFER_SIZE];

void TimevalConv(char *timeData, struct tm *timeValue)
{
    char *token = strtok(timeData, "-T:");
            
    if (token != NULL) {
        timeValue->tm_year = (atoi(token) - 1900); 
    }
    token = strtok(NULL, "-T:");
    if (token != NULL) {
        timeValue->tm_mon = (atoi(token) - 1);  
    }
    token = strtok(NULL, "-T:");
    if (token != NULL) {
        timeValue->tm_mday = atoi(token); 
    }                
    token = strtok(NULL, "-T:");
    if (token != NULL) {
        timeValue->tm_hour = atoi(token); 
    }   
    token = strtok(NULL, "-T:");
    if (token != NULL) {
        timeValue->tm_min = atoi(token); 
    }   
         
    printf("Program Date and Time: %04d-%02d-%02dT%02d:%02d\n",
           timeValue->tm_year + 1900, timeValue->tm_mon + 1, timeValue->tm_mday,
           timeValue->tm_hour, timeValue->tm_min);
}

int8_t init_server(st_server *s_server)
{
    // socket create and verification
    s_server->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_server->server_fd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    // Binding newly created socket to given IP and verification 
    if ((bind(s_server->server_fd, (SA*)&servaddr, sizeof(servaddr))) != 0) { 
        printf("socket bind failed...\n"); 
        exit(0); 
    } 
    else
        printf("Socket successfully binded..\n"); 
  
    // Now server is ready to listen and verification 
    if ((listen(s_server->server_fd, 5)) != 0) { 
        printf("Listen failed...\n"); 
        exit(0); 
    } 
    else
        printf("Server listening..\n"); 
    s_server->server_len = sizeof(cli); 

    client_connect(s_server);
    server_run = true;   

    if (pthread_create(&s_server->th_recv, NULL, &ServerIntHandler, (void *)(s_server)) != 0)
    {
        printf("ERROR : initial thread server failed\r\n");
        return -1;
    }
}

void Server_SendString(st_server *s_server, const char *data)
{
    char dataServer[SERVER_BUFFER_SIZE];
    
    memset(dataServer, 0, sizeof(dataServer));
    sprintf(dataServer, "<%s>", data);
    write(s_server->conf_fd, dataServer, strlen(dataServer));    
}

void sendLocation(st_server *s_server, const char *Latitude, const char *Longitude)
{
    char dataServer[SERVER_BUFFER_SIZE];
    char MACDevice[14];
    struct ifreq s;

    strcpy(s.ifr_name, "eth0");
    if (0 == ioctl(s_server->conf_fd, SIOCGIFHWADDR, &s)) {

        int i;
        for (i = 0; i < 6; i++)
            sprintf(&MACDevice[i*2],"%02X",((unsigned char*)s.ifr_addr.sa_data)[i]);
        MACDevice[12]='\0';
    }
    
    memset(dataServer, 0, sizeof(dataServer));
    sprintf(dataServer, "{initResponse:{\"serial_id\": \"%s\", \"location\": \"bogota\", \"latitude\": %s, \"longitude\": %s}}", MACDevice, Latitude, Longitude);
    write(s_server->conf_fd, dataServer, strlen(dataServer));
}

int stringLen(char *str)
{
    int length = 0;
  
    // Loop till the NULL character is found
    while (*str != '\0')
    {
        length++;

        // Move to the next character
        str++;
    }
    return length;
}

int8_t client_connect(st_server *s_server)
{  
    system("sudo systemctl start monraf-client");
    // Accept the data packet from client and verification 
    s_server->conf_fd = accept(s_server->server_fd, (SA*)&cli, &s_server->server_len); 
    if (s_server->conf_fd < 0) { 
        printf("server accept failed...\n"); 
        exit(0); 
    } 
    else
        printf("server accept the client...\n");

    getData = 0;
    client_open = true; 
}

void close_server(st_server *s_server)
{   
    server_run = false;
    close(s_server->conf_fd);
}

void* ServerIntHandler(void* arg)
{
    st_server *s_server = (st_server *)arg;
    fd_set rset;
    struct timeval tv;
    int32_t count = 0;
    uint8_t i = 0;
    char* endptr = NULL;

    do
    {
        FD_ZERO(&rset);
        FD_SET(s_server->conf_fd, &rset);
        tv.tv_sec = 30;
        tv.tv_usec = 0;

        count = select(s_server->conf_fd + 1, &rset, NULL, NULL, &tv);

        if(count > 0)
        {
            memset(SERVER_BUFFER, 0, sizeof(SERVER_BUFFER));
            s_server->recv_buff_cnt = read(s_server->conf_fd, &SERVER_BUFFER, sizeof(SERVER_BUFFER));          
            
            if(s_server->recv_buff_cnt > 0)
            {
                int dataLen = stringLen(SERVER_BUFFER);

                if(dataLen < 10) 
                {
                    printf("Clent send: %s\r\n", SERVER_BUFFER);
                    char *token = strtok(SERVER_BUFFER, ",");
                
                    if (token != NULL) {
                        sprintf(serialID, "%s", token);
                        if(!strcmp(serialID, "init")) {
                            getData = 0;
                        } else if (!strcmp(serialID, "stop")) {
                            getData = 10;
                        }
                    }
                } else {
                    printf("Client send: %s\r\n", SERVER_BUFFER);
                    cJSON *json = cJSON_Parse(SERVER_BUFFER);
                    if (json == NULL) { 
                        const char *error_ptr = cJSON_GetErrorPtr(); 
                        if (error_ptr != NULL) { 
                            printf("Error: %s\n", error_ptr); 
                        } 
                        cJSON_Delete(json); 
                    } else {                        
                        cJSON *band = cJSON_GetObjectItemCaseSensitive(json, "band"); 
                        cJSON *fmin = cJSON_GetObjectItemCaseSensitive(json, "fmin"); 
                        cJSON *fmax = cJSON_GetObjectItemCaseSensitive(json, "fmax"); 
                        cJSON *measure = cJSON_GetObjectItemCaseSensitive(json, "measure"); 
                        cJSON *start = cJSON_GetObjectItemCaseSensitive(json, "startDate"); 
                        cJSON *stop = cJSON_GetObjectItemCaseSensitive(json, "endDate");
                        if (!strcmp(measure->valuestring, "RMTDT")) {
                            cJSON *channel = cJSON_GetObjectItemCaseSensitive(json, "channel"); 
                            cJSON *location = cJSON_GetObjectItemCaseSensitive(json, "location");
                            sprintf(Tchan, "%s", channel->valuestring);
                            sprintf(Tcity, "%s", location->valuestring);
                        } else {
                            if(!strcmp(band->valuestring, "VHF") && !strcmp(fmin->valuestring, "88")) {
                                bands = VHF1;
                            } else if(!strcmp(band->valuestring, "VHF") && !strcmp(fmin->valuestring, "137")) {  
                                bands = VHF2;                            
                            } else if(!strcmp(band->valuestring, "VHF") && !strcmp(fmin->valuestring, "148")) {
                                bands = VHF3;
                            } else if(!strcmp(band->valuestring, "VHF") && !strcmp(fmin->valuestring, "154")) {
                                bands = VHF4;
                            } else if(!strcmp(band->valuestring, "UHF") && !strcmp(fmin->valuestring, "400")) {
                                bands = UHF1;
                            } else if(!strcmp(band->valuestring, "UHF") && !strcmp(fmin->valuestring, "420")) {
                                bands = UHF1_2;
                            } else if(!strcmp(band->valuestring, "UHF") && !strcmp(fmin->valuestring, "440")) {
                                bands = UHF1_3;
                            } else if(!strcmp(band->valuestring, "UHF") && !strcmp(fmin->valuestring, "450")) {
                                bands = UHF1_4;
                            } else if(!strcmp(band->valuestring, "TDT") && !strcmp(fmin->valuestring, "470")) {
                                bands = UHF2_1;
                            } else if(!strcmp(band->valuestring, "TDT") && !strcmp(fmin->valuestring, "488")) {
                                bands = UHF2_2;
                            } else if(!strcmp(band->valuestring, "TDT") && !strcmp(fmin->valuestring, "506")) {
                                bands = UHF2_3;
                            } else if(!strcmp(band->valuestring, "TDT") && !strcmp(fmin->valuestring, "524")) {
                                bands = UHF2_4;
                            } else if(!strcmp(band->valuestring, "TDT") && !strcmp(fmin->valuestring, "542")) {
                                bands = UHF2_5;
                            } else if(!strcmp(band->valuestring, "TDT") && !strcmp(fmin->valuestring, "560")) {
                                bands = UHF2_6;
                            } else if(!strcmp(band->valuestring, "TDT") && !strcmp(fmin->valuestring, "578")) {
                                bands = UHF2_7;
                            } else if(!strcmp(band->valuestring, "TDT") && !strcmp(fmin->valuestring, "596")) {
                                bands = UHF2_8;
                            } else if(!strcmp(band->valuestring, "TDT") && !strcmp(fmin->valuestring, "614")) {
                                bands = UHF2_9;
                            } else if(!strcmp(band->valuestring, "TDT") && !strcmp(fmin->valuestring, "632")) {
                                bands = UHF2_10;
                            } else if(!strcmp(band->valuestring, "TDT") && !strcmp(fmin->valuestring, "650")) {
                                bands = UHF2_11;
                            } else if(!strcmp(band->valuestring, "TDT") && !strcmp(fmin->valuestring, "668")) {
                                bands = UHF2_12;
                            } else if(!strcmp(band->valuestring, "TDT") && !strcmp(fmin->valuestring, "678")) {
                                bands = UHF2_13;
                            } else if(!strcmp(band->valuestring, "UHF") && !strcmp(fmin->valuestring, "1708")) {
                                bands = UHF3;
                            } else if(!strcmp(band->valuestring, "UHF") && !strcmp(fmin->valuestring, "1735")) {
                                bands = UHF3_1;
                            } else if(!strcmp(band->valuestring, "UHF") && !strcmp(fmin->valuestring, "1805")) {
                                bands = UHF3_2;
                            } else if(!strcmp(band->valuestring, "UHF") && !strcmp(fmin->valuestring, "1848")) {
                                bands = UHF3_3;
                            } else if(!strcmp(band->valuestring, "UHF") && !strcmp(fmin->valuestring, "1868")) {
                                bands = UHF3_4;
                            } else if(!strcmp(band->valuestring, "UHF") && !strcmp(fmin->valuestring, "1877")) {
                                bands = UHF3_5;
                            } else if(!strcmp(band->valuestring, "SHF") && !strcmp(fmin->valuestring, "2550")) {
                                bands = SHF1;
                            } else if(!strcmp(band->valuestring, "SHF") && !strcmp(fmin->valuestring, "3295")) {
                                bands = SHF2;
                            } else if(!strcmp(band->valuestring, "SHF") && !strcmp(fmin->valuestring, "3338")) {
                                bands = SHF2_2;
                            } else if(!strcmp(band->valuestring, "SHF") && !strcmp(fmin->valuestring, "3375")) {
                                bands = SHF2_3;
                            } else if(!strcmp(band->valuestring, "SHF") && !strcmp(fmin->valuestring, "3444")) {
                                bands = SHF2_4;
                            } else if(!strcmp(band->valuestring, "SHF") && !strcmp(fmin->valuestring, "3538")) {
                                bands = SHF2_5;
                            } else if(!strcmp(band->valuestring, "SHF") && !strcmp(fmin->valuestring, "3550")) {
                                bands = SHF2_6;
                            } else if(!strcmp(band->valuestring, "SHF") && !strcmp(fmin->valuestring, "3580")) {
                                bands = SHF2_7;
                            }

                            sprintf(banda, "%s", band->valuestring);
                        }
                        sprintf(Flow, "%s", fmin->valuestring);
                        sprintf(Fhigh, "%s", fmax->valuestring);
                        sprintf(t_start, "%s", start->valuestring);
                        sprintf(t_stop, "%s", stop->valuestring);
                        if(!strcmp(t_start, "(null)")) {
                            printf(" NO programado\n");
                            //busy = 1;
                            program = false;
                        } else {                               
                            printf(" programado\n");
                            //busy = 2;
                            program = true;
                            TimevalConv(t_start, &time1);
                            TimevalConv(t_stop, &time2);
                            startTime = mktime(&time1);
                            stopTime = mktime(&time2);   
                        }

                         if(!strcmp(measure->valuestring, "RMER")) {
                             getData = 1;
                        } else if(!strcmp(measure->valuestring, "RMTDT")) {
                            getData = 2;
                        }else if(!strcmp(measure->valuestring, "RNI")) {
                            getData = 3;
                        }

                        // delete the JSON object 
                        cJSON_Delete(json); 
                    }   
                }
                rfhack = true;                
            } else {
                printf(" client disconnected\n");
                client_open = false;
                rfhack = false; 
                client_connect(s_server);
            }                
        }
        else
        {
            if(s_server->server_fd < 0)
            {
                //close server
                server_run = false;
                close(s_server->server_fd);                
                printf("Server close 2\r\n");                 
            }          
        }        
    } while(server_run);
    close(s_server->server_fd);
    printf("Server close\r\n");  
}
