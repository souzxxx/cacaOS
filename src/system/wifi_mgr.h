#pragma once

#include <stdbool.h>

/**
 * Start WiFi connection asynchronously.
 * Non-blocking — returns immediately. Call wifi_mgr_loop() each iteration
 * of main loop() to drive connection state.
 *
 * Reads SSID/PASS from config.h.
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
