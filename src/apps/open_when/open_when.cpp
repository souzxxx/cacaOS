/**
 * @file open_when.cpp
 * @brief Lists /open_when/*.txt files on SD; tap opens content.
 *
 * Each file format:
 *   Line 1: title
 *   Line 2: ---
 *   Rest:   body (multi-line)
 */

#include "open_when.h"

#include <Arduino.h>
#include <SD.h>
#include <lvgl.h>
#include <string.h>

#include "../../ui/theme.h"
#include "../../ui/nav.h"
#include "../../system/sdcard.h"
#include "../../system/text_utils.h"

static constexpr size_t MAX_ENTRIES = 16;
static constexpr size_t MAX_TITLE_LEN = 40;
static constexpr size_t MAX_PATH_LEN  = 56;
static constexpr size_t MAX_BODY_LEN  = 1024;

struct Entry {
    char title[MAX_TITLE_LEN];
    char path[MAX_PATH_LEN];
};

static Entry s_entries[MAX_ENTRIES];
static size_t s_entry_count = 0;
static char   s_body_buf[MAX_BODY_LEN];

static void parse_title_from(File& f, char* out_title) {
    out_title[0] = '\0';
    if (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        text_ascii_fold(line.c_str(), out_title, MAX_TITLE_LEN);
    }
}

static void scan_entries(void) {
    s_entry_count = 0;
    if (!sdcard_is_mounted()) return;

    File dir = SD.open("/open_when");
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        return;
    }

    while (s_entry_count < MAX_ENTRIES) {
        File entry = dir.openNextFile();
        if (!entry) break;
        if (entry.isDirectory()) { entry.close(); continue; }

        const char* name = entry.name();
        size_t namelen = strlen(name);
        if (namelen < 4 || strcasecmp(name + namelen - 4, ".txt") != 0) {
            entry.close();
            continue;
        }
        if (strcasecmp(name, "README.txt") == 0) {
            entry.close();
            continue;
        }

        Entry& e = s_entries[s_entry_count];
        snprintf(e.path, sizeof(e.path), "/open_when/%s", name);
        parse_title_from(entry, e.title);
        if (e.title[0] == '\0') {
            strncpy(e.title, name, MAX_TITLE_LEN - 1);
            e.title[MAX_TITLE_LEN - 1] = '\0';
        }
        entry.close();
        s_entry_count++;
    }
    dir.close();
    Serial.printf("[open_when] indexed %u letters\n", (unsigned)s_entry_count);
}

static void load_body(const char* path) {
    s_body_buf[0] = '\0';
    File f = SD.open(path, FILE_READ);
    if (!f) return;

    bool past_separator = false;
    size_t pos = 0;
    char line_ascii[256];
    while (f.available() && pos < MAX_BODY_LEN - 1) {
        String line = f.readStringUntil('\n');
        if (!past_separator) {
            String trimmed = line;
            trimmed.trim();
            if (trimmed == "---") {
                past_separator = true;
                continue;
            }
            // first line was title; skip until ---
            continue;
        }
        size_t n = text_ascii_fold(line.c_str(), line_ascii, sizeof(line_ascii));
        size_t copy_len = (n < (MAX_BODY_LEN - 1 - pos)) ? n : (MAX_BODY_LEN - 1 - pos);
        memcpy(s_body_buf + pos, line_ascii, copy_len);
        pos += copy_len;
        if (pos < MAX_BODY_LEN - 1) s_body_buf[pos++] = '\n';
    }
    s_body_buf[pos] = '\0';
    f.close();
}

static void back_event_cb(lv_event_t* /*e*/) {
    nav_pop(NAV_ANIM_SLIDE_RIGHT);
}

static lv_obj_t* make_header(lv_obj_t* parent, const char* title_text) {
    lv_obj_t* header = lv_obj_create(parent);
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
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_color(title, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    return header;
}

static void show_letter(const Entry* entry);

static void letter_event_cb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    const Entry* entry = (const Entry*)lv_obj_get_user_data(btn);
    if (entry) show_letter(entry);
}

static void letter_fade_in_cb(void* var, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t*)var, (lv_opa_t)v, LV_PART_MAIN);
}

static void show_letter(const Entry* entry) {
    load_body(entry->path);

    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, theme_color_bg(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    make_header(scr, entry->title);

    lv_obj_t* card = lv_obj_create(scr);
    lv_obj_set_size(card, 220, 250);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_add_style(card, &theme_style_card, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 12, LV_PART_MAIN);

    lv_obj_t* body_lbl = lv_label_create(card);
    lv_label_set_long_mode(body_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(body_lbl, 196);
    lv_label_set_text(body_lbl, s_body_buf[0] ? s_body_buf : "(vazio)");
    lv_obj_set_style_text_color(body_lbl, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(body_lbl, &lv_font_montserrat_14, LV_PART_MAIN);

    // Envelope-open feel: fade in the body card after the screen slides in
    lv_obj_set_style_opa(card, 0, LV_PART_MAIN);

    nav_push(scr, NAV_ANIM_SLIDE_LEFT);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, card);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_duration(&a, 320);
    lv_anim_set_delay(&a, 80);
    lv_anim_set_exec_cb(&a, letter_fade_in_cb);
    lv_anim_start(&a);
}

void open_when_show(void) {
    scan_entries();

    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, theme_color_bg(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    make_header(scr, "Cartinhas");

    lv_obj_t* list = lv_obj_create(scr);
    lv_obj_set_size(list, 240, 280);
    lv_obj_set_pos(list, 0, 40);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(list, 10, LV_PART_MAIN);

    if (s_entry_count == 0) {
        lv_obj_t* empty = lv_label_create(list);
        lv_label_set_long_mode(empty, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(empty, 200);
        lv_label_set_text(empty, "(adicione arquivos .txt em /open_when/ no cartao SD)");
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_color(empty, theme_color_text_light(), LV_PART_MAIN);
        nav_push(scr, NAV_ANIM_SLIDE_LEFT);
        return;
    }

    for (size_t i = 0; i < s_entry_count; ++i) {
        lv_obj_t* btn = lv_button_create(list);
        lv_obj_set_size(btn, 210, 44);
        lv_obj_set_style_bg_color(btn, theme_color_card(), LV_PART_MAIN);
        lv_obj_set_style_radius(btn, THEME_RADIUS_CARD, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
        lv_obj_set_user_data(btn, (void*)&s_entries[i]);
        lv_obj_add_event_cb(btn, letter_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t* env = lv_label_create(btn);
        lv_label_set_text(env, LV_SYMBOL_ENVELOPE);
        lv_obj_set_style_text_color(env, theme_color_accent(), LV_PART_MAIN);
        lv_obj_set_style_text_font(env, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_align(env, LV_ALIGN_LEFT_MID, 4, 0);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, s_entries[i].title);
        lv_obj_set_style_text_color(lbl, theme_color_text(), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        // Long titles scroll left-to-right in a loop so she can read the whole thing.
        lv_obj_set_width(lbl, 170);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_anim_duration(lbl, 6000, LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 32, 0);
    }

    nav_push(scr, NAV_ANIM_SLIDE_LEFT);
}
