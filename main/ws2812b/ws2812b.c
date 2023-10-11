#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_task_wdt.h"
#include "time.h"
#include "unistd.h"
#include "string.h"
#include "esp_log.h"
#include "esp_system.h"

#include "ws2812b.h"
#define BLINK_GPIO 7
#define ws2812b_din_pin_set() gpio_set_level(BLINK_GPIO, 1)
#define ws2812b_din_pin_rst() gpio_set_level(BLINK_GPIO, 0);
#define STORAGE_NAME_SPACE "storage_data"

static const char *TAG = "ws2812b";

size_t led_len = 3;
uint32_t led[] = {0xffffff, 0, 0};
uint32_t *leds_number = led;


void setPixelColor(int pixel, uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness)
{
    uint32_t color = ((green / brightness) << 16) | ((red / brightness) << 8) | (blue / brightness);
    leds_number[pixel] = color;

    nvs_handle_t handle;
    esp_err_t err;
    err = nvs_open(STORAGE_NAME_SPACE, NVS_READWRITE, &handle);
    err = nvs_get_u32(handle, "led_len", &led_len);
    nvs_set_blob(handle, "leds", leds_number, sizeof(leds_number[0]) * led_len);
    nvs_commit(handle);
    nvs_close(handle);
}

bool get_led_state_from_nvs()
{
    esp_err_t err;
    nvs_handle_t handle;
    nvs_open(STORAGE_NAME_SPACE, NVS_READWRITE, &handle);
    err = nvs_get_u32(handle, "led_len", &led_len);
    ESP_LOGI(TAG, "error  %s\n", esp_err_to_name(err));
    ESP_LOGI(TAG, "led_len  %d", led_len);
    led_len=led_len*4;//存在nvs中的leds长是存进去的4倍
    uint32_t *p = (uint32_t *)malloc(sizeof(leds_number[0])*led_len);
    leds_number=p;
    err = nvs_get_blob(handle, "leds", leds_number, &led_len);
    switch (err)
    {
    case ESP_OK:
        ESP_LOGI(TAG, "nvs_get     led");
        ESP_LOGI(TAG, "led_len  %d", led_len);
        break;

    default:  
        ESP_LOGI(TAG, "error  %s\n", esp_err_to_name(err));
        ESP_LOGI(TAG, "led_len  %d", led_len);
        break;
    }

    nvs_close(handle);
    return true;
}

void delay_ns(int data) // 估计延时100us
{
    int i;
    for (i = 0; i < (data * 20); i++)
        ;
}

void ws2812b_writebyte_byt(uint32_t led)
{
    for (int i = 0; i < 24; i++)
    {
        if (led & 0x800000)
        {
            ws2812b_din_pin_set();
            usleep(1);
            ws2812b_din_pin_rst();
            delay_ns(300);
        }
        else
        {
            ws2812b_din_pin_set();
            delay_ns(300);
            ws2812b_din_pin_rst();
            usleep(1);
        }
        led <<= 1;
    }
}

void ws2812b_start()
{

    esp_rom_gpio_pad_select_gpio(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    // get_led_state_from_nvs();//调用就蹦有问题，

    while (1)
    {
        for (int i = 0; i < led_len; i++)
        {
            ws2812b_writebyte_byt(leds_number[i]);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void ws2812b_reset(int led_total)
{

    uint32_t *p = (uint32_t *)malloc(sizeof(leds_number[0]) * led_total);
    p[0] = 0xffffff;
    for (int i = 1; i < led_total; i++)
        p[i] = 0;
    leds_number = p;
    
    led_len=led_total;
    nvs_handle_t handle;
    nvs_open(STORAGE_NAME_SPACE, NVS_READWRITE, &handle);
    nvs_set_u32(handle, "led_len", led_len);
    esp_err_t err;
    err=nvs_set_blob(handle, "leds", leds_number, sizeof(leds_number[0]) * led_len);
    ESP_LOGI(TAG, " reset error  %s\n", esp_err_to_name(err));
    nvs_commit(handle);
    nvs_close(handle);
    free(p);
}
