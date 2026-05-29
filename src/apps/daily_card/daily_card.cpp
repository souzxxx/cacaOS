/**
 * @file daily_card.cpp
 * @brief Card-of-the-day from /messages.json on SD. Index = day-of-year.
 */

#include "daily_card.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <lvgl.h>
#include <time.h>

#include "../../ui/theme.h"
#include "../../ui/nav.h"
#include "../../system/sdcard.h"
#include "../../system/text_utils.h"

static constexpr size_t MAX_MESSAGES = 56;
static constexpr size_t MAX_MESSAGE_LEN = 128;

static char  s_messages[MAX_MESSAGES][MAX_MESSAGE_LEN];
static size_t s_message_count = 0;
static int    s_current_index = 0;
static lv_obj_t* s_text_label = nullptr;

static const char* FALLBACK_MESSAGE =
    "(coloca o cartao SD com /messages.json pra ver uma mensagem do dia)";

static void load_messages(void) {
    s_message_count = 0;
    if (!sdcard_is_mounted()) return;

    File f = SD.open("/messages.json", FILE_READ);
    if (!f) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("[daily_card] json parse error: %s\n", err.c_str());
        return;
    }

    JsonArray arr = doc["messages"].as<JsonArray>();
    for (JsonVariant v : arr) {
        if (s_message_count >= MAX_MESSAGES) break;
        const char* s = v.as<const char*>();
        if (!s) continue;
        text_ascii_fold(s, s_messages[s_message_count], MAX_MESSAGE_LEN);
        s_message_count++;
    }
    Serial.printf("[daily_card] loaded %u messages\n", (unsigned)s_message_count);
}

static int day_of_year_now(void) {
    time_t now = time(nullptr);
    struct tm tm_local;
    localtime_r(&now, &tm_local);
    return tm_local.tm_yday;
}

static void show_index(int idx) {
    if (!s_text_label) return;
    if (s_message_count == 0) {
        lv_label_set_text(s_text_label, FALLBACK_MESSAGE);
        return;
    }
    s_current_index = ((idx % (int)s_message_count) + (int)s_message_count) % (int)s_message_count;
    lv_label_set_text(s_text_label, s_messages[s_current_index]);
}

static void back_event_cb(lv_event_t* /*e*/) {
    s_text_label = nullptr;
    nav_pop(NAV_ANIM_SLIDE_RIGHT);
}

static void next_event_cb(lv_event_t* /*e*/) {
    show_index(s_current_index + 1);
}

void daily_card_show(void) {
    load_messages();

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
    lv_label_set_text(title, "Cartinha do dia");
    lv_obj_set_style_text_color(title, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Card with message
    lv_obj_t* card = lv_obj_create(scr);
    lv_obj_set_size(card, 210, 210);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 55);
    lv_obj_add_style(card, &theme_style_card, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 14, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    // Subtle pink->white vertical gradient (kawaii touch; keeps text readable).
    lv_obj_set_style_bg_color(card, theme_color_text_light(), LV_PART_MAIN);   // light pink (top)
    lv_obj_set_style_bg_grad_color(card, theme_color_card(), LV_PART_MAIN);    // white (bottom)
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);

    s_text_label = lv_label_create(card);
    lv_label_set_long_mode(s_text_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_text_label, 180);
    lv_obj_set_style_text_align(s_text_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_text_label, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_text_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_center(s_text_label);

    // "Outra" button
    lv_obj_t* next_btn = lv_button_create(scr);
    lv_obj_set_size(next_btn, 120, 36);
    lv_obj_align(next_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_style(next_btn, &theme_style_button_primary, LV_PART_MAIN);
    lv_obj_add_event_cb(next_btn, next_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* next_lbl = lv_label_create(next_btn);
    lv_label_set_text(next_lbl, "outra " LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_color(next_lbl, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(next_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(next_lbl);

    show_index(day_of_year_now());

    nav_push(scr, NAV_ANIM_SLIDE_LEFT);
}
