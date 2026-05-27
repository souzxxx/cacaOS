/**
 * @file rgb_led_sim.cpp
 * @brief No-op RGB LED. Could be visualized but not worth the LVGL surface
 *        cost for a 3-channel hint.
 */

#include "../system/rgb_led.h"
#include <Arduino.h>

void rgb_led_init(void) {
    Serial.println("[rgb:sim] no-op");
}
void rgb_led_set(uint8_t r, uint8_t g, uint8_t b) {
    Serial.printf("[rgb:sim] set %u,%u,%u\n", r, g, b);
}
void rgb_led_off(void) {}
