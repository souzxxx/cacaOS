#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * Initialize NVS (non-volatile storage).
 * Each module uses its own namespace via Preferences.
 *
 * This file provides convenience helpers for common scenarios.
 * For module-specific state (like Tamagotchi), use Preferences directly
 * with the module's namespace.
 *
 * Reserved namespaces:
 *   "touch"   - touch calibration
 *   "tama"    - tamagotchi state mirror
 *   "memory"  - memory game best times
 *   "mood"    - mood tracker entries
 *   "system"  - global system prefs (brightness, theme, etc.)
 *   "wifi"    - on-device WiFi credentials (ssid, pass)
 */
void storage_init(void);

// Convenience for system prefs
uint8_t storage_get_brightness(uint8_t default_value);
void    storage_set_brightness(uint8_t value);
