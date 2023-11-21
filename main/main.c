#include <stdio.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "wifi_smart_config.h"
#include "tcp_server.h"
#include "mqtt_server.h"
#include "ws2812b.h"
#include "json_handle.h"

static const char *TAG = "main";

SemaphoreHandle_t mutex;

void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    uint32_t led_length = get_led_length();
    led_strip_handle_t led_strip = led_strip_init(led_length);
    led_started_show_redlight(led_strip);

    if (wifi_restart_by_nvs())
    {
        ESP_LOGI(TAG, "wifi_config_by_nvs success");
    }
    else
    {
        wifi_smart_config_init();
    }
    wifi_connected_show_yellowlight(led_strip);

    while (wifi_smart_get_connect_state() != true)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    esp_mqtt_client_config_t mqtt_cfg;
    set_mqtt_config_by_spiffs(&mqtt_cfg);
    if (mqtt_app_start(mqtt_cfg))
    {
        ESP_LOGI(TAG, "mqtt set by spiffs success");
    }
    else
    {
        tcp_connected_set_mqtt_show_bluelight(led_strip);
        ESP_LOGI(TAG, "mqtt set by tcp");
        int socket = 0;
        while (!(socket = create_tcp_server()))
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        mqtt_cfg = get_mqttconfig_by_tcp(socket);
        del_tcp_server(socket);
    }
    mqtt_connected_show_greenlight(led_strip);

    set_leds_color_from_spiffs(led_strip);
    json_get_led_strip(led_strip);
    while (1)
    {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}