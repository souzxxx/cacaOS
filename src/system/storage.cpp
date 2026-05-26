/**
 * @file storage.cpp
 * @brief NVS storage helpers via Preferences.
 */

#include "storage.h"

#include <Arduino.h>
#include <Preferences.h>
#include <nvs_flash.h>

void storage_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        Serial.println(F("[nvs] erasing and reinitializing..."));
        nvs_flash_erase();
        nvs_flash_init();
    }
    Serial.println(F("[nvs] ready"));
}

uint8_t storage_get_brightness(uint8_t default_value) {
    Preferences prefs;
    if (!prefs.begin("system", true)) return default_value;
    uint8_t v = prefs.getUChar("brightness", default_value);
    prefs.end();
    return v;
}

void storage_set_brightness(uint8_t value) {
    Preferences prefs;
    if (!prefs.begin("system", false)) return;
    prefs.putUChar("brightness", value);
    prefs.end();
}
