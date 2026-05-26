#pragma once

/**
 * Open When app — Envelope-style cards. Reads /open_when/*.txt from SD.
 *
 * Entry point. Creates the app's screen and pushes it via nav_push().
 * Returns to homescreen when user taps the back button.
 *
 * TODO: see PLAN.md section 5 for full spec.
 */
void open_when_show(void);
