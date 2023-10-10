#ifndef __mqtt_server_h__
#define __mqtt_server_h__

#include "mqtt_client.h"

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
bool mqtt_app_start(esp_mqtt_client_config_t cfg);
bool mqtt_config_by_tcp();
bool mqtt_config_by_nvs();
void mqtt_connect();
#endif