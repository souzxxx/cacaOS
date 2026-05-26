#pragma once

#include <stdint.h>

/**
 * Initialize XPT2046 touch controller on dedicated SPI bus.
 * Reads calibration from NVS if available, otherwise applies defaults.
 */
void touch_init(void);

/**
 * Register touch with LVGL as a pointer input device.
 * Must be called AFTER lv_init() and display_register_with_lvgl().
 */
void touch_register_with_lvgl(void);

/**
 * Trigger interactive touch calibration.
 * Shows 4 corner targets, asks user to tap each, computes affine mapping,
 * saves to NVS. Returns true on success.
 *
 * Call this from a "Settings → Calibrate Touch" menu entry, OR
 * automatically on first boot if NVS has no calibration data.
 */
bool touch_calibrate(void);

/**
 * Check if calibration data exists in NVS.
 */
bool touch_is_calibrated(void);

/**
 * Reset calibration (next boot will run touch_calibrate()).
 */
void touch_reset_calibration(void);
