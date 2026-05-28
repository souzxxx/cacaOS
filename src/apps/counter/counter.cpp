/**
 * @file counter.cpp
 * @brief Days/hours/minutes since RELATIONSHIP_START (live updating).
 *
 * Numbers use a small odometer-style roller: when a value changes the old
 * digits slide up and fade out while the new digits slide in from below.
 * Every 100 days a celebratory overlay covers the screen briefly.
 */

#include "counter.h"

#include <Arduino.h>
#include <lvgl.h>
#include <time.h>
#include <string.h>

#include "../../config.h"
#include "../../ui/theme.h"
#include "../../ui/nav.h"

// ---------- Odometer-style roller ----------
struct Roller {
    lv_obj_t*           clip;       // parent that clips
    lv_obj_t*           current;    // label currently centred in `clip`
    const lv_font_t*    font;
    lv_color_t          color;
    char                last[16];   // text currently shown
};

static lv_obj_t* roller_make_label(lv_obj_t* parent, const Roller* r, const char* text) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, r->color, LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, r->font, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(lbl, LV_PCT(100));
    lv_obj_center(lbl);
    return lbl;
}

static void roller_init(Roller* r, lv_obj_t* parent, int x, int y, int w, int h,
                        const lv_font_t* font, lv_color_t color) {
    r->font  = font;
    r->color = color;
    r->last[0] = '\0';

    r->clip = lv_obj_create(parent);
    lv_obj_set_size(r->clip, w, h);
    lv_obj_set_pos(r->clip, x, y);
    lv_obj_set_style_bg_opa(r->clip, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(r->clip, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(r->clip, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(r->clip, 0, LV_PART_MAIN);
    lv_obj_clear_flag(r->clip, LV_OBJ_FLAG_SCROLLABLE);

    r->current = roller_make_label(r->clip, r, "--");
}

static void anim_translate_y_cb(void* var, int32_t v) {
    lv_obj_set_style_translate_y((lv_obj_t*)var, v, LV_PART_MAIN);
}
static void anim_opa_cb(void* var, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t*)var, (lv_opa_t)v, LV_PART_MAIN);
}
static void delete_obj_anim_cb(lv_anim_t* a) {
    lv_obj_t* obj = (lv_obj_t*)a->var;
    if (obj) lv_obj_delete(obj);
}

static void roller_set(Roller* r, const char* text) {
    if (!r || !r->clip) return;
    if (strcmp(r->last, text) == 0) return;     // no change → no animation

    bool first = (r->last[0] == '\0');
    strncpy(r->last, text, sizeof(r->last) - 1);
    r->last[sizeof(r->last) - 1] = '\0';

    if (first) {
        lv_label_set_text(r->current, text);
        return;
    }

    int h = lv_obj_get_height(r->clip);
    if (h <= 0) h = 32;     // fallback if layout not yet computed

    // Old label translates up + fades out.
    lv_obj_t* old = r->current;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, old);
    lv_anim_set_values(&a, 0, -h);
    lv_anim_set_duration(&a, 280);
    lv_anim_set_exec_cb(&a, anim_translate_y_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&a, delete_obj_anim_cb);
    lv_anim_start(&a);

    lv_anim_t fade_out;
    lv_anim_init(&fade_out);
    lv_anim_set_var(&fade_out, old);
    lv_anim_set_values(&fade_out, 255, 0);
    lv_anim_set_duration(&fade_out, 220);
    lv_anim_set_exec_cb(&fade_out, anim_opa_cb);
    lv_anim_start(&fade_out);

    // New label spawns centred but translated below, then animates to 0.
    lv_obj_t* fresh = roller_make_label(r->clip, r, text);
    lv_obj_set_style_translate_y(fresh, h, LV_PART_MAIN);
    lv_obj_set_style_opa(fresh, 0, LV_PART_MAIN);
    r->current = fresh;

    lv_anim_t in;
    lv_anim_init(&in);
    lv_anim_set_var(&in, fresh);
    lv_anim_set_values(&in, h, 0);
    lv_anim_set_duration(&in, 280);
    lv_anim_set_exec_cb(&in, anim_translate_y_cb);
    lv_anim_set_path_cb(&in, lv_anim_path_ease_out);
    lv_anim_start(&in);

    lv_anim_t fade_in;
    lv_anim_init(&fade_in);
    lv_anim_set_var(&fade_in, fresh);
    lv_anim_set_values(&fade_in, 0, 255);
    lv_anim_set_duration(&fade_in, 280);
    lv_anim_set_delay(&fade_in, 40);
    lv_anim_set_exec_cb(&fade_in, anim_opa_cb);
    lv_anim_start(&fade_in);
}

// ---------- App state ----------
static Roller s_days_roller;
static Roller s_hours_roller;
static Roller s_mins_roller;
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

static int64_t compute_days_now(void) {
    time_t now = time(nullptr);
    time_t start = parse_start_date();
    if (start == 0 || now < start) return -1;
    return ((int64_t)now - (int64_t)start) / 86400;
}

