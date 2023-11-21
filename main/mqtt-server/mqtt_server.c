#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "mqtt_server.h"
#include "cJSON.h"
#include "esp_spiffs.h"
#include "json_handle.h"
#include <stdio.h>
#include <string.h>

#define STORAGE_NAME_SPACE "storage_data"
#define MQTT_TOPIC "/fd7764f2-ad70-d04c-9427-d72b837cb935/recv"

static const char *TAG = "mqtt";

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, MQTT_TOPIC, 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        msg_id = esp_mqtt_client_publish(client, MQTT_TOPIC, "map_controller connected now ", 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        msg_id = esp_mqtt_client_publish(client, MQTT_TOPIC, "map_controller disconnected now ", 0, 1, 0);
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        cJSON *json = cJSON_Parse(event->data);
        if (json == NULL)
        {
            cJSON_Delete(json);
            printf("JSON parsing error!\n");
            return;
        }

        handle_result_t result = json_msg_handle(json);
        switch (result)
        {
        case reset_led:
            esp_mqtt_client_publish(client, MQTT_TOPIC, " the led reset now", 0, 1, 0);
            break;
        case reset_all:
            esp_mqtt_client_publish(client, MQTT_TOPIC, "  reset the all now, please reboot the device ", 0, 1, 0);
            break;
        case set_led:
            esp_mqtt_client_publish(client, MQTT_TOPIC, " change led color success", 0, 1, 0);
            break;
        case set_led_err:
            esp_mqtt_client_publish(client, MQTT_TOPIC, " change led color fail, Err: the index > led length !", 0, 1, 0);
            break;
        default:
            break;
        }

        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

bool mqtt_app_start(esp_mqtt_client_config_t cfg)
{
    esp_mqtt_client_config_t mqtt_cfg = cfg;
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL)
        return false;
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_err_t err;
    err = esp_mqtt_client_start(client);
    if (err != ESP_OK)
        return false;
    return true;
}

bool set_mqtt_config_by_spiffs(esp_mqtt_client_config_t* mqtt_cfg)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 10,
        .format_if_mount_failed = true};
    esp_vfs_spiffs_register(&conf);

    FILE *f = fopen("/spiffs/client.txt", "r");
    if (f == NULL)
        return false;
    char* client_id = (char*)malloc(sizeof(char)*64);
    fgets(client_id, sizeof(char)*64, f);
    fclose(f);
    f = fopen("/spiffs/url.txt", "r");
    char* mqtt_url = (char*)malloc(sizeof(char)*128);
    fgets(mqtt_url, sizeof(char)*64, f);
    fclose(f);
    f = fopen("/spiffs/username.txt", "r");
    char* username = (char*)malloc(sizeof(char)*64);
    fgets(username, sizeof(char)*64, f);
    fclose(f);
    f = fopen("/spiffs/password.txt", "r");
    char* password = (char*)malloc(sizeof(char)*64);
    fgets(password, sizeof(char)*64, f);
    fclose(f);
    f = fopen("/spiffs/port.txt", "r");
    char* mqtt_port_s = (char*)malloc(sizeof(char)*64);
    fgets(mqtt_port_s, sizeof(char)*64, f);
    int mqtt_port_i = atoi(mqtt_port_s);
    fclose(f);

    esp_vfs_spiffs_unregister(conf.partition_label);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = mqtt_url,
        .credentials.client_id = "fiberdoctormqttclient",
        .credentials.username = username,
        .credentials.authentication.password = password,
        .broker.address.port = mqtt_port_i,
        .network.disable_auto_reconnect = false,
        .session.keepalive = 100,
    };
    *mqtt_cfg =cfg;   
     ESP_LOGI(TAG, "mqtt_cfg  id %s url %s username %s mqtt_port %ld",  (*mqtt_cfg).credentials.client_id,  (*mqtt_cfg).broker.address.uri,  (*mqtt_cfg).credentials.username,  (*mqtt_cfg).broker.address.port);                                                                       
    return true;
}
