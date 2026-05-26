/**
 * @file memory_game.cpp
 * @brief 4x4 memory game with 8 color pairs. Best time persisted in NVS.
 *
 * NVS namespace "memory":
 *   best_s        (uint16) — best completion time in seconds (0 = none)
 *   plays         (uint16) — games played
 */

#include "memory_game.h"

#include <Arduino.h>
#include <Preferences.h>
#include <lvgl.h>
#include <stdlib.h>

#include "../../ui/theme.h"
#include "../../ui/nav.h"

static constexpr int GRID_COLS = 4;
static constexpr int GRID_ROWS = 4;
static constexpr int CARD_COUNT = GRID_COLS * GRID_ROWS;
static constexpr int PAIR_COUNT = CARD_COUNT / 2;

struct Card {
    uint8_t value;
    bool    revealed;
    bool    matched;
    lv_obj_t* obj;
};

static Card s_cards[CARD_COUNT];
static int  s_first_idx = -1;
static int  s_second_idx = -1;
static int  s_attempts = 0;
static int  s_matched_pairs = 0;
static uint32_t s_start_ms = 0;
static bool s_finished = false;
static lv_timer_t* s_hide_timer = nullptr;
static lv_timer_t* s_clock_timer = nullptr;

static lv_obj_t* s_attempts_label = nullptr;
static lv_obj_t* s_clock_label = nullptr;
static lv_obj_t* s_status_label = nullptr;

static const uint32_t PAIR_COLORS[PAIR_COUNT] = {
    0xFF8FAB, 0xFB6F92, 0xB5E48C, 0xA8C8FF,
    0xFFD166, 0xC4A0FF, 0xFFC09F, 0x9AE5E0,
};

static uint16_t load_best_time(void) {
    Preferences p;
    if (!p.begin("memory", true)) return 0;
    uint16_t v = p.getUShort("best_s", 0);
    p.end();
    return v;
}

static void save_best_time(uint16_t seconds) {
    Preferences p;
    if (!p.begin("memory", false)) return;
    p.putUShort("best_s", seconds);
    p.putUShort("plays", p.getUShort("plays", 0) + 1);
    p.end();
}

static void shuffle_cards(void) {
    uint8_t values[CARD_COUNT];
    for (int i = 0; i < PAIR_COUNT; ++i) {
        values[2 * i]     = i;
        values[2 * i + 1] = i;
    }
    for (int i = CARD_COUNT - 1; i > 0; --i) {
        int j = esp_random() % (i + 1);
        uint8_t tmp = values[i]; values[i] = values[j]; values[j] = tmp;
    }
    for (int i = 0; i < CARD_COUNT; ++i) {
        s_cards[i].value = values[i];
        s_cards[i].revealed = false;
        s_cards[i].matched = false;
    }
}

static void paint_card(int idx) {
    Card& c = s_cards[idx];
    if (!c.obj) return;
    if (c.revealed || c.matched) {
        lv_obj_set_style_bg_color(c.obj, lv_color_hex(PAIR_COLORS[c.value]), LV_PART_MAIN);
        lv_obj_set_style_border_width(c.obj, c.matched ? 2 : 0, LV_PART_MAIN);
        lv_obj_set_style_border_color(c.obj, theme_color_accent(), LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_color(c.obj, theme_color_primary(), LV_PART_MAIN);
        lv_obj_set_style_border_width(c.obj, 0, LV_PART_MAIN);
    }
}

static void refresh_status(void) {
    if (s_attempts_label) {
        char buf[24];
        snprintf(buf, sizeof(buf), "tentativas: %d", s_attempts);
        lv_label_set_text(s_attempts_label, buf);
    }
    if (s_status_label) {
        if (s_finished) {
            uint16_t best = load_best_time();
            uint16_t cur = (uint16_t)((millis() - s_start_ms) / 1000);
            char buf[40];
            if (best == 0 || cur < best) {
                snprintf(buf, sizeof(buf), "novo recorde! %us", (unsigned)cur);
            } else {
                snprintf(buf, sizeof(buf), "venceu em %us (recorde %us)", (unsigned)cur, (unsigned)best);
            }
            lv_label_set_text(s_status_label, buf);
        } else {
            uint16_t best = load_best_time();
            if (best > 0) {
                char buf[32];
                snprintf(buf, sizeof(buf), "recorde: %us", (unsigned)best);
                lv_label_set_text(s_status_label, buf);
            } else {
                lv_label_set_text(s_status_label, "");
            }
        }
    }
}

static void update_clock(void) {
    if (!s_clock_label || s_finished) return;
    uint32_t elapsed = (millis() - s_start_ms) / 1000;
    char buf[12];
    snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)(elapsed / 60), (unsigned)(elapsed % 60));
    lv_label_set_text(s_clock_label, buf);
}

