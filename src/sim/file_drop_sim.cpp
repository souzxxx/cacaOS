/**
 * @file file_drop_sim.cpp
 * @brief No-op file receiver for the desktop sim (no real WiFi/SD).
 */

#include "../apps/file_drop/file_drop.h"

#include <Arduino.h>

void file_drop_show(void) {
    Serial.println("[file_drop:sim] not available in sim");
}
