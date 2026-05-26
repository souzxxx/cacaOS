/**
 * @file sdcard.cpp
 * @brief microSD card mounting on dedicated SPI bus.
 *
 * Pinout (see PLAN.md):
 *   MISO=19, MOSI=23, SCLK=18, CS=5
 */

#include "sdcard.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

static constexpr int SD_CS    = 5;
static constexpr int SD_MOSI  = 23;
static constexpr int SD_MISO  = 19;
static constexpr int SD_SCLK  = 18;

static bool s_mounted = false;

bool sdcard_init(void) {
    // Use the default SPI bus reconfigured for SD pins.
    // Important: TFT_eSPI uses its own SPI instance (HSPI), so no conflict.
    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);

    if (!SD.begin(SD_CS, SPI, 25000000)) {
        Serial.println(F("[sd] mount failed"));
        s_mounted = false;
        return false;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println(F("[sd] no card attached"));
        s_mounted = false;
        return false;
    }

    const char* type_str = "UNKNOWN";
    switch (cardType) {
        case CARD_MMC:  type_str = "MMC"; break;
        case CARD_SD:   type_str = "SDSC"; break;
        case CARD_SDHC: type_str = "SDHC"; break;
        default: break;
    }

    uint64_t card_size_mb = SD.cardSize() / (1024ULL * 1024ULL);
    Serial.printf("[sd] mounted: %s, %llu MB\n", type_str, card_size_mb);

    s_mounted = true;
    return true;
}

bool sdcard_is_mounted(void) {
    return s_mounted;
}

size_t sdcard_total_bytes(void) {
    if (!s_mounted) return 0;
    return SD.totalBytes();
}

size_t sdcard_free_bytes(void) {
    if (!s_mounted) return 0;
    return SD.totalBytes() - SD.usedBytes();
}

bool sdcard_file_exists(const char* path) {
    if (!s_mounted) return false;
    return SD.exists(path);
}
