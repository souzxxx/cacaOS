# WiFi Config (sem reflashar) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let the user pick a WiFi network and enter its password on the device screen, persisting credentials to NVS so they survive reboots without reflashing.

**Architecture:** `wifi_mgr` resolves credentials from NVS (namespace `"wifi"`) with a `config.h` fallback, and exposes a non-blocking apply+poll API. A new `wifi_config` screen, launched from the `settings` app, runs an async WiFi scan, lets the user pick a network and type a password, then kicks off the apply and polls for the result.

**Tech Stack:** PlatformIO, Arduino-ESP32 (`WiFi.h`, `Preferences.h`), LVGL 9.x.

---

## Testing note (read first)

This codebase has **no unit-test framework**, and the board is not yet
available. Per-task verification is therefore:

- **`pio run -e cyd`** must stay green (this is the real regression gate — it
  caught the DRAM overflow earlier).
- **`pio run -e sim`** must stay green (link check for the desktop build).

On-device manual verification is deferred until the board arrives; the exact
manual steps are listed in the final task.

`wifi_mgr.cpp` and the new `wifi_config.cpp` are **excluded from the `sim`
build** (they call `WiFi.*`, which has no sim shim). The `settings` screen is
in the sim build, so a no-op `wifi_config_sim.cpp` provides the
`wifi_config_show()` symbol there.

---

## File structure

| File | Responsibility |
|------|----------------|
| `src/system/wifi_mgr.h` | public WiFi lifecycle API + new apply/status/current-ssid |
| `src/system/wifi_mgr.cpp` | NVS↔config.h credential resolution, async apply, persist (device) |
| `src/system/storage.h` | document the `"wifi"` reserved NVS namespace |
| `src/apps/wifi_config/wifi_config.h` | `void wifi_config_show(void);` |
| `src/apps/wifi_config/wifi_config.cpp` | scan → list → password → connect UI (device) |
| `src/sim/wifi_config_sim.cpp` | no-op `wifi_config_show()` for the sim link |
| `src/apps/settings/settings.cpp` | "WiFi" button → `wifi_config_show()`; scrollable screen |
| `platformio.ini` | exclude `wifi_config.cpp` from the `sim` env |

---

## Task 1: wifi_mgr — NVS credentials + async apply API

**Files:**
- Modify: `src/system/wifi_mgr.h`
- Modify: `src/system/wifi_mgr.cpp`
- Modify: `src/system/storage.h:14-21` (namespace doc comment)

- [ ] **Step 1: Extend the header**

Add to `src/system/wifi_mgr.h`, after the existing `wifi_mgr_disable()` declaration:

```c
/**
 * Status of a UI-initiated credential change (see wifi_mgr_apply_credentials).
 */
typedef enum {
    WIFI_APPLY_IDLE,     // nothing in progress
    WIFI_APPLY_TESTING,  // attempt in flight
    WIFI_APPLY_OK,       // connected + persisted to NVS
    WIFI_APPLY_FAILED,   // timed out / wrong password; old creds restored
} wifi_apply_status_t;

/**
 * Non-blocking. Store `ssid`/`pass` as pending credentials and start a
 * connection attempt. Returns immediately; drive the attempt via
 * wifi_mgr_loop() and poll wifi_mgr_apply_status().
 *
 * On success the credentials are persisted to NVS (namespace "wifi") and take
 * precedence on every subsequent boot. On failure the previously resolved
 * credentials are reconnected and NVS is left untouched.
 */
void wifi_mgr_apply_credentials(const char* ssid, const char* pass);

/**
 * Poll the result of the most recent wifi_mgr_apply_credentials() call.
 */
wifi_apply_status_t wifi_mgr_apply_status(void);

/**
 * The SSID currently in use (resolved from NVS or config.h). Valid after
 * wifi_mgr_begin(). Used by the UI to display/mark the active network.
 */
const char* wifi_mgr_current_ssid(void);
```

