#include "string.h"
#include "stdint.h"
#include "stdbool.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include <math.h>

#include "tcp_server.h"
#include "lwip/sockets.h"




#define  TAG   "TCP"
//static int tcp_connect_socket = 0;
static int tcp_server_socket = 0;
static struct sockaddr_in tcp_client_addr;
	static unsigned int tcp_socklen = sizeof(tcp_client_addr);



void close_tcp_server()
{
	close(tcp_connect_socket);
	close(tcp_server_socket);
}

int  create_tcp_server()
{
	static struct sockaddr_in tcp_server_addr;
	tcp_server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (tcp_server_socket < 0)
	{
		close(tcp_server_socket);
		return ESP_FAIL;
	}

	tcp_server_addr.sin_family = AF_INET;
	tcp_server_addr.sin_port = htons(Listen_TCP_Port);
	tcp_server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	ESP_LOGI(TAG, "Server Socket Listen Port : %d", Listen_TCP_Port);

	if (bind(tcp_server_socket, (struct sockaddr *)&tcp_server_addr, sizeof(tcp_server_addr)) < 0)
	{
		close(tcp_server_socket);
		return ESP_FAIL;
	}

	if (listen(tcp_server_socket, 5) < 0)
	{
		close(tcp_server_socket);
		return ESP_FAIL;
	}

	
	tcp_connect_socket = accept(tcp_server_socket, (struct sockaddr *)&tcp_client_addr, &tcp_socklen);
	if (tcp_connect_socket < 0)
	{
		close(tcp_server_socket);
		return ESP_FAIL;
	}
	ESP_LOGI(TAG, "tcp connection :%d",tcp_connect_socket);
	return tcp_connect_socket;
}

void tcp_send(int socket,char* data,int data_len)
{
	send(socket, data, data_len, 0);
	ESP_LOGI(TAG, "data send success");
}
int tcp_recvs(int socket,char* data,int data_len)
{
    int i=recv(socket,data,data_len,0);
	printf("receive_data: %s   %d\n",data,i);
	send(socket,data,i,0);
	send(socket,"\r\n",3,0);
	return 1;
}

int reconnect()
{
	listen(tcp_server_socket, 5);
	
	tcp_connect_socket = accept(tcp_server_socket, (struct sockaddr *)&tcp_client_addr, &tcp_socklen);
	ESP_LOGI(TAG, "tcp connection :%d\n",tcp_connect_socket);
	return tcp_connect_socket;
}