static void refresh_values(void) {
    if (!s_days_roller.clip) return;

    time_t now = time(nullptr);
    time_t start = parse_start_date();

    if (start == 0 || now < start) {
        roller_set(&s_days_roller,  "--");
        roller_set(&s_hours_roller, "--");
        roller_set(&s_mins_roller,  "--");
        return;
    }

    int64_t diff = (int64_t)now - (int64_t)start;
    int64_t days  = diff / 86400;
    int64_t hours = (diff % 86400) / 3600;
    int64_t mins  = (diff % 3600) / 60;

    char buf[16];
    snprintf(buf, sizeof(buf), "%lld",   (long long)days);  roller_set(&s_days_roller,  buf);
    snprintf(buf, sizeof(buf), "%02lld", (long long)hours); roller_set(&s_hours_roller, buf);
    snprintf(buf, sizeof(buf), "%02lld", (long long)mins);  roller_set(&s_mins_roller,  buf);
}

// ---------- Milestone overlay + fireworks ----------

// Each pixel-art firework particle is a 4x4 square that flies outward,
// fading as it goes. Eight directions per burst, three bursts staggered.
static void anim_x_cb(void* var, int32_t v) { lv_obj_set_x((lv_obj_t*)var, v); }
static void anim_y_cb(void* var, int32_t v) { lv_obj_set_y((lv_obj_t*)var, v); }

