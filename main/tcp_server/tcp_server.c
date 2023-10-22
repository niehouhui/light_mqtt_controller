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
#include "mqtt_client.h"
#include "mqtt_server.h"
#include "esp_spiffs.h"

#define TAG "TCP"
static int tcp_connect_socket = 0;
static int tcp_server_socket = 0;
static struct sockaddr_in tcp_client_addr;
static unsigned int tcp_socklen = sizeof(tcp_client_addr);

int create_tcp_server();
void close_tcp_server();
void tcp_send(int socket, char *data, int data_len);
int tcp_recvs(int socket, char *data, int data_len);
int reconnect();
bool tcp_config_mqtt();

void close_tcp_server()
{
	close(tcp_connect_socket);
	close(tcp_server_socket);
}

int create_tcp_server()
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
	ESP_LOGI(TAG, "tcp connection :%d", tcp_connect_socket);
	return tcp_connect_socket;
}

void tcp_send(int socket, char *data, int data_len)
{
	send(socket, data, data_len, 0);
	ESP_LOGI(TAG, "data send success");
}
int tcp_recvs(int socket, char *data, int data_len)
{
	int i = recv(socket, data, data_len, 0);
	printf("receive_data: %s   %d\n", data, i);
	send(socket, data, i, 0);
	send(socket, "\r\n", 3, 0);
	return 1;
}

int reconnect()
{
	listen(tcp_server_socket, 5);

	tcp_connect_socket = accept(tcp_server_socket, (struct sockaddr *)&tcp_client_addr, &tcp_socklen);
	ESP_LOGI(TAG, "tcp connection :%d\n", tcp_connect_socket);
	return tcp_connect_socket;
}

bool tcp_config_mqtt()
{
	char mqtt_url[128] = {0};
	char client_id[128] = {0};
	char username[128] = {0};
	char password[128] = {0};
	char mqtt_port_s[128] = {0};
	int mqtt_port_i = 0;
	char check_end[5] = {0};
	int len = 128;
	int tcp_socket = 0;
	while (!(tcp_socket = create_tcp_server()))
	{
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}

	while (!(strncmp("yes", check_end, 4) == 0))
	{
		char *guide_string = "please input the mqtt url,end with Enter\r\n";
		tcp_send(tcp_socket, guide_string, strlen(guide_string));
		while (tcp_recvs(tcp_socket, mqtt_url, len) != 1)
		{
			vTaskDelay(2000 / portTICK_PERIOD_MS);
		}
		guide_string = "please input the client_id,end with Enter\r\n";
		tcp_send(tcp_socket, guide_string, strlen(guide_string));
		while (tcp_recvs(tcp_socket, client_id, len) != 1)
		{
			vTaskDelay(2000 / portTICK_PERIOD_MS);
		}
		guide_string = "please input the username,end with Enter\r\n";
		tcp_send(tcp_socket, guide_string, strlen(guide_string));
		while (tcp_recvs(tcp_socket, username, len) != 1)
		{
			vTaskDelay(2000 / portTICK_PERIOD_MS);
		}
		guide_string = "please input the password,end with Enter\r\n";
		tcp_send(tcp_socket, guide_string, strlen(guide_string));
		while (tcp_recvs(tcp_socket, password, len) != 1)
		{
			vTaskDelay(2000 / portTICK_PERIOD_MS);
		}
		guide_string = "please input the mqtt_port,end with Enterr\r\n";
		tcp_send(tcp_socket, guide_string, strlen(guide_string));
		while (tcp_recvs(tcp_socket, mqtt_port_s, len) != 1)
		{
			vTaskDelay(2000 / portTICK_PERIOD_MS);
		}
		mqtt_port_i = atoi(mqtt_port_s);
		guide_string = "Check the message,input \"yes\" to save message, input \"not\" to reset the message\r\n";
		tcp_send(tcp_socket, guide_string, strlen(guide_string));
		while (tcp_recvs(tcp_socket, check_end, 4) != 1)
		{
			vTaskDelay(2000 / portTICK_PERIOD_MS);
		}
	}
	close_tcp_server();

	ESP_LOGI(TAG, "Initializing SPIFFS");
	esp_vfs_spiffs_conf_t conf = {
		.base_path = "/spiffs",
		.partition_label = NULL,
		.max_files = 10,
		.format_if_mount_failed = true};
	esp_vfs_spiffs_register(&conf);
	FILE *f = fopen("/spiffs/url.txt", "w");
	fprintf(f, mqtt_url);
	fclose(f);
	f = fopen("/spiffs/client.txt", "w");
	fprintf(f, client_id);
	fclose(f);
	f = fopen("/spiffs/username.txt", "w");
	fprintf(f, username);
	fclose(f);
	f = fopen("/spiffs/password.txt", "w");
	fprintf(f, password);
	fclose(f);
	f = fopen("/spiffs/port.txt", "w");
	fprintf(f, mqtt_port_s);
	fclose(f);
	esp_vfs_spiffs_unregister(conf.partition_label);

	esp_mqtt_client_config_t mqtt_tcp_cfg = {
		.broker.address.uri = mqtt_url,
		.credentials.client_id = client_id,
		.credentials.username = username,
		.credentials.authentication.password = password,
		.broker.address.port = mqtt_port_i,
		.network.disable_auto_reconnect = false,
		.session.keepalive = 10,
	};
	mqtt_app_start(mqtt_tcp_cfg);
	return true;
}