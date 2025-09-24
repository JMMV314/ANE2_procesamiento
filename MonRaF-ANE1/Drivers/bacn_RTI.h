#ifndef BACN_RTI_H
#define BACN_RTI_H

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

#define SERVER_BUFFER_SIZE 1000
#define PORT 2000
#define SA struct sockaddr

typedef struct
{
    uint32_t server_fd;
    pthread_t th_recv;
    uint32_t conf_fd;
    uint32_t server_len;
    int32_t recv_buff_cnt;

}st_server;

void TimevalConv(char *timeData, struct tm *timeValue);
int8_t init_server(st_server *s_server);
void Server_SendString(st_server *s_server, const char *data);
void sendLocation(st_server *s_server, const char *Latitude, const char *Longitude);
int stringLen(char *str);
int8_t client_connect(st_server *s_server);
void close_server(st_server *s_server);
void* ServerIntHandler(void* arg);

#endif // BACN_RTI_H