static void spawn_firework_burst(lv_obj_t* parent, int cx, int cy) {
    static const uint32_t COLORS[] = {
        0xFB6F92, 0xFFB089, 0xE8B547, 0x8FD9B6, 0xC8A8E9, 0x7FB3F0,
    };
    static constexpr int N_COLORS = sizeof(COLORS) / sizeof(COLORS[0]);
    // 8 directions, in (dx,dy) units of 1/100. End offset ≈ 60px.
    static const int8_t DIRS[8][2] = {
        { 100,   0}, { 71,  71}, {  0, 100}, {-71,  71},
        {-100,   0}, {-71, -71}, {  0,-100}, { 71, -71},
    };
    constexpr int REACH = 60;
    constexpr int SIZE  = 4;
    constexpr int DURATION_MS = 650;

    for (int i = 0; i < 8; ++i) {
        lv_obj_t* p = lv_obj_create(parent);
        lv_obj_set_size(p, SIZE, SIZE);
        lv_obj_set_pos(p, cx - SIZE / 2, cy - SIZE / 2);
        lv_obj_set_style_bg_color(p, lv_color_hex(COLORS[i % N_COLORS]), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(p, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(p, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(p, 0, LV_PART_MAIN);          // square = pixel art
        lv_obj_set_style_shadow_width(p, 0, LV_PART_MAIN);
        lv_obj_clear_flag(p, LV_OBJ_FLAG_CLICKABLE);

        int end_x = cx + (DIRS[i][0] * REACH) / 100 - SIZE / 2;
        int end_y = cy + (DIRS[i][1] * REACH) / 100 - SIZE / 2;

        lv_anim_t ax;
        lv_anim_init(&ax);
        lv_anim_set_var(&ax, p);
        lv_anim_set_values(&ax, cx - SIZE / 2, end_x);
        lv_anim_set_duration(&ax, DURATION_MS);
        lv_anim_set_exec_cb(&ax, anim_x_cb);
        lv_anim_set_path_cb(&ax, lv_anim_path_ease_out);
        lv_anim_start(&ax);

        lv_anim_t ay;
        lv_anim_init(&ay);
        lv_anim_set_var(&ay, p);
        lv_anim_set_values(&ay, cy - SIZE / 2, end_y);
        lv_anim_set_duration(&ay, DURATION_MS);
        lv_anim_set_exec_cb(&ay, anim_y_cb);
        lv_anim_set_path_cb(&ay, lv_anim_path_ease_out);
        lv_anim_start(&ay);

        lv_anim_t fade;
        lv_anim_init(&fade);
        lv_anim_set_var(&fade, p);
        lv_anim_set_values(&fade, 255, 0);
        lv_anim_set_duration(&fade, DURATION_MS);
        lv_anim_set_delay(&fade, 120);  // hold bright for a bit, then fade
        lv_anim_set_exec_cb(&fade, anim_opa_cb);
        lv_anim_set_completed_cb(&fade, delete_obj_anim_cb);
        lv_anim_start(&fade);
    }
}

struct DelayedBurst { lv_obj_t* parent; int cx; int cy; };

static void delayed_burst_cb(lv_timer_t* t) {
    DelayedBurst* db = (DelayedBurst*)lv_timer_get_user_data(t);
    if (db && db->parent) spawn_firework_burst(db->parent, db->cx, db->cy);
    if (db) lv_free(db);
    lv_timer_delete(t);
}

static void schedule_burst(lv_obj_t* parent, int cx, int cy, uint32_t delay_ms) {
    DelayedBurst* db = (DelayedBurst*)lv_malloc(sizeof(DelayedBurst));
    if (!db) return;
    db->parent = parent;
    db->cx = cx;
    db->cy = cy;
    lv_timer_t* t = lv_timer_create(delayed_burst_cb, delay_ms, db);
    lv_timer_set_repeat_count(t, 1);
}

static void dismiss_milestone_overlay(lv_event_t* e) {
    lv_obj_t* overlay = (lv_obj_t*)lv_event_get_target(e);
    if (overlay) lv_obj_delete(overlay);
}

static void auto_dismiss_milestone_cb(lv_timer_t* t) {
    lv_obj_t* overlay = (lv_obj_t*)lv_timer_get_user_data(t);
    if (overlay) lv_obj_delete(overlay);
    lv_timer_delete(t);
}

static void show_milestone_overlay(int64_t days) {
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
    lv_obj_add_flag(dim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(dim, dismiss_milestone_overlay, LV_EVENT_CLICKED, NULL);

    lv_obj_t* card = lv_obj_create(dim);
    lv_obj_set_size(card, 200, 140);
    lv_obj_center(card);
    lv_obj_add_style(card, &theme_style_card, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* big = lv_label_create(card);
    char buf[40];
    snprintf(buf, sizeof(buf), "%lld dias!", (long long)days);
    lv_label_set_text(big, buf);
    lv_obj_set_style_text_color(big, theme_color_accent(), LV_PART_MAIN);
    lv_obj_set_style_text_font(big, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(big, LV_ALIGN_TOP_MID, 0, 14);

    lv_obj_t* sub = lv_label_create(card);
    lv_label_set_text(sub, LV_SYMBOL_OK " " LV_SYMBOL_OK " " LV_SYMBOL_OK "\nfestejando esse marco");
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(sub, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(sub, LV_ALIGN_BOTTOM_MID, 0, -8);

    lv_timer_t* auto_dismiss = lv_timer_create(auto_dismiss_milestone_cb, 4000, dim);
    lv_timer_set_repeat_count(auto_dismiss, 1);

    // Three pixel-art firework bursts staggered around the card.
    spawn_firework_burst(dim, 120, 120);
    schedule_burst(dim,  60, 100, 250);
    schedule_burst(dim, 180, 100, 450);
}

static void tick_cb(lv_timer_t* /*t*/) {
    refresh_values();
}

static void back_event_cb(lv_event_t* /*e*/) {
    if (s_tick) {
        lv_timer_delete(s_tick);
        s_tick = nullptr;
    }
    s_days_roller  = {};
    s_hours_roller = {};
    s_mins_roller  = {};
    s_caption_label = nullptr;
    nav_pop(NAV_ANIM_SLIDE_RIGHT);
}

// Build a card with a roller centred inside and a static "unit" caption.
static void make_unit_card(lv_obj_t* parent, int x, int y, int w, int h,
                           const char* unit_text, Roller* out_roller,
                           const lv_font_t* font) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_add_style(card, &theme_style_card, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(card, 6, LV_PART_MAIN);

    // Roller occupies the upper portion of the card; leaves room for unit.
    constexpr int UNIT_RESERVED = 18;
    int roller_h = h - 12 - UNIT_RESERVED;
    roller_init(out_roller, card, 0, 0, w - 12, roller_h,
                font, theme_color_accent());

    lv_obj_t* unit = lv_label_create(card);
    lv_label_set_text(unit, unit_text);
    lv_obj_add_style(unit, &theme_style_caption, LV_PART_MAIN);
    lv_obj_align(unit, LV_ALIGN_BOTTOM_MID, 0, -2);
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

    // Caption: "com <name> desde <date>"
    s_caption_label = lv_label_create(scr);
    char caption[64];
    snprintf(caption, sizeof(caption), "com %s desde %s", HER_NAME, RELATIONSHIP_START);
    lv_label_set_text(s_caption_label, caption);
    lv_obj_set_style_text_color(s_caption_label, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_caption_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_caption_label, LV_ALIGN_TOP_MID, 0, 50);

    // Big "days" card with odometer
    make_unit_card(scr, 20, 80, 200, 100, "dias",
                   &s_days_roller, &lv_font_montserrat_32);

    // Two small cards: hours + minutes
    make_unit_card(scr, 20, 200, 95, 70, "horas",
                   &s_hours_roller, &lv_font_montserrat_24);
    make_unit_card(scr, 125, 200, 95, 70, "minutos",
                   &s_mins_roller, &lv_font_montserrat_24);

    // Footer
    lv_obj_t* heart = lv_label_create(scr);
    lv_label_set_text(heart, LV_SYMBOL_OK "  amo  " LV_SYMBOL_OK);
    lv_obj_set_style_text_color(heart, theme_color_accent(), LV_PART_MAIN);
    lv_obj_set_style_text_font(heart, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(heart, LV_ALIGN_BOTTOM_MID, 0, -8);

    refresh_values();
    s_tick = lv_timer_create(tick_cb, 30 * 1000, NULL);

    nav_push(scr, NAV_ANIM_SLIDE_LEFT);

    // Milestone celebration: every 100 days starting from day 100
    int64_t days_now = compute_days_now();
    if (days_now >= 100 && days_now % 100 == 0) {
        show_milestone_overlay(days_now);
    }
}