- [ ] **Step 2: Add includes + state to wifi_mgr.cpp**

In `src/system/wifi_mgr.cpp`, add `#include <Preferences.h>` and `#include <string.h>` below the existing includes, and add these statics after the existing state block (after `s_connect_start_ms`):

```c
static bool                s_apply_pending = false;
static wifi_apply_status_t s_apply_status  = WIFI_APPLY_IDLE;
static char                s_pending_ssid[33] = {0};
static char                s_pending_pass[64] = {0};
static char                s_active_ssid[33]  = {0};
static char                s_active_pass[64]  = {0};
```

Add the forward declarations next to the existing ones:

```c
static void resolve_credentials(char* ssid_out, char* pass_out);
static void persist_credentials(const char* ssid, const char* pass);
```

- [ ] **Step 3: Implement the credential helpers**

Append to the bottom of `src/system/wifi_mgr.cpp`:

```c
static void resolve_credentials(char* ssid_out, char* pass_out) {
    bool have = false;
    Preferences p;
    if (p.begin("wifi", true)) {            // read-only
        if (p.isKey("ssid")) {
            p.getString("ssid", ssid_out, 33);
            p.getString("pass", pass_out, 64);
            have = (ssid_out[0] != '\0');
        }
        p.end();
    }
    if (!have) {
        strlcpy(ssid_out, WIFI_SSID, 33);   // factory default from config.h
        strlcpy(pass_out, WIFI_PASS, 64);
    }
}

static void persist_credentials(const char* ssid, const char* pass) {
    Preferences p;
    if (p.begin("wifi", false)) {           // read-write
        p.putString("ssid", ssid);
        p.putString("pass", pass);
        p.end();
        Serial.printf("[wifi] credentials persisted for '%s'\n", ssid);
    }
}
```

- [ ] **Step 4: Use resolved credentials in start_connection**

Replace the body of `start_connection()` (currently `wifi_mgr.cpp:97-102`):

```c
static void start_connection(void) {
    resolve_credentials(s_active_ssid, s_active_pass);
    Serial.printf("[wifi] connecting to '%s'...\n", s_active_ssid);
    WiFi.begin(s_active_ssid, s_active_pass);
    s_connect_start_ms = millis();
    s_state = WIFI_STATE_CONNECTING;
}
```

- [ ] **Step 5: Implement apply/status/current_ssid**

Add after `wifi_mgr_disable()` in `wifi_mgr.cpp`:

```c
void wifi_mgr_apply_credentials(const char* ssid, const char* pass) {
    strlcpy(s_pending_ssid, ssid, sizeof(s_pending_ssid));
    strlcpy(s_pending_pass, pass, sizeof(s_pending_pass));
    s_apply_pending = true;
    s_apply_status  = WIFI_APPLY_TESTING;
    Serial.printf("[wifi] testing new credentials for '%s'\n", s_pending_ssid);
    WiFi.disconnect();
    WiFi.begin(s_pending_ssid, s_pending_pass);
    s_connect_start_ms = millis();
    s_state = WIFI_STATE_CONNECTING;
}

wifi_apply_status_t wifi_mgr_apply_status(void) {
    return s_apply_status;
}

const char* wifi_mgr_current_ssid(void) {
    return s_active_ssid;
}
```

- [ ] **Step 6: Handle the pending apply in the state machine**

In `wifi_mgr_loop()`, replace the `WIFI_STATE_CONNECTING` case (currently `wifi_mgr.cpp:47-57`) with:

