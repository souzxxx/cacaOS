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

static void start_connection(void);
static void try_sync_ntp(void);

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
                Serial.printf("[wifi] connected, IP=%s\n", WiFi.localIP().toString().c_str());
                try_sync_ntp();
            } else if (millis() - s_connect_start_ms > CONNECT_TIMEOUT_MS) {
                Serial.println(F("[wifi] connect timeout, will retry"));
                s_state = WIFI_STATE_DISCONNECTED;
                s_last_retry_ms = millis();
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

// ----------------------------------------------------------------
static void start_connection(void) {
    Serial.printf("[wifi] connecting to '%s'...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
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
