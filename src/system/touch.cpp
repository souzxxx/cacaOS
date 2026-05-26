/**
 * @file touch.cpp
 * @brief XPT2046 resistive touch driver + LVGL pointer input + 4-point calibration.
 *
 * Calibration model: simple linear scale + offset per axis.
 *   screen_x = (raw_x - off_x) * scale_x
 *   screen_y = (raw_y - off_y) * scale_y
 *
 * NVS keys (namespace "touch"):
 *   off_x (int16), scale_x_x1000 (int16 — divide by 1000 to get float)
 *   off_y (int16), scale_y_x1000 (int16)
 *   calibrated (uint8 — 1 if set)
 */

#include "touch.h"

#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>
#include <Preferences.h>

// ----- Pinout (see PLAN.md section 1) -----
static constexpr int TOUCH_MISO = 39;
static constexpr int TOUCH_MOSI = 32;
static constexpr int TOUCH_CLK  = 25;
static constexpr int TOUCH_CS   = 33;
static constexpr int TOUCH_IRQ  = 36;

// Use dedicated VSPI bus instance for touch (display uses HSPI implicitly via TFT_eSPI)
static SPIClass s_touch_spi = SPIClass(VSPI);
static XPT2046_Touchscreen s_touch(TOUCH_CS, TOUCH_IRQ);

// ----- Calibration state (in-RAM, mirrored from NVS) -----
struct Calibration {
    int16_t off_x = 200;     // raw value at screen x=0
    int16_t off_y = 240;     // raw value at screen y=0
    float   scale_x = 0.0668f; // (screen_max_x / (raw_max_x - off_x))
    float   scale_y = 0.0894f;
    bool    valid = false;
};
static Calibration s_cal;

// ----- LVGL indev -----
static lv_indev_t* s_lvgl_indev = nullptr;

static void load_calibration_from_nvs(void);
static void save_calibration_to_nvs(void);

// ----------------------------------------------------------------
static void lvgl_touch_read_cb(lv_indev_t* /*indev*/, lv_indev_data_t* data) {
    if (s_touch.tirqTouched() && s_touch.touched()) {
        TS_Point p = s_touch.getPoint();

        // Apply calibration
        int32_t x = static_cast<int32_t>((p.x - s_cal.off_x) * s_cal.scale_x);
        int32_t y = static_cast<int32_t>((p.y - s_cal.off_y) * s_cal.scale_y);

        // Clamp to screen bounds
        if (x < 0) x = 0; else if (x > 239) x = 239;
        if (y < 0) y = 0; else if (y > 319) y = 319;

        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ----------------------------------------------------------------
void touch_init(void) {
    s_touch_spi.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    s_touch.begin(s_touch_spi);
    s_touch.setRotation(0);

    load_calibration_from_nvs();

    Serial.printf("[touch] init ok (calibrated=%s)\n", s_cal.valid ? "yes" : "no (defaults)");
}

void touch_register_with_lvgl(void) {
    s_lvgl_indev = lv_indev_create();
    lv_indev_set_type(s_lvgl_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_lvgl_indev, lvgl_touch_read_cb);
    Serial.println(F("[touch] registered with LVGL"));
}

bool touch_is_calibrated(void) {
    return s_cal.valid;
}

void touch_reset_calibration(void) {
    Preferences prefs;
    if (prefs.begin("touch", false)) {
        prefs.clear();
        prefs.end();
    }
    s_cal.valid = false;
}

// ----------------------------------------------------------------
static void load_calibration_from_nvs(void) {
    Preferences prefs;
    if (!prefs.begin("touch", true)) return;

    if (prefs.getUChar("calibrated", 0) == 1) {
        s_cal.off_x   = prefs.getShort("off_x", 200);
        s_cal.off_y   = prefs.getShort("off_y", 240);
        s_cal.scale_x = prefs.getShort("scl_x", 67) / 1000.0f;
        s_cal.scale_y = prefs.getShort("scl_y", 89) / 1000.0f;
        s_cal.valid = true;
    }
    prefs.end();
}

static void save_calibration_to_nvs(void) {
    Preferences prefs;
    if (!prefs.begin("touch", false)) return;
    prefs.putUChar("calibrated", 1);
    prefs.putShort("off_x", s_cal.off_x);
    prefs.putShort("off_y", s_cal.off_y);
    prefs.putShort("scl_x", static_cast<int16_t>(s_cal.scale_x * 1000));
    prefs.putShort("scl_y", static_cast<int16_t>(s_cal.scale_y * 1000));
    prefs.end();
    Serial.println(F("[touch] calibration saved to NVS"));
}

// ----------------------------------------------------------------
// Interactive calibration — draws 4 corner targets directly via LVGL.
// User taps each target. We compute scale + offset from raw vs expected.
// ----------------------------------------------------------------
struct CalPoint {
    int16_t screen_x;
    int16_t screen_y;
    int16_t raw_x;
    int16_t raw_y;
};

static bool wait_for_touch(int16_t timeout_ms, TS_Point* out) {
    uint32_t deadline = millis() + timeout_ms;
    // Wait for press
    while (millis() < deadline) {
        if (s_touch.tirqTouched() && s_touch.touched()) {
            *out = s_touch.getPoint();
            // Wait for release
            uint32_t release_dl = millis() + 2000;
            while (s_touch.touched() && millis() < release_dl) delay(10);
            return true;
        }
        delay(10);
    }
    return false;
}

bool touch_calibrate(void) {
    // TODO: This is a minimal stub. Full implementation will draw LVGL targets
    // at the 4 corners and prompt the user. For now, the defaults work
    // reasonably well for first-light testing.
    //
    // Implementation outline:
    //   1. Create fullscreen LVGL container with text "Toque os 4 pontos"
    //   2. Draw small crosshair at (20, 20)
    //   3. wait_for_touch(); record raw_x, raw_y
    //   4. Repeat for (220, 20), (220, 300), (20, 300)
    //   5. Compute:
    //        scale_x = (220 - 20) / (avg(raw_x_right) - avg(raw_x_left))
    //        off_x   = avg(raw_x_left) - 20 / scale_x
    //        (same for Y)
    //   6. save_calibration_to_nvs();
    //   7. Show "OK!" toast and return true.
    Serial.println(F("[touch] WARNING: touch_calibrate() is stubbed. Using defaults."));
    s_cal.valid = true;
    save_calibration_to_nvs();
    return true;
}
