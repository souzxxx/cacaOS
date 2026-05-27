/**
 * @file weather_sim.cpp
 * @brief Returns a static, plausible weather payload so the homescreen
 *        chip renders. No network calls in sim.
 */

#include "../system/weather.h"

#include <Arduino.h>
#include <cstring>
#include <ctime>

static WeatherData s_data = {};

static void ensure_initialized(void) {
    if (s_data.valid) return;
    s_data.temp_celsius       = 23;
    s_data.feels_like_celsius = 24;
    s_data.humidity           = 62;
    s_data.condition_id       = 1; // clouds
    strncpy(s_data.description, "parcialmente nublado",
            sizeof(s_data.description) - 1);
    s_data.fetched_at_unix    = (uint32_t)time(nullptr);
    s_data.valid              = true;
}

void weather_refresh_async(void) {
    ensure_initialized();
    s_data.fetched_at_unix = (uint32_t)time(nullptr);
}

const WeatherData* weather_get(void) {
    ensure_initialized();
    return &s_data;
}

uint32_t weather_age_seconds(void) {
    if (!s_data.valid) return UINT32_MAX;
    uint32_t now = (uint32_t)time(nullptr);
    return now - s_data.fetched_at_unix;
}
