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
#include "system/rgb_led.h"
#include "system/ldr.h"
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

    // 0. RGB LED (used as boot indicator + per-app feedback)
    rgb_led_init();
    rgb_led_set(255, 143, 171);  // boot in pink (matches theme primary)

    // 1. NVS (non-volatile prefs)
    storage_init();

    // 2. SD card (best-effort — system works without it, just degraded)
    if (!sdcard_init()) {
        Serial.println(F("[boot] SD card not detected. Continuing without it."));
    }

    // 3. Display + LVGL
    display_init();
    display_set_brightness(storage_get_brightness(255));
    ldr_init();
    lv_init();
    display_register_with_lvgl();

    // 4. Touch (after display because shared SPI configuration)
    touch_init();
    touch_register_with_lvgl();

    // 5. Theme (LVGL styles registered globally)
    theme_init();

    // 5b. First-boot touch calibration. Skips if NVS already has values.
    if (!touch_is_calibrated()) {
        Serial.println(F("[boot] touch not calibrated — running 4-point flow"));
        touch_calibrate();
    }

    // 6. Splash while WiFi connects in background
    show_splash();
    lv_timer_handler();

    // 7. WiFi async connect — won't block boot
    wifi_mgr_begin();

    // 8. Build homescreen and route to it
    nav_init();
    homescreen_show();

    s_last_tick_ms = millis();
    rgb_led_off();   // boot complete; apps can claim the LED
    Serial.println(F("[boot] CacaOS ready."));

    // Heap baseline after full init. LVGL now allocates from this general heap
    // (CLIB allocator) instead of a fixed static pool — these numbers show how
    // much room apps actually have. largest_block matters for fragmentation:
    // the keyboard/picker need a few contiguous KB.
    Serial.printf("[heap] post-boot: free=%u largest_block=%u min_ever=%u\n",
                  ESP.getFreeHeap(), ESP.getMaxAllocHeap(), ESP.getMinFreeHeap());
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
    ldr_loop();

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

    // Soft rounded card in the middle
    lv_obj_t* card = lv_obj_create(scr);
    lv_obj_set_size(card, 220, 240);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 24, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(card, 0, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // Try to show the white bunny idle sprite from SD as a logo (frame 0)
    lv_obj_t* bunny = lv_image_create(card);
    lv_image_set_src(bunny, "S:/tamagotchi_sprites/cacaos_pet_assets/pets/white/idle.png");
    lv_obj_set_size(bunny, 32, 32);
    lv_image_set_scale(bunny, 1024);            // 4x = 128x128 visible area
    lv_image_set_inner_align(bunny, LV_IMAGE_ALIGN_TOP_LEFT);
    lv_obj_align(bunny, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, "CacaOS");
    lv_obj_set_style_text_color(title, theme_color_accent(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_BOTTOM_MID, 0, -38);

    lv_obj_t* sub = lv_label_create(card);
    lv_label_set_text(sub, "carregando...");
    lv_obj_set_style_text_color(sub, theme_color_text_light(), LV_PART_MAIN);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(sub, LV_ALIGN_BOTTOM_MID, 0, -14);
}
