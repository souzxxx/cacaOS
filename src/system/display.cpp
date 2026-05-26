/**
 * @file display.cpp
 * @brief TFT_eSPI driver init + LVGL flush callback for ILI9341 on CYD.
 */

#include "display.h"

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

// Static TFT instance — configured via build flags in platformio.ini
static TFT_eSPI s_tft = TFT_eSPI();

// LVGL display object
static lv_display_t* s_lvgl_display = nullptr;

// Double buffer for LVGL — sized to 1/10 of screen.
// 240 * 32 = 7680 pixels * 2 bytes (RGB565) = 15360 bytes per buffer.
// We use two buffers for DMA-style ping-pong updates.
static constexpr uint32_t LVGL_BUF_LINES = 40;
static constexpr uint32_t LVGL_BUF_SIZE  = 240 * LVGL_BUF_LINES;

static lv_color_t s_buf_a[LVGL_BUF_SIZE];
static lv_color_t s_buf_b[LVGL_BUF_SIZE];

// Backlight PWM on GPIO 21
static constexpr int      BL_PIN     = 21;
static constexpr uint8_t  BL_CHANNEL = 0;
static constexpr uint32_t BL_FREQ    = 5000;
static constexpr uint8_t  BL_RES_BITS = 8;

// ----------------------------------------------------------------
static void IRAM_ATTR lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    s_tft.startWrite();
    s_tft.setAddrWindow(area->x1, area->y1, w, h);
    s_tft.pushPixels(reinterpret_cast<uint16_t*>(px_map), w * h);
    s_tft.endWrite();

    lv_display_flush_ready(disp);
}

// ----------------------------------------------------------------
void display_init(void) {
    s_tft.init();
    s_tft.setRotation(0);   // 0 = portrait 240x320, USB on bottom
    s_tft.fillScreen(TFT_BLACK);

    // Backlight via LEDC PWM (full brightness initially)
    ledcSetup(BL_CHANNEL, BL_FREQ, BL_RES_BITS);
    ledcAttachPin(BL_PIN, BL_CHANNEL);
    ledcWrite(BL_CHANNEL, 255);

    Serial.printf("[display] init ok, %dx%d\n", s_tft.width(), s_tft.height());
}

void display_register_with_lvgl(void) {
    s_lvgl_display = lv_display_create(240, 320);
    lv_display_set_flush_cb(s_lvgl_display, lvgl_flush_cb);
    lv_display_set_buffers(
        s_lvgl_display,
        s_buf_a,
        s_buf_b,
        LVGL_BUF_SIZE * sizeof(lv_color_t),
        LV_DISPLAY_RENDER_MODE_PARTIAL
    );
    Serial.println(F("[display] registered with LVGL"));
}

void display_set_brightness(uint8_t value) {
    ledcWrite(BL_CHANNEL, value);
}

void display_sleep(bool sleep) {
    ledcWrite(BL_CHANNEL, sleep ? 0 : 255);
}
