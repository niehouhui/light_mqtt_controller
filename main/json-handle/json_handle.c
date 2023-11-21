#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "ws2812b.h"
#include "json_handle.h"
#include "led_strip.h"
#include "led_strip_rmt.h"

#define STORAGE_NAME_SPACE "storage_data"
#define TAG "json_handle"

led_strip_handle_t json_led_strip = NULL;

void json_get_led_strip(led_strip_handle_t led_strip)
{
    json_led_strip = led_strip;
}

handle_result_t json_msg_handle(cJSON *json)
{
    if (json_led_strip == NULL)
        return reset_led_err;
    cJSON *js_reset_led = cJSON_GetObjectItemCaseSensitive(json, "reset_led");
    cJSON *js_led_length = cJSON_GetObjectItemCaseSensitive(json, "led_length");
    if (js_reset_led != NULL && cJSON_IsNumber(js_reset_led))
        if (js_reset_led->valueint == 1)
        {
            save_led_length(js_led_length->valueint);
            json_led_strip = led_strip_init(js_led_length->valueint);
            ESP_LOGI(TAG, "reset leds color and length :%d", js_led_length->valueint);
            return reset_led;
        }

    cJSON *js_reset_all = cJSON_GetObjectItemCaseSensitive(json, "reset_all");
    if (js_reset_all != NULL && cJSON_IsNumber(js_reset_all))
        if (js_reset_all->valueint == 1)
        {
            nvs_handle_t handle;
            nvs_open(STORAGE_NAME_SPACE, NVS_READWRITE, &handle);
            nvs_erase_all(handle);
            nvs_commit(handle);
            nvs_close(handle);

            esp_vfs_spiffs_conf_t conf = {
                .base_path = "/spiffs",
                .partition_label = NULL,
                .max_files = 10,
                .format_if_mount_failed = true};
            esp_vfs_spiffs_register(&conf);

            unlink("/spiffs/url.txt");
            unlink("/spiffs/client.txt");
            unlink("/spiffs/username.txt");
            unlink("/spiffs/password.txt");
            unlink("/spiffs/port.txt");
            unlink("/spiffs/led_color.dat");
            esp_vfs_spiffs_unregister(conf.partition_label);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            ESP_LOGI(TAG, "reset the all,wifi,mqtt,and led_state");
            return reset_all;
        };

    cJSON *js_index = cJSON_GetObjectItemCaseSensitive(json, "index");
    if (js_index != NULL && cJSON_IsNumber(js_index))
    {
        ESP_LOGI(TAG, "index: %d\n", js_index->valueint);
    }
    cJSON *js_brightness = cJSON_GetObjectItemCaseSensitive(json, "brightness");
    cJSON *js_red = cJSON_GetObjectItemCaseSensitive(json, "red");
    cJSON *js_green = cJSON_GetObjectItemCaseSensitive(json, "green");
    cJSON *js_blue = cJSON_GetObjectItemCaseSensitive(json, "blue");
    set_led_color(json_led_strip, (js_index->valueint), (uint32_t)js_red->valueint, (uint32_t)js_green->valueint, (uint32_t)js_blue->valueint, js_brightness->valueint, true);
    return set_led;
}