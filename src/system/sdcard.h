#pragma once

#include <stdbool.h>
#include <stddef.h>

/**
 * Mount the SD card via SPI bus (CS=GPIO 5).
 * Returns true if mounted successfully, false if no card or failed.
 *
 * Safe to call even if no card present — system continues without it.
 */
bool sdcard_init(void);

/**
 * Check if SD is currently available.
 */
bool sdcard_is_mounted(void);

/**
 * Get total / free bytes on SD card. Returns 0 if not mounted.
 */
size_t sdcard_total_bytes(void);
size_t sdcard_free_bytes(void);

/**
 * Check if a file exists at the given path. Path should NOT include LVGL
 * driver letter ('S:'), just the absolute filesystem path (e.g. "/photos/01.jpg").
 */
bool sdcard_file_exists(const char* path);
