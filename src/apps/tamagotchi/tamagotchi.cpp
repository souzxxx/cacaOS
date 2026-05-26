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

static constexpr uint8_t STAT_MAX = 100;
static constexpr uint32_t MAX_DECAY_HOURS = 24;

struct PetState {
    char    name[16];
    char    slug[16];
    uint8_t hunger;
    uint8_t happiness;
    uint8_t energy;
    uint8_t cleanliness;
    uint32_t last_unix;
    bool    initialized;   // false = first boot, run wizard
};

static PetState s_state;

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
static char s_sprite_path[96];

static constexpr int SPRITE_FRAME_W = 32;
static constexpr int SPRITE_FRAME_H = 32;
static constexpr int SPRITE_FRAME_COUNT = 12;
static constexpr const char* BG_PATH_DEFAULT =
    "S:/tamagotchi_sprites/cacaos_pet_assets/backgrounds/classic/02.png";

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
    if (s_state.happiness >= 80 && s_state.hunger >= 80) return "happy";
    return "idle";
}

static const char* sprite_path_for(const char* slug, const char* anim) {
    snprintf(s_sprite_path, sizeof(s_sprite_path),
             "S:/tamagotchi_sprites/cacaos_pet_assets/pets/%s/%s.png",
             slug, anim);
    return s_sprite_path;
}

static const char* status_message(void) {
    if (s_state.hunger      < 25) return "Caca ta com fominha...";
    if (s_state.happiness   < 25) return "Caca ta meio triste...";
    if (s_state.energy      < 25) return "Caca ta com sono...";
    if (s_state.cleanliness < 25) return "Caca precisa de banho...";
    if (s_state.hunger >= 80 && s_state.happiness >= 80) return "Caca ta feliz!";
    return "";
}

static void refresh_sprite_src(void) {
    if (!s_pet_img) return;
    sprite_path_for(s_state.slug, current_animation());
    lv_image_set_src(s_pet_img, s_sprite_path);
    s_anim_frame = 0;
    lv_image_set_offset_x(s_pet_img, 0);
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
    if (s_status_text) lv_label_set_text(s_status_text, status_message());
}

static void sprite_tick_cb(lv_timer_t* /*t*/) {
    if (!s_pet_img) return;
    s_anim_frame = (s_anim_frame + 1) % SPRITE_FRAME_COUNT;
    lv_image_set_offset_x(s_pet_img, -(s_anim_frame * SPRITE_FRAME_W));
}

static void feed_cb(lv_event_t* /*e*/) {
    s_state.hunger      = clamp_stat((int)s_state.hunger + 20);
    s_state.cleanliness = clamp_stat((int)s_state.cleanliness - 2);
    update_last_unix_and_save();
    refresh_ui();
}

static void play_cb(lv_event_t* /*e*/) {
    s_state.happiness = clamp_stat((int)s_state.happiness + 15);
    s_state.energy    = clamp_stat((int)s_state.energy - 10);
    update_last_unix_and_save();
    refresh_ui();
}

static void sleep_cb(lv_event_t* /*e*/) {
    // V1 simplification: one-shot energy restore. Real "sleep over time"
    // will need a separate screen state — see TAMAGOTCHI_SPEC.md 2.2.
    s_state.energy = STAT_MAX;
    update_last_unix_and_save();
    refresh_ui();
}

static void brush_cb(lv_event_t* /*e*/) {
    s_state.cleanliness = clamp_stat((int)s_state.cleanliness + 15);
    s_state.happiness   = clamp_stat((int)s_state.happiness + 5);
    update_last_unix_and_save();
    refresh_ui();
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
    s_name_label = s_hunger_bar = s_happy_bar = s_energy_bar = s_clean_bar = nullptr;
    s_hunger_val = s_happy_val = s_energy_val = s_clean_val = nullptr;
    s_pet_img = s_status_text = nullptr;
    nav_pop(NAV_ANIM_SLIDE_RIGHT);
}

static void confirm_reset_dialog(void);

static void reset_btn_cb(lv_event_t* /*e*/) {
    confirm_reset_dialog();
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
    lv_image_set_src(bg_img, BG_PATH_DEFAULT);
    lv_image_set_inner_align(bg_img, LV_IMAGE_ALIGN_BOTTOM_MID);
    lv_obj_align(bg_img, LV_ALIGN_CENTER, 0, 0);

    s_pet_img = lv_image_create(pet_card);
    lv_obj_set_size(s_pet_img, SPRITE_FRAME_W, SPRITE_FRAME_H);
    lv_image_set_scale(s_pet_img, 768);   // 3x = 96x96 visible
    lv_image_set_inner_align(s_pet_img, LV_IMAGE_ALIGN_TOP_LEFT);
    lv_obj_align(s_pet_img, LV_ALIGN_CENTER, 0, 0);

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

    lv_obj_t* bunny = lv_image_create(card);
    lv_image_set_src(bunny, "S:/tamagotchi_sprites/cacaos_pet_assets/pets/white/idle.png");
    lv_obj_set_size(bunny, SPRITE_FRAME_W, SPRITE_FRAME_H);
    lv_image_set_scale(bunny, 1024);
    lv_image_set_inner_align(bunny, LV_IMAGE_ALIGN_TOP_LEFT);
    lv_obj_align(bunny, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* msg = lv_label_create(card);
    lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg, 180);
    lv_label_set_text(msg, "voce vai cuidar de uma\ncaquinha pixel\nentao escolhe um pra ser seu");
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(msg, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(msg, LV_ALIGN_BOTTOM_MID, 0, 0);

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
    // Persist slug, then go to naming screen
    strncpy(s_state.slug, PET_SLUGS[s_picker_selected], sizeof(s_state.slug) - 1);
    s_state.slug[sizeof(s_state.slug) - 1] = '\0';

    for (int i = 0; i < PET_COUNT; ++i) s_picker_cards[i] = nullptr;
    s_picker_preview_name = nullptr;

    nav_pop(NAV_ANIM_NONE);
    wizard_show_naming();
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
    lv_label_set_text(title, "Escolha sua Caca");
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

        lv_obj_t* sprite = lv_image_create(card);
        snprintf(s_picker_paths[i], sizeof(s_picker_paths[i]),
                 "S:/tamagotchi_sprites/cacaos_pet_assets/pets/%s/idle.png",
                 PET_SLUGS[i]);
        lv_image_set_src(sprite, s_picker_paths[i]);
        lv_obj_set_size(sprite, SPRITE_FRAME_W, SPRITE_FRAME_H);
        lv_image_set_scale(sprite, 384);  // 1.5x = 48x48
        lv_image_set_inner_align(sprite, LV_IMAGE_ALIGN_TOP_LEFT);
        lv_obj_center(sprite);

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
    s_state.initialized = true;
    update_last_unix_and_save();

    s_name_input = nullptr;
    nav_pop(NAV_ANIM_NONE);
    show_main_screen();
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

    // Selected pet preview (small sprite frame 0)
    lv_obj_t* preview = lv_image_create(scr);
    snprintf(s_name_sprite_path, sizeof(s_name_sprite_path),
             "S:/tamagotchi_sprites/cacaos_pet_assets/pets/%s/idle.png", s_state.slug);
    lv_image_set_src(preview, s_name_sprite_path);
    lv_obj_set_size(preview, SPRITE_FRAME_W, SPRITE_FRAME_H);
    lv_image_set_scale(preview, 512);   // 2x
    lv_image_set_inner_align(preview, LV_IMAGE_ALIGN_TOP_LEFT);
    lv_obj_align(preview, LV_ALIGN_TOP_MID, 0, 50);

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
