#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__
#include "stdint.h"
#include "stdbool.h"
#include <sys/socket.h>
#include <freertos/event_groups.h>
#include "mqtt_client.h"
#include "mqtt_server.h"

#define Listen_TCP_Port 8080

typedef int tcp_connect_socket_i;

tcp_connect_socket_i create_tcp_server();
void del_tcp_server(tcp_connect_socket_i tcp_connect_socket);
void tcp_send(int socket, char *data, int data_len);
bool tcp_recvs(int socket, char *data, int data_len);
esp_mqtt_client_config_t tcp_config_mqtt_and_save_config(tcp_connect_socket_i tcp_socket);

#endif
