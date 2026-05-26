#pragma once

#include <lvgl.h>

/**
 * CacaOS visual theme — kawaii pixel art, pastel pink palette.
 *
 * Call theme_init() ONCE after lv_init().
 * Then use theme_color_*() and the exported lv_style_t variables.
 */

void theme_init(void);

// --- Color accessors (computed once, returned by reference) ---
lv_color_t theme_color_bg(void);
lv_color_t theme_color_card(void);
lv_color_t theme_color_primary(void);
lv_color_t theme_color_accent(void);
lv_color_t theme_color_text(void);
lv_color_t theme_color_text_light(void);
lv_color_t theme_color_success(void);
lv_color_t theme_color_warn(void);

// --- Global styles (use in apps via lv_obj_add_style) ---
extern lv_style_t theme_style_card;
extern lv_style_t theme_style_button_primary;
extern lv_style_t theme_style_button_secondary;
extern lv_style_t theme_style_title;
extern lv_style_t theme_style_body;
extern lv_style_t theme_style_caption;

// --- Metric constants ---
#define THEME_RADIUS_CARD    12
#define THEME_RADIUS_BUTTON  20
#define THEME_PADDING_CARD   12
#define THEME_PADDING_SMALL  6
