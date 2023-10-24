#include <stdio.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "wifi_smart_config.h"
#include "tcp_server.h"
#include "string.h"
#include "stdlib.h"
#include "mqtt_server.h"
#include "esp_spiffs.h"
#include "led_strip.h"
#include "led_strip_rmt.h"
#include "ws2812b.h"
#include "json_handle.h"

static const char *TAG = "main";
led_strip_handle_t led_strip;

void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    int led_length = get_led_length();
    led_strip_handle_t led_strip = led_strip_init(led_length);
    set_led_color(led_strip, 0, 255, 0, 0, 1, false);

    wifi_connect();
    while (wifi_smart_get_connect_state() != true)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    set_led_color(led_strip, 0, 0, 255, 255, 1, false);

    if (mqtt_config_by_spiffs())
    {
        ESP_LOGI(TAG, "mqtt set by spiffs success");
    }
    else
    {
        set_led_color(led_strip, 0, 0, 0, 255, 1, false);
        ESP_LOGI(TAG, "mqtt set by tcp");
        tcp_connect_socket_i socket = 0;
        while (!(socket = create_tcp_server()))
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        esp_mqtt_client_config_t mqtt_cfg = tcp_config_mqtt_and_save_config(socket);
        mqtt_app_start(mqtt_cfg);
        del_tcp_server(socket);
    }
    set_led_color(led_strip, 0, 0, 255, 0, 1, false);
    get_leds_color_from_spiffs(led_strip);
    json_get_led_strip(led_strip); 
    while (1)
    {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}