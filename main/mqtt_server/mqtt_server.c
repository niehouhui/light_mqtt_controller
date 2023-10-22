#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "protocol_examples_common.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "mqtt_server.h"
#include "cJSON.h"
#include "ws2812b.h"
#include "tcp_server.h"
#include "esp_spiffs.h"
#include "json_handle.h"

#define STORAGE_NAME_SPACE "storage_data"
#define MQTT_TOPIC "/fd7764f2-ad70-d04c-9427-d72b837cb935/recvnhh"

static const char *TAG = "mqtt";
size_t url_len;
size_t client_id_len;
size_t username_len;
size_t password_len;

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
bool mqtt_app_start(esp_mqtt_client_config_t cfg);
bool mqtt_config_by_spiffs();
void mqtt_connect();

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
    {
        return false;
    }
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    esp_err_t err;
    err = esp_mqtt_client_start(client);
    if (err == ESP_OK)
    { // mark 迭代nvs命名空间
        nvs_handle_t handle;
        ESP_ERROR_CHECK(nvs_open(STORAGE_NAME_SPACE, NVS_READWRITE, &handle));

        nvs_iterator_t it = NULL;
        esp_err_t res = nvs_entry_find("nvs", STORAGE_NAME_SPACE, NVS_TYPE_ANY, &it);
        while (res == ESP_OK)
        {
            nvs_entry_info_t info;
            nvs_entry_info(it, &info);
            printf("key '%s', type '%d' \n", info.key, info.type);
            res = nvs_entry_next(&it);
        }
        nvs_release_iterator(it);

        nvs_close(handle);
        return true;
    }
    return false;
}

bool mqtt_config_by_spiffs()
{
    char mqtt_url[128] = {0};
    char client_id[128] = {0};
    char username[128] = {0};
    char password[128] = {0};
    char mqtt_port_s[128] = {0};
    int mqtt_port_i = 0;

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
    fgets(client_id, sizeof(client_id), f);
    fclose(f);
    f = fopen("/spiffs/url.txt", "r");
    fgets(mqtt_url, sizeof(mqtt_url), f);
    fclose(f);
    f = fopen("/spiffs/username.txt", "r");
    fgets(username, sizeof(username), f);
    fclose(f);
    f = fopen("/spiffs/password.txt", "r");
    fgets(password, sizeof(password), f);
    fclose(f);
    f = fopen("/spiffs/port.txt", "r");
    fgets(mqtt_port_s, sizeof(mqtt_port_s), f);
    mqtt_port_i = atoi(mqtt_port_s);
    fclose(f);

    esp_vfs_spiffs_unregister(conf.partition_label);
    memcpy(client_id, "MQTT_Clients", sizeof("MQTT_Clients"));

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = mqtt_url,
        .credentials.client_id = client_id,
        .credentials.username = username,
        .credentials.authentication.password = password,
        .broker.address.port = mqtt_port_i,
        .network.disable_auto_reconnect = false,
        .session.keepalive = 10,
    };
    ESP_LOGI(TAG, "mqtt_cfg  id %s url %s username %s mqtt_port %ld", mqtt_cfg.credentials.client_id, mqtt_cfg.broker.address.uri, mqtt_cfg.credentials.username, mqtt_cfg.broker.address.port);
    return mqtt_app_start(mqtt_cfg);
}

void mqtt_connect()
{
    if (mqtt_config_by_spiffs())
    {
        ESP_LOGI(TAG, "mqtt set by spiffs success");
    }
    else
    {
        indicator_led(0, 0, 0, 255, 1);
        ESP_LOGI(TAG, "mqtt set by tcp");
        tcp_config_mqtt();
    }
}