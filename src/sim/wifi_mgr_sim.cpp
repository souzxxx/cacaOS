/**
 * @file wifi_mgr_sim.cpp
 * @brief No-op wifi: pretends to be connected so weather and NTP-dependent
 *        apps don't hide their UI.
 */

#include "../system/wifi_mgr.h"

#include <Arduino.h>
#include <time.h>

static bool s_time_synced = false;

void wifi_mgr_begin(void) {
    Serial.println("[wifi:sim] pretending connected");
    s_time_synced = true; // host clock is already correct
}

void wifi_mgr_loop(void) {
    // nothing to do — host clock advances on its own
}

bool wifi_mgr_is_connected(void) { return true; }
bool wifi_mgr_time_is_synced(void) { return s_time_synced; }
void wifi_mgr_disable(void) {}
void wifi_mgr_pause(void) {}
void wifi_mgr_resume(void) {}
