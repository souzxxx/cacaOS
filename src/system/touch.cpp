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

// Render-only helper: forces LVGL to draw whatever is currently on the
// active screen. Used between calibration steps so the moved target
// is visible before we block on wait_for_touch().
static void force_render(void) {
    lv_refr_now(NULL);
}

bool touch_calibrate(void) {
    Serial.println(F("[touch] starting 4-point calibration"));

    lv_obj_t* scr = lv_screen_active();
    if (!scr) {
        Serial.println(F("[touch] no active screen; aborting"));
        return false;
    }
    lv_obj_set_style_bg_color(scr, lv_color_white(), LV_PART_MAIN);

    lv_obj_t* instr = lv_label_create(scr);
    lv_label_set_long_mode(instr, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(instr, 200);
    lv_obj_set_style_text_align(instr, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(instr, lv_color_make(0, 0, 0), LV_PART_MAIN);
    lv_label_set_text(instr, "Toque os 4 pontos\nrosa pra calibrar");
    lv_obj_align(instr, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* target = lv_obj_create(scr);
    lv_obj_set_size(target, 24, 24);
    lv_obj_set_style_radius(target, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(target, lv_color_make(0xFB, 0x6F, 0x92), LV_PART_MAIN);
    lv_obj_set_style_border_width(target, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(target, 0, LV_PART_MAIN);
    lv_obj_clear_flag(target, LV_OBJ_FLAG_SCROLLABLE);

    CalPoint cps[4] = {
        {  20,  20, 0, 0 },
        { 220,  20, 0, 0 },
        { 220, 300, 0, 0 },
        {  20, 300, 0, 0 },
    };

    for (int i = 0; i < 4; ++i) {
        lv_obj_set_pos(target, cps[i].screen_x - 12, cps[i].screen_y - 12);
        force_render();

        TS_Point p;
        if (!wait_for_touch(20000, &p)) {
            Serial.println(F("[touch] calibration timeout; keeping defaults"));
            lv_obj_delete(instr);
            lv_obj_delete(target);
            return false;
        }
        cps[i].raw_x = p.x;
        cps[i].raw_y = p.y;
        Serial.printf("[touch] cal pt %d: screen=(%d,%d) raw=(%d,%d)\n",
                      i, cps[i].screen_x, cps[i].screen_y, cps[i].raw_x, cps[i].raw_y);
        delay(250);
    }

    // Average raw values at each rail
    int16_t avg_raw_x_left  = (cps[0].raw_x + cps[3].raw_x) / 2;
    int16_t avg_raw_x_right = (cps[1].raw_x + cps[2].raw_x) / 2;
    int16_t avg_raw_y_top   = (cps[0].raw_y + cps[1].raw_y) / 2;
    int16_t avg_raw_y_bot   = (cps[2].raw_y + cps[3].raw_y) / 2;

    if (avg_raw_x_right == avg_raw_x_left || avg_raw_y_bot == avg_raw_y_top) {
        Serial.println(F("[touch] degenerate calibration values; aborting"));
        lv_obj_delete(instr);
        lv_obj_delete(target);
        return false;
    }

    // screen_x = (raw_x - off_x) * scale_x; same form for Y.
    // Distance between marker rails is (220 - 20) = 200 on X, (300 - 20) = 280 on Y.
    float scale_x = 200.0f / (float)(avg_raw_x_right - avg_raw_x_left);
    float off_x_f = (float)avg_raw_x_left - (20.0f / scale_x);
    float scale_y = 280.0f / (float)(avg_raw_y_bot - avg_raw_y_top);
    float off_y_f = (float)avg_raw_y_top - (20.0f / scale_y);

    s_cal.off_x   = (int16_t)off_x_f;
    s_cal.off_y   = (int16_t)off_y_f;
    s_cal.scale_x = scale_x;
    s_cal.scale_y = scale_y;
    s_cal.valid   = true;
    save_calibration_to_nvs();

    // Brief "OK" feedback
    lv_label_set_text(instr, "ok!");
    lv_obj_align(instr, LV_ALIGN_CENTER, 0, 0);
    lv_obj_delete(target);
    force_render();
    delay(800);

    lv_obj_delete(instr);
    return true;
}
