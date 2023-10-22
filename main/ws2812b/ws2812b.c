#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_err.h"
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
    int brightness;
} led_color_t;

led_color_t leds_init_color[1] = {{255, 0, 0, 1}};
led_color_t *leds_color = leds_init_color;

int led_len = 4;
led_strip_handle_t led_strip;

bool led_strip_config();
bool led_strip_rm();
bool led_display();
void indicator_led(int index, uint32_t red, uint32_t green, uint32_t blue, int brightness);
bool set_led_color(int index, uint32_t red, uint32_t green, uint32_t blue, int brightness);
bool set_led_length(int led_lens);
bool get_led_length_from_nvs();
bool get_leds_color_from_spiffs();

bool led_strip_config()
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = led_len,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
    };
    led_strip_rm();
    led_strip_handle_t led_strip_temp;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip_temp));
    led_strip = led_strip_temp;
    led_strip_clear(led_strip);
    leds_color = (led_color_t *)malloc(sizeof(led_color_t) * led_len);
    for (int i = 0; i < led_len; i++)
    {
        led_color_t temp = {0, 0, 0, 1};
        leds_color[i] = temp;
    }
    led_display();
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    return true;
}
bool led_strip_rm()
{
    led_strip_del(led_strip);
    return true;
}

bool set_led_length(int led_lens)
{
    led_len = led_lens;
    led_strip_config();
    esp_err_t err;
    nvs_handle_t handle;
    err = nvs_open(STORAGE_NAME_SPACE, NVS_READWRITE, &handle);
    ESP_LOGI(TAG, "set led_len error  %s\n", esp_err_to_name(err));
    err = nvs_set_i32(handle, "led_len", led_len);
    switch (err)
    {
    case ESP_OK:
        ESP_LOGI(TAG, "nvs_set led");
        break;

    default:
        ESP_LOGI(TAG, "set led_len error  %s\n", esp_err_to_name(err));
        break;
    }
    nvs_commit(handle);
    nvs_close(handle);
    return true;
}

bool led_display()
{
    for (int i = 0; i < led_len; i++)
    {
        led_strip_set_pixel(led_strip, i, (leds_color[i].red) / leds_color[i].brightness, (leds_color[i].green) / leds_color[i].brightness, (leds_color[i].blue) / leds_color[i].brightness);
    }
    led_strip_refresh(led_strip);
    return true;
}

void indicator_led(int index, uint32_t red, uint32_t green, uint32_t blue, int brightness)
{
    led_color_t temp = {red, green, blue, brightness};
    leds_color[index] = temp;
    led_display();
}
bool set_led_color(int index, uint32_t red, uint32_t green, uint32_t blue, int brightness)
{
    if (index >= led_len)
        return false;
    led_color_t temp = {red, green, blue, brightness};
    leds_color[index] = temp;
    led_display();
    ESP_LOGI(TAG, "nvs set led color %ld  %ld  %ld ", leds_color[index].red, leds_color[index].green, leds_color[index].blue);

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
    FILE *f = fopen("/spiffs/led_color", "wb");
    fwrite(leds_color, sizeof(leds_color[0]), led_len, f);
    fclose(f);
    esp_vfs_spiffs_unregister(conf.partition_label);
    return true;
}

bool get_led_length_from_nvs()
{
    esp_err_t err;
    nvs_handle_t handle;
    nvs_open(STORAGE_NAME_SPACE, NVS_READWRITE, &handle);
    err = nvs_get_i32(handle, "led_len", (int32_t *)&led_len);
    ESP_LOGI(TAG, "led_len  %d", led_len);
    switch (err)
    {
    case ESP_OK:
        ESP_LOGI(TAG, "nvs_get   led-len");
        break;

    default:
        ESP_LOGI(TAG, "nvs get led state error  %s\n", esp_err_to_name(err));
        break;
    }
    nvs_close(handle);
    return true;
}

bool get_leds_color_from_spiffs()
{
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
    FILE *f = fopen("/spiffs/led_color", "rb");
    if (f == NULL)
    {
        return false;
    }
    fread(leds_color, sizeof(leds_color[0]), led_len, f);
    fclose(f);
    esp_vfs_spiffs_unregister(conf.partition_label);
    ESP_LOGI(TAG, "spiffs get led color %ld  %ld  %ld ", leds_color[0].red, leds_color[0].green, leds_color[0].blue);
    return true;
}