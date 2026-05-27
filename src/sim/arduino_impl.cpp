/**
 * @file arduino_impl.cpp
 * @brief Implementation of the Arduino stub (millis, delay, Serial).
 */

#include <Arduino.h>
#include <SDL2/SDL.h>
#include <cstdarg>
#include <cstdio>
#include <ctime>

SerialClass Serial;

int SerialClass::printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int r = ::vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
    return r;
}

// Use a monotonic boot-relative clock; SDL_GetTicks() is fine but ties us
// to SDL_Init() — fall back to CLOCK_MONOTONIC for early calls.
static uint64_t s_boot_ms_anchor = 0;

static uint64_t monotonic_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

uint32_t millis(void) {
    if (s_boot_ms_anchor == 0) s_boot_ms_anchor = monotonic_ms();
    return (uint32_t)(monotonic_ms() - s_boot_ms_anchor);
}

void delay(uint32_t ms) {
    SDL_Delay(ms);
}

void delayMicroseconds(uint32_t us) {
    struct timespec ts;
    ts.tv_sec  = us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000;
    nanosleep(&ts, nullptr);
}
