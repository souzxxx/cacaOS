/**
 * @file wifi_config.cpp
 * @brief On-device WiFi network picker + password entry. Device-only (uses
 *        WiFi.*); excluded from the sim build.
 */

#include "wifi_config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>
#include <stdlib.h>
#include <string.h>

#include "../../ui/theme.h"
#include "../../ui/nav.h"
#include "../../system/wifi_mgr.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr char MANUAL_SSID_LABEL[] = "Outra (digitar)";

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------
static lv_obj_t*   s_list        = nullptr;  // scrollable network list
static lv_obj_t*   s_status      = nullptr;  // "procurando…" / errors
static lv_timer_t* s_scan_timer  = nullptr;  // polls WiFi.scanComplete()
static char        s_sel_ssid[33] = {0};     // network chosen by the user
static bool        s_sel_secured  = false;   // chosen network is encrypted
static lv_obj_t*   s_pw_input    = nullptr;  // password textarea
static lv_obj_t*   s_ssid_input  = nullptr;  // SSID textarea (only for "Outra")

// ---------------------------------------------------------------------------
// Screen-delete handler — cancels timer + frees scan results
// ---------------------------------------------------------------------------
static void screen_delete_cb(lv_event_t* /*e*/) {
    if (s_scan_timer) {
        lv_timer_delete(s_scan_timer);
        s_scan_timer = nullptr;
    }
    WiFi.scanDelete();
    s_list   = nullptr;
    s_status = nullptr;
}

// ---------------------------------------------------------------------------
// Row helpers
// ---------------------------------------------------------------------------
struct RowInfo { char ssid[33]; bool secured; };

static void start_scan(void);
static void open_password_step(const char* ssid, bool secured);  // shows the password-entry screen
static void try_connect(const char* ssid, const char* pass);     // filled in Task 5
static lv_obj_t* build_header(lv_obj_t* scr);                   // shared header with back button

static void network_row_cb(lv_event_t* e) {
    lv_obj_t* row  = (lv_obj_t*)lv_event_get_target(e);
    RowInfo*  info = (RowInfo*)lv_obj_get_user_data(row);
    if (!info) return;
    open_password_step(info->ssid, info->secured);
}

static void row_free_cb(lv_event_t* e) {
    lv_obj_t* row  = (lv_obj_t*)lv_event_get_target(e);
    RowInfo*  info = (RowInfo*)lv_obj_get_user_data(row);
    free(info);
}

