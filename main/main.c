#include <stdio.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ws2812b.h"
#include "wifi_smart_config.h"
#include "tcp_server.h"
#include "string.h"
#include "stdlib.h"
#include "mqtt_server.h"
#include "esp_spiffs.h"

void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    get_led_length_from_nvs();
    led_strip_config();

    indicator_led(0, 255, 0, 0, 1);
    wifi_connect();
    while (wifi_smart_get_connect_state() != true)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    indicator_led(0, 255, 255, 0, 1);
    mqtt_connect();
    
    indicator_led(0, 0, 255, 0, 1);
    get_leds_color_from_spiffs();
    led_display();

    while (1)
    {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}