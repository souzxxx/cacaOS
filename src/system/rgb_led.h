#pragma once

#include <stdint.h>

/**
 * On-board RGB LED on the ESP32-2432S028R (pins from PLAN.md).
 *   R = GPIO 4, G = GPIO 16, B = GPIO 17 — all active LOW.
 *
 * We drive each channel via LEDC PWM (channels 1..3; channel 0 is the
 * display backlight). 8-bit resolution; 0 = off, 255 = full intensity
 * — the active-LOW inversion happens inside the module.
 */
void rgb_led_init(void);

/** Set all three channels at once. r/g/b in 0..255, 0 = off. */
void rgb_led_set(uint8_t r, uint8_t g, uint8_t b);

/** Convenience: turn LED off. */
void rgb_led_off(void);
