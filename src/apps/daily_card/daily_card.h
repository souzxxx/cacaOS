#pragma once

/**
 * Cartinha app — Daily message rotation. Reads /messages.json from SD.
 *
 * Entry point. Creates the app's screen and pushes it via nav_push().
 * Returns to homescreen when user taps the back button.
 *
 * TODO: see PLAN.md section 5 for full spec.
 */
void daily_card_show(void);
