/**
 * @file pomodoro.cpp
 * @brief 25/5 focus timer with play/pause/reset + completed-pomodoros counter.
 */

#include "pomodoro.h"

#include <Arduino.h>
#include <lvgl.h>

#include <Preferences.h>

#include "../../ui/theme.h"
#include "../../ui/nav.h"
#include "../../system/rgb_led.h"

enum PomState { POMO_IDLE, POMO_FOCUS, POMO_BREAK, POMO_PAUSED_FOCUS, POMO_PAUSED_BREAK };

static constexpr uint32_t FOCUS_SECONDS = 25 * 60;
static constexpr uint32_t BREAK_SECONDS = 5 * 60;
static constexpr int SPEAKER_PIN = 26;

static void beep(unsigned freq_hz, unsigned dur_ms) {
    tone(SPEAKER_PIN, freq_hz, dur_ms);
}

static PomState   s_state = POMO_IDLE;
static uint32_t   s_seconds_left = FOCUS_SECONDS;
static uint32_t   s_completed_focus = 0;
static lv_timer_t* s_tick = nullptr;

static lv_obj_t* s_state_label = nullptr;
static lv_obj_t* s_time_label = nullptr;
static lv_obj_t* s_count_label = nullptr;
static lv_obj_t* s_play_btn = nullptr;
static lv_obj_t* s_play_lbl = nullptr;
static lv_obj_t* s_pet_img = nullptr;
static lv_timer_t* s_pet_anim_timer = nullptr;
static int        s_pet_frame = 0;
static char       s_pet_slug[16] = "white";
static char       s_pet_sprite_path[96];

static constexpr int PET_FRAME_W = 32;
static constexpr int PET_FRAME_H = 32;
static constexpr int PET_FRAME_COUNT = 12;

static void load_pet_slug(void) {
    Preferences p;
    if (!p.begin("tama", true)) return;
    String slug = p.getString("slug", "white");
    strncpy(s_pet_slug, slug.c_str(), sizeof(s_pet_slug) - 1);
    s_pet_slug[sizeof(s_pet_slug) - 1] = '\0';
    p.end();
}

static const char* pet_anim_for_state(void) {
    switch (s_state) {
        case POMO_FOCUS:        return "rest";     // pet "concentrates"
        case POMO_BREAK:        return "happy";    // pet rejoices
        case POMO_PAUSED_FOCUS:
        case POMO_PAUSED_BREAK: return "idle";
        case POMO_IDLE:
        default:                return "idle";
    }
}

static void refresh_pet_sprite(void) {
    if (!s_pet_img) return;
    snprintf(s_pet_sprite_path, sizeof(s_pet_sprite_path),
             "S:/tamagotchi_sprites/cacaos_pet_assets/pets/%s/%s.png",
             s_pet_slug, pet_anim_for_state());
    lv_image_set_src(s_pet_img, s_pet_sprite_path);
    s_pet_frame = 0;
    lv_image_set_offset_x(s_pet_img, 0);
}

static void pet_anim_tick_cb(lv_timer_t* /*t*/) {
    if (!s_pet_img) return;
    s_pet_frame = (s_pet_frame + 1) % PET_FRAME_COUNT;
    lv_image_set_offset_x(s_pet_img, -(s_pet_frame * PET_FRAME_W));
}

static void update_time_label(void) {
    if (!s_time_label) return;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)(s_seconds_left / 60), (unsigned)(s_seconds_left % 60));
    lv_label_set_text(s_time_label, buf);
}

static void update_state_label(void) {
    if (!s_state_label) return;
    const char* text = "pronta?";
    lv_color_t color = theme_color_text();
    switch (s_state) {
        case POMO_IDLE:          text = "pronta?";   break;
        case POMO_FOCUS:         text = "focada"; color = theme_color_accent(); break;
        case POMO_BREAK:         text = "descanso"; color = theme_color_success(); break;
        case POMO_PAUSED_FOCUS:  text = "pausado (foco)"; break;
        case POMO_PAUSED_BREAK:  text = "pausado (descanso)"; break;
    }
    lv_label_set_text(s_state_label, text);
    lv_obj_set_style_text_color(s_state_label, color, LV_PART_MAIN);
}

