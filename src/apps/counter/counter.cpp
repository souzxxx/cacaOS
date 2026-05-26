/**
 * @file counter.cpp
 * @brief Days/hours/minutes since RELATIONSHIP_START (live updating).
 */

#include "counter.h"

#include <Arduino.h>
#include <lvgl.h>
#include <time.h>

#include "../../config.h"
#include "../../ui/theme.h"
#include "../../ui/nav.h"

static lv_obj_t* s_days_label = nullptr;
static lv_obj_t* s_hours_label = nullptr;
static lv_obj_t* s_mins_label = nullptr;
static lv_obj_t* s_caption_label = nullptr;
static lv_timer_t* s_tick = nullptr;

static time_t parse_start_date(void) {
    struct tm tm_start = {};
    int y = 0, m = 0, d = 0;
    if (sscanf(RELATIONSHIP_START, "%d-%d-%d", &y, &m, &d) != 3) {
        return 0;
    }
    tm_start.tm_year = y - 1900;
    tm_start.tm_mon  = m - 1;
    tm_start.tm_mday = d;
    tm_start.tm_hour = 0;
    tm_start.tm_min  = 0;
    tm_start.tm_sec  = 0;
    tm_start.tm_isdst = -1;
    return mktime(&tm_start);
}

static void refresh_values(void) {
    if (!s_days_label) return;

    time_t now = time(nullptr);
    time_t start = parse_start_date();

    if (start == 0 || now < start) {
        lv_label_set_text(s_days_label, "--");
        lv_label_set_text(s_hours_label, "--");
        lv_label_set_text(s_mins_label, "--");
        return;
    }

    int64_t diff = (int64_t)now - (int64_t)start;
    int64_t days  = diff / 86400;
    int64_t hours = (diff % 86400) / 3600;
    int64_t mins  = (diff % 3600) / 60;

    char buf[16];
    snprintf(buf, sizeof(buf), "%lld", (long long)days);
    lv_label_set_text(s_days_label, buf);
    snprintf(buf, sizeof(buf), "%02lld", (long long)hours);
    lv_label_set_text(s_hours_label, buf);
    snprintf(buf, sizeof(buf), "%02lld", (long long)mins);
    lv_label_set_text(s_mins_label, buf);
}

static void tick_cb(lv_timer_t* /*t*/) {
    refresh_values();
}

static void back_event_cb(lv_event_t* /*e*/) {
    if (s_tick) {
        lv_timer_delete(s_tick);
        s_tick = nullptr;
    }
    s_days_label = s_hours_label = s_mins_label = s_caption_label = nullptr;
    nav_pop(NAV_ANIM_SLIDE_RIGHT);
}

static lv_obj_t* make_card(lv_obj_t* parent, int x, int y, int w, int h, const char* unit_text, lv_obj_t** out_value) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_add_style(card, &theme_style_card, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(card, 6, LV_PART_MAIN);

    lv_obj_t* value = lv_label_create(card);
    lv_label_set_text(value, "--");
    lv_obj_set_style_text_color(value, theme_color_accent(), LV_PART_MAIN);
    lv_obj_set_style_text_font(value, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(value, LV_ALIGN_CENTER, 0, -6);

    lv_obj_t* unit = lv_label_create(card);
    lv_label_set_text(unit, unit_text);
    lv_obj_add_style(unit, &theme_style_caption, LV_PART_MAIN);
    lv_obj_align(unit, LV_ALIGN_BOTTOM_MID, 0, -2);

    *out_value = value;
    return card;
}

void counter_show(void) {
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
    lv_label_set_text(title, "Contador");
    lv_obj_set_style_text_color(title, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Body — caption
    s_caption_label = lv_label_create(scr);
    char caption[64];
    snprintf(caption, sizeof(caption), "com %s desde %s", HER_NAME, RELATIONSHIP_START);
    lv_label_set_text(s_caption_label, caption);
    lv_obj_set_style_text_color(s_caption_label, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_caption_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_caption_label, LV_ALIGN_TOP_MID, 0, 50);

    // Big days card
    lv_obj_t* big_card = lv_obj_create(scr);
    lv_obj_set_size(big_card, 200, 100);
    lv_obj_align(big_card, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_add_style(big_card, &theme_style_card, LV_PART_MAIN);
    lv_obj_clear_flag(big_card, LV_OBJ_FLAG_SCROLLABLE);

    s_days_label = lv_label_create(big_card);
    lv_label_set_text(s_days_label, "--");
    lv_obj_set_style_text_color(s_days_label, theme_color_accent(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_days_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(s_days_label, LV_ALIGN_CENTER, 0, -8);

    lv_obj_t* days_unit = lv_label_create(big_card);
    lv_label_set_text(days_unit, "dias");
    lv_obj_add_style(days_unit, &theme_style_caption, LV_PART_MAIN);
    lv_obj_align(days_unit, LV_ALIGN_BOTTOM_MID, 0, -6);

    // Two small cards: hours + minutes
    make_card(scr, 20, 200, 95, 70, "horas", &s_hours_label);
    make_card(scr, 125, 200, 95, 70, "minutos", &s_mins_label);

    // Footer
    lv_obj_t* heart = lv_label_create(scr);
    lv_label_set_text(heart, LV_SYMBOL_OK "  amo " LV_SYMBOL_OK);
    lv_obj_set_style_text_color(heart, theme_color_accent(), LV_PART_MAIN);
    lv_obj_set_style_text_font(heart, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(heart, LV_ALIGN_BOTTOM_MID, 0, -8);

    refresh_values();
    s_tick = lv_timer_create(tick_cb, 30 * 1000, NULL);

    nav_push(scr, NAV_ANIM_SLIDE_LEFT);
}
