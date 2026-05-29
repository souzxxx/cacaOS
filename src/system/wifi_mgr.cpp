/**
 * @file wifi_mgr.cpp
 * @brief Async WiFi connect + NTP time sync.
 *
 * Strategy:
 *   - Don't block boot on WiFi. Start connection in background.
 *   - Once connected, sync NTP once.
 *   - On disconnect, auto-retry every 30s.
 */

#include "wifi_mgr.h"

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <Preferences.h>
#include <string.h>

#include "config.h"

enum WifiState : uint8_t {
    WIFI_STATE_IDLE,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_DISCONNECTED,
};

static WifiState s_state = WIFI_STATE_IDLE;
static bool      s_time_synced = false;
static uint32_t  s_last_retry_ms = 0;
static uint32_t  s_connect_start_ms = 0;

static constexpr uint32_t CONNECT_TIMEOUT_MS = 20000;
static constexpr uint32_t RETRY_INTERVAL_MS  = 30000;
static constexpr size_t   SSID_BUF_LEN = 33;   // 32 chars + NUL (IEEE 802.11)
static constexpr size_t   PASS_BUF_LEN = 64;   // 63 chars + NUL (WPA2 max)

static bool                s_apply_pending = false;
static wifi_apply_status_t s_apply_status  = WIFI_APPLY_IDLE;
static char                s_pending_ssid[SSID_BUF_LEN] = {0};
static char                s_pending_pass[PASS_BUF_LEN] = {0};
static char                s_active_ssid[SSID_BUF_LEN]  = {0};
static char                s_active_pass[PASS_BUF_LEN]  = {0};

static void start_connection(void);
static void try_sync_ntp(void);
static void resolve_credentials(char* ssid_out, char* pass_out);
static void persist_credentials(const char* ssid, const char* pass);

void wifi_mgr_begin(void) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    start_connection();
}

void wifi_mgr_loop(void) {
    wl_status_t status = WiFi.status();

    switch (s_state) {
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

        case WIFI_STATE_CONNECTED:
            if (status != WL_CONNECTED) {
                Serial.println(F("[wifi] disconnected"));
                s_state = WIFI_STATE_DISCONNECTED;
                s_last_retry_ms = millis();
            } else if (!s_time_synced) {
                // Retry NTP if not synced yet (e.g. first try failed)
                try_sync_ntp();
            }
            break;

        case WIFI_STATE_DISCONNECTED:
            if (millis() - s_last_retry_ms > RETRY_INTERVAL_MS) {
                Serial.println(F("[wifi] retrying connection..."));
                start_connection();
            }
            break;

        default:
            break;
    }
}

bool wifi_mgr_is_connected(void) {
    return s_state == WIFI_STATE_CONNECTED && WiFi.status() == WL_CONNECTED;
}

bool wifi_mgr_time_is_synced(void) {
    return s_time_synced;
}

void wifi_mgr_disable(void) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    s_state = WIFI_STATE_IDLE;
}

void wifi_mgr_apply_credentials(const char* ssid, const char* pass) {
    if (s_apply_pending) return;          // a test is already in flight
    if (!ssid || !ssid[0]) return;        // reject missing/empty SSID
    strlcpy(s_pending_ssid, ssid, sizeof(s_pending_ssid));
    strlcpy(s_pending_pass, pass ? pass : "", sizeof(s_pending_pass));
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

// ----------------------------------------------------------------
static void start_connection(void) {
    resolve_credentials(s_active_ssid, s_active_pass);
    Serial.printf("[wifi] connecting to '%s'...\n", s_active_ssid);
    WiFi.begin(s_active_ssid, s_active_pass);
    s_connect_start_ms = millis();
    s_state = WIFI_STATE_CONNECTING;
}

static void try_sync_ntp(void) {
    Serial.println(F("[ntp] syncing..."));
    // configTzTime applies TZ_INFO from config.h. São Paulo: BRT3 (UTC-3, no DST).
    configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2);

    // Best-effort wait: up to 5 seconds for time to come back.
    struct tm timeinfo;
    uint32_t deadline = millis() + 5000;
    while (millis() < deadline) {
        if (getLocalTime(&timeinfo, 100)) {
            char buf[64];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
            Serial.printf("[ntp] synced: %s\n", buf);
            s_time_synced = true;
            return;
        }
        delay(100);
    }
    Serial.println(F("[ntp] sync failed, will retry"));
}

static void resolve_credentials(char* ssid_out, char* pass_out) {
    bool have = false;
    Preferences p;
    if (p.begin("wifi", true)) {            // read-only
        if (p.isKey("ssid")) {
            p.getString("ssid", ssid_out, SSID_BUF_LEN);
            p.getString("pass", pass_out, PASS_BUF_LEN);
            have = (ssid_out[0] != '\0');
        }
        p.end();
    }
    if (!have) {
        strlcpy(ssid_out, WIFI_SSID, SSID_BUF_LEN);   // factory default from config.h
        strlcpy(pass_out, WIFI_PASS, PASS_BUF_LEN);
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
