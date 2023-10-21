#ifndef _ws2812b_h
#define _ws2812b_h

#include <stdio.h>
#include "unistd.h"




bool led_strip_config();
bool led_strip_rm();
bool led_display();
bool set_led_color(int index, uint32_t red, uint32_t green, uint32_t blue, int brightness);
bool set_led_length(int led_lens);
bool get_led_config_from_nvs();


#endif