```c
        case WIFI_STATE_CONNECTING:
            if (status == WL_CONNECTED) {
                s_state = WIFI_STATE_CONNECTED;
                if (s_apply_pending) {
                    persist_credentials(s_pending_ssid, s_pending_pass);
                    strlcpy(s_active_ssid, s_pending_ssid, sizeof(s_active_ssid));
                    strlcpy(s_active_pass, s_pending_pass, sizeof(s_active_pass));
                    s_apply_pending = false;
                    s_apply_status  = WIFI_APPLY_OK;
                }
                Serial.printf("[wifi] connected, IP=%s\n", WiFi.localIP().toString().c_str());
                try_sync_ntp();
            } else if (millis() - s_connect_start_ms > CONNECT_TIMEOUT_MS) {
                Serial.println(F("[wifi] connect timeout, will retry"));
                if (s_apply_pending) {
                    s_apply_pending = false;
                    s_apply_status  = WIFI_APPLY_FAILED;
                    start_connection();   // reconnect with resolved (old) creds
                } else {
                    s_state = WIFI_STATE_DISCONNECTED;
                    s_last_retry_ms = millis();
                }
            }
            break;
```

- [ ] **Step 7: Document the NVS namespace**

In `src/system/storage.h`, add to the reserved-namespaces comment block (after the `"system"` line):

```c
 *   "wifi"    - on-device WiFi credentials (ssid, pass)
```

- [ ] **Step 8: Build both envs**

Run: `pio run 2>&1 | tail -6`
Expected: `cyd SUCCESS` and `sim SUCCESS`. (`wifi_mgr.cpp` is excluded from
sim; the header's new declarations are unused there, so the sim link is
unaffected.)

- [ ] **Step 9: Commit**

```bash
git add src/system/wifi_mgr.h src/system/wifi_mgr.cpp src/system/storage.h
git commit -m "feat(wifi): NVS-backed credentials with async apply API

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 2: wifi_config screen skeleton + wiring

Bootstrap the screen, the sim shim, the settings entry, and the build
exclusion so everything links before the UI logic is filled in.

**Files:**
- Create: `src/apps/wifi_config/wifi_config.h`
- Create: `src/apps/wifi_config/wifi_config.cpp`
- Create: `src/sim/wifi_config_sim.cpp`
- Modify: `platformio.ini:20-22` (cyd src filter is fine; add sim exclusion at `:80-90`)
- Modify: `src/apps/settings/settings.cpp`

- [ ] **Step 1: Create the header**

`src/apps/wifi_config/wifi_config.h`:

```c
#pragma once

/**
 * WiFi configuration screen. Scans nearby networks, lets the user pick one and
 * enter its password, then applies + persists the credentials via wifi_mgr.
 *
 * Launched from the settings app. Pushes its own screen via nav_push() and
 * returns with nav_pop().
 */
void wifi_config_show(void);
```

- [ ] **Step 2: Create a minimal screen implementation**

`src/apps/wifi_config/wifi_config.cpp` (header + back button only for now; the
scan/list/password logic is added in later tasks):

```cpp
/**
 * @file wifi_config.cpp
 * @brief On-device WiFi network picker + password entry. Device-only (uses
 *        WiFi.*); excluded from the sim build.
 */

#include "wifi_config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>

#include "../../ui/theme.h"
#include "../../ui/nav.h"
#include "../../system/wifi_mgr.h"

static void back_event_cb(lv_event_t* /*e*/) {
    nav_pop(NAV_ANIM_SLIDE_RIGHT);
}

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

