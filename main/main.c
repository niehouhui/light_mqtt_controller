#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "esp_task_wdt.h"
#include "time.h"
#include "unistd.h"
///////////////////////////////
#define LEDS 12 // led灯数
uint32_t leds_number[LEDS];
uint32_t Colors[7] = {
    0xFF0000, // 红色
    0xFF7F00, // 橙色
    0xFFFF00, // 黄色
    0x00FF00, // 绿色
    0x0000FF, // 蓝色
    0x4B0082, // 靛色
    0x8B00FF  // 紫色
};

/////////////////////////////
#define BLINK_GPIO 7                                         // 定义一个IO，用于WS2812通信
#define BLINK1_GPIO 0                                        // 定义一个IO，用于呼吸灯
#define ws2812b_din_pin_set() gpio_set_level(BLINK_GPIO, 1)  // WS2812宏定义，输出高电平
#define ws2812b_din_pin_rst() gpio_set_level(BLINK_GPIO, 0); // WS2812宏定义，输出低电平

TaskHandle_t start_task_handler; // 创建test_task任务句柄
void delay_ns(int data)          // 假设是100ns，实际上时间不确定
{
    int i;
    for (i = 0; i < (data * 20); i++)
        ;
}

void ws2812b_rst(void)
{
    ws2812b_din_pin_set();
    // esp_rom_delay_us(300);
    delay_ns(500); // 50us
    ws2812b_din_pin_rst();
}

void ws2812b_writebyte_byt(uint32_t led)
{
    unsigned char i;
    for (i = 0; i < 24; i++)
    {
        if (led & 0x800000)
        {
            ws2812b_din_pin_set();
            // esp_rom_delay_us(780);
            usleep(1);
            ws2812b_din_pin_rst();
            delay_ns(5);
        }
        else
        {
            ws2812b_din_pin_set();
            delay_ns(3); // 延时***ns
            ws2812b_din_pin_rst();
            usleep(1); // 延时1us
        }
        led <<= 1;
    }
}

void ws2812_task(void *pvParameters)
{

    while (1)
    {
        for (int i = 0; i < LEDS; i++)
        {
            leds_number[i] = Colors[random() % 7];
        }
        for (int i = 0; i < LEDS; i++)
        {
            ws2812b_writebyte_byt(leds_number[i]);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void start_task(void *pvParameters)
{
    xTaskCreate(&ws2812_task, "ws2812_task", 8192, NULL, 4, NULL);
    vTaskDelete(start_task_handler);
}

void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_rom_gpio_pad_select_gpio(BLINK_GPIO);
    esp_rom_gpio_pad_select_gpio(BLINK1_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(BLINK1_GPIO, GPIO_MODE_OUTPUT);

    xTaskCreate(&start_task, "start_task", 2048, NULL, 5, start_task_handler);
    esp_task_wdt_deinit();
}
