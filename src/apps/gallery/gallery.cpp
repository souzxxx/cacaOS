/**
 * @file gallery.cpp
 * @brief Photo carousel from /photos/*.jpg on SD via LVGL FS driver.
 *
 * Image paths are resolved as "S:/photos/<name>". LVGL FS_STDIO is configured
 * with letter 'S' and base path "/sd" (where ESP32 Arduino mounts the card),
 * so the open call lands at fopen("/sd/photos/<name>").
 */

#include "gallery.h"

#include <Arduino.h>
#include <SD.h>
#include <lvgl.h>
#include <string.h>

#include "../../ui/theme.h"
#include "../../ui/nav.h"
#include "../../system/sdcard.h"

static constexpr size_t MAX_PHOTOS = 64;
static constexpr size_t MAX_NAME_LEN = 32;

static char  s_photos[MAX_PHOTOS][MAX_NAME_LEN];
static size_t s_photo_count = 0;
static int    s_current_index = 0;

static lv_obj_t* s_image = nullptr;
static lv_obj_t* s_empty_label = nullptr;
static lv_obj_t* s_counter_label = nullptr;
static char  s_image_path[80];

static bool ext_matches_image(const char* name) {
    size_t len = strlen(name);
    if (len < 5) return false;
    const char* ext = name + len - 4;
    return strcasecmp(ext, ".jpg") == 0 ||
           strcasecmp(ext, ".png") == 0 ||
           strcasecmp(ext, "jpeg") == 0;
}

static void scan_photos(void) {
    s_photo_count = 0;
    if (!sdcard_is_mounted()) return;

    File dir = SD.open("/photos");
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        return;
    }

    while (s_photo_count < MAX_PHOTOS) {
        File entry = dir.openNextFile();
        if (!entry) break;
        if (entry.isDirectory()) { entry.close(); continue; }
        const char* name = entry.name();
        if (ext_matches_image(name)) {
            strncpy(s_photos[s_photo_count], name, MAX_NAME_LEN - 1);
            s_photos[s_photo_count][MAX_NAME_LEN - 1] = '\0';
            s_photo_count++;
        }
        entry.close();
    }
    dir.close();
    Serial.printf("[gallery] indexed %u photos\n", (unsigned)s_photo_count);
}

static void refresh(void) {
    if (!s_counter_label) return;

    if (s_photo_count == 0) {
        if (s_empty_label) lv_obj_remove_flag(s_empty_label, LV_OBJ_FLAG_HIDDEN);
        if (s_image)       lv_obj_add_flag(s_image, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_counter_label, "0 / 0");
        return;
    }

    s_current_index = ((s_current_index % (int)s_photo_count) + (int)s_photo_count) % (int)s_photo_count;

    snprintf(s_image_path, sizeof(s_image_path), "S:/photos/%s", s_photos[s_current_index]);
    if (s_image) {
        lv_obj_remove_flag(s_image, LV_OBJ_FLAG_HIDDEN);
        lv_image_set_src(s_image, s_image_path);
    }
    if (s_empty_label) lv_obj_add_flag(s_empty_label, LV_OBJ_FLAG_HIDDEN);

    char buf[16];
    snprintf(buf, sizeof(buf), "%d / %u", s_current_index + 1, (unsigned)s_photo_count);
    lv_label_set_text(s_counter_label, buf);
}

static void prev_cb(lv_event_t* /*e*/) { s_current_index--; refresh(); }
static void next_cb(lv_event_t* /*e*/) { s_current_index++; refresh(); }

static void back_event_cb(lv_event_t* /*e*/) {
    s_image = s_empty_label = s_counter_label = nullptr;
    nav_pop(NAV_ANIM_SLIDE_RIGHT);
}

void gallery_show(void) {
    scan_photos();
    s_current_index = 0;

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
    lv_label_set_text(title, "Galeria");
    lv_obj_set_style_text_color(title, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Preview container (image fills, with letterboxing for non-240x320 photos)
    lv_obj_t* preview = lv_obj_create(scr);
    lv_obj_set_size(preview, 240, 220);
    lv_obj_align(preview, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_color(preview, theme_color_text_light(), LV_PART_MAIN);
    lv_obj_set_style_border_width(preview, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(preview, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(preview, 0, LV_PART_MAIN);
    lv_obj_clear_flag(preview, LV_OBJ_FLAG_SCROLLABLE);

    s_image = lv_image_create(preview);
    lv_obj_set_size(s_image, 240, 220);
    lv_image_set_inner_align(s_image, LV_IMAGE_ALIGN_CONTAIN);
    lv_obj_center(s_image);

    s_empty_label = lv_label_create(preview);
    lv_label_set_long_mode(s_empty_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_empty_label, 200);
    lv_obj_set_style_text_align(s_empty_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_empty_label, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_empty_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_label_set_text(s_empty_label, "sem fotos\n(adicione JPGs em /photos/ no SD)");
    lv_obj_center(s_empty_label);
    lv_obj_add_flag(s_empty_label, LV_OBJ_FLAG_HIDDEN);

    // Counter
    s_counter_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_counter_label, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_counter_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_counter_label, theme_color_accent(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_counter_label, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(s_counter_label, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(s_counter_label, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(s_counter_label, 12, LV_PART_MAIN);
    lv_obj_align(s_counter_label, LV_ALIGN_TOP_RIGHT, -10, 50);

    // Prev / next buttons
    auto make_nav_btn = [&](int x, lv_event_cb_t cb, const char* sym) {
        lv_obj_t* b = lv_button_create(scr);
        lv_obj_set_size(b, 56, 44);
        lv_obj_set_pos(b, x, 268);
        lv_obj_set_style_bg_color(b, theme_color_accent(), LV_PART_MAIN);
        lv_obj_set_style_radius(b, THEME_RADIUS_BUTTON, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, sym);
        lv_obj_set_style_text_color(l, theme_color_card(), LV_PART_MAIN);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_center(l);
    };
    make_nav_btn(40, prev_cb, LV_SYMBOL_LEFT);
    make_nav_btn(144, next_cb, LV_SYMBOL_RIGHT);

    refresh();

    nav_push(scr, NAV_ANIM_SLIDE_LEFT);
}