static void clock_tick_cb(lv_timer_t* /*t*/) { update_clock(); }

static void hide_pair_cb(lv_timer_t* t) {
    if (s_first_idx >= 0)  { s_cards[s_first_idx].revealed = false;  paint_card(s_first_idx); }
    if (s_second_idx >= 0) { s_cards[s_second_idx].revealed = false; paint_card(s_second_idx); }
    s_first_idx = -1;
    s_second_idx = -1;
    lv_timer_delete(t);
    s_hide_timer = nullptr;
}

static void finish_game(void) {
    s_finished = true;
    uint16_t best = load_best_time();
    uint16_t cur  = (uint16_t)((millis() - s_start_ms) / 1000);
    if (best == 0 || cur < best) {
        save_best_time(cur);
    } else {
        Preferences p;
        if (p.begin("memory", false)) {
            p.putUShort("plays", p.getUShort("plays", 0) + 1);
            p.end();
        }
    }
    refresh_status();
}

static void card_clicked_cb(lv_event_t* e) {
    if (s_finished) return;
    if (s_hide_timer) return;

    lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
    intptr_t idx = (intptr_t)lv_obj_get_user_data(obj);
    Card& c = s_cards[idx];
    if (c.revealed || c.matched) return;

    c.revealed = true;
    paint_card(idx);

    if (s_first_idx < 0) {
        s_first_idx = idx;
        return;
    }
    if (idx == s_first_idx) return;

    s_second_idx = idx;
    s_attempts++;

    if (s_cards[s_first_idx].value == s_cards[s_second_idx].value) {
        s_cards[s_first_idx].matched = true;
        s_cards[s_second_idx].matched = true;
        paint_card(s_first_idx);
        paint_card(s_second_idx);
        s_first_idx = -1;
        s_second_idx = -1;
        s_matched_pairs++;
        if (s_matched_pairs >= PAIR_COUNT) finish_game();
    } else {
        s_hide_timer = lv_timer_create(hide_pair_cb, 800, NULL);
        lv_timer_set_repeat_count(s_hide_timer, 1);
    }
    refresh_status();
}

static void back_event_cb(lv_event_t* /*e*/) {
    if (s_hide_timer)  { lv_timer_delete(s_hide_timer);  s_hide_timer = nullptr; }
    if (s_clock_timer) { lv_timer_delete(s_clock_timer); s_clock_timer = nullptr; }
    s_attempts_label = s_clock_label = s_status_label = nullptr;
    for (int i = 0; i < CARD_COUNT; ++i) s_cards[i].obj = nullptr;
    nav_pop(NAV_ANIM_SLIDE_RIGHT);
}

void memory_game_show(void) {
    shuffle_cards();
    s_first_idx = s_second_idx = -1;
    s_attempts = 0;
    s_matched_pairs = 0;
    s_start_ms = millis();
    s_finished = false;

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
    lv_label_set_text(title, "Memory");
    lv_obj_set_style_text_color(title, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Stats row
    s_attempts_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_attempts_label, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_attempts_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_attempts_label, LV_ALIGN_TOP_LEFT, 14, 50);

    s_clock_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_clock_label, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_clock_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_clock_label, LV_ALIGN_TOP_RIGHT, -14, 50);

    // Grid
    constexpr int CARD_W = 50;
    constexpr int CARD_H = 50;
    constexpr int GAP = 4;
    constexpr int GRID_W = CARD_W * GRID_COLS + GAP * (GRID_COLS - 1);
    int gx = (240 - GRID_W) / 2;
    int gy = 76;

    for (int i = 0; i < CARD_COUNT; ++i) {
        int col = i % GRID_COLS;
        int row = i / GRID_COLS;
        lv_obj_t* obj = lv_obj_create(scr);
        lv_obj_set_size(obj, CARD_W, CARD_H);
        lv_obj_set_pos(obj, gx + col * (CARD_W + GAP), gy + row * (CARD_H + GAP));
        lv_obj_set_style_radius(obj, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(obj, (void*)(intptr_t)i);
        lv_obj_add_event_cb(obj, card_clicked_cb, LV_EVENT_CLICKED, NULL);
        s_cards[i].obj = obj;
        paint_card(i);
    }

    // Status text
    s_status_label = lv_label_create(scr);
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_status_label, 220);
    lv_obj_set_style_text_color(s_status_label, theme_color_accent(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(s_status_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    refresh_status();
    update_clock();
    s_clock_timer = lv_timer_create(clock_tick_cb, 1000, NULL);

    nav_push(scr, NAV_ANIM_SLIDE_LEFT);
}
