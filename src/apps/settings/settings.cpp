/**
 * @file settings.cpp
 * @brief Brightness slider + sobre + reset actions (pet/touch).
 */

#include "settings.h"

#include <Arduino.h>
#include <Preferences.h>
#include <lvgl.h>

#include "../../ui/theme.h"
#include "../../ui/nav.h"
#include "../../system/display.h"
#include "../../system/storage.h"
#include "../../system/touch.h"
#include "../wifi_config/wifi_config.h"

#define CACAOS_VERSION "v0.1"

static lv_obj_t* s_brightness_label = nullptr;

static void back_event_cb(lv_event_t* /*e*/) {
    s_brightness_label = nullptr;
    nav_pop(NAV_ANIM_SLIDE_RIGHT);
}

static void brightness_changed_cb(lv_event_t* e) {
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
    int v = (int)lv_slider_get_value(slider);
    display_set_brightness((uint8_t)v);
    storage_set_brightness((uint8_t)v);

    if (s_brightness_label) {
        char buf[24];
        snprintf(buf, sizeof(buf), "brilho: %d%%", (v * 100) / 255);
        lv_label_set_text(s_brightness_label, buf);
    }
}

static void reset_pet_confirm_cb(lv_event_t* e) {
    lv_obj_t* overlay = (lv_obj_t*)lv_event_get_user_data(e);
    if (overlay) lv_obj_delete(overlay);

    Preferences p;
    if (p.begin("tama", false)) {
        p.putBool("init", false);
        p.end();
        Serial.println(F("[settings] tamagotchi reset"));
    }
}

static void reset_touch_confirm_cb(lv_event_t* e) {
    lv_obj_t* overlay = (lv_obj_t*)lv_event_get_user_data(e);
    if (overlay) lv_obj_delete(overlay);

    touch_reset_calibration();
    Serial.println(F("[settings] touch calibration reset"));
}

static void overlay_dismiss_cb(lv_event_t* e) {
    lv_obj_t* overlay = (lv_obj_t*)lv_event_get_user_data(e);
    if (overlay) lv_obj_delete(overlay);
}

