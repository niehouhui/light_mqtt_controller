#ifndef _ws2812b_h
#define _ws2812b_h

#include <stdio.h>
#include "unistd.h"

#define LEDS 12                                              // led灯数
#define BLINK_GPIO 7                                         // 定义一个IO，用于WS2812通信


void setPixelColor(int pixel, uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness);
void ws2812b_start();
void get_led_state_from_nvs();
// void ws2812b_reset();


#endif