#pragma once

#include <stdbool.h>

/**
 * Start WiFi connection asynchronously.
 * Non-blocking — returns immediately. Call wifi_mgr_loop() each iteration
 * of main loop() to drive connection state.
 *
 * Reads SSID/PASS from NVS (namespace "wifi"); falls back to config.h defaults.
 */
void wifi_mgr_begin(void);

/**
 * Drive connection state machine. Call from main loop() periodically.
 * Handles initial connect, NTP sync once connected, and auto-reconnect on disconnect.
 */
void wifi_mgr_loop(void);

/**
 * Returns true if WiFi is currently connected.
 */
bool wifi_mgr_is_connected(void);

/**
 * Returns true if NTP has successfully synced time at least once since boot.
 */
bool wifi_mgr_time_is_synced(void);

/**
 * Disconnect and disable WiFi (saves power if app is offline).
 */
void wifi_mgr_disable(void);

/**
 * Free the radio for a scan: abort any in-flight connection attempt and stop
 * auto-reconnect. esp_wifi_scan_start() fails with ESP_ERR_WIFI_STATE while
 * the STA is mid-connect, so a scan started during the boot/retry connect
 * loop never runs. No-op when already connected (scanning while associated
 * works). Pair with wifi_mgr_resume().
 */
void wifi_mgr_pause(void);

/**
 * Undo wifi_mgr_pause(): restart the connection attempt with the stored
 * credentials. No-op unless paused (an active connection or a credential
 * test via wifi_mgr_apply_credentials is left alone).
 */
void wifi_mgr_resume(void);

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
