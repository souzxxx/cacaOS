/**
 * @file storage_sim.cpp
 * @brief Implements the storage module via the Preferences shim.
 *        Same behavior as the device version (NVS-backed) but on disk.
 */

#include "../system/storage.h"

#include <Arduino.h>
#include <Preferences.h>

void storage_init(void) {
    Serial.println("[storage:sim] init ok (~/.cacaos_sim_prefs.json)");
}

uint8_t storage_get_brightness(uint8_t default_value) {
    Preferences p;
    p.begin("system", true);
    uint8_t v = p.getUChar("brightness", default_value);
    p.end();
    return v;
}

void storage_set_brightness(uint8_t value) {
    Preferences p;
    p.begin("system", false);
    p.putUChar("brightness", value);
    p.end();
}
