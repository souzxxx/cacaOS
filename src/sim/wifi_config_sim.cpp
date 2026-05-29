/**
 * @file wifi_config_sim.cpp
 * @brief No-op WiFi config screen for the desktop sim (no real WiFi).
 */

#include "../apps/wifi_config/wifi_config.h"

#include <Arduino.h>

void wifi_config_show(void) {
    Serial.println("[wifi_config:sim] not available in sim");
}
