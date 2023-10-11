#ifndef _ws2812b_h
#define _ws2812b_h

#include <stdio.h>
#include "unistd.h"





void setPixelColor(int pixel, uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness);
void ws2812b_start();
bool get_led_state_from_nvs();
void ws2812b_reset(int led_total);


#endif