void wifi_config_show(void) {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, theme_color_bg(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    build_header(scr);

    nav_push(scr, NAV_ANIM_SLIDE_LEFT);
}
```

- [ ] **Step 3: Create the sim shim**

`src/sim/wifi_config_sim.cpp`:

```cpp
/**
 * @file wifi_config_sim.cpp
 * @brief No-op WiFi config screen for the desktop sim (no real WiFi).
 */

#include "../apps/wifi_config/wifi_config.h"

#include <Arduino.h>

void wifi_config_show(void) {
    Serial.println("[wifi_config:sim] not available in sim");
}
```

- [ ] **Step 4: Exclude wifi_config.cpp from the sim build**

In `platformio.ini`, in the `[env:sim]` `build_src_filter` block (currently
`:80-90`), add this line alongside the other exclusions:

```ini
    -<apps/wifi_config/wifi_config.cpp>
```

- [ ] **Step 5: Add the WiFi button to settings + make screen scrollable**

In `src/apps/settings/settings.cpp`:

Add the include near the top (with the other app/system includes):

```cpp
#include "../wifi_config/wifi_config.h"
```

Add a callback near the other `*_btn_cb` functions:

```cpp
static void wifi_btn_cb(lv_event_t* /*e*/) {
    wifi_config_show();
}
```

In `settings_show()`, remove the line that disables scrolling so the extra
button is reachable:

```cpp
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);   // DELETE this line
```

Insert a WiFi button between the "Sobre" card and the reset buttons. Place it
at y=196 and shift the two reset buttons down by 48px each (to y=244 and
y=292). The WiFi button:

```cpp
    // --- WiFi config ---
    lv_obj_t* wifi_btn = lv_button_create(scr);
    lv_obj_set_size(wifi_btn, 220, 40);
    lv_obj_align(wifi_btn, LV_ALIGN_TOP_MID, 0, 196);
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
```

Change the existing `reset_pet` align from `196` to `244`, and `reset_touch`
from `244` to `292`.

- [ ] **Step 6: Build both envs**

Run: `pio run 2>&1 | tail -6`
Expected: `cyd SUCCESS` and `sim SUCCESS`. In sim, `settings.cpp` links against
the no-op `wifi_config_show()`.

- [ ] **Step 7: Commit**

```bash
git add src/apps/wifi_config/ src/sim/wifi_config_sim.cpp platformio.ini src/apps/settings/settings.cpp
git commit -m "feat(wifi): wifi_config screen skeleton + settings entry

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 3: Async network scan + list

Fill in `wifi_config.cpp` with the scan and the tappable network list. Tapping
a row stores the selection and (next task) opens the password step.

**Files:**
- Modify: `src/apps/wifi_config/wifi_config.cpp`

- [ ] **Step 1: Add scan state + a content container**

At the top of `wifi_config.cpp` (after includes), add module state:

```cpp
static lv_obj_t*  s_list      = nullptr;   // scrollable network list
static lv_obj_t*  s_status    = nullptr;   // "procurando…" / errors
static lv_timer_t* s_scan_timer = nullptr; // polls WiFi.scanComplete()
static char       s_sel_ssid[33] = {0};    // network chosen by the user
static bool       s_sel_secured  = false;  // chosen network is encrypted
```

- [ ] **Step 2: Add a delete handler to cancel the scan timer**

Add a screen-delete callback so a half-finished scan/timer never dangles:

```cpp
static void screen_delete_cb(lv_event_t* /*e*/) {
    if (s_scan_timer) {
        lv_timer_delete(s_scan_timer);
        s_scan_timer = nullptr;
    }
    WiFi.scanDelete();
    s_list = nullptr;
    s_status = nullptr;
}
```

- [ ] **Step 3: Add forward declarations and the row-tap handler**

Each row carries its SSID + secured flag in a heap-allocated `RowInfo` stored
as the object's user_data, freed on the row's `LV_EVENT_DELETE`:

```cpp
struct RowInfo { char ssid[33]; bool secured; };

static void start_scan(void);
static void open_password_step(const char* ssid, bool secured);  // Task 4

static void network_row_cb(lv_event_t* e) {
    lv_obj_t* row = (lv_obj_t*)lv_event_get_target(e);
    RowInfo* info = (RowInfo*)lv_obj_get_user_data(row);
    if (!info) return;
    open_password_step(info->ssid, info->secured);
}

static void row_free_cb(lv_event_t* e) {
    lv_obj_t* row = (lv_obj_t*)lv_event_get_target(e);
    RowInfo* info = (RowInfo*)lv_obj_get_user_data(row);
    free(info);
}
```

