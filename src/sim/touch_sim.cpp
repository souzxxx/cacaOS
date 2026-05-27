/**
 * @file touch_sim.cpp
 * @brief SDL mouse pointer mapped to the LVGL display.
 *        Replaces src/system/touch.cpp in env:sim.
 */

#include "../system/touch.h"

#include <Arduino.h>
#include <lvgl.h>

static lv_indev_t* s_mouse = nullptr;

void touch_init(void) {
    Serial.println("[touch:sim] init ok (SDL mouse)");
}

void touch_register_with_lvgl(void) {
    s_mouse = lv_sdl_mouse_create();
    Serial.println("[touch:sim] registered with LVGL");
}

bool touch_is_calibrated(void) {
    // No calibration needed when input is already pixel-accurate.
    return true;
}

bool touch_calibrate(void) {
    // Skip — return success so the boot flow doesn't sit on calibration.
    return true;
}

void touch_reset_calibration(void) {
    // No-op in sim.
}
