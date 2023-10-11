#include "stdio.h"
#include "string.h"
#include "stdbool.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_smartconfig.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_system.h"
#include "wifi_smart_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define STORAGE_NAME_SPACE     "storage_data"

static const char *TAG = "wifi smart config";

bool wifi_is_connect = false;

void wifi_smart_config_task(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "wifi sta start");
        esp_smartconfig_set_type(SC_TYPE_ESPTOUCH);
        smartconfig_start_config_t smart_config_start_config = SMARTCONFIG_START_CONFIG_DEFAULT();
        esp_smartconfig_start(&smart_config_start_config);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_is_connect = false;
        esp_wifi_disconnect();
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD)
    {
        ESP_LOGI(TAG, "smart got ssid and password");

        smartconfig_event_got_ssid_pswd_t *smart_config_event_got_ssid_password = (smartconfig_event_got_ssid_pswd_t *)event_data;

        wifi_config_t wifi_config;
        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, smart_config_event_got_ssid_password->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, smart_config_event_got_ssid_password->password, sizeof(wifi_config.sta.password));
        wifi_config.sta.bssid_set = smart_config_event_got_ssid_password->bssid_set;
        if (wifi_config.sta.bssid_set == true)
        {
            memcpy(wifi_config.sta.bssid, smart_config_event_got_ssid_password->bssid, sizeof(wifi_config.sta.bssid));
        }

        ESP_LOGI(TAG, "ssid : %s", smart_config_event_got_ssid_password->ssid);
        ESP_LOGI(TAG, "password : %s", smart_config_event_got_ssid_password->password);
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        if (esp_wifi_connect() == ESP_OK)
        {
            ESP_LOGI(TAG, "wifi sta connect success");

            // nhh nvs
            esp_err_t err;
            nvs_handle_t handle;
            nvs_open(STORAGE_NAME_SPACE, NVS_READWRITE, &handle);
            err = nvs_set_blob(handle, "blob_wifi", &wifi_config, sizeof(wifi_config));
            switch (err)
            {
            case ESP_OK:
                ESP_LOGI(TAG, "save the wifi message: ssid:%s passwd:%s\r\n", wifi_config.sta.ssid, wifi_config.sta.password);
                nvs_commit(handle);
                nvs_close(handle);
                break;
            default:
                ESP_LOGI(TAG, "error  %s\n", esp_err_to_name(err));
                break;
            }
            wifi_is_connect = true;
        }
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE)
    {
        ESP_LOGI(TAG, "smart config send ack done");
        wifi_is_connect = true;
        esp_smartconfig_stop();
    }
}

void wifi_smart_config_init()
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init_config);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_smart_config_task, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_smart_config_task, NULL);
    esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &wifi_smart_config_task, NULL);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
}

bool wifi_smart_get_connect_state()
{
    return wifi_is_connect;
}


static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
}

bool nvs_restart_wificonfig()
{
    esp_err_t err;
    wifi_config_t wifi_config;
    uint32_t wifi_len = sizeof(wifi_config);
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(STORAGE_NAME_SPACE, NVS_READWRITE, &handle));
    err = nvs_get_blob(handle, "blob_wifi", &wifi_config, &wifi_len);
    switch (err)
    {
    case ESP_OK:
        ESP_LOGI(TAG, "[data3]: ssid:%s passwd:%s\r\n", wifi_config.sta.ssid, wifi_config.sta.password);
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGI(TAG, "wifi_config not save yet\n");
       return wifi_is_connect = false;
        break;
    default:
        ESP_LOGI(TAG, "error  %s\n", esp_err_to_name(err));
        return wifi_is_connect = false;
        break;
    }
    nvs_close(handle);

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_t *nvs_sta = esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init_config);
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    int counter = 0;
    while (err != ESP_OK || counter < 3)
    {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        err = esp_wifi_start();
        if (err == ESP_OK)
            return wifi_is_connect = true;
        counter++;
    }
    if (err != ESP_OK) ESP_LOGI(TAG, "nvs_wifi sta connect fail return to ESP Touch config wifi");
    esp_netif_destroy(nvs_sta);
    return wifi_is_connect = false;
}

void wifi_connect()
{
    if (nvs_restart_wificonfig())
    {
        ESP_LOGI(TAG, "wifi_config_by_nvs success");
    }
    else
    {
        wifi_smart_config_init();
    }
}