// ---------------------------------------------------------------------------
// List-building helpers
// ---------------------------------------------------------------------------
static void add_row(const char* ssid, bool secured) {
    if (!s_list) return;
    lv_obj_t* row = lv_button_create(s_list);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_style_bg_color(row, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_radius(row, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(row, 0, LV_PART_MAIN);

    RowInfo* info = (RowInfo*)malloc(sizeof(RowInfo));
    if (!info) { lv_obj_delete(row); return; }   // OOM: drop the row, don't crash
    strlcpy(info->ssid, ssid, sizeof(info->ssid));
    info->secured = secured;
    lv_obj_set_user_data(row, info);
    lv_obj_add_event_cb(row, network_row_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(row, row_free_cb,    LV_EVENT_DELETE,  NULL);

    lv_obj_t* lbl = lv_label_create(row);
    char buf[48];
    // Open networks are flagged in text (PT-BR); secured ones show no badge —
    // LVGL's built-in font has no lock glyph (a custom one is a follow-up).
    snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " %s%s",
             ssid, secured ? "" : "  (aberta)");
    lv_label_set_text(lbl, buf);
    lv_obj_set_style_text_color(lbl, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(lbl);

    // Mark the active network
    if (strcmp(ssid, wifi_mgr_current_ssid()) == 0) {
        lv_obj_set_style_border_width(row, 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(row, theme_color_accent(), LV_PART_MAIN);
    }
}

static void build_list_from_scan(int n) {
    for (int i = 0; i < n; ++i) {
        // De-dup: skip if this SSID already appeared earlier (scanNetworks
        // returns sorted by RSSI desc, so the first occurrence is strongest).
        bool dup = false;
        for (int j = 0; j < i; ++j) {
            if (WiFi.SSID(i) == WiFi.SSID(j)) { dup = true; break; }
        }
        if (dup || WiFi.SSID(i).length() == 0) continue;
        bool secured = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        add_row(WiFi.SSID(i).c_str(), secured);
    }
    // Trailing manual-entry row for hidden networks. Assumed secured; an
    // open-hidden toggle is a follow-up.
    add_row(MANUAL_SSID_LABEL, true);
}

// ---------------------------------------------------------------------------
// Scan poll timer + start_scan
// ---------------------------------------------------------------------------
static void scan_poll_cb(lv_timer_t* t) {
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return;   // still scanning
    lv_timer_delete(t);
    s_scan_timer = nullptr;

    if (n <= 0) {
        if (s_status) lv_label_set_text(s_status, "nenhuma rede encontrada");
        return;
    }
    if (s_status) lv_obj_add_flag(s_status, LV_OBJ_FLAG_HIDDEN);
    build_list_from_scan(n);
    WiFi.scanDelete();
}

static void start_scan(void) {
    if (s_status) {
        lv_obj_clear_flag(s_status, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_status, "procurando redes...");
    }
    if (s_list) {
        lv_obj_clean(s_list);   // clears rows (their row_free_cb frees RowInfo)
    }
    WiFi.scanDelete();
    WiFi.scanNetworks(true /* async */);
    if (!s_scan_timer) {
        s_scan_timer = lv_timer_create(scan_poll_cb, 250, NULL);
    }
}

// ---------------------------------------------------------------------------
// Password-entry step
// ---------------------------------------------------------------------------
static void pw_ready_cb(lv_event_t* e) {
    const char* ssid = s_ssid_input ? lv_textarea_get_text(s_ssid_input)
                                     : s_sel_ssid;
    const char* pass = s_pw_input ? lv_textarea_get_text(s_pw_input) : "";
    // Null before any nav change (tamagotchi naming pattern) to avoid a
    // dangling static after the screen is later popped.
    s_ssid_input = nullptr;
    s_pw_input = nullptr;
    try_connect(ssid, pass);
}

static void open_password_step(const char* ssid, bool secured) {
    if (!secured) {
        // Open network: no password needed, connect immediately.
        strlcpy(s_sel_ssid, ssid, sizeof(s_sel_ssid));
        s_sel_secured = false;
        s_ssid_input = nullptr;
        s_pw_input = nullptr;
        try_connect(ssid, "");
        return;
    }

    bool manual = (strcmp(ssid, MANUAL_SSID_LABEL) == 0);
    strlcpy(s_sel_ssid, manual ? "" : ssid, sizeof(s_sel_ssid));
    s_sel_secured = secured;
    s_ssid_input = nullptr;
    s_pw_input = nullptr;

    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, theme_color_bg(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    build_header(scr);   // back button (back_event_cb -> nav_pop)

    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, manual ? "rede + senha" : ssid);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title, 220);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 48);

    int y = 80;
    if (manual) {
        s_ssid_input = lv_textarea_create(scr);
        lv_textarea_set_one_line(s_ssid_input, true);
        lv_textarea_set_max_length(s_ssid_input, 32);
        lv_textarea_set_placeholder_text(s_ssid_input, "nome da rede");
        lv_obj_set_width(s_ssid_input, 220);
        lv_obj_align(s_ssid_input, LV_ALIGN_TOP_MID, 0, y);
        y += 44;
    }

    s_pw_input = lv_textarea_create(scr);
    lv_textarea_set_one_line(s_pw_input, true);
    lv_textarea_set_password_mode(s_pw_input, true);
    lv_textarea_set_max_length(s_pw_input, 63);
    lv_textarea_set_placeholder_text(s_pw_input, "senha");
    lv_obj_set_width(s_pw_input, 220);
    lv_obj_align(s_pw_input, LV_ALIGN_TOP_MID, 0, y);

    lv_obj_t* kb = lv_keyboard_create(scr);
    lv_obj_set_size(kb, 240, 150);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb, manual ? s_ssid_input : s_pw_input);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_add_event_cb(kb, pw_ready_cb, LV_EVENT_READY, NULL);

    nav_push(scr, NAV_ANIM_SLIDE_LEFT);
}

// ---------------------------------------------------------------------------
// Temporary stub — replaced in Task 5
// ---------------------------------------------------------------------------
static void try_connect(const char* ssid, const char* pass) {
    Serial.printf("[wifi_config] would connect to '%s'\n", ssid);
    (void)pass;
}

// ---------------------------------------------------------------------------
// Back button
// ---------------------------------------------------------------------------
static void back_event_cb(lv_event_t* /*e*/) {
    nav_pop(NAV_ANIM_SLIDE_RIGHT);
}

// ---------------------------------------------------------------------------
// Header
// ---------------------------------------------------------------------------
static lv_obj_t* build_header(lv_obj_t* scr) {
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
    lv_label_set_text(title, "WiFi");
    lv_obj_set_style_text_color(title, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    return header;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
void wifi_config_show(void) {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, theme_color_bg(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    build_header(scr);

    lv_obj_add_event_cb(scr, screen_delete_cb, LV_EVENT_DELETE, NULL);

    s_status = lv_label_create(scr);
    lv_obj_set_style_text_color(s_status, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_status, LV_ALIGN_TOP_MID, 0, 52);

    s_list = lv_obj_create(scr);
    lv_obj_set_size(s_list, 230, 320 - 48);
    lv_obj_align(s_list, LV_ALIGN_TOP_MID, 0, 44);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_list, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_list, 0, LV_PART_MAIN);

    start_scan();

    nav_push(scr, NAV_ANIM_SLIDE_LEFT);
}
