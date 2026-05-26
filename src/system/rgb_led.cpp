/**
 * @file rgb_led.cpp
 * @brief On-board RGB LED driver (active LOW, LEDC-PWM).
 */

#include "rgb_led.h"

#include <Arduino.h>

static constexpr int RGB_R_PIN = 4;
static constexpr int RGB_G_PIN = 16;
static constexpr int RGB_B_PIN = 17;

static constexpr uint8_t  CH_R = 1;
static constexpr uint8_t  CH_G = 2;
static constexpr uint8_t  CH_B = 3;
static constexpr uint32_t LED_FREQ_HZ = 5000;
static constexpr uint8_t  LED_RES_BITS = 8;

void rgb_led_init(void) {
    ledcSetup(CH_R, LED_FREQ_HZ, LED_RES_BITS);
    ledcSetup(CH_G, LED_FREQ_HZ, LED_RES_BITS);
    ledcSetup(CH_B, LED_FREQ_HZ, LED_RES_BITS);
    ledcAttachPin(RGB_R_PIN, CH_R);
    ledcAttachPin(RGB_G_PIN, CH_G);
    ledcAttachPin(RGB_B_PIN, CH_B);
    rgb_led_off();
}

void rgb_led_set(uint8_t r, uint8_t g, uint8_t b) {
    // Active LOW: 0 duty = full ON, 255 duty = OFF. Invert from intuitive 0..255.
    ledcWrite(CH_R, (uint32_t)(255 - r));
    ledcWrite(CH_G, (uint32_t)(255 - g));
    ledcWrite(CH_B, (uint32_t)(255 - b));
}

void rgb_led_off(void) {
    ledcWrite(CH_R, 255);
    ledcWrite(CH_G, 255);
    ledcWrite(CH_B, 255);
}
