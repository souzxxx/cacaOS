/**
 * @file ldr.cpp
 * @brief Ambient light sensor (GPIO 34) -> auto-dim driver.
 */

#include "ldr.h"

#include <Arduino.h>

#include "display.h"
#include "storage.h"

static constexpr int LDR_PIN = 34;
static constexpr uint32_t SAMPLE_INTERVAL_MS = 500;
static constexpr uint32_t APPLY_INTERVAL_MS  = 3000;
static constexpr float    EMA_ALPHA          = 0.20f;
static constexpr float    DIM_FLOOR          = 0.40f;   // never drop below 40% of user setting

static float    s_filtered = 2048.0f;   // mid scale (ADC 12-bit max = 4095)
static uint32_t s_last_sample_ms = 0;
static uint32_t s_last_apply_ms  = 0;

void ldr_init(void) {
    analogReadResolution(12);
    analogSetPinAttenuation(LDR_PIN, ADC_11db);
    pinMode(LDR_PIN, INPUT);
    // Seed filter so the first apply isn't a big jump
    int raw = analogRead(LDR_PIN);
    s_filtered = (float)raw;
}

float ldr_normalized(void) {
    return s_filtered / 4095.0f;
}

static void apply_auto_brightness(void) {
    uint8_t user_ceiling = storage_get_brightness(255);
    float lux = ldr_normalized();
    if (lux < 0.0f) lux = 0.0f;
    if (lux > 1.0f) lux = 1.0f;
    float scale = DIM_FLOOR + (1.0f - DIM_FLOOR) * lux;
    uint8_t target = (uint8_t)((float)user_ceiling * scale);
    if (target < 16) target = 16;   // hard floor so screen never goes fully black
    display_set_brightness(target);
}

void ldr_loop(void) {
    uint32_t now = millis();

    if (now - s_last_sample_ms >= SAMPLE_INTERVAL_MS) {
        int raw = analogRead(LDR_PIN);
        s_filtered = (s_filtered * (1.0f - EMA_ALPHA)) + ((float)raw * EMA_ALPHA);
        s_last_sample_ms = now;
    }

    if (now - s_last_apply_ms >= APPLY_INTERVAL_MS) {
        apply_auto_brightness();
        s_last_apply_ms = now;
    }
}