- [ ] **Step 4: Build the list from scan results**

```cpp
static void add_row(const char* ssid, bool secured) {
    if (!s_list) return;
    lv_obj_t* row = lv_button_create(s_list);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_style_bg_color(row, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_radius(row, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(row, 0, LV_PART_MAIN);

    RowInfo* info = (RowInfo*)malloc(sizeof(RowInfo));
    strlcpy(info->ssid, ssid, sizeof(info->ssid));
    info->secured = secured;
    lv_obj_set_user_data(row, info);
    lv_obj_add_event_cb(row, network_row_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(row, row_free_cb, LV_EVENT_DELETE, NULL);

    lv_obj_t* lbl = lv_label_create(row);
    char buf[48];
    snprintf(buf, sizeof(buf), "%s%s%s",
             secured ? LV_SYMBOL_WIFI " " : LV_SYMBOL_WIFI " ",
             ssid,
             secured ? "  " LV_SYMBOL_CLOSE : "");
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
        // De-dup: skip if this SSID already added earlier (stronger RSSI first,
        // and scanNetworks returns sorted by RSSI desc).
        bool dup = false;
        for (int j = 0; j < i; ++j) {
            if (WiFi.SSID(i) == WiFi.SSID(j)) { dup = true; break; }
        }
        if (dup || WiFi.SSID(i).length() == 0) continue;
        bool secured = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        add_row(WiFi.SSID(i).c_str(), secured);
    }
    // Trailing manual-entry row for hidden networks
    add_row("Outra (digitar)", true);
}
```

- [ ] **Step 5: Implement the scan poll timer + start_scan**

```cpp
static void scan_poll_cb(lv_timer_t* t) {
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return;        // still scanning
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
```

- [ ] **Step 6: Wire scan + list into wifi_config_show**

In `wifi_config_show()`, after `build_header(scr);` and before `nav_push(...)`,
add the status label, the list container, the delete handler, and kick off the
scan:

```cpp
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
```

- [ ] **Step 7: Add a temporary stub for open_password_step**

So the file compiles before Task 4 fills it in, add a stub near the forward
declarations (it is replaced in Task 4):

```cpp
static void open_password_step(const char* ssid, bool secured) {
    strlcpy(s_sel_ssid, ssid, sizeof(s_sel_ssid));
    s_sel_secured = secured;
    Serial.printf("[wifi_config] selected '%s' (secured=%d)\n", ssid, secured);
}
```

- [ ] **Step 8: Build (cyd only — wifi_config is not in sim)**

Run: `pio run -e cyd 2>&1 | tail -3`
Expected: `cyd SUCCESS`.

Also confirm sim still links: `pio run -e sim 2>&1 | tail -3` → `SUCCESS`.

- [ ] **Step 9: Commit**

