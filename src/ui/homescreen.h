#pragma once

/**
 * Build and show the homescreen.
 * Creates the main launcher with:
 *   - Status bar (time, weather, wifi indicator)
 *   - 3x3 grid of app icons
 *   - Caca mascot peeking in corner
 *
 * Updates time and weather every minute via internal LVGL timer.
 */
void homescreen_show(void);

/**
 * Force a refresh of all dynamic elements (time, weather, wifi).
 * Useful after returning from an app that may have changed state.
 */
void homescreen_refresh(void);
