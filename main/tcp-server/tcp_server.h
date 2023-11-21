#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__
#include "stdint.h"
#include "stdbool.h"
#include <sys/socket.h>
#include <freertos/event_groups.h>
#include "mqtt_client.h"
#include "mqtt_server.h"

#define Listen_TCP_Port 8080

int create_tcp_server();
void del_tcp_server(int tcp_connect_socket);
void tcp_send(int socket, char *data, int data_len);
bool tcp_recvs(int socket, char *data, int data_len);
esp_mqtt_client_config_t get_mqttconfig_by_tcp(int tcp_socket);

#endif
