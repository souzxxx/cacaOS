/**
 * @file config.h (example template)
 *
 * COPY THIS to config.h and fill in your values.
 * config.h is GITIGNORED — never commit it.
 *
 *   cp src/config.example.h src/config.h
 */

#pragma once

// ============================================================
// WiFi credentials
// ============================================================
#define WIFI_SSID         "YOUR_WIFI_NAME"
#define WIFI_PASS         "YOUR_WIFI_PASSWORD"

// ============================================================
// OpenWeatherMap (sign up at openweathermap.org/api, free tier)
// ============================================================
#define OPENWEATHER_API_KEY  "YOUR_API_KEY_HERE"
#define OPENWEATHER_CITY     "Sao Paulo"
#define OPENWEATHER_COUNTRY  "BR"
#define OPENWEATHER_UNITS    "metric"   // metric | imperial

// ============================================================
// NTP time sync
// ============================================================
#define NTP_SERVER_1         "pool.ntp.org"
#define NTP_SERVER_2         "br.pool.ntp.org"
#define TZ_INFO              "BRT3"     // São Paulo = UTC-3, no DST since 2019

// ============================================================
// Personal context (used by Counter app and Tamagotchi default name)
// ============================================================
#define RELATIONSHIP_START   "2024-01-01"   // YYYY-MM-DD
#define HER_NAME             "amor"
#define DEFAULT_PET_NAME     "Caca"

// ============================================================
// OTA (optional, for future use)
// ============================================================
#define OTA_HOSTNAME         "caca-os"
#define OTA_PASSWORD         "changeme"