static void update_play_label(void) {
    if (!s_play_lbl) return;
    bool running = (s_state == POMO_FOCUS || s_state == POMO_BREAK);
    lv_label_set_text(s_play_lbl, running ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
}

static void update_count_label(void) {
    if (!s_count_label) return;
    char buf[24];
    snprintf(buf, sizeof(buf), "%u pomodoro(s)", (unsigned)s_completed_focus);
    lv_label_set_text(s_count_label, buf);
}

static void update_led(void) {
    switch (s_state) {
        case POMO_FOCUS:        rgb_led_set(120, 230, 140); break;   // green
        case POMO_BREAK:        rgb_led_set(250, 120, 100); break;   // warm red
        case POMO_PAUSED_FOCUS:
        case POMO_PAUSED_BREAK: rgb_led_set(60, 60, 60); break;       // dim white (paused)
        case POMO_IDLE:
        default:                rgb_led_off(); break;
    }
}

static void refresh_all(void) {
    update_state_label();
    update_time_label();
    update_play_label();
    update_count_label();
    update_led();
    refresh_pet_sprite();
}

static void tick_cb(lv_timer_t* /*t*/) {
    if (s_state != POMO_FOCUS && s_state != POMO_BREAK) return;

    if (s_seconds_left > 0) {
        s_seconds_left--;
        update_time_label();
        return;
    }

    // Cycle ended
    if (s_state == POMO_FOCUS) {
        s_completed_focus++;
        s_state = POMO_BREAK;
        s_seconds_left = BREAK_SECONDS;
        beep(880, 200);
    } else {
        s_state = POMO_FOCUS;
        s_seconds_left = FOCUS_SECONDS;
        beep(660, 200);
    }
    refresh_all();
}

static void play_pause_cb(lv_event_t* /*e*/) {
    switch (s_state) {
        case POMO_IDLE:
            s_state = POMO_FOCUS;
            s_seconds_left = FOCUS_SECONDS;
            break;
        case POMO_FOCUS:
            s_state = POMO_PAUSED_FOCUS;
            break;
        case POMO_BREAK:
            s_state = POMO_PAUSED_BREAK;
            break;
        case POMO_PAUSED_FOCUS:
            s_state = POMO_FOCUS;
            break;
        case POMO_PAUSED_BREAK:
            s_state = POMO_BREAK;
            break;
    }
    refresh_all();
}

static void reset_cb(lv_event_t* /*e*/) {
    s_state = POMO_IDLE;
    s_seconds_left = FOCUS_SECONDS;
    refresh_all();
}

static void skip_cb(lv_event_t* /*e*/) {
    s_seconds_left = 1; // next tick rolls over to next phase
}

static void back_event_cb(lv_event_t* /*e*/) {
    if (s_tick)           { lv_timer_delete(s_tick);            s_tick = nullptr; }
    if (s_pet_anim_timer) { lv_timer_delete(s_pet_anim_timer);  s_pet_anim_timer = nullptr; }
    rgb_led_off();
    s_state_label = s_time_label = s_count_label = s_play_btn = s_play_lbl = nullptr;
    s_pet_img = nullptr;
    nav_pop(NAV_ANIM_SLIDE_RIGHT);
}

void pomodoro_show(void) {
    // Reset visible state on each open, but preserve completed count across sessions.
    s_state = POMO_IDLE;
    s_seconds_left = FOCUS_SECONDS;

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
    lv_label_set_text(title, "Pomodoro");
    lv_obj_set_style_text_color(title, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // State label
    s_state_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_state_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(s_state_label, LV_ALIGN_TOP_MID, 0, 56);

    // Big timer card (shrunk to make room for pet sprite)
    lv_obj_t* time_card = lv_obj_create(scr);
    lv_obj_set_size(time_card, 200, 76);
    lv_obj_align(time_card, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_add_style(time_card, &theme_style_card, LV_PART_MAIN);
    lv_obj_clear_flag(time_card, LV_OBJ_FLAG_SCROLLABLE);

    s_time_label = lv_label_create(time_card);
    lv_obj_set_style_text_color(s_time_label, theme_color_accent(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_center(s_time_label);

    // Pet sprite (animated, mirrors tamagotchi selection)
    load_pet_slug();
    s_pet_img = lv_image_create(scr);
    lv_obj_set_size(s_pet_img, PET_FRAME_W, PET_FRAME_H);
    lv_image_set_scale(s_pet_img, 768);   // 3x = 96x96
    lv_image_set_inner_align(s_pet_img, LV_IMAGE_ALIGN_TOP_LEFT);
    lv_obj_align(s_pet_img, LV_ALIGN_TOP_MID, 0, 168);

    // Count label
    s_count_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_count_label, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_count_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_count_label, LV_ALIGN_TOP_MID, 0, 230);

    // Button row: reset / play-pause / skip
    constexpr int BTN_W = 56;
    constexpr int BTN_H = 40;
    constexpr int GAP = 14;
    constexpr int row_w = BTN_W * 3 + GAP * 2;
    int sx = (240 - row_w) / 2;
    int sy = 268;

    auto make_btn = [&](int x, lv_event_cb_t cb, const char* sym, bool primary) -> lv_obj_t* {
        lv_obj_t* b = lv_button_create(scr);
        lv_obj_set_size(b, BTN_W, BTN_H);
        lv_obj_set_pos(b, x, sy);
        lv_obj_set_style_bg_color(b, primary ? theme_color_accent() : theme_color_card(), LV_PART_MAIN);
        lv_obj_set_style_radius(b, THEME_RADIUS_BUTTON, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, sym);
        lv_obj_set_style_text_color(l, primary ? theme_color_card() : theme_color_accent(), LV_PART_MAIN);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_center(l);
        return b;
    };

    make_btn(sx, reset_cb, LV_SYMBOL_REFRESH, false);
    s_play_btn = make_btn(sx + BTN_W + GAP, play_pause_cb, LV_SYMBOL_PLAY, true);
    make_btn(sx + (BTN_W + GAP) * 2, skip_cb, LV_SYMBOL_NEXT, false);

    s_play_lbl = lv_obj_get_child(s_play_btn, 0);

    refresh_all();
    s_tick = lv_timer_create(tick_cb, 1000, NULL);
    s_pet_anim_timer = lv_timer_create(pet_anim_tick_cb, 200, NULL);

    nav_push(scr, NAV_ANIM_SLIDE_LEFT);
}