static void show_confirm_overlay(const char* message, lv_event_cb_t yes_cb) {
    lv_obj_t* parent = lv_screen_active();
    if (!parent) return;

    lv_obj_t* dim = lv_obj_create(parent);
    lv_obj_set_size(dim, 240, 320);
    lv_obj_set_pos(dim, 0, 0);
    lv_obj_set_style_bg_color(dim, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dim, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_border_width(dim, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(dim, 0, LV_PART_MAIN);
    lv_obj_clear_flag(dim, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* card = lv_obj_create(dim);
    lv_obj_set_size(card, 200, 140);
    lv_obj_center(card);
    lv_obj_add_style(card, &theme_style_card, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 14, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* msg = lv_label_create(card);
    lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg, 170);
    lv_label_set_text(msg, message);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(msg, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* no_btn = lv_button_create(card);
    lv_obj_set_size(no_btn, 70, 36);
    lv_obj_align(no_btn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(no_btn, theme_color_text_light(), LV_PART_MAIN);
    lv_obj_set_style_radius(no_btn, THEME_RADIUS_BUTTON, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(no_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(no_btn, overlay_dismiss_cb, LV_EVENT_CLICKED, dim);
    lv_obj_t* no_lbl = lv_label_create(no_btn);
    lv_label_set_text(no_lbl, "nao");
    lv_obj_set_style_text_color(no_lbl, theme_color_text(), LV_PART_MAIN);
    lv_obj_center(no_lbl);

    lv_obj_t* yes_btn = lv_button_create(card);
    lv_obj_set_size(yes_btn, 70, 36);
    lv_obj_align(yes_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(yes_btn, theme_color_accent(), LV_PART_MAIN);
    lv_obj_set_style_radius(yes_btn, THEME_RADIUS_BUTTON, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(yes_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(yes_btn, yes_cb, LV_EVENT_CLICKED, dim);
    lv_obj_t* yes_lbl = lv_label_create(yes_btn);
    lv_label_set_text(yes_lbl, "resetar");
    lv_obj_set_style_text_color(yes_lbl, theme_color_card(), LV_PART_MAIN);
    lv_obj_center(yes_lbl);
}

static void reset_pet_btn_cb(lv_event_t* /*e*/) {
    show_confirm_overlay("Resetar pet?\nvai pra escolha de Caca de novo", reset_pet_confirm_cb);
}

static void reset_touch_btn_cb(lv_event_t* /*e*/) {
    show_confirm_overlay("Resetar calibracao do touch?\nProximo boot vai recalibrar", reset_touch_confirm_cb);
}

static void wifi_btn_cb(lv_event_t* /*e*/) {
    wifi_config_show();
}

void settings_show(void) {
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
    lv_label_set_text(title, "Ajustes");
    lv_obj_set_style_text_color(title, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Scrollable content area below the fixed header
    lv_obj_t* content = lv_obj_create(scr);
    lv_obj_set_size(content, 240, 280);   // 320 - 40 header
    lv_obj_set_pos(content, 0, 40);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(content, 0, LV_PART_MAIN);

    // --- Brightness ---
    lv_obj_t* bright_card = lv_obj_create(content);
    lv_obj_set_size(bright_card, 220, 70);
    lv_obj_align(bright_card, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_add_style(bright_card, &theme_style_card, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bright_card, 10, LV_PART_MAIN);
    lv_obj_clear_flag(bright_card, LV_OBJ_FLAG_SCROLLABLE);

    s_brightness_label = lv_label_create(bright_card);
    lv_obj_set_style_text_color(s_brightness_label, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_brightness_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_brightness_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* slider = lv_slider_create(bright_card);
    lv_obj_set_size(slider, 200, 14);
    lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_slider_set_range(slider, 16, 255);
    uint8_t current = storage_get_brightness(255);
    lv_slider_set_value(slider, current, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, theme_color_text_light(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, theme_color_accent(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, theme_color_accent(), LV_PART_KNOB);
    lv_obj_add_event_cb(slider, brightness_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    {
        char buf[24];
        snprintf(buf, sizeof(buf), "brilho: %d%%", (current * 100) / 255);
        lv_label_set_text(s_brightness_label, buf);
    }

    // --- Sobre ---
    lv_obj_t* about_card = lv_obj_create(content);
    lv_obj_set_size(about_card, 220, 56);
    lv_obj_align(about_card, LV_ALIGN_TOP_MID, 0, 84);
    lv_obj_add_style(about_card, &theme_style_card, LV_PART_MAIN);
    lv_obj_set_style_pad_all(about_card, 10, LV_PART_MAIN);
    lv_obj_clear_flag(about_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* about_title = lv_label_create(about_card);
    lv_label_set_text(about_title, "CacaOS " CACAOS_VERSION);
    lv_obj_set_style_text_color(about_title, theme_color_accent(), LV_PART_MAIN);
    lv_obj_set_style_text_font(about_title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(about_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* about_sub = lv_label_create(about_card);
    lv_label_set_text(about_sub, "feito com " LV_SYMBOL_OK " no esp32");
    lv_obj_set_style_text_color(about_sub, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(about_sub, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(about_sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // --- WiFi config ---
    lv_obj_t* wifi_btn = lv_button_create(content);
    lv_obj_set_size(wifi_btn, 220, 40);
    lv_obj_align(wifi_btn, LV_ALIGN_TOP_MID, 0, 146);
    lv_obj_set_style_bg_color(wifi_btn, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_radius(wifi_btn, THEME_RADIUS_BUTTON, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(wifi_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(wifi_btn, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(wifi_btn, theme_color_accent(), LV_PART_MAIN);
    lv_obj_add_event_cb(wifi_btn, wifi_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* wifi_lbl = lv_label_create(wifi_btn);
    lv_label_set_text(wifi_lbl, LV_SYMBOL_WIFI "  configurar wifi");
    lv_obj_set_style_text_color(wifi_lbl, theme_color_accent(), LV_PART_MAIN);
    lv_obj_set_style_text_font(wifi_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(wifi_lbl);

    // --- Reset actions ---
    lv_obj_t* reset_pet = lv_button_create(content);
    lv_obj_set_size(reset_pet, 220, 40);
    lv_obj_align(reset_pet, LV_ALIGN_TOP_MID, 0, 192);
    lv_obj_set_style_bg_color(reset_pet, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_radius(reset_pet, THEME_RADIUS_BUTTON, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(reset_pet, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(reset_pet, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(reset_pet, theme_color_accent(), LV_PART_MAIN);
    lv_obj_add_event_cb(reset_pet, reset_pet_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* rp_lbl = lv_label_create(reset_pet);
    lv_label_set_text(rp_lbl, "resetar pet");
    lv_obj_set_style_text_color(rp_lbl, theme_color_accent(), LV_PART_MAIN);
    lv_obj_set_style_text_font(rp_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(rp_lbl);

    lv_obj_t* reset_touch = lv_button_create(content);
    lv_obj_set_size(reset_touch, 220, 40);
    lv_obj_align(reset_touch, LV_ALIGN_TOP_MID, 0, 238);
    lv_obj_set_style_bg_color(reset_touch, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_radius(reset_touch, THEME_RADIUS_BUTTON, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(reset_touch, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(reset_touch, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(reset_touch, theme_color_accent(), LV_PART_MAIN);
    lv_obj_add_event_cb(reset_touch, reset_touch_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* rt_lbl = lv_label_create(reset_touch);
    lv_label_set_text(rt_lbl, "recalibrar touch");
    lv_obj_set_style_text_color(rt_lbl, theme_color_accent(), LV_PART_MAIN);
    lv_obj_set_style_text_font(rt_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(rt_lbl);

    nav_push(scr, NAV_ANIM_SLIDE_LEFT);
}
