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
/////////////////////////////////////////////////////ws2812b灯
#define ws2812b_din_pin_set() gpio_set_level(BLINK_GPIO, 1)  // WS2812宏定义，输出高电平
#define ws2812b_din_pin_rst() gpio_set_level(BLINK_GPIO, 0); // WS2812宏定义，输出低电平

// TaskHandle_t start_task_handler; // 创建test_task任务句柄
static const char* TAG ="ws2812b";

uint32_t leds_number[LEDS] = {0xffffff};
uint32_t Colors[8] = {
    0x000000, // 黑
    0xFFFFFF, // 白
    0xFF0000, // 绿色
    0x00FF00, // 红色
    0x0000FF, // 蓝色
    0x00FFFF, // 粉色
    0xFF00FF, // 青色
    0xFFFF00, // 黄色
};

void setPixelColor(int pixel, uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness)
{
    uint32_t color = ((green / brightness) << 16) | ((red / brightness) << 8) | (blue / brightness);
    leds_number[pixel] = color;

    nvs_handle_t handle;
    esp_err_t err;
     err= nvs_open("nhh_data", NVS_READWRITE, &handle);
     ESP_LOGI(TAG, "error  %s\n", esp_err_to_name(err));
     nvs_set_blob(handle, "leds", leds_number, sizeof(leds_number[1]) * LEDS);
    nvs_commit(handle);
    nvs_close(handle);
}

void get_led_state_from_nvs()
{
    esp_err_t err;
    nvs_handle_t handle;
    err=nvs_open("nhh_data", NVS_READWRITE, &handle);
    size_t nvs_led_len=sizeof(leds_number[1]) * LEDS;
    err=nvs_get_blob(handle, "leds", leds_number,&nvs_led_len );
    switch (err)
    {
    case ESP_OK:
        ESP_LOGI(TAG,"nvs_get     led  \r\n");
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGI(TAG, "led_config not save yet\n");
        leds_number[0] = 0xffffff;
        break;
    default:
        ESP_LOGI(TAG, "error  %s\n", esp_err_to_name(err));
        break;
    }
    nvs_close(handle);

}

void delay_ns(int data) //估计延时100us
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

void ws2812b_set_light(int index, uint32_t color)
{
    leds_number[index] = color;
    for (int i = 0; i < LEDS; i++)
    {
        ws2812b_writebyte_byt(leds_number[i]);
    }
}

void ws2812b_start()
{

    esp_rom_gpio_pad_select_gpio(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    // get_led_state_from_nvs();//调用就蹦有问题，


 
    while (1)
    {
        for (int i = 0; i < LEDS; i++)
        {
            ws2812b_writebyte_byt(leds_number[i]);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// void ws2812b_reset()
// {
//     uint32_t reset_led[LEDS] = {0xffffff};
//     nvs_handle_t leds_state;
//     ESP_ERROR_CHECK(nvs_open("nhh_data", NVS_READWRITE, &leds_state));
//     ESP_ERROR_CHECK(nvs_set_blob(leds_state, "leds", reset_led, sizeof(reset_led[1]) * LEDS));
//     nvs_commit(leds_state);
//     nvs_close(leds_state);
// }
