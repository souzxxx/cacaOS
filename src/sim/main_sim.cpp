/**
 * @file main_sim.cpp
 * @brief Native entry point. Mirrors src/main.cpp setup order but adapted
 *        for the SDL host: no SDL_PollEvent loop (LVGL's SDL driver pumps
 *        events itself), no calibration step (sim already calibrated).
 */

#include <Arduino.h>
#include <lvgl.h>
#include <SDL2/SDL.h>
#include <signal.h>

#include "../system/display.h"
#include "../system/touch.h"
#include "../system/sdcard.h"
#include "../system/storage.h"
#include "../system/wifi_mgr.h"
#include "../system/weather.h"
#include "../system/rgb_led.h"
#include "../system/ldr.h"
#include "../ui/theme.h"
#include "../ui/nav.h"
#include "../ui/homescreen.h"

static volatile sig_atomic_t s_keep_running = 1;
static void on_sigint(int) { s_keep_running = 0; }

static constexpr uint32_t WEATHER_REFRESH_INTERVAL_MS = 30UL * 60UL * 1000UL;

static void show_splash(void) {
    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, theme_color_bg(), LV_PART_MAIN);

    lv_obj_t* card = lv_obj_create(scr);
    lv_obj_set_size(card, 220, 240);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 24, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(card, 0, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, "CacaOS — sim");
    lv_obj_set_style_text_color(title, theme_color_accent(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_BOTTOM_MID, 0, -38);

    lv_obj_t* sub = lv_label_create(card);
    lv_label_set_text(sub, "carregando...");
    lv_obj_set_style_text_color(sub, theme_color_text_light(), LV_PART_MAIN);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(sub, LV_ALIGN_BOTTOM_MID, 0, -14);
}

int main(int /*argc*/, char** /*argv*/) {
    signal(SIGINT, on_sigint);

    Serial.println("=============================================");
    Serial.println("  CacaOS booting in SIM mode...");
    Serial.println("=============================================");

    rgb_led_init();
    rgb_led_set(255, 143, 171);

    storage_init();

    if (!sdcard_init()) {
        Serial.println("[boot] sd_card/ folder missing — apps will degrade");
    }

    display_init();
    display_set_brightness(storage_get_brightness(255));
    ldr_init();

    lv_init();

    display_register_with_lvgl();
    touch_init();
    touch_register_with_lvgl();

    theme_init();

    show_splash();
    lv_timer_handler();

    wifi_mgr_begin();

    nav_init();
    homescreen_show();

    rgb_led_off();
    Serial.println("[boot] CacaOS sim ready.");

    uint32_t last_tick = millis();
    uint32_t last_weather = 0;

    while (s_keep_running) {
        uint32_t now = millis();
        uint32_t delta = now - last_tick;
        if (delta > 0) {
            lv_tick_inc(delta);
            last_tick = now;
        }
        uint32_t next_ms = lv_timer_handler();

        wifi_mgr_loop();
        ldr_loop();

        if (wifi_mgr_is_connected()) {
            if (now - last_weather > WEATHER_REFRESH_INTERVAL_MS || last_weather == 0) {
                weather_refresh_async();
                last_weather = now;
            }
        }

        // LVGL tells us how long until its next due timer. Cap the nap so
        // we stay responsive to the OS even when nothing's scheduled.
        if (next_ms > 16) next_ms = 16;
        SDL_Delay(next_ms);
    }

    Serial.println("\n[boot] received SIGINT — bye");
    return 0;
}