```bash
git add src/apps/wifi_config/wifi_config.cpp
git commit -m "feat(wifi): async network scan + tappable list

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 4: Password entry step

Replace the `open_password_step` stub with a textarea + keyboard, reusing the
tamagotchi naming-wizard pattern (`tamagotchi.cpp:1158-1175`). The "Conectar"
action hands off to Task 5.

**Files:**
- Modify: `src/apps/wifi_config/wifi_config.cpp`

- [ ] **Step 1: Add password-step state + forward decl**

Add to the module state block:

```cpp
static lv_obj_t* s_pw_input = nullptr;     // password textarea
static lv_obj_t* s_ssid_input = nullptr;   // SSID textarea (only for "Outra")
```

Add a forward declaration near the others:

```cpp
static void try_connect(const char* ssid, const char* pass);  // Task 5
```

- [ ] **Step 2: Implement the keyboard "ready" handler**

```cpp
static void pw_ready_cb(lv_event_t* e) {
    // Fired when the keyboard checkmark (LV_EVENT_READY) is tapped.
    const char* ssid = s_ssid_input ? lv_textarea_get_text(s_ssid_input)
                                     : s_sel_ssid;
    const char* pass = s_pw_input ? lv_textarea_get_text(s_pw_input) : "";
    try_connect(ssid, pass);
}
```

- [ ] **Step 3: Implement open_password_step (replace the stub)**

```cpp
static void open_password_step(const char* ssid, bool secured) {
    bool manual = (strcmp(ssid, "Outra (digitar)") == 0);
    strlcpy(s_sel_ssid, manual ? "" : ssid, sizeof(s_sel_ssid));
    s_sel_secured = secured;
    s_ssid_input = nullptr;
    s_pw_input = nullptr;

    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, theme_color_bg(), LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, manual ? "rede + senha" : ssid);
    lv_obj_set_style_text_color(title, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    int y = 36;
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
    lv_keyboard_set_textarea(kb, s_pw_input);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_add_event_cb(kb, pw_ready_cb, LV_EVENT_READY, NULL);

    // Focus the SSID field first in manual mode so the keyboard targets it.
    if (manual) lv_keyboard_set_textarea(kb, s_ssid_input);

    nav_push(scr, NAV_ANIM_SLIDE_LEFT);
}
```

- [ ] **Step 4: Add a temporary stub for try_connect**

So the file compiles before Task 5 (replaced there):

```cpp
static void try_connect(const char* ssid, const char* pass) {
    Serial.printf("[wifi_config] would connect to '%s'\n", ssid);
    (void)pass;
}
```

- [ ] **Step 5: Build**

Run: `pio run -e cyd 2>&1 | tail -3` → `cyd SUCCESS`
Run: `pio run -e sim 2>&1 | tail -3` → `sim SUCCESS`

- [ ] **Step 6: Commit**

```bash
git add src/apps/wifi_config/wifi_config.cpp
git commit -m "feat(wifi): password entry step with keyboard

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 5: Connect flow + result UI

Replace the `try_connect` stub: kick off `wifi_mgr_apply_credentials`, show a
spinner, poll `wifi_mgr_apply_status()`, and present success/error.

**Files:**
- Modify: `src/apps/wifi_config/wifi_config.cpp`

- [ ] **Step 1: Add connect-step state**

```cpp
static lv_obj_t*  s_connect_scr   = nullptr;
static lv_obj_t*  s_connect_label = nullptr;
static lv_timer_t* s_apply_timer  = nullptr;
```

- [ ] **Step 2: Add the apply poll timer**

```cpp
static void apply_poll_cb(lv_timer_t* t) {
    wifi_apply_status_t st = wifi_mgr_apply_status();
    if (st == WIFI_APPLY_TESTING) return;

    lv_timer_delete(t);
    s_apply_timer = nullptr;
    if (!s_connect_label) return;

    if (st == WIFI_APPLY_OK) {
        char buf[64];
        snprintf(buf, sizeof(buf), LV_SYMBOL_OK " conectado!\n%s",
                 WiFi.localIP().toString().c_str());
        lv_label_set_text(s_connect_label, buf);
    } else {  // WIFI_APPLY_FAILED
        lv_label_set_text(s_connect_label,
                          LV_SYMBOL_CLOSE " falhou\nsenha errada?");
    }
}
```

- [ ] **Step 3: Add a delete handler for the connect screen**

```cpp
static void connect_scr_delete_cb(lv_event_t* /*e*/) {
    if (s_apply_timer) {
        lv_timer_delete(s_apply_timer);
        s_apply_timer = nullptr;
    }
    s_connect_scr = nullptr;
    s_connect_label = nullptr;
}
```

- [ ] **Step 4: Implement try_connect (replace the stub)**

