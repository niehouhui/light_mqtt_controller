#ifndef __mqtt_server_h__
#define __mqtt_server_h__

#include "mqtt_client.h"

bool mqtt_app_start(esp_mqtt_client_config_t cfg);
void mqtt_connect();

#endif