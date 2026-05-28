/**
 * @file homescreen.cpp
 * @brief Main launcher with status bar + 3x3 app grid.
 *
 * Layout (240x320):
 *   y=0..50    Status bar (time + weather + wifi)
 *   y=55..280  3x3 grid of app cards (cards ~67x70 each)
 *   y=285..320 Footer/decorative area
 */

#include "homescreen.h"
#include "theme.h"
#include "nav.h"

#include <Arduino.h>
#include <lvgl.h>
#include <time.h>

#include "../system/weather.h"
#include "../system/wifi_mgr.h"

// App show functions (forward declarations from each app)
#include "../apps/gallery/gallery.h"
#include "../apps/daily_card/daily_card.h"
#include "../apps/counter/counter.h"
#include "../apps/open_when/open_when.h"
#include "../apps/memory_game/memory_game.h"
#include "../apps/pomodoro/pomodoro.h"
#include "../apps/mood_tracker/mood_tracker.h"
#include "../apps/tamagotchi/tamagotchi.h"
#include "../apps/settings/settings.h"

static lv_obj_t* s_screen = nullptr;
static lv_obj_t* s_time_label = nullptr;
static lv_obj_t* s_weather_label = nullptr;
static lv_obj_t* s_wifi_indicator = nullptr;
static lv_timer_t* s_refresh_timer = nullptr;

struct AppEntry {
    const char* label;
    const char* symbol;            // LVGL symbol or emoji string
    uint32_t    icon_hex;          // 0xRRGGBB tint for the glyph
    void (*launch_fn)(void);       // called on tap
};

// Pastel palette per app, picked to vary while staying in the kawaii range.
static const AppEntry s_apps[] = {
    { "Galeria",   LV_SYMBOL_IMAGE,    0xFFB089, gallery_show      },  // peach
    { "Cartinha",  LV_SYMBOL_ENVELOPE, 0xFB6F92, daily_card_show   },  // accent pink
    { "Contador",  LV_SYMBOL_CHARGE,   0xE8B547, counter_show      },  // honey
    { "Cartinhas", LV_SYMBOL_FILE,     0xC8A8E9, open_when_show    },  // lavender
    { "Memory",    LV_SYMBOL_REFRESH,  0x8FD9B6, memory_game_show  },  // mint
    { "Pomodoro",  LV_SYMBOL_LOOP,     0xE76F51, pomodoro_show     },  // tomato
    { "Humor",     LV_SYMBOL_EYE_OPEN, 0x7FB3F0, mood_tracker_show },  // sky
    { "Pet",       LV_SYMBOL_HOME,     0xC58A5C, tamagotchi_show   },  // warm tan
    { "Ajustes",   LV_SYMBOL_SETTINGS, 0xA59EBC, settings_show     },  // dusk grey
};

static void update_time_label(void);
static void update_weather_label(void);
static void update_wifi_indicator(void);
static void refresh_timer_cb(lv_timer_t* /*t*/);
static void app_card_event_cb(lv_event_t* e);

