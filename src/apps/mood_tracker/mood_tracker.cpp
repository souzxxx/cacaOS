/**
 * @file mood_tracker.cpp
 * @brief 5-emoji daily mood + 7-day heatmap. Persisted in NVS namespace "mood".
 *
 * NVS keys: "YYYYMMDD" -> uint8 (1..5, 0 = not set)
 */

#include "mood_tracker.h"

#include <Arduino.h>
#include <Preferences.h>
#include <lvgl.h>
#include <time.h>

#include "../../ui/theme.h"
#include "../../ui/nav.h"

static lv_obj_t* s_today_label = nullptr;
static lv_obj_t* s_heatmap_row = nullptr;
static lv_obj_t* s_buttons[5]  = {nullptr};
static uint8_t   s_today_mood  = 0;

// Mood meta: label + color
static const char* MOOD_EMOJI[5] = { ":D", ":)", ":|", ":(", ":/" };
static const char* MOOD_LABEL[5] = { "feliz", "ok", "mais ou menos", "triste", "irritada" };
static uint32_t MOOD_COLOR_HEX[5] = {
    0xB5E48C, // feliz   - green
    0xC9F0DA, // ok      - light green
    0xE8E8E8, // meh     - grey
    0xA8C8FF, // triste  - blue
    0xFFC09F, // irritada - peach/red
};

static void today_key(char* out, size_t out_sz) {
    time_t now = time(nullptr);
    struct tm tm_local;
    localtime_r(&now, &tm_local);
    strftime(out, out_sz, "%Y%m%d", &tm_local);
}

static void key_for_offset(char* out, size_t out_sz, int days_back) {
    time_t now = time(nullptr) - (time_t)days_back * 86400;
    struct tm tm_local;
    localtime_r(&now, &tm_local);
    strftime(out, out_sz, "%Y%m%d", &tm_local);
}

static uint8_t load_mood(const char* key) {
    Preferences prefs;
    if (!prefs.begin("mood", true)) return 0;
    uint8_t v = prefs.getUChar(key, 0);
    prefs.end();
    return v;
}

static void save_mood(const char* key, uint8_t value) {
    Preferences prefs;
    if (!prefs.begin("mood", false)) return;
    prefs.putUChar(key, value);
    prefs.end();
}

static void refresh_button_states(void) {
    for (int i = 0; i < 5; ++i) {
        if (!s_buttons[i]) continue;
        bool selected = (s_today_mood == (uint8_t)(i + 1));
        lv_obj_set_style_border_width(s_buttons[i], selected ? 3 : 0, LV_PART_MAIN);
        lv_obj_set_style_border_color(s_buttons[i], theme_color_accent(), LV_PART_MAIN);
    }
}

static void refresh_today_label(void) {
    if (!s_today_label) return;
    if (s_today_mood == 0) {
        lv_label_set_text(s_today_label, "como ta hoje?");
    } else {
        char buf[40];
        snprintf(buf, sizeof(buf), "hoje voce ta %s", MOOD_LABEL[s_today_mood - 1]);
        lv_label_set_text(s_today_label, buf);
    }
}

static void mood_clicked_cb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    intptr_t idx = (intptr_t)lv_obj_get_user_data(btn);
    s_today_mood = (uint8_t)(idx + 1);

    char key[12];
    today_key(key, sizeof(key));
    save_mood(key, s_today_mood);

    refresh_button_states();
    refresh_today_label();
}

static void back_event_cb(lv_event_t* /*e*/) {
    s_today_label = nullptr;
    s_heatmap_row = nullptr;
    for (int i = 0; i < 5; ++i) s_buttons[i] = nullptr;
    nav_pop(NAV_ANIM_SLIDE_RIGHT);
}

