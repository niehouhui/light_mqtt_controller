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
#include "esp_spiffs.h"

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
                msg_id = esp_mqtt_client_publish(client, "/fd7764f2-ad70-d04c-9427-d72b837cb935/recv", " the led reset now", 0, 1, 0);
                vTaskDelay(100 / portTICK_PERIOD_MS);
                break;
            }

                cJSON *js_reset_all = cJSON_GetObjectItemCaseSensitive(json, "reset_all");
        if (js_reset_all != NULL && cJSON_IsNumber(js_reset_all))
            if (js_reset_all->valueint == 1)
            {
                nvs_erase_all(handle);
                nvs_commit(handle);

                esp_vfs_spiffs_conf_t conf = {
                    .base_path = "/spiffs",
                    .partition_label = NULL,
                    .max_files = 10,
                    .format_if_mount_failed = true};
                esp_err_t ret = esp_vfs_spiffs_register(&conf);

                unlink("/spiffs/url.txt");
                unlink("/spiffs/client.txt");
                unlink("/spiffs/username.txt");
                unlink("/spiffs/password.txt");
                unlink("/spiffs/port.txt");
                esp_vfs_spiffs_unregister(conf.partition_label);
                vTaskDelay(100 / portTICK_PERIOD_MS);
                printf("nvs_erase the all\n");
                break;
            };    

        nvs_close(handle);

        cJSON *js_index = cJSON_GetObjectItemCaseSensitive(json, "index");
        if (js_index != NULL && cJSON_IsNumber(js_index))
        {
            printf("index: %d\n", js_index->valueint);
        }
        cJSON *js_brightness = cJSON_GetObjectItemCaseSensitive(json, "brightness");
        cJSON *js_red = cJSON_GetObjectItemCaseSensitive(json, "red");
        cJSON *js_green = cJSON_GetObjectItemCaseSensitive(json, "green");
        cJSON *js_blue = cJSON_GetObjectItemCaseSensitive(json, "blue");
        setPixelColor((js_index->valueint), (uint8_t)js_red->valueint, (uint8_t)js_green->valueint, (uint8_t)js_blue->valueint, js_brightness->valueint);
        msg_id = esp_mqtt_client_publish(client, "/fd7764f2-ad70-d04c-9427-d72b837cb935/recv", " the led changed now", 0, 1, 0);

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
        while (tcp_recvs(tcp_socket, mqtt_url, len) != 1)
        {
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
        tcp_recvs(tcp_socket, mqtt_url, len);

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
    //////////////////使用spiffs存储mqtt连接状态数据，
    ESP_LOGI(TAG, "Initializing SPIFFS");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 10,
        .format_if_mount_failed = true};
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_spiffs_format(conf.partition_label);
        return false;
    }
    else
    {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

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

    return mqtt_app_start(mqtt_tcp_cfg);
}

bool mqtt_app_start(esp_mqtt_client_config_t cfg)
{
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

// bool mqtt_config_by_nvs()
// {
//     esp_mqtt_client_config_t mqtt_cfg;
//     // 将mqtt数据以字符串存入nvs再读出，读出失败，程序崩溃
//     esp_err_t err;
//     nvs_handle_t handle;
//     ESP_ERROR_CHECK(nvs_open(STORAGE_NAME_SPACE, NVS_READWRITE, &handle));

//     err = nvs_get_u32(handle, "url_len", &url_len);
//     if (err != ESP_OK)
//     {
//         ESP_LOGI(TAG, " reset error  %s\n", esp_err_to_name(err));
//         return false;
//     }
//     ESP_ERROR_CHECK(nvs_get_u32(handle, "client_len", &client_id_len));
//     ESP_ERROR_CHECK(nvs_get_u32(handle, "username_len", &username_len));
//     ESP_ERROR_CHECK(nvs_get_u32(handle, "password_len", &password_len));

//     ESP_ERROR_CHECK(nvs_get_str(handle, "mqtt_url", &mqtt_cfg.broker.address.uri, &url_len));
//     if (err != ESP_OK)
//     {
//         ESP_LOGI(TAG, " reset error  %s\n", esp_err_to_name(err));
//         return false;
//     }
//     ESP_ERROR_CHECK(nvs_get_str(handle, "client_id", &mqtt_cfg.credentials.client_id, &client_id_len));
//     ESP_ERROR_CHECK(nvs_get_str(handle, "username", &mqtt_cfg.credentials.username, &username_len));
//     ESP_ERROR_CHECK(nvs_get_str(handle, "password", &mqtt_cfg.credentials.authentication.password, &password_len));
//     ESP_ERROR_CHECK(nvs_get_u32(handle, "mqtt_port", &mqtt_cfg.broker.address.port));

//     ///////将mqtt数据以二进制数据直接存取，读取失败，为空
//     // size_t mqtt_len=sizeof(mqtt_cfg);
//     // err=nvs_get_blob(handle,"blob_mqtt",&mqtt_cfg,&mqtt_len);
//     //  if (err != ESP_OK)
//     // {
//     //     ESP_LOGI(TAG, " reset error  %s\n", esp_err_to_name(err));
//     //     nvs_close(handle);
//     //     return false;
//     // }
//     // //只能读出1883端口，其他都是乱码
//     // ESP_LOGI(TAG, "faill to open file mqtt_cfg  %s 1 %s 1 %s 1 %ld", mqtt_cfg.broker.address.uri,mqtt_cfg.credentials.client_id,mqtt_cfg.credentials.username,mqtt_cfg.broker.address.port);
//     // nvs_close(handle);

//     // esp_mqtt_client_config_t mqtt_cfg = {
//     //     // .broker.address.uri = CONFIG_BROKER_URL,

//     //     .broker.address.uri = "mqtt://fiber-doctor.com",
//     //     .credentials.client_id = "MQTT_Clients",
//     //     .credentials.username = "guest",
//     //     .credentials.authentication.password = "guest",
//     //     .broker.address.port = 1883,
//     // };
//     ESP_LOGI(TAG, "faill to open file mqtt_cfg  %s 1 %s 1 %s 1 %ld", mqtt_cfg.broker.address.uri, mqtt_cfg.credentials.client_id, mqtt_cfg.credentials.username, mqtt_cfg.broker.address.port);

//     mqtt_app_start(mqtt_cfg);
//     return true;
// }

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
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_spiffs_format(conf.partition_label);
        return false;
    }
    else
    {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

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

    ESP_LOGI(TAG, "mqtt_cfg  id %s url %s username %s mqtt_port %d", client_id, mqtt_url, username, mqtt_port_i);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = mqtt_url,
        .credentials.client_id = client_id,
        .credentials.username = username,
        .credentials.authentication.password = password,
        .broker.address.port = mqtt_port_i,
    };

    return mqtt_app_start(mqtt_cfg);
}

void mqtt_connect()
{
    // if (mqtt_config_by_nvs())
    if (mqtt_config_by_spiffs())
    {
        // ESP_LOGI(TAG, "mqtt set by nvs success");
        ESP_LOGI(TAG, "mqtt set by spiffs success");
    }
    else
    {
        setPixelColor(0, 0, 0, 255, 1);
        mqtt_config_by_tcp();
    }
}