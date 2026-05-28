/**
 * @file tamagotchi.cpp
 * @brief Virtual pet — V1 core: stats, decay, actions, NVS persistence.
 *
 * Scope of this V1 (vs full TAMAGOTCHI_SPEC.md):
 *   - Adoption wizard: skipped, uses DEFAULT_PET_NAME from config.h
 *   - Pet sprite: loads idle/sad/sleep PNG from SD, 12-frame loop at 200ms
 *   - Background image: solid theme color (background sprite TBD)
 *   - Settings menu: not yet
 *
 * NVS namespace "tama":
 *   name        (string)
 *   hunger      (uint8 0..100)
 *   happiness   (uint8 0..100)
 *   energy      (uint8 0..100)
 *   cleanliness (uint8 0..100)
 *   last_unix   (uint32)
 */

#include "tamagotchi.h"

#include <Arduino.h>
#include <Preferences.h>
#include <lvgl.h>
#include <time.h>
#include <string.h>

#include "../../config.h"
#include "../../ui/theme.h"
#include "../../ui/nav.h"
#include "../../system/rgb_led.h"

static constexpr uint8_t STAT_MAX = 100;
static constexpr uint32_t MAX_DECAY_HOURS = 24;

struct PetState {
    char    name[16];
    char    slug[16];
    char    bg[24];        // e.g. "classic/02.png" or "xmas/05.png"
    uint8_t hunger;
    uint8_t happiness;
    uint8_t energy;
    uint8_t cleanliness;
    uint32_t last_unix;
    bool    initialized;   // false = first boot, run wizard
};

static PetState s_state;

static constexpr int CLASSIC_BG_COUNT = 20;
static constexpr int XMAS_BG_COUNT    = 5;
static constexpr int TOTAL_BG_COUNT   = CLASSIC_BG_COUNT + XMAS_BG_COUNT;

static char s_full_bg_path[96];

static const char* full_bg_path(const char* short_bg) {
    snprintf(s_full_bg_path, sizeof(s_full_bg_path),
             "S:/tamagotchi_sprites/cacaos_pet_assets/backgrounds/%s", short_bg);
    return s_full_bg_path;
}

static void short_bg_for_index(int idx, char* out, size_t out_sz) {
    if (idx < CLASSIC_BG_COUNT) {
        snprintf(out, out_sz, "classic/%02d.png", idx + 1);
    } else {
        snprintf(out, out_sz, "xmas/%02d.png", (idx - CLASSIC_BG_COUNT) + 1);
    }
}

// Pet variants — 9 picker slots + demonic locked (spec §7 achievement)
static const char* PET_SLUGS[] = {
    "white", "blackwhite", "brown",
    "brownwhite", "brown2color", "lightbrown",
    "grey", "black", "fantasy",
};
static constexpr int PET_COUNT = (int)(sizeof(PET_SLUGS) / sizeof(PET_SLUGS[0]));
static const char* PET_LABELS[PET_COUNT] = {
    "branquinha", "preto+branco", "marrom",
    "marrom+branco", "marrom 2 cor", "marromclaro",
    "cinza", "preto", "fantasia",
};

static int s_picker_selected = 0;

// Edit mode lets settings-driven pickers skip the wizard chain.
enum EditTarget { EDIT_NONE = 0, EDIT_PET, EDIT_NAME, EDIT_BG };
static EditTarget s_edit_target = EDIT_NONE;

// Forward declarations (definitions lower in the file)
static void show_main_screen(void);
static void wizard_show_welcome(void);
static void wizard_show_picker(void);
static void wizard_show_naming(void);
static void wizard_show_bg_picker(void);

// ---- Achievements ----
static bool ach_check(const char* slug) {
    Preferences p;
    if (!p.begin("tama", true)) return false;
    char key[16];
    snprintf(key, sizeof(key), "ach_%s", slug);
    bool earned = p.getBool(key, false);
    p.end();
    return earned;
}

static void ach_award(const char* slug) {
    Preferences p;
    if (!p.begin("tama", false)) return;
    char key[16];
    snprintf(key, sizeof(key), "ach_%s", slug);
    p.putBool(key, true);
    p.end();
}

static void toast_auto_dismiss_cb(lv_timer_t* t) {
    lv_obj_t* toast = (lv_obj_t*)lv_timer_get_user_data(t);
    if (toast) lv_obj_delete(toast);
    lv_timer_delete(t);
}

static void show_toast(const char* text) {
    lv_obj_t* parent = lv_screen_active();
    if (!parent) return;

    lv_obj_t* toast = lv_obj_create(parent);
    lv_obj_set_size(toast, 200, 40);
    lv_obj_align(toast, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_bg_color(toast, theme_color_accent(), LV_PART_MAIN);
    lv_obj_set_style_radius(toast, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(toast, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(toast, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(toast, 0, LV_PART_MAIN);
    lv_obj_clear_flag(toast, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(toast);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(lbl);

    lv_timer_t* t = lv_timer_create(toast_auto_dismiss_cb, 2500, toast);
    lv_timer_set_repeat_count(t, 1);
}

static bool ach_all_basic_earned(void) {
    return ach_check("feed") && ach_check("play") && ach_check("sleep") && ach_check("brush");
}

static void ach_trigger(const char* slug, const char* label) {
    if (ach_check(slug)) return;
    ach_award(slug);
    show_toast(label);
    // After awarding, check for the "complete set" meta-achievement
    if (ach_all_basic_earned() && !ach_check("complete_set")) {
        ach_award("complete_set");
        // Slight delay before second toast so they don't overlap
        // (cheap approach: just queue a follow-up via timer)
        lv_timer_t* delayed = lv_timer_create([](lv_timer_t* t) {
            show_toast("cuidadora exemplar!");
            lv_timer_delete(t);
        }, 2700, nullptr);
        lv_timer_set_repeat_count(delayed, 1);
    }
}

static lv_obj_t* s_name_label = nullptr;
static lv_obj_t* s_hunger_bar = nullptr;
static lv_obj_t* s_happy_bar  = nullptr;
static lv_obj_t* s_energy_bar = nullptr;
static lv_obj_t* s_clean_bar  = nullptr;
static lv_obj_t* s_hunger_val = nullptr;
static lv_obj_t* s_happy_val  = nullptr;
static lv_obj_t* s_energy_val = nullptr;
static lv_obj_t* s_clean_val  = nullptr;
static lv_obj_t* s_pet_img    = nullptr;
static lv_obj_t* s_status_text = nullptr;
static lv_timer_t* s_decay_timer = nullptr;
static lv_timer_t* s_anim_timer  = nullptr;
static int  s_anim_frame = 0;
static int  s_anim_frame_count = 12;   // recomputed each time the src changes
static char s_sprite_path[96];

static constexpr int SPRITE_FRAME_W = 32;
static constexpr int SPRITE_FRAME_H = 32;
static constexpr int SPRITE_DISPLAY_SCALE = 768;     // 3x — keeps pixel art crisp
static constexpr int SPRITE_DISPLAY_W = SPRITE_FRAME_W * SPRITE_DISPLAY_SCALE / 256;
static constexpr int SPRITE_DISPLAY_H = SPRITE_FRAME_H * SPRITE_DISPLAY_SCALE / 256;
static constexpr const char* BG_DEFAULT_SHORT = "classic/02.png";

static uint8_t clamp_stat(int v) {
    if (v < 0) return 0;
    if (v > STAT_MAX) return STAT_MAX;
    return (uint8_t)v;
}

static void load_state(void) {
    Preferences p;
    if (!p.begin("tama", true)) {
        // No NVS access; safe defaults, treat as uninitialized so wizard runs
        strncpy(s_state.name, DEFAULT_PET_NAME, sizeof(s_state.name) - 1);
        s_state.name[sizeof(s_state.name) - 1] = '\0';
        strncpy(s_state.slug, PET_SLUGS[0], sizeof(s_state.slug) - 1);
        s_state.slug[sizeof(s_state.slug) - 1] = '\0';
        strncpy(s_state.bg, BG_DEFAULT_SHORT, sizeof(s_state.bg) - 1);
        s_state.bg[sizeof(s_state.bg) - 1] = '\0';
        s_state.hunger = s_state.happiness = s_state.energy = s_state.cleanliness = 80;
        s_state.last_unix = 0;
        s_state.initialized = false;
        return;
    }
    String n = p.getString("name", DEFAULT_PET_NAME);
    strncpy(s_state.name, n.c_str(), sizeof(s_state.name) - 1);
    s_state.name[sizeof(s_state.name) - 1] = '\0';
    String slug = p.getString("slug", PET_SLUGS[0]);
    strncpy(s_state.slug, slug.c_str(), sizeof(s_state.slug) - 1);
    s_state.slug[sizeof(s_state.slug) - 1] = '\0';
    String bg = p.getString("bg", BG_DEFAULT_SHORT);
    strncpy(s_state.bg, bg.c_str(), sizeof(s_state.bg) - 1);
    s_state.bg[sizeof(s_state.bg) - 1] = '\0';
    s_state.hunger      = p.getUChar("hunger",      80);
    s_state.happiness   = p.getUChar("happiness",   80);
    s_state.energy      = p.getUChar("energy",      80);
    s_state.cleanliness = p.getUChar("clean",       80);
    s_state.last_unix   = p.getUInt("last_unix",    0);
    s_state.initialized = p.getBool("init",         false);
    p.end();
}

static void save_state(void) {
    Preferences p;
    if (!p.begin("tama", false)) return;
    p.putString("name", s_state.name);
    p.putString("slug", s_state.slug);
    p.putString("bg",   s_state.bg);
    p.putUChar("hunger",    s_state.hunger);
    p.putUChar("happiness", s_state.happiness);
    p.putUChar("energy",    s_state.energy);
    p.putUChar("clean",     s_state.cleanliness);
    p.putUInt("last_unix",  s_state.last_unix);
    p.putBool("init",       s_state.initialized);
    p.end();
}

// Apply decay for `hours_elapsed` real hours since last_unix.
static void apply_decay(uint32_t hours_elapsed) {
    if (hours_elapsed == 0) return;
    if (hours_elapsed > MAX_DECAY_HOURS) hours_elapsed = MAX_DECAY_HOURS;

    s_state.hunger      = clamp_stat((int)s_state.hunger      - (int)hours_elapsed);
    s_state.happiness   = clamp_stat((int)s_state.happiness   - (int)hours_elapsed);
    s_state.energy      = clamp_stat((int)s_state.energy      - (int)(hours_elapsed * 2));
    s_state.cleanliness = clamp_stat((int)s_state.cleanliness - (int)(hours_elapsed / 2));
}

static void update_last_unix_and_save(void) {
    time_t now = time(nullptr);
    s_state.last_unix = (now > 0) ? (uint32_t)now : s_state.last_unix;
    save_state();
}

static void catch_up_decay(void) {
    time_t now = time(nullptr);
    if (now <= 0 || s_state.last_unix == 0) {
        // Time not set or first run — just stamp now.
        if (now > 0) s_state.last_unix = (uint32_t)now;
        return;
    }
    uint32_t delta_s = (uint32_t)now - s_state.last_unix;
    uint32_t hours = delta_s / 3600;
    if (hours == 0) return;
    apply_decay(hours);
    s_state.last_unix += hours * 3600;
}

static const char* current_animation(void) {
    if (s_state.energy < 20) return "sleep";
    if (s_state.hunger < 20 || s_state.happiness < 20) return "sad";
    // idle in all other states: it's a stationary breathing loop. happy.png
    // is a hop/celebration that reads as "running" at small sizes.
    return "idle";
}

static const char* sprite_path_for(const char* slug, const char* anim) {
    snprintf(s_sprite_path, sizeof(s_sprite_path),
             "S:/tamagotchi_sprites/cacaos_pet_assets/pets/%s/%s.png",
             slug, anim);
    return s_sprite_path;
}

static bool any_stat_critical(void) {
    return s_state.hunger == 0 || s_state.happiness == 0 ||
           s_state.energy == 0 || s_state.cleanliness == 0;
}

static const char* status_message(void) {
    // Critical: any stat at 0 — most urgent voice
    if (s_state.hunger      == 0) return "Caca ta com MUITA fome!";
    if (s_state.cleanliness == 0) return "Caca precisa de banho ja!";
    if (s_state.energy      == 0) return "Caca ta exausto!";
    if (s_state.happiness   == 0) return "Caca ta muito triste...";
    // Low: nudges
    if (s_state.hunger      < 25) return "Caca ta com fominha...";
    if (s_state.happiness   < 25) return "Caca ta meio triste...";
    if (s_state.energy      < 25) return "Caca ta com sono...";
    if (s_state.cleanliness < 25) return "Caca precisa de banho...";
    // Happy
    if (s_state.hunger >= 80 && s_state.happiness >= 80) return "Caca ta feliz!";
    return "";
}

static void refresh_sprite_src(void) {
    if (!s_pet_img) return;
    sprite_path_for(s_state.slug, current_animation());
    lv_image_set_src(s_pet_img, s_sprite_path);
    // Frame count varies per animation: idle=12, sad=8, sleep=6, etc.
    int32_t src_w = lv_image_get_src_width(s_pet_img);
    s_anim_frame_count = (src_w > 0) ? (src_w / SPRITE_FRAME_W) : 1;
    if (s_anim_frame_count < 1) s_anim_frame_count = 1;
    s_anim_frame = 0;
    lv_obj_set_x(s_pet_img, 0);
}

static void refresh_ui(void) {
    if (s_name_label) lv_label_set_text(s_name_label, s_state.name);

    auto set_bar = [](lv_obj_t* bar, lv_obj_t* val_lbl, uint8_t v) {
        if (bar) lv_bar_set_value(bar, v, LV_ANIM_OFF);
        if (val_lbl) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%u%%", (unsigned)v);
            lv_label_set_text(val_lbl, buf);
        }
    };
    set_bar(s_hunger_bar, s_hunger_val, s_state.hunger);
    set_bar(s_happy_bar,  s_happy_val,  s_state.happiness);
    set_bar(s_energy_bar, s_energy_val, s_state.energy);
    set_bar(s_clean_bar,  s_clean_val,  s_state.cleanliness);

    refresh_sprite_src();

    // Mood-based LED: green (healthy), yellow (low), red (critical)
    if (any_stat_critical()) {
        rgb_led_set(220, 80, 80);
    } else {
        uint8_t lowest = s_state.hunger;
        if (s_state.happiness   < lowest) lowest = s_state.happiness;
        if (s_state.energy      < lowest) lowest = s_state.energy;
        if (s_state.cleanliness < lowest) lowest = s_state.cleanliness;
        if (lowest < 25)      rgb_led_set(220, 180, 60);   // warning yellow
        else if (lowest < 60) rgb_led_set(255, 143, 171);  // theme pink
        else                  rgb_led_set(120, 230, 140);  // happy green
    }

    if (s_status_text) {
        lv_label_set_text(s_status_text, status_message());
        if (any_stat_critical()) {
            // Strong visual emphasis when any stat zeros out
            lv_obj_set_style_bg_color(s_status_text, theme_color_accent(), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(s_status_text, LV_OPA_90, LV_PART_MAIN);
            lv_obj_set_style_text_color(s_status_text, theme_color_card(), LV_PART_MAIN);
            lv_obj_set_style_pad_hor(s_status_text, 10, LV_PART_MAIN);
            lv_obj_set_style_pad_ver(s_status_text, 4, LV_PART_MAIN);
            lv_obj_set_style_radius(s_status_text, 10, LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_opa(s_status_text, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_text_color(s_status_text, theme_color_accent(), LV_PART_MAIN);
            lv_obj_set_style_pad_hor(s_status_text, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_ver(s_status_text, 0, LV_PART_MAIN);
        }
    }
}

static void sprite_tick_cb(lv_timer_t* /*t*/) {
    if (!s_pet_img) return;
    s_anim_frame = (s_anim_frame + 1) % s_anim_frame_count;
    // Move the whole image left by one displayed-frame width. The parent
    // container clips to a single frame, so we slide the spritesheet
    // through that window. lv_image_set_offset_x doesn't reliably clip
    // when scale is active (it uses ext_draw_size and renders past obj).
    lv_obj_set_x(s_pet_img, -s_anim_frame * SPRITE_DISPLAY_W);
}

static void feed_cb(lv_event_t* /*e*/) {
    s_state.hunger      = clamp_stat((int)s_state.hunger + 20);
    s_state.cleanliness = clamp_stat((int)s_state.cleanliness - 2);
    update_last_unix_and_save();
    refresh_ui();
    ach_trigger("feed", "primeira refeicao!");
}

static void play_cb(lv_event_t* /*e*/) {
    s_state.happiness = clamp_stat((int)s_state.happiness + 15);
    s_state.energy    = clamp_stat((int)s_state.energy - 10);
    update_last_unix_and_save();
    refresh_ui();
    ach_trigger("play", "primeira brincadeira!");
}

static void sleep_cb(lv_event_t* /*e*/) {
    // V1 simplification: one-shot energy restore. Real "sleep over time"
    // will need a separate screen state — see TAMAGOTCHI_SPEC.md 2.2.
    s_state.energy = STAT_MAX;
    update_last_unix_and_save();
    refresh_ui();
    ach_trigger("sleep", "primeira soneca!");
}

static void brush_cb(lv_event_t* /*e*/) {
    s_state.cleanliness = clamp_stat((int)s_state.cleanliness + 15);
    s_state.happiness   = clamp_stat((int)s_state.happiness + 5);
    update_last_unix_and_save();
    refresh_ui();
    ach_trigger("brush", "primeira escovada!");
}

static void decay_tick_cb(lv_timer_t* /*t*/) {
    // Re-apply catch-up decay every 15 minutes (handles long open sessions).
    catch_up_decay();
    refresh_ui();
}

static void back_event_cb(lv_event_t* /*e*/) {
    update_last_unix_and_save();
    if (s_decay_timer) { lv_timer_delete(s_decay_timer); s_decay_timer = nullptr; }
    if (s_anim_timer)  { lv_timer_delete(s_anim_timer);  s_anim_timer  = nullptr; }
    rgb_led_off();
    s_name_label = s_hunger_bar = s_happy_bar = s_energy_bar = s_clean_bar = nullptr;
    s_hunger_val = s_happy_val = s_energy_val = s_clean_val = nullptr;
    s_pet_img = s_status_text = nullptr;
    nav_pop(NAV_ANIM_SLIDE_RIGHT);
}

static void confirm_reset_dialog(void);
static void show_settings_menu(void);

static void reset_btn_cb(lv_event_t* /*e*/) {
    show_settings_menu();
}

static void settings_close_cb(lv_event_t* e) {
    lv_obj_t* overlay = (lv_obj_t*)lv_event_get_user_data(e);
    if (overlay) lv_obj_delete(overlay);
}

static void settings_pick_pet_cb(lv_event_t* e) {
    lv_obj_t* overlay = (lv_obj_t*)lv_event_get_user_data(e);
    if (overlay) lv_obj_delete(overlay);
    s_edit_target = EDIT_PET;
    // Close current main screen and open picker as a fresh push
    if (s_decay_timer) { lv_timer_delete(s_decay_timer); s_decay_timer = nullptr; }
    if (s_anim_timer)  { lv_timer_delete(s_anim_timer);  s_anim_timer  = nullptr; }
    rgb_led_off();
    s_name_label = s_hunger_bar = s_happy_bar = s_energy_bar = s_clean_bar = nullptr;
    s_hunger_val = s_happy_val = s_energy_val = s_clean_val = nullptr;
    s_pet_img = s_status_text = nullptr;
    nav_pop(NAV_ANIM_NONE);
    wizard_show_picker();
}

static void settings_pick_name_cb(lv_event_t* e) {
    lv_obj_t* overlay = (lv_obj_t*)lv_event_get_user_data(e);
    if (overlay) lv_obj_delete(overlay);
    s_edit_target = EDIT_NAME;
    if (s_decay_timer) { lv_timer_delete(s_decay_timer); s_decay_timer = nullptr; }
    if (s_anim_timer)  { lv_timer_delete(s_anim_timer);  s_anim_timer  = nullptr; }
    rgb_led_off();
    s_name_label = s_hunger_bar = s_happy_bar = s_energy_bar = s_clean_bar = nullptr;
    s_hunger_val = s_happy_val = s_energy_val = s_clean_val = nullptr;
    s_pet_img = s_status_text = nullptr;
    nav_pop(NAV_ANIM_NONE);
    wizard_show_naming();
}

static void settings_pick_bg_cb(lv_event_t* e) {
    lv_obj_t* overlay = (lv_obj_t*)lv_event_get_user_data(e);
    if (overlay) lv_obj_delete(overlay);
    s_edit_target = EDIT_BG;
    if (s_decay_timer) { lv_timer_delete(s_decay_timer); s_decay_timer = nullptr; }
    if (s_anim_timer)  { lv_timer_delete(s_anim_timer);  s_anim_timer  = nullptr; }
    rgb_led_off();
    s_name_label = s_hunger_bar = s_happy_bar = s_energy_bar = s_clean_bar = nullptr;
    s_hunger_val = s_happy_val = s_energy_val = s_clean_val = nullptr;
    s_pet_img = s_status_text = nullptr;
    nav_pop(NAV_ANIM_NONE);
    wizard_show_bg_picker();
}

static void settings_pick_reset_cb(lv_event_t* e) {
    lv_obj_t* overlay = (lv_obj_t*)lv_event_get_user_data(e);
    if (overlay) lv_obj_delete(overlay);
    confirm_reset_dialog();
}

static void show_settings_menu(void) {
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
    lv_obj_set_size(card, 200, 230);
    lv_obj_center(card);
    lv_obj_add_style(card, &theme_style_card, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 12, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, "Ajustes da Caca");
    lv_obj_set_style_text_color(title, theme_color_accent(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    auto make_menu_btn = [&](int y_off, const char* text, lv_event_cb_t cb, bool destructive) {
        lv_obj_t* b = lv_button_create(card);
        lv_obj_set_size(b, 174, 32);
        lv_obj_align(b, LV_ALIGN_TOP_MID, 0, y_off);
        lv_obj_set_style_bg_color(b, destructive ? theme_color_text_light() : theme_color_card(),
                                  LV_PART_MAIN);
        lv_obj_set_style_border_width(b, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(b, theme_color_accent(), LV_PART_MAIN);
        lv_obj_set_style_radius(b, THEME_RADIUS_BUTTON, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, dim);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, text);
        lv_obj_set_style_text_color(l, theme_color_accent(), LV_PART_MAIN);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_center(l);
    };

    make_menu_btn(34,  "trocar pet",       settings_pick_pet_cb,   false);
    make_menu_btn(70,  "renomear",         settings_pick_name_cb,  false);
    make_menu_btn(106, "trocar quarto",    settings_pick_bg_cb,    false);
    make_menu_btn(146, "resetar tudo",     settings_pick_reset_cb, true);

    // Close button at the bottom
    lv_obj_t* close = lv_button_create(card);
    lv_obj_set_size(close, 174, 28);
    lv_obj_align(close, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(close, theme_color_text_light(), LV_PART_MAIN);
    lv_obj_set_style_radius(close, THEME_RADIUS_BUTTON, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(close, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(close, settings_close_cb, LV_EVENT_CLICKED, dim);
    lv_obj_t* close_lbl = lv_label_create(close);
    lv_label_set_text(close_lbl, "fechar");
    lv_obj_set_style_text_color(close_lbl, theme_color_text(), LV_PART_MAIN);
    lv_obj_center(close_lbl);
}

static void confirm_reset_yes_cb(lv_event_t* e) {
    lv_obj_t* msgbox = (lv_obj_t*)lv_event_get_user_data(e);
    if (msgbox) lv_obj_delete(msgbox);

    s_state.initialized = false;
    save_state();
    // Force back to homescreen so re-entering the app triggers the wizard
    back_event_cb(nullptr);
}

static void confirm_reset_no_cb(lv_event_t* e) {
    lv_obj_t* msgbox = (lv_obj_t*)lv_event_get_user_data(e);
    if (msgbox) lv_obj_delete(msgbox);
}

static void confirm_reset_dialog(void) {
    lv_obj_t* parent = lv_screen_active();

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
    lv_label_set_text(msg, "Resetar pet?\n(volta pra escolha de Caca)");
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
    lv_obj_add_event_cb(no_btn, confirm_reset_no_cb, LV_EVENT_CLICKED, dim);
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
    lv_obj_add_event_cb(yes_btn, confirm_reset_yes_cb, LV_EVENT_CLICKED, dim);
    lv_obj_t* yes_lbl = lv_label_create(yes_btn);
    lv_label_set_text(yes_lbl, "resetar");
    lv_obj_set_style_text_color(yes_lbl, theme_color_card(), LV_PART_MAIN);
    lv_obj_center(yes_lbl);
}

static lv_obj_t* make_stat_row(lv_obj_t* parent, int y, const char* label_text,
                               uint32_t color_hex, lv_obj_t** out_bar, lv_obj_t** out_val) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, 220, 22);
    lv_obj_set_pos(row, 10, y);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* icon = lv_label_create(row);
    lv_label_set_text(icon, label_text);
    lv_obj_set_style_text_color(icon, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* bar = lv_bar_create(row);
    lv_obj_set_size(bar, 110, 12);
    lv_obj_align(bar, LV_ALIGN_LEFT_MID, 60, 0);
    lv_bar_set_range(bar, 0, STAT_MAX);
    lv_obj_set_style_bg_color(bar, theme_color_text_light(), LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(color_hex), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 6, LV_PART_INDICATOR);

    lv_obj_t* val = lv_label_create(row);
    lv_obj_set_style_text_color(val, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(val, LV_ALIGN_RIGHT_MID, 0, 0);

    *out_bar = bar;
    *out_val = val;
    return row;
}

static lv_obj_t* make_action_btn(lv_obj_t* parent, int x, int y, const char* sym, lv_event_cb_t cb) {
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_size(btn, 46, 38);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, theme_color_accent(), LV_PART_MAIN);
    lv_obj_set_style_radius(btn, THEME_RADIUS_BUTTON, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, sym);
    lv_obj_set_style_text_color(lbl, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_center(lbl);
    return btn;
}

static void show_main_screen(void);
static void wizard_show_welcome(void);
static void wizard_show_picker(void);
static void wizard_show_naming(void);
static void wizard_show_bg_picker(void);

void tamagotchi_show(void) {
    load_state();

    if (!s_state.initialized) {
        wizard_show_welcome();
        return;
    }

    catch_up_decay();
    show_main_screen();
}

static void show_main_screen(void) {
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

    s_name_label = lv_label_create(header);
    lv_obj_set_style_text_color(s_name_label, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_name_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(s_name_label, LV_ALIGN_CENTER, 0, 0);

    // Reset/settings button (right side of header)
    lv_obj_t* reset_btn = lv_button_create(header);
    lv_obj_set_size(reset_btn, 36, 28);
    lv_obj_align(reset_btn, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_bg_color(reset_btn, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_radius(reset_btn, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(reset_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(reset_btn, reset_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* gear = lv_label_create(reset_btn);
    lv_label_set_text(gear, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(gear, theme_color_accent(), LV_PART_MAIN);
    lv_obj_center(gear);

    // Stat bars
    make_stat_row(scr, 48,  "fome",   0xFF8FAB, &s_hunger_bar, &s_hunger_val);
    make_stat_row(scr, 74,  "feliz",  0xFB6F92, &s_happy_bar,  &s_happy_val);
    make_stat_row(scr, 100, "ener",   0xFFD166, &s_energy_bar, &s_energy_val);
    make_stat_row(scr, 126, "limpa",  0x9AE5E0, &s_clean_bar,  &s_clean_val);

    // Pet area: background image fills container, sprite on top, status below
    lv_obj_t* pet_card = lv_obj_create(scr);
    lv_obj_set_size(pet_card, 240, 100);
    lv_obj_align(pet_card, LV_ALIGN_TOP_MID, 0, 152);
    lv_obj_set_style_bg_color(pet_card, theme_color_text_light(), LV_PART_MAIN);
    lv_obj_set_style_border_width(pet_card, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(pet_card, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(pet_card, 0, LV_PART_MAIN);
    lv_obj_clear_flag(pet_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* bg_img = lv_image_create(pet_card);
    lv_obj_set_size(bg_img, 240, 100);
    lv_image_set_src(bg_img, full_bg_path(s_state.bg));
    lv_image_set_inner_align(bg_img, LV_IMAGE_ALIGN_BOTTOM_MID);
    lv_obj_align(bg_img, LV_ALIGN_CENTER, 0, 0);

    // Clipping container: parent clips children to its content area, so the
    // scaled spritesheet (rendered as 1152x96 for a 12-frame sheet) only
    // shows one frame at a time. The image obj sits inside at x=0 and is
    // shifted left via lv_obj_set_x during animation.
    lv_obj_t* sprite_box = lv_obj_create(pet_card);
    lv_obj_set_size(sprite_box, SPRITE_DISPLAY_W, SPRITE_DISPLAY_H);
    lv_obj_set_style_bg_opa(sprite_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(sprite_box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(sprite_box, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(sprite_box, 0, LV_PART_MAIN);
    lv_obj_clear_flag(sprite_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(sprite_box);

    s_pet_img = lv_image_create(sprite_box);
    lv_image_set_pivot(s_pet_img, 0, 0);    // scale anchored at top-left
    lv_image_set_scale(s_pet_img, SPRITE_DISPLAY_SCALE);
    lv_obj_set_pos(s_pet_img, 0, 0);

    s_status_text = lv_label_create(scr);
    lv_obj_set_style_text_color(s_status_text, theme_color_accent(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_status_text, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_status_text, LV_ALIGN_TOP_MID, 0, 256);

    // Action buttons
    constexpr int BTN_W = 46;
    constexpr int GAP = 8;
    constexpr int row_w = BTN_W * 4 + GAP * 3;
    int sx = (240 - row_w) / 2;
    int sy = 278;

    make_action_btn(scr, sx,                            sy, "feed",  feed_cb);
    make_action_btn(scr, sx + (BTN_W + GAP),            sy, "play",  play_cb);
    make_action_btn(scr, sx + (BTN_W + GAP) * 2,        sy, "zzz",   sleep_cb);
    make_action_btn(scr, sx + (BTN_W + GAP) * 3,        sy, "bath",  brush_cb);

    refresh_ui();

    // Re-apply decay every 15 minutes while screen is open
    s_decay_timer = lv_timer_create(decay_tick_cb, 15UL * 60UL * 1000UL, NULL);
    // Sprite animation at 200ms/frame (5 FPS — matches spec)
    s_anim_timer = lv_timer_create(sprite_tick_cb, 200, NULL);

    nav_push(scr, NAV_ANIM_SLIDE_LEFT);
}

// ============================================================
// Adoption wizard
// ============================================================

static void wizard_welcome_back_cb(lv_event_t* /*e*/) {
    // User backed out of wizard — return to homescreen, keep first_boot=true
    nav_pop(NAV_ANIM_SLIDE_RIGHT);
}

static void wizard_welcome_next_cb(lv_event_t* /*e*/) {
    // Replace current screen with picker
    nav_pop(NAV_ANIM_NONE);
    wizard_show_picker();
}

static void wizard_show_welcome(void) {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, theme_color_bg(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Header with back
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
    lv_obj_add_event_cb(back_btn, wizard_welcome_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_lbl, theme_color_accent(), LV_PART_MAIN);
    lv_obj_center(back_lbl);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Boas-vindas");
    lv_obj_set_style_text_color(title, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Big card with bunny sprite + welcome text
    lv_obj_t* card = lv_obj_create(scr);
    lv_obj_set_size(card, 220, 200);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_add_style(card, &theme_style_card, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 16, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // Static welcome bunny: clip container + image at frame 0 (no animation).
    // 4x scale ran into the message; 2.5x leaves a comfortable gap.
    constexpr int WELCOME_SCALE = 640;  // 2.5x
    constexpr int WELCOME_W = SPRITE_FRAME_W * WELCOME_SCALE / 256;
    constexpr int WELCOME_H = SPRITE_FRAME_H * WELCOME_SCALE / 256;
    lv_obj_t* bunny_box = lv_obj_create(card);
    lv_obj_set_size(bunny_box, WELCOME_W, WELCOME_H);
    lv_obj_set_style_bg_opa(bunny_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(bunny_box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bunny_box, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bunny_box, 0, LV_PART_MAIN);
    lv_obj_clear_flag(bunny_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(bunny_box, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t* bunny = lv_image_create(bunny_box);
    lv_image_set_src(bunny, "S:/tamagotchi_sprites/cacaos_pet_assets/pets/white/idle.png");
    lv_image_set_pivot(bunny, 0, 0);
    lv_image_set_scale(bunny, WELCOME_SCALE);
    lv_obj_set_pos(bunny, 0, 0);

    lv_obj_t* msg = lv_label_create(card);
    lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg, 180);
    lv_label_set_text(msg, "voce vai cuidar de uma\ncaquinha pixel\nentao escolhe um pra ser seu");
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(msg, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(msg, LV_ALIGN_BOTTOM_MID, 0, -4);

    // Adoption button
    lv_obj_t* btn = lv_button_create(scr);
    lv_obj_set_size(btn, 200, 44);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_add_style(btn, &theme_style_button_primary, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, wizard_welcome_next_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "Adotar uma Caca " LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(btn_lbl, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_center(btn_lbl);

    nav_push(scr, NAV_ANIM_SLIDE_LEFT);
}

// ----- Picker -----
static lv_obj_t* s_picker_cards[PET_COUNT] = {nullptr};
static lv_obj_t* s_picker_preview_name = nullptr;
static char s_picker_paths[PET_COUNT][96];  // persistent paths for lv_image src

static void picker_repaint_selection(void) {
    for (int i = 0; i < PET_COUNT; ++i) {
        if (!s_picker_cards[i]) continue;
        bool sel = (i == s_picker_selected);
        lv_obj_set_style_border_width(s_picker_cards[i], sel ? 3 : 0, LV_PART_MAIN);
        lv_obj_set_style_border_color(s_picker_cards[i], theme_color_accent(), LV_PART_MAIN);
    }
    if (s_picker_preview_name) {
        lv_label_set_text(s_picker_preview_name, PET_LABELS[s_picker_selected]);
    }
}

static void picker_card_cb(lv_event_t* e) {
    lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
    intptr_t idx = (intptr_t)lv_obj_get_user_data(obj);
    s_picker_selected = (int)idx;
    picker_repaint_selection();
}

static void picker_back_cb(lv_event_t* /*e*/) {
    for (int i = 0; i < PET_COUNT; ++i) s_picker_cards[i] = nullptr;
    s_picker_preview_name = nullptr;
    nav_pop(NAV_ANIM_NONE);
    wizard_show_welcome();
}

static void picker_confirm_cb(lv_event_t* /*e*/) {
    strncpy(s_state.slug, PET_SLUGS[s_picker_selected], sizeof(s_state.slug) - 1);
    s_state.slug[sizeof(s_state.slug) - 1] = '\0';

    for (int i = 0; i < PET_COUNT; ++i) s_picker_cards[i] = nullptr;
    s_picker_preview_name = nullptr;

    if (s_edit_target == EDIT_PET) {
        s_edit_target = EDIT_NONE;
        save_state();
        nav_pop(NAV_ANIM_NONE);
        show_main_screen();
    } else {
        nav_pop(NAV_ANIM_NONE);
        wizard_show_naming();
    }
}

static void wizard_show_picker(void) {
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
    lv_obj_add_event_cb(back_btn, picker_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_lbl, theme_color_accent(), LV_PART_MAIN);
    lv_obj_center(back_lbl);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Sua Caca");
    lv_obj_set_style_text_color(title, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // 3x3 grid of pet cards
    constexpr int CARD_W = 64;
    constexpr int CARD_H = 64;
    constexpr int GAP = 6;
    constexpr int GRID_W = CARD_W * 3 + GAP * 2;
    int gx = (240 - GRID_W) / 2;
    int gy = 50;

    for (int i = 0; i < PET_COUNT; ++i) {
        int col = i % 3;
        int row = i / 3;
        lv_obj_t* card = lv_obj_create(scr);
        lv_obj_set_size(card, CARD_W, CARD_H);
        lv_obj_set_pos(card, gx + col * (CARD_W + GAP), gy + row * (CARD_H + GAP));
        lv_obj_set_style_bg_color(card, theme_color_card(), LV_PART_MAIN);
        lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(card, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(card, (void*)(intptr_t)i);
        lv_obj_add_event_cb(card, picker_card_cb, LV_EVENT_CLICKED, NULL);

        // Clip container so the 1.5x-scaled spritesheet only shows frame 0.
        constexpr int PICKER_SCALE = 384;  // 1.5x
        constexpr int PICKER_W = SPRITE_FRAME_W * PICKER_SCALE / 256;
        constexpr int PICKER_H = SPRITE_FRAME_H * PICKER_SCALE / 256;
        lv_obj_t* sprite_box = lv_obj_create(card);
        lv_obj_set_size(sprite_box, PICKER_W, PICKER_H);
        lv_obj_set_style_bg_opa(sprite_box, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(sprite_box, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(sprite_box, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(sprite_box, 0, LV_PART_MAIN);
        lv_obj_clear_flag(sprite_box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(sprite_box, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_center(sprite_box);

        lv_obj_t* sprite = lv_image_create(sprite_box);
        snprintf(s_picker_paths[i], sizeof(s_picker_paths[i]),
                 "S:/tamagotchi_sprites/cacaos_pet_assets/pets/%s/idle.png",
                 PET_SLUGS[i]);
        lv_image_set_src(sprite, s_picker_paths[i]);
        lv_image_set_pivot(sprite, 0, 0);
        lv_image_set_scale(sprite, PICKER_SCALE);
        lv_obj_set_pos(sprite, 0, 0);

        s_picker_cards[i] = card;
    }

    // Selected pet name preview
    s_picker_preview_name = lv_label_create(scr);
    lv_obj_set_style_text_color(s_picker_preview_name, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_picker_preview_name, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_picker_preview_name, LV_ALIGN_TOP_MID, 0, 248);

    // Confirm button
    lv_obj_t* confirm = lv_button_create(scr);
    lv_obj_set_size(confirm, 200, 44);
    lv_obj_align(confirm, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_add_style(confirm, &theme_style_button_primary, LV_PART_MAIN);
    lv_obj_add_event_cb(confirm, picker_confirm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* clbl = lv_label_create(confirm);
    lv_label_set_text(clbl, "Escolher " LV_SYMBOL_OK);
    lv_obj_set_style_text_color(clbl, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(clbl, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_center(clbl);

    // Initialize selection to first pet
    s_picker_selected = 0;
    picker_repaint_selection();

    nav_push(scr, NAV_ANIM_SLIDE_LEFT);
}

// ----- Naming -----
static lv_obj_t* s_name_input = nullptr;
static char s_name_sprite_path[96];

static void naming_back_cb(lv_event_t* /*e*/) {
    s_name_input = nullptr;
    nav_pop(NAV_ANIM_NONE);
    wizard_show_picker();
}

static void naming_confirm_cb(lv_event_t* /*e*/) {
    if (!s_name_input) return;
    const char* typed = lv_textarea_get_text(s_name_input);
    if (!typed || typed[0] == '\0') typed = DEFAULT_PET_NAME;
    strncpy(s_state.name, typed, sizeof(s_state.name) - 1);
    s_state.name[sizeof(s_state.name) - 1] = '\0';

    s_name_input = nullptr;

    if (s_edit_target == EDIT_NAME) {
        s_edit_target = EDIT_NONE;
        save_state();
        nav_pop(NAV_ANIM_NONE);
        show_main_screen();
    } else {
        nav_pop(NAV_ANIM_NONE);
        wizard_show_bg_picker();
    }
}

static void naming_ready_cb(lv_event_t* e) {
    // Triggered when keyboard's "ok" key is pressed
    if (lv_event_get_code(e) == LV_EVENT_READY) {
        naming_confirm_cb(e);
    }
}

static void wizard_show_naming(void) {
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
    lv_obj_add_event_cb(back_btn, naming_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_lbl, theme_color_accent(), LV_PART_MAIN);
    lv_obj_center(back_lbl);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Nome da pet");
    lv_obj_set_style_text_color(title, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Selected pet preview (clip container + frame 0 at 2x)
    constexpr int NAMING_SCALE = 512;  // 2x
    constexpr int NAMING_W = SPRITE_FRAME_W * NAMING_SCALE / 256;
    constexpr int NAMING_H = SPRITE_FRAME_H * NAMING_SCALE / 256;
    lv_obj_t* preview_box = lv_obj_create(scr);
    lv_obj_set_size(preview_box, NAMING_W, NAMING_H);
    lv_obj_set_style_bg_opa(preview_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(preview_box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(preview_box, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(preview_box, 0, LV_PART_MAIN);
    lv_obj_clear_flag(preview_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(preview_box, LV_ALIGN_TOP_MID, 0, 50);

    lv_obj_t* preview = lv_image_create(preview_box);
    snprintf(s_name_sprite_path, sizeof(s_name_sprite_path),
             "S:/tamagotchi_sprites/cacaos_pet_assets/pets/%s/idle.png", s_state.slug);
    lv_image_set_src(preview, s_name_sprite_path);
    lv_image_set_pivot(preview, 0, 0);
    lv_image_set_scale(preview, NAMING_SCALE);
    lv_obj_set_pos(preview, 0, 0);

    // Textarea with default name
    s_name_input = lv_textarea_create(scr);
    lv_obj_set_size(s_name_input, 200, 40);
    lv_obj_align(s_name_input, LV_ALIGN_TOP_MID, 0, 120);
    lv_textarea_set_one_line(s_name_input, true);
    lv_textarea_set_max_length(s_name_input, 12);
    lv_textarea_set_text(s_name_input, DEFAULT_PET_NAME);
    lv_textarea_set_placeholder_text(s_name_input, "como ela se chama?");
    lv_obj_set_style_text_font(s_name_input, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_add_event_cb(s_name_input, naming_ready_cb, LV_EVENT_READY, NULL);

    // Keyboard (bottom half)
    lv_obj_t* kb = lv_keyboard_create(scr);
    lv_obj_set_size(kb, 240, 150);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb, s_name_input);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);

    nav_push(scr, NAV_ANIM_SLIDE_LEFT);
}

// ----- Background picker -----
static int s_bg_picker_idx = 0;
static lv_obj_t* s_bg_preview = nullptr;
static lv_obj_t* s_bg_label = nullptr;
static char s_bg_picker_short[24];

static void bg_picker_refresh_preview(void) {
    short_bg_for_index(s_bg_picker_idx, s_bg_picker_short, sizeof(s_bg_picker_short));
    if (s_bg_preview) lv_image_set_src(s_bg_preview, full_bg_path(s_bg_picker_short));
    if (s_bg_label) {
        char buf[24];
        if (s_bg_picker_idx < CLASSIC_BG_COUNT) {
            snprintf(buf, sizeof(buf), "Classic %d/%d", s_bg_picker_idx + 1, CLASSIC_BG_COUNT);
        } else {
            snprintf(buf, sizeof(buf), "Xmas %d/%d",
                     s_bg_picker_idx - CLASSIC_BG_COUNT + 1, XMAS_BG_COUNT);
        }
        lv_label_set_text(s_bg_label, buf);
    }
}

static void bg_picker_prev_cb(lv_event_t* /*e*/) {
    s_bg_picker_idx = (s_bg_picker_idx - 1 + TOTAL_BG_COUNT) % TOTAL_BG_COUNT;
    bg_picker_refresh_preview();
}

static void bg_picker_next_cb(lv_event_t* /*e*/) {
    s_bg_picker_idx = (s_bg_picker_idx + 1) % TOTAL_BG_COUNT;
    bg_picker_refresh_preview();
}

static void bg_picker_back_cb(lv_event_t* /*e*/) {
    s_bg_preview = s_bg_label = nullptr;
    nav_pop(NAV_ANIM_NONE);
    wizard_show_naming();
}

static void bg_picker_confirm_cb(lv_event_t* /*e*/) {
    short_bg_for_index(s_bg_picker_idx, s_state.bg, sizeof(s_state.bg));
    if (s_edit_target == EDIT_BG) {
        s_edit_target = EDIT_NONE;
    } else {
        // First-boot wizard finished here
        s_state.initialized = true;
    }
    update_last_unix_and_save();

    s_bg_preview = s_bg_label = nullptr;
    nav_pop(NAV_ANIM_NONE);
    show_main_screen();
}

static void wizard_show_bg_picker(void) {
    // Start carousel at current selection (so settings flow lands on saved bg)
    s_bg_picker_idx = 1; // classic/02 default
    for (int i = 0; i < TOTAL_BG_COUNT; ++i) {
        char tmp[24];
        short_bg_for_index(i, tmp, sizeof(tmp));
        if (strcmp(tmp, s_state.bg) == 0) { s_bg_picker_idx = i; break; }
    }

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
    lv_obj_add_event_cb(back_btn, bg_picker_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_lbl, theme_color_accent(), LV_PART_MAIN);
    lv_obj_center(back_lbl);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Quartinho");
    lv_obj_set_style_text_color(title, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Preview frame (containers padded so the 180x320 bg fits nicely)
    lv_obj_t* preview_frame = lv_obj_create(scr);
    lv_obj_set_size(preview_frame, 200, 180);
    lv_obj_align(preview_frame, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_bg_color(preview_frame, theme_color_text_light(), LV_PART_MAIN);
    lv_obj_set_style_border_width(preview_frame, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(preview_frame, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(preview_frame, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(preview_frame, 0, LV_PART_MAIN);
    lv_obj_clear_flag(preview_frame, LV_OBJ_FLAG_SCROLLABLE);

    s_bg_preview = lv_image_create(preview_frame);
    lv_obj_set_size(s_bg_preview, 200, 180);
    lv_image_set_inner_align(s_bg_preview, LV_IMAGE_ALIGN_CONTAIN);
    lv_obj_center(s_bg_preview);

    // Label
    s_bg_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_bg_label, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_bg_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_bg_label, LV_ALIGN_TOP_MID, 0, 234);

    // Prev / next
    auto make_nav = [&](int x, lv_event_cb_t cb, const char* sym) {
        lv_obj_t* b = lv_button_create(scr);
        lv_obj_set_size(b, 50, 40);
        lv_obj_set_pos(b, x, 260);
        lv_obj_set_style_bg_color(b, theme_color_card(), LV_PART_MAIN);
        lv_obj_set_style_radius(b, THEME_RADIUS_BUTTON, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, sym);
        lv_obj_set_style_text_color(l, theme_color_accent(), LV_PART_MAIN);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_center(l);
    };
    make_nav(14,  bg_picker_prev_cb, LV_SYMBOL_LEFT);
    make_nav(176, bg_picker_next_cb, LV_SYMBOL_RIGHT);

    // Confirm
    lv_obj_t* confirm = lv_button_create(scr);
    lv_obj_set_size(confirm, 100, 40);
    lv_obj_align(confirm, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_style(confirm, &theme_style_button_primary, LV_PART_MAIN);
    lv_obj_add_event_cb(confirm, bg_picker_confirm_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* clbl = lv_label_create(confirm);
    lv_label_set_text(clbl, "Pronto! " LV_SYMBOL_OK);
    lv_obj_set_style_text_color(clbl, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(clbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(clbl);

    bg_picker_refresh_preview();
    nav_push(scr, NAV_ANIM_SLIDE_LEFT);
}
