#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
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

#include "esp_log.h"
//////////////////////////////////////

#include "ws2812b.h"
#include "wifi_smart_config.h"
#include "tcp_server.h"
#include "string.h"
#include "stdlib.h"
#include "mqtt_server.h"



void app_main()
{
    esp_err_t ret = nvs_flash_init();
    // if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    // {
    //     ESP_ERROR_CHECK(nvs_flash_erase());
    //     ret = nvs_flash_init();
    // }
    ESP_ERROR_CHECK(ret);

get_led_state_from_nvs();/////////
    xTaskCreate(&ws2812b_start, "ws2812_task", 1024, NULL, 4, NULL);
    setPixelColor(0, 255, 0, 0, 1);

    wifi_connect();
    while (wifi_smart_get_connect_state() != true)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    setPixelColor(0, 255, 255, 0, 1);

    mqtt_connect();
    // setPixelColor(0, 0, 255, 0, 1);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    // setPixelColor(0, 0, 0, 0, 1);

    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}