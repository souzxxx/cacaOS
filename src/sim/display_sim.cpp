/**
 * @file display_sim.cpp
 * @brief SDL window driver. Replaces src/system/display.cpp in env:sim.
 *        Same public API (display.h) so the rest of the codebase is unaware.
 */

#include "../system/display.h"

#include <Arduino.h>
#include <lvgl.h>
#include <SDL2/SDL.h>

static lv_display_t* s_lvgl_display = nullptr;

void display_init(void) {
    // SDL_Init is called once by lv_sdl_window_create internally, but we
    // ensure the video subsystem is ready so SDL_Delay etc. work even
    // before LVGL spins up its window.
    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    }
    Serial.println("[display:sim] init ok (SDL2)");
}

void display_register_with_lvgl(void) {
    s_lvgl_display = lv_sdl_window_create(240, 320);
    lv_sdl_window_set_title(s_lvgl_display, "CacaOS — sim");
    Serial.println("[display:sim] registered with LVGL");
}

void display_set_brightness(uint8_t /*value*/) {
    // No-op in sim — could lerp a fade overlay in LVGL but not worth it.
}

void display_sleep(bool /*sleep*/) {
    // No-op in sim.
}
