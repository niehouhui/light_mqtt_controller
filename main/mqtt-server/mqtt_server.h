#ifndef __mqtt_server_h__
#define __mqtt_server_h__

#include "mqtt_client.h"

bool mqtt_app_start(esp_mqtt_client_config_t cfg);
bool set_mqtt_config_by_spiffs(esp_mqtt_client_config_t* mqtt_cfg);

#endif