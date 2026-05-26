/**
 * @file main.cpp
 * @brief CacaOS entry point. Wires hardware init, LVGL, and the UI router.
 *
 * Boot order matters:
 *   1. Serial (debug)
 *   2. NVS storage (so we can read saved prefs)
 *   3. SD card (for assets — degrades gracefully if missing)
 *   4. Display
 *   5. LVGL core
 *   6. Touch (depends on display init for shared SPI bus settings)
 *   7. Theme (LVGL styles applied)
 *   8. WiFi (async — don't block boot on this)
 *   9. UI: splash → homescreen
 */

#include <Arduino.h>
#include <lvgl.h>

#include "config.h"
#include "system/display.h"
#include "system/touch.h"
#include "system/sdcard.h"
#include "system/storage.h"
#include "system/wifi_mgr.h"
#include "system/weather.h"
#include "ui/theme.h"
#include "ui/nav.h"
#include "ui/homescreen.h"

static uint32_t s_last_tick_ms = 0;
static uint32_t s_last_weather_ms = 0;

static constexpr uint32_t WEATHER_REFRESH_INTERVAL_MS = 30UL * 60UL * 1000UL; // 30min

static void show_splash(void);

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println(F("============================================="));
    Serial.println(F("  CacaOS booting..."));
    Serial.println(F("============================================="));

    // 1. NVS (non-volatile prefs)
    storage_init();

    // 2. SD card (best-effort — system works without it, just degraded)
    if (!sdcard_init()) {
        Serial.println(F("[boot] SD card not detected. Continuing without it."));
    }

    // 3. Display + LVGL
    display_init();
    lv_init();
    display_register_with_lvgl();

    // 4. Touch (after display because shared SPI configuration)
    touch_init();
    touch_register_with_lvgl();

    // 5. Theme (LVGL styles registered globally)
    theme_init();

    // 6. Splash while WiFi connects in background
    show_splash();
    lv_timer_handler();

    // 7. WiFi async connect — won't block boot
    wifi_mgr_begin();

    // 8. Build homescreen and route to it
    nav_init();
    homescreen_show();

    s_last_tick_ms = millis();
    Serial.println(F("[boot] CacaOS ready."));
}

void loop() {
    // LVGL tick handler — must be called periodically.
    uint32_t now = millis();
    uint32_t delta = now - s_last_tick_ms;
    if (delta > 0) {
        lv_tick_inc(delta);
        s_last_tick_ms = now;
    }
    lv_timer_handler();

    // Background services (non-blocking polling)
    wifi_mgr_loop();

    if (wifi_mgr_is_connected()) {
        if (now - s_last_weather_ms > WEATHER_REFRESH_INTERVAL_MS || s_last_weather_ms == 0) {
            weather_refresh_async();
            s_last_weather_ms = now;
        }
    }

    delay(5); // keep loop responsive but don't busy-wait
}

// ----------------------------------------------------------------
static void show_splash(void) {
    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, theme_color_bg(), LV_PART_MAIN);

    lv_obj_t* label = lv_label_create(scr);
    lv_label_set_text(label, "CacaOS\n" LV_SYMBOL_DRIVE " carregando...");
    lv_obj_set_style_text_color(label, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_center(label);
}
