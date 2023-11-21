#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "led_strip.h"
#include "led_strip_rmt.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "ws2812b.h"
#include "esp_spiffs.h"

#define LED_STRIP_GPIO 7
#define LED_STRIP_RMT_RES_HZ (10 * 1000 * 1000)
#define STORAGE_NAME_SPACE "storage_data"
#define PARTITION_NAME "map_data"

static const char *TAG = "led_strip";
typedef struct
{
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    uint32_t brightness;
} led_color_t;

esp_vfs_spiffs_conf_t conf = {
    .base_path = "/spiffs",
    .partition_label = NULL,
    .max_files = 10,
    .format_if_mount_failed = true};

led_color_t leds_init_color[2] = {{255, 0, 0, 1}, {255, 0, 0, 1}};
led_color_t *leds_color = leds_init_color;

led_strip_handle_t led_strip_init(uint32_t led_length)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = led_length,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
    };
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);
    leds_color = (led_color_t *)malloc(sizeof(leds_color[0]) * led_length);
    for (int i = 0; i < led_length; i++)
    {
        led_color_t temp = {0, 0, 0, 1};
        leds_color[i] = temp;
    }
    ESP_LOGI(TAG, "Created LED strip object ");
    return led_strip;
}

void led_strip_deinit(led_strip_handle_t led_strip)
{
    led_strip_del(led_strip);
}

void save_led_length(uint32_t led_length)
{
    esp_err_t err;
    nvs_handle_t handle;
    err = nvs_open(STORAGE_NAME_SPACE, NVS_READWRITE, &handle);
    ESP_LOGI(TAG, "set led_len error  %s\n", esp_err_to_name(err));
    err = nvs_set_i32(handle, "led_len", led_length);
    switch (err)
    {
    case ESP_OK:
        ESP_LOGI(TAG, "nvs storage led length");
        break;

    default:
        ESP_LOGI(TAG, "set led_len error  %s\n", esp_err_to_name(err));
        break;
    }
    nvs_commit(handle);
    nvs_close(handle);
}

uint32_t get_led_length()
{
    uint32_t length = 3;
    esp_err_t err;
    nvs_handle_t handle;
    nvs_open(STORAGE_NAME_SPACE, NVS_READWRITE, &handle);
    err = nvs_get_i32(handle, "led_len", (int32_t *)&length);
    switch (err)
    {
    case ESP_OK:
        ESP_LOGI(TAG, "nvs_get   led-length:%ld", length);
        break;
    default:
        ESP_LOGI(TAG, "nvs get led state error  %s\n", esp_err_to_name(err));
        break;
    }
    nvs_close(handle);
    return length;
}

void set_led_color(led_strip_handle_t led_strip, uint32_t index, uint32_t red, uint32_t green, uint32_t blue, uint32_t brightness, bool is_save)
{
    led_strip_set_pixel(led_strip, index, (red / brightness), (green / brightness), (blue / brightness));
    led_strip_refresh(led_strip);
    if (is_save)
    {
        led_color_t temp = {red, green, blue, brightness};
        leds_color[index] = temp;
        esp_vfs_spiffs_register(&conf);
        FILE *f = fopen("/spiffs/led_color.dat", "wb");
        uint32_t length = get_led_length();
        fwrite(leds_color, sizeof(leds_color[0]), length, f);
        fclose(f);
        esp_vfs_spiffs_unregister(conf.partition_label);
    }
}

void set_leds_color_from_spiffs(led_strip_handle_t led_strip)
{
    uint32_t length = get_led_length();
    esp_vfs_spiffs_register(&conf);
    FILE *f = fopen("/spiffs/led_color.dat", "rb");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "leds_color file not found");
    }
    else
        fread(leds_color, sizeof(led_color_t), length, f);
    fclose(f);
    esp_vfs_spiffs_unregister(conf.partition_label);

    for (int i = 0; i < length; i++)
    {
        led_strip_set_pixel(led_strip, i, (leds_color[i].red) / leds_color[i].brightness, (leds_color[i].green) / leds_color[i].brightness, (leds_color[i].blue) / leds_color[i].brightness);
    }
    led_strip_refresh(led_strip);
}

void led_started_show_redlight(led_strip_handle_t led_strip)
{
    set_led_color(led_strip, 0, 255, 0, 0, 1, false);
}

void wifi_connected_show_yellowlight(led_strip_handle_t led_strip)
{
    set_led_color(led_strip, 0, 0, 255, 255, 1, false);
}

void mqtt_connected_show_greenlight(led_strip_handle_t led_strip)
{
    set_led_color(led_strip, 0, 0, 255, 0, 1, false);
}

void tcp_connected_set_mqtt_show_bluelight(led_strip_handle_t led_strip)
{
    set_led_color(led_strip, 0, 0, 0, 255, 1, false);
}