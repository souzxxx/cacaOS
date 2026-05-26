#pragma once

#include <stdint.h>

/**
 * Ambient light sensor (LDR) on GPIO 34. Drives auto-dim of the display.
 *
 * Reading model: analogRead returns 0..4095 (12-bit). Low = dark, high = bright.
 * Internal exponential moving average smooths jitter; ldr_loop() must be
 * called from the main loop and self-throttles its work.
 *
 * The auto-dim multiplies the user's stored brightness ceiling
 * (storage_get_brightness) by 0.4..1.0 based on normalized lux.
 */
void ldr_init(void);

/** Sample + maybe re-apply brightness. Self-throttled — safe to call every loop. */
void ldr_loop(void);

/** Latest filtered normalized lux value (0.0 = dark, 1.0 = bright). */
float ldr_normalized(void);
