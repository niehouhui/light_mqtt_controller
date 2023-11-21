#ifndef _ws2812b_h
#define _ws2812b_h

#include <stdio.h>
#include "unistd.h"
#include "led_strip.h"
#include "led_strip_rmt.h"

led_strip_handle_t led_strip_init(uint32_t led_length);
void led_strip_deinit(led_strip_handle_t led_strip);
void save_led_length(uint32_t led_length);
uint32_t get_led_length();
void set_led_color(led_strip_handle_t led_strip, uint32_t index, uint32_t red, uint32_t green, uint32_t blue, uint32_t brightness, bool is_save);
void set_leds_color_from_spiffs(led_strip_handle_t led_strip);
void led_started_show_redlight(led_strip_handle_t led_strip);
void wifi_connected_show_yellowlight(led_strip_handle_t led_strip);
void mqtt_connected_show_greenlight(led_strip_handle_t led_strip);
void tcp_connected_set_mqtt_show_bluelight(led_strip_handle_t led_strip);

#endif