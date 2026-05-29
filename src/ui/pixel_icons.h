#pragma once

#include <lvgl.h>

/**
 * 16x16 pixel-art icons for the homescreen. Each icon is encoded as a
 * 16-row char-grid:
 *   ' '  transparent (card shows through)
 *   '#'  primary colour (per-icon tint)
 *   '+'  secondary colour (optional accent, e.g. the green leaf on the
 *        Pomodoro tomato). Defaults to the primary if no secondary is passed.
 *
 * Designs follow the pixel-art guidance from skills/pixel-art-sprites:
 * silhouette-first, hard edges, no anti-aliasing, integer scaling.
 *
 * Display size on the homescreen is 32x32 (rendered via lv_canvas at 2x).
 */

enum PixelIconId {
    PIX_GALLERY = 0,
    PIX_DAILY_CARD,
    PIX_COUNTER,
    PIX_OPEN_WHEN,
    PIX_MEMORY,
    PIX_POMODORO,
    PIX_MOOD,
    PIX_PET,
    PIX_SETTINGS,
    PIX_LOCK,        // padlock badge (used by the WiFi list for secured networks)
    PIX_COUNT
};

// Creates an lv_canvas inside `parent` rendering the icon at the requested
// pixel size (use 2 for 32x32, 1 for native 16x16). The canvas is positioned
// with lv_obj_align; caller can re-align afterwards.
//
// `primary_hex` colours '#' cells; `secondary_hex` colours '+' cells. Pass
// the same value for both if the icon only uses '#'.
lv_obj_t* pixel_icon_create(lv_obj_t* parent, PixelIconId id,
                            uint32_t primary_hex, uint32_t secondary_hex,
                            int pixel_size);
