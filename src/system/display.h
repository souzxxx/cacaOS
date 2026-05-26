#pragma once

#include <stdint.h>

/**
 * Initialize the TFT_eSPI display driver for ILI9341 on CYD.
 * Configures backlight, rotation (portrait 240x320), and clears screen.
 */
void display_init(void);

/**
 * Register the display with LVGL. Must be called AFTER lv_init().
 * Creates LVGL display object with flush callback.
 */
void display_register_with_lvgl(void);

/**
 * Set backlight brightness (0-255). PWM on GPIO 21.
 * Uses LEDC channel 0.
 */
void display_set_brightness(uint8_t value);

/**
 * Turn display on/off (backlight).
 */
void display_sleep(bool sleep);