```cpp
static void try_connect(const char* ssid, const char* pass) {
    if (ssid[0] == '\0') return;  // nothing typed for a manual SSID

    s_connect_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_connect_scr, theme_color_bg(), LV_PART_MAIN);
    lv_obj_clear_flag(s_connect_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_connect_scr, connect_scr_delete_cb, LV_EVENT_DELETE, NULL);

    lv_obj_t* spinner = lv_spinner_create(s_connect_scr);
    lv_obj_set_size(spinner, 48, 48);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, -30);

    s_connect_label = lv_label_create(s_connect_scr);
    lv_label_set_text(s_connect_label, "conectando...");
    lv_obj_set_style_text_align(s_connect_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_connect_label, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_connect_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(s_connect_label, LV_ALIGN_CENTER, 0, 30);

    nav_push(s_connect_scr, NAV_ANIM_FADE);

    wifi_mgr_apply_credentials(ssid, pass);
    s_apply_timer = lv_timer_create(apply_poll_cb, 200, NULL);
}
```

- [ ] **Step 5: Build**

Run: `pio run -e cyd 2>&1 | tail -3` → `cyd SUCCESS`
Run: `pio run -e sim 2>&1 | tail -3` → `sim SUCCESS`

- [ ] **Step 6: Commit**

```bash
git add src/apps/wifi_config/wifi_config.cpp
git commit -m "feat(wifi): connect flow with async status polling + result UI

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 6: Final verification + on-device test checklist

**Files:** none (verification only)

- [ ] **Step 1: Full build, both envs**

Run: `pio run 2>&1 | tail -6`
Expected: `cyd SUCCESS`, `sim SUCCESS`. Note the cyd RAM/Flash % — should stay
well under 100% (RAM was ~28% after the CLIB fix).

- [ ] **Step 2: Record the on-device manual test (run when the board arrives)**

These cannot run now (no hardware). Document them in the commit / PR body:

1. Boot with `config.h` credentials → connects as before (NVS empty → fallback).
2. Settings → "configurar wifi" → list populates with nearby networks within
   ~2s; the active network is outlined.
3. Pick a network, type the correct password, Conectar → spinner →
   "conectado! <ip>". Serial shows `[wifi] credentials persisted`.
4. Reboot → reconnects to the chosen network automatically (NVS precedence).
5. Pick a network, type a **wrong** password → "falhou / senha errada?"; serial
   shows timeout; the previously working network is reconnected and still
   persisted (reboot still uses the good one).
6. "Outra (digitar)" → type SSID + password → same connect flow.
7. Watch `[heap]` logs: free heap stays healthy through scan + keyboard.

- [ ] **Step 3: Finalize**

Use the `superpowers:finishing-a-development-branch` skill to decide
merge/PR/cleanup for the `v2` branch.

---

## Self-review

**Spec coverage:**
- NVS↔config.h resolution → Task 1 (Steps 3–4). ✓
- Async apply + persist-on-success + restore-on-fail → Task 1 (Steps 5–6). ✓
- `"wifi"` namespace documented → Task 1 (Step 7). ✓
- Scan + dedup + RSSI order + 🔒 + active mark + "Outra" → Task 3. ✓
- Password step (textarea + keyboard, open-net skip, manual SSID) → Task 4. ✓
- Connect spinner + poll + success(IP)/error → Task 5. ✓
- Launched from settings, scrollable screen, no new home icon → Task 2. ✓
- Sim handling (exclude + shim) → Task 2 (Steps 3–4). ✓

**Placeholder scan:** No TBD/TODO or vague steps; every code step shows complete
code. The only forward stubs (`open_password_step` in Task 3, `try_connect` in
Task 4) are explicitly labeled "replaced in Task N" and exist so each task still
builds green on its own commit.

**Type consistency:** `wifi_apply_status_t` enum values, `wifi_mgr_apply_*`
signatures, and `RowInfo`/`s_sel_ssid` names are used consistently across tasks.
`open_password_step(const char*, bool)` and `try_connect(const char*, const
char*)` signatures match between forward decls and definitions.