static void make_heatmap_cells(lv_obj_t* parent) {
    // 7 cols x 5 rows = 35 days. Today is bottom-right; oldest at top-left.
    constexpr int COLS = 7;
    constexpr int ROWS = 5;
    constexpr int CELL = 24;
    constexpr int GAP  = 4;

    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLS; ++col) {
            int days_back = ((ROWS - 1) * COLS + (COLS - 1)) - (row * COLS + col);
            char key[12];
            key_for_offset(key, sizeof(key), days_back);
            uint8_t m = load_mood(key);

            lv_obj_t* cell = lv_obj_create(parent);
            lv_obj_set_size(cell, CELL, CELL);
            lv_obj_set_pos(cell, col * (CELL + GAP), row * (CELL + GAP));
            lv_obj_set_style_radius(cell, 5, LV_PART_MAIN);
            lv_obj_set_style_border_width(cell, 0, LV_PART_MAIN);
            lv_obj_set_style_shadow_width(cell, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(cell, 0, LV_PART_MAIN);
            lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

            if (m >= 1 && m <= 5) {
                lv_obj_set_style_bg_color(cell, lv_color_hex(MOOD_COLOR_HEX[m - 1]), LV_PART_MAIN);
            } else {
                lv_obj_set_style_bg_color(cell, theme_color_text_light(), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(cell, LV_OPA_30, LV_PART_MAIN);
            }

            // Subtle outline on today's cell
            if (days_back == 0) {
                lv_obj_set_style_border_width(cell, 2, LV_PART_MAIN);
                lv_obj_set_style_border_color(cell, theme_color_accent(), LV_PART_MAIN);
            }
        }
    }
}

void mood_tracker_show(void) {
    char key[12];
    today_key(key, sizeof(key));
    s_today_mood = load_mood(key);

    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, theme_color_bg(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Header
    lv_obj_t* header = lv_obj_create(scr);
    lv_obj_set_size(header, 240, 40);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, theme_color_primary(), LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* back_btn = lv_button_create(header);
    lv_obj_set_size(back_btn, 36, 28);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_bg_color(back_btn, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_radius(back_btn, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(back_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(back_btn, back_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_lbl, theme_color_accent(), LV_PART_MAIN);
    lv_obj_center(back_lbl);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Humor");
    lv_obj_set_style_text_color(title, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Today header label
    s_today_label = lv_label_create(scr);
    lv_obj_align(s_today_label, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_style_text_color(s_today_label, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_today_label, &lv_font_montserrat_18, LV_PART_MAIN);
    refresh_today_label();

    // Mood buttons (5 horizontal)
    constexpr int BTN_SIZE = 42;
    constexpr int BTN_GAP  = 6;
    constexpr int row_width = BTN_SIZE * 5 + BTN_GAP * 4;
    int start_x = (240 - row_width) / 2;
    int row_y = 100;

    for (int i = 0; i < 5; ++i) {
        lv_obj_t* btn = lv_button_create(scr);
        lv_obj_set_size(btn, BTN_SIZE, BTN_SIZE);
        lv_obj_set_pos(btn, start_x + i * (BTN_SIZE + BTN_GAP), row_y);
        lv_obj_set_style_bg_color(btn, lv_color_hex(MOOD_COLOR_HEX[i]), LV_PART_MAIN);
        lv_obj_set_style_radius(btn, BTN_SIZE / 2, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
        lv_obj_set_user_data(btn, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, mood_clicked_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t* face = lv_label_create(btn);
        lv_label_set_text(face, MOOD_EMOJI[i]);
        lv_obj_set_style_text_color(face, theme_color_text(), LV_PART_MAIN);
        lv_obj_set_style_text_font(face, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_center(face);

        s_buttons[i] = btn;
    }
    refresh_button_states();

    // History label
    lv_obj_t* hist_label = lv_label_create(scr);
    lv_label_set_text(hist_label, "ultimos 35 dias");
    lv_obj_set_style_text_color(hist_label, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(hist_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(hist_label, LV_ALIGN_TOP_MID, 0, 158);

    // Heatmap container — 7 cols x 5 rows = 192 wide, 136 tall
    s_heatmap_row = lv_obj_create(scr);
    lv_obj_set_size(s_heatmap_row, 192, 136);
    lv_obj_align(s_heatmap_row, LV_ALIGN_TOP_MID, 0, 180);
    lv_obj_set_style_bg_opa(s_heatmap_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_heatmap_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_heatmap_row, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_heatmap_row, LV_OBJ_FLAG_SCROLLABLE);
    make_heatmap_cells(s_heatmap_row);

    nav_push(scr, NAV_ANIM_SLIDE_LEFT);
}
