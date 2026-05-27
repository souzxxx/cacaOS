/**
 * @file Arduino.h (simulator stub)
 *
 * Minimal subset of the Arduino API used by CacaOS apps.
 * Only included when CACAOS_SIM is defined (via -I src/sim/include in env:sim).
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>

// ----- Macros that show up in app code -----
#ifndef F
  #define F(x) (x)
#endif

#ifndef PROGMEM
  #define PROGMEM
#endif

#ifndef IRAM_ATTR
  #define IRAM_ATTR
#endif

#ifndef HIGH
  #define HIGH 1
#endif
#ifndef LOW
  #define LOW 0
#endif

// ----- Timing -----
uint32_t millis(void);
void     delay(uint32_t ms);
void     delayMicroseconds(uint32_t us);

// ----- Serial -----
class SerialClass {
public:
    void begin(unsigned long /*baud*/) {}
    int  printf(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    void print(const char* s)       { fputs(s, stdout); fflush(stdout); }
    void print(int v)               { ::printf("%d", v); fflush(stdout); }
    void print(unsigned v)          { ::printf("%u", v); fflush(stdout); }
    void print(long v)              { ::printf("%ld", v); fflush(stdout); }
    void print(unsigned long v)     { ::printf("%lu", v); fflush(stdout); }
    void print(double v)            { ::printf("%f", v); fflush(stdout); }
    void println()                  { fputc('\n', stdout); fflush(stdout); }
    void println(const char* s)     { fputs(s, stdout); fputc('\n', stdout); fflush(stdout); }
    void println(int v)             { ::printf("%d\n", v); fflush(stdout); }
    void println(unsigned v)        { ::printf("%u\n", v); fflush(stdout); }
    void println(long v)            { ::printf("%ld\n", v); fflush(stdout); }
    void println(unsigned long v)   { ::printf("%lu\n", v); fflush(stdout); }
};
extern SerialClass Serial;

// ----- Arduino String (minimal subset used by apps) -----
class String {
public:
    String() = default;
    String(const char* s) : _s(s ? s : "") {}
    String(const std::string& s) : _s(s) {}
    String(const String& other) = default;
    String& operator=(const String& other) = default;
    String& operator=(const char* s) { _s = s ? s : ""; return *this; }

    const char* c_str() const { return _s.c_str(); }
    size_t      length() const { return _s.size(); }
    bool        isEmpty() const { return _s.empty(); }

    void trim() {
        size_t start = _s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) { _s.clear(); return; }
        size_t end = _s.find_last_not_of(" \t\r\n");
        _s = _s.substr(start, end - start + 1);
    }

    bool operator==(const char* s) const { return _s == (s ? s : ""); }
    bool operator==(const String& o) const { return _s == o._s; }
    bool operator!=(const char* s) const { return !(*this == s); }

    char operator[](size_t i) const { return _s[i]; }

    // Allow internal append by other shims (e.g., File::readStringUntil).
    std::string& _raw() { return _s; }
    const std::string& _raw() const { return _s; }

private:
    std::string _s;
};

// ----- PWM stubs (display + rgb_led use these but those modules are excluded) -----
inline void analogWrite(int /*pin*/, int /*value*/) {}
inline int  analogRead(int /*pin*/) { return 2048; }
inline void pinMode(int /*pin*/, int /*mode*/) {}
inline void digitalWrite(int /*pin*/, int /*value*/) {}
inline int  digitalRead(int /*pin*/) { return 0; }

// ----- Piezo (pomodoro chime) — silent in sim -----
inline void tone(int /*pin*/, unsigned int /*freq*/, unsigned long /*dur_ms*/ = 0) {}
inline void noTone(int /*pin*/) {}

// ----- ESP32 random (memory_game uses it for shuffle) -----
#include <cstdlib>
inline uint32_t esp_random(void) { return (uint32_t)rand(); }

// ----- ESP32 time helper (Arduino-ESP32 specific) -----
// Wraps localtime_r + retry-on-timeout. In sim, the host clock is always
// "synced", so we just call localtime_r once.
#include <time.h>
inline bool getLocalTime(struct tm* info, uint32_t /*ms_timeout*/ = 5000) {
    time_t now = ::time(nullptr);
    return ::localtime_r(&now, info) != nullptr;
}