void homescreen_show(void) {
    if (s_refresh_timer) {
        lv_timer_delete(s_refresh_timer);
        s_refresh_timer = nullptr;
    }

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, theme_color_bg(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_screen, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    // --- Status bar (top) ---
    lv_obj_t* status = lv_obj_create(s_screen);
    lv_obj_set_size(status, 240, 50);
    lv_obj_set_pos(status, 0, 0);
    lv_obj_set_style_bg_color(status, theme_color_primary(), LV_PART_MAIN);
    lv_obj_set_style_border_width(status, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(status, 0, LV_PART_MAIN);
    lv_obj_clear_flag(status, LV_OBJ_FLAG_SCROLLABLE);

    s_time_label = lv_label_create(status);
    lv_obj_set_style_text_color(s_time_label, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_label_set_text(s_time_label, "--:--");
    lv_obj_align(s_time_label, LV_ALIGN_LEFT_MID, 12, 0);

    s_weather_label = lv_label_create(status);
    lv_obj_set_style_text_color(s_weather_label, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_weather_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_label_set_text(s_weather_label, "--°");
    lv_obj_align(s_weather_label, LV_ALIGN_RIGHT_MID, -32, 0);

    s_wifi_indicator = lv_label_create(status);
    lv_obj_set_style_text_color(s_wifi_indicator, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_wifi_indicator, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_label_set_text(s_wifi_indicator, LV_SYMBOL_WIFI);
    lv_obj_align(s_wifi_indicator, LV_ALIGN_RIGHT_MID, -8, 0);

    // --- 3x3 app grid ---
    constexpr int CARD_W = 68;
    constexpr int CARD_H = 70;
    constexpr int GAP    = 6;
    constexpr int GRID_W = CARD_W * 3 + GAP * 2;
    constexpr int START_X = (240 - GRID_W) / 2;
    constexpr int START_Y = 60;

    for (int i = 0; i < (int)(sizeof(s_apps) / sizeof(s_apps[0])); ++i) {
        int col = i % 3;
        int row = i / 3;
        int x = START_X + col * (CARD_W + GAP);
        int y = START_Y + row * (CARD_H + GAP);

        lv_obj_t* card = lv_obj_create(s_screen);
        lv_obj_set_size(card, CARD_W, CARD_H);
        lv_obj_set_pos(card, x, y);
        lv_obj_add_style(card, &theme_style_card, LV_PART_MAIN);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(card, (void*)&s_apps[i]);
        lv_obj_add_event_cb(card, app_card_event_cb, LV_EVENT_CLICKED, NULL);

        // Symbol icon (placeholder for pixel-art icon — see TODO below)
        // TODO: replace LV_SYMBOL with custom pixel-art icon image via lv_image_create
        lv_obj_t* icon = lv_label_create(card);
        lv_label_set_text(icon, s_apps[i].symbol);
        lv_obj_set_style_text_color(icon, lv_color_hex(s_apps[i].icon_hex), LV_PART_MAIN);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, LV_PART_MAIN);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 2);

        // Label
        lv_obj_t* label = lv_label_create(card);
        lv_label_set_text(label, s_apps[i].label);
        lv_obj_add_style(label, &theme_style_caption, LV_PART_MAIN);
        lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -8);
    }

    lv_screen_load(s_screen);

    update_time_label();
    update_weather_label();
    update_wifi_indicator();

    // Refresh status bar every 30 seconds
    s_refresh_timer = lv_timer_create(refresh_timer_cb, 30000, NULL);
}

void homescreen_refresh(void) {
    update_time_label();
    update_weather_label();
    update_wifi_indicator();
}

// ----------------------------------------------------------------
static void update_time_label(void) {
    if (!s_time_label) return;
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 50)) {
        char buf[8];
        strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
        lv_label_set_text(s_time_label, buf);
    }
}

static void update_weather_label(void) {
    if (!s_weather_label) return;
    const WeatherData* w = weather_get();
    if (w->valid) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d°", w->temp_celsius);
        lv_label_set_text(s_weather_label, buf);
    } else {
        lv_label_set_text(s_weather_label, "--°");
    }
}

static void update_wifi_indicator(void) {
    if (!s_wifi_indicator) return;
    lv_obj_set_style_text_color(
        s_wifi_indicator,
        wifi_mgr_is_connected() ? theme_color_card() : theme_color_text_light(),
        LV_PART_MAIN
    );
}

static void refresh_timer_cb(lv_timer_t* /*t*/) {
    homescreen_refresh();
}

static void app_card_event_cb(lv_event_t* e) {
    lv_obj_t* card = (lv_obj_t*)lv_event_get_target(e);
    const AppEntry* entry = (const AppEntry*)lv_obj_get_user_data(card);
    if (entry && entry->launch_fn) {
        Serial.printf("[home] launching '%s'\n", entry->label);
        entry->launch_fn();
    }
}
