#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"

#include "ws2812b.h"
#include "tcp_server.h"
#include "mqtt_server.h"

#define STORAGE_NAME_SPACE "storage_data"

static const char *TAG = "mqtt";
size_t url_len;
size_t client_id_len;
size_t username_len;
size_t password_len;

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "/fd7764f2-ad70-d04c-9427-d72b837cb935/recv", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        msg_id = esp_mqtt_client_publish(client, "/fd7764f2-ad70-d04c-9427-d72b837cb935/recv", "map_controller online now ", 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
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
        ////////////////////
        nvs_handle_t handle;
        nvs_open(STORAGE_NAME_SPACE, NVS_READWRITE, &handle);
        cJSON *js_reset_led = cJSON_GetObjectItemCaseSensitive(json, "reset_led");
        cJSON *js_led_total = cJSON_GetObjectItemCaseSensitive(json, "led_total");
        if (js_reset_led != NULL && cJSON_IsNumber(js_reset_led))
            if (js_reset_led->valueint == 1)
            {
                nvs_erase_key(handle, "leds");
                ws2812b_reset(js_led_total->valueint);
                nvs_commit(handle);
                printf("nvs_erase the led\n");
                vTaskDelay(100 / portTICK_PERIOD_MS);
                break;
            }

        cJSON *js_reset_mqtt = cJSON_GetObjectItemCaseSensitive(json, "reset_mqtt");
        if (js_reset_mqtt != NULL && cJSON_IsNumber(js_reset_mqtt))
            if (js_reset_mqtt->valueint == 1)
            {
                nvs_erase_key(handle, "mqtt");
                nvs_commit(handle);
                printf("nvs_erase the mqtt\n");
                vTaskDelay(100 / portTICK_PERIOD_MS);
                break;
            }

        cJSON *js_reset_all = cJSON_GetObjectItemCaseSensitive(json, "reset_all");
        if (js_reset_all != NULL && cJSON_IsNumber(js_reset_all))
            if (js_reset_all->valueint == 1)
            {
                nvs_erase_all(handle);
                nvs_commit(handle);
                printf("nvs_erase the all\n");
                vTaskDelay(100 / portTICK_PERIOD_MS);
                break;
            };

        nvs_close(handle);
        ////////////////////
        cJSON *js_index = cJSON_GetObjectItemCaseSensitive(json, "index");
        if (js_index != NULL && cJSON_IsNumber(js_index))
        {
            printf("index: %d\n", js_index->valueint);
        }
        cJSON *js_brightness = cJSON_GetObjectItemCaseSensitive(json, "brightness");
        cJSON *js_red = cJSON_GetObjectItemCaseSensitive(json, "red");
        cJSON *js_green = cJSON_GetObjectItemCaseSensitive(json, "green");
        cJSON *js_blue = cJSON_GetObjectItemCaseSensitive(json, "blue");
        setPixelColor(js_index->valueint, (uint8_t)js_red->valueint, (uint8_t)js_green->valueint, (uint8_t)js_blue->valueint, js_brightness->valueint);
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

bool mqtt_config_by_tcp()
{
    int tcp_socket = 0;
    while (!(tcp_socket = create_tcp_server()))
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    char mqtt_url[128] = {0};
    char client_id[128] = {0};
    char username[128] = {0};
    char password[128] = {0};
    char mqtt_port_s[128] = {0};
    int mqtt_port_i = 0;
    char check_end[5] = {0};
    int len = 128;
    while (!(strncmp("save", check_end, 4) == 0))
    { /////这个过程中tcp中断会死循环；
        char *guide_string = "please input the mqtt url,end with Enter\r\n";
        tcp_send(tcp_socket, guide_string, strlen(guide_string));
        // while (tcp_recvs(tcp_socket, mqtt_url, len) != 1)
        // {
        //     vTaskDelay(2000 / portTICK_PERIOD_MS);
        // }
        tcp_recvs(tcp_socket, mqtt_url, len);

        guide_string = "please input the client_id,end with Enter\r\n";
        tcp_send(tcp_socket, guide_string, strlen(guide_string));
        // while (tcp_recvs(tcp_socket, client_id, len) != 1)
        // {
        //     vTaskDelay(2000 / portTICK_PERIOD_MS);
        // }

        guide_string = "please input the username,end with Enter\r\n";
        tcp_send(tcp_socket, guide_string, strlen(guide_string));
        // while (tcp_recvs(tcp_socket, username, len) != 1)
        // {
        //     vTaskDelay(2000 / portTICK_PERIOD_MS);
        // }

        guide_string = "please input the password,end with Enter\r\n";
        tcp_send(tcp_socket, guide_string, strlen(guide_string));
        // while (tcp_recvs(tcp_socket, password, len) != 1)
        // {
        //     vTaskDelay(2000 / portTICK_PERIOD_MS);
        // }

        guide_string = "please input the mqtt_port,end with Enterr\r\n";
        tcp_send(tcp_socket, guide_string, strlen(guide_string));
        // while (tcp_recvs(tcp_socket, mqtt_port_s, len) != 1)
        // {
        //     vTaskDelay(2000 / portTICK_PERIOD_MS);
        // }
        mqtt_port_i = atoi(mqtt_port_s);

        guide_string = "Check the message,input \"save\" to save message, input \"reset\" to reset the message\r\n";
        tcp_send(tcp_socket, guide_string, strlen(guide_string));
        while (tcp_recvs(tcp_socket, check_end, 5) != 1)
        {
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    }

    esp_mqtt_client_config_t mqtt_tcp_cfg = {
        .broker.address.uri = mqtt_url,
        .credentials.client_id = client_id,
        .credentials.username = username,
        .credentials.authentication.password = password,
        .broker.address.port = mqtt_port_i,
    };

    return mqtt_app_start(mqtt_tcp_cfg);
}

bool mqtt_app_start(esp_mqtt_client_config_t cfg)
{
    // esp_mqtt_client_config_t mqtt_cfg = {
    //     // .broker.address.uri = CONFIG_BROKER_URL,

    //     .broker.address.uri = mqtt_url,
    //     .credentials.client_id = client_id,
    //     .credentials.username = username,
    //     .credentials.authentication.password = password,
    //     .broker.address.port = mqtt_port_i,
    // };
    // #if CONFIG_BROKER_URL_FROM_STDIN
    //     char line[128];

    //     if (strcmp(mqtt_cfg.broker.address.uri, "FROM_STDIN") == 0)
    //     {
    //         int count = 0;
    //         printf("Please enter url of mqtt broker\n");
    //         while (count < 128)
    //         {
    //             int c = fgetc(stdin);
    //             if (c == '\n')
    //             {
    //                 line[count] = '\0';
    //                 break;
    //             }
    //             else if (c > 0 && c < 127)
    //             {
    //                 line[count] = c;
    //                 ++count;
    //             }
    //             vTaskDelay(10 / portTICK_PERIOD_MS);
    //         }
    //         mqtt_cfg.broker.address.uri = line;
    //         printf("Broker url: %s\n", line);
    //     }
    //     else
    //     {
    //         ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
    //         abort();
    //     }
    // #endif /* CONFIG_BROKER_URL_FROM_STDIN */

    esp_mqtt_client_config_t mqtt_cfg = cfg;
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL)
    {
        return false;
    }

    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_err_t err;
    err = esp_mqtt_client_start(client);
    if (err == ESP_OK)
    {
        nvs_handle_t handle;
        ESP_ERROR_CHECK(nvs_open(STORAGE_NAME_SPACE, NVS_READWRITE, &handle));

        ESP_ERROR_CHECK(nvs_set_str(handle, "mqtt_url", mqtt_cfg.broker.address.uri));
        nvs_get_str(handle, "mqtt_url", NULL, &url_len);
        ESP_LOGI(TAG, "url len %d", url_len);
        ESP_ERROR_CHECK(nvs_set_str(handle, "client_id", mqtt_cfg.credentials.client_id));
        nvs_get_str(handle, "client_id", NULL, &client_id_len);
        ESP_LOGI(TAG, "client id len %d", client_id_len);
        ESP_ERROR_CHECK(nvs_set_str(handle, "username", mqtt_cfg.credentials.username));
        nvs_get_str(handle, "username", NULL, &username_len);
        ESP_LOGI(TAG, "username len%d", username_len);
        ESP_ERROR_CHECK(nvs_set_str(handle, "password", mqtt_cfg.credentials.authentication.password));
        nvs_get_str(handle, "password", NULL, &password_len);
        ESP_LOGI(TAG, "password len%d", password_len);
        ESP_ERROR_CHECK(nvs_set_u32(handle, "mqtt_port", mqtt_cfg.broker.address.port));

        ESP_ERROR_CHECK(nvs_set_u32(handle, "url_len", url_len));
        ESP_ERROR_CHECK(nvs_set_u32(handle, "client_len", client_id_len));
        ESP_ERROR_CHECK(nvs_set_u32(handle, "username_len", username_len));
        ESP_ERROR_CHECK(nvs_set_u32(handle, "password_len", password_len));

        ESP_ERROR_CHECK(nvs_commit(handle));

        // 迭代nvs命名空间
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

bool mqtt_config_by_nvs()
{
    // esp_mqtt_client_config_t mqtt_cfg;
    // esp_err_t err;
    // nvs_handle_t handle;
    // ESP_ERROR_CHECK(nvs_open(STORAGE_NAME_SPACE, NVS_READWRITE, &handle));

    // err = nvs_get_u32(handle, "url_len", &url_len);
    // if (err != ESP_OK)
    // {
    //     ESP_LOGI(TAG, " reset error  %s\n", esp_err_to_name(err));
    //     return false;
    // }
    // ESP_ERROR_CHECK(nvs_get_u32(handle, "client_len", &client_id_len));
    // ESP_ERROR_CHECK(nvs_get_u32(handle, "username_len", &username_len));
    // ESP_ERROR_CHECK(nvs_get_u32(handle, "password_len", &password_len));

    // ESP_ERROR_CHECK(nvs_get_str(handle, "mqtt_url", &mqtt_cfg.broker.address.uri, &url_len));
    // if (err != ESP_OK)
    // {
    //     ESP_LOGI(TAG, " reset error  %s\n", esp_err_to_name(err));
    //     return false;
    // }
    // ESP_ERROR_CHECK(nvs_get_str(handle, "client_id", &mqtt_cfg.credentials.client_id, &client_id_len));
    // ESP_ERROR_CHECK(nvs_get_str(handle, "username", &mqtt_cfg.credentials.username, &username_len));
    // ESP_ERROR_CHECK(nvs_get_str(handle, "password", &mqtt_cfg.credentials.authentication.password, &password_len));
    // ESP_ERROR_CHECK(nvs_get_u32(handle, "mqtt_port", &mqtt_cfg.broker.address.port));

    // nvs_close(handle);

    esp_mqtt_client_config_t mqtt_cfg = {
        // .broker.address.uri = CONFIG_BROKER_URL,

        .broker.address.uri = "mqtt://fiber-doctor.com",
        .credentials.client_id = "MQTT_Client",
        .credentials.username = "guest",
        .credentials.authentication.password = "guest",
        .broker.address.port = 1883,
    };

    mqtt_app_start(mqtt_cfg);
    return true;
}

void mqtt_connect()
{
    if (mqtt_config_by_nvs())
    {
        ESP_LOGI(TAG, "mqtt set by nvs success");
    }
    else
    {
        setPixelColor(0, 0, 0, 255, 1);
        mqtt_config_by_tcp();
    }
}