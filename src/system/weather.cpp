/**
 * @file weather.cpp
 * @brief OpenWeather client. Fetches current conditions and caches them.
 *
 * Performs HTTP GET on a FreeRTOS task to avoid blocking LVGL.
 */

#include "weather.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>

#include "config.h"

static WeatherData s_data = {};
static volatile bool s_fetch_in_progress = false;

static uint8_t condition_id_from_openweather(int owm_id) {
    // OpenWeather condition codes: openweathermap.org/weather-conditions
    if (owm_id >= 200 && owm_id < 300) return 3; // thunderstorm
    if (owm_id >= 300 && owm_id < 600) return 2; // drizzle + rain
    if (owm_id >= 600 && owm_id < 700) return 4; // snow
    if (owm_id >= 700 && owm_id < 800) return 5; // atmosphere (mist, fog, etc.)
    if (owm_id == 800)                 return 0; // clear
    if (owm_id > 800)                  return 1; // clouds
    return 1;
}

static void fetch_task(void* /*pv*/) {
    HTTPClient http;

    char url[256];
    snprintf(url, sizeof(url),
        "http://api.openweathermap.org/data/2.5/weather?q=%s,%s&units=%s&lang=pt_br&appid=%s",
        OPENWEATHER_CITY, OPENWEATHER_COUNTRY, OPENWEATHER_UNITS, OPENWEATHER_API_KEY);

    http.begin(url);
    http.setTimeout(5000);

    int code = http.GET();
    if (code == HTTP_CODE_OK) {
        String body = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (!err) {
            s_data.temp_celsius       = static_cast<int8_t>(doc["main"]["temp"] | 0.0f);
            s_data.feels_like_celsius = static_cast<int8_t>(doc["main"]["feels_like"] | 0.0f);
            s_data.humidity           = static_cast<uint8_t>(doc["main"]["humidity"] | 0);

            int owm_id = doc["weather"][0]["id"] | 800;
            s_data.condition_id = condition_id_from_openweather(owm_id);

            const char* desc = doc["weather"][0]["description"] | "";
            strncpy(s_data.description, desc, sizeof(s_data.description) - 1);
            s_data.description[sizeof(s_data.description) - 1] = '\0';

            // Capture current Unix time for staleness tracking
            time_t now;
            time(&now);
            s_data.fetched_at_unix = static_cast<uint32_t>(now);
            s_data.valid = true;

            Serial.printf("[weather] %d°C %s\n", s_data.temp_celsius, s_data.description);
        } else {
            Serial.printf("[weather] JSON parse failed: %s\n", err.c_str());
        }
    } else {
        Serial.printf("[weather] HTTP %d\n", code);
    }

    http.end();
    s_fetch_in_progress = false;
    vTaskDelete(NULL);
}

void weather_refresh_async(void) {
    if (s_fetch_in_progress) return;
    if (WiFi.status() != WL_CONNECTED) return;

    s_fetch_in_progress = true;
    // Pin to core 0 (LVGL runs on core 1 by default in Arduino-ESP32)
    xTaskCreatePinnedToCore(fetch_task, "weather", 8192, NULL, 1, NULL, 0);
}

const WeatherData* weather_get(void) {
    return &s_data;
}

uint32_t weather_age_seconds(void) {
    if (!s_data.valid) return UINT32_MAX;
    time_t now;
    time(&now);
    if (static_cast<uint32_t>(now) < s_data.fetched_at_unix) return 0;
    return static_cast<uint32_t>(now) - s_data.fetched_at_unix;
}
