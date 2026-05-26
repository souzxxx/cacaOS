#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * Weather data structure.
 * Conditions are mapped to a small int for icon selection:
 *   0 = clear, 1 = clouds, 2 = rain, 3 = thunderstorm, 4 = snow, 5 = mist
 */
struct WeatherData {
    int8_t   temp_celsius;
    int8_t   feels_like_celsius;
    uint8_t  humidity;
    uint8_t  condition_id;
    char     description[32];      // PT-BR description from OpenWeather
    uint32_t fetched_at_unix;      // when this data was retrieved
    bool     valid;
};

/**
 * Trigger an async weather fetch. Non-blocking.
 * Result becomes available via weather_get() after a few hundred ms.
 *
 * Safe to call repeatedly — internally deduplicates concurrent fetches.
 */
void weather_refresh_async(void);

/**
 * Get latest cached weather data. If never fetched or stale, .valid will be false.
 */
const WeatherData* weather_get(void);

/**
 * Return age in seconds of the cached weather data.
 * Returns UINT32_MAX if no data yet.
 */
uint32_t weather_age_seconds(void);
