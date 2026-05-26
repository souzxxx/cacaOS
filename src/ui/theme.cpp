/**
 * @file theme.cpp
 * @brief Kawaii pastel pink theme — registers global LVGL styles.
 */

#include "theme.h"

#include <Arduino.h>

// Palette (matches PLAN.md section 7)
//   BG          #FFE5EC  rosa muito claro
//   CARD        #FFFFFF
//   PRIMARY     #FF8FAB  rosa médio
//   ACCENT      #FB6F92  rosa quente
//   TEXT        #C9184A  vinho
//   TEXT_LIGHT  #FFC2D1  rosa baby
//   SUCCESS     #B5E48C  verde claro
//   WARN        #FFC09F  pêssego

lv_style_t theme_style_card;
lv_style_t theme_style_button_primary;
lv_style_t theme_style_button_secondary;
lv_style_t theme_style_title;
lv_style_t theme_style_body;
lv_style_t theme_style_caption;

lv_color_t theme_color_bg(void)         { return lv_color_hex(0xFFE5EC); }
lv_color_t theme_color_card(void)       { return lv_color_hex(0xFFFFFF); }
lv_color_t theme_color_primary(void)    { return lv_color_hex(0xFF8FAB); }
lv_color_t theme_color_accent(void)     { return lv_color_hex(0xFB6F92); }
lv_color_t theme_color_text(void)       { return lv_color_hex(0xC9184A); }
lv_color_t theme_color_text_light(void) { return lv_color_hex(0xFFC2D1); }
lv_color_t theme_color_success(void)    { return lv_color_hex(0xB5E48C); }
lv_color_t theme_color_warn(void)       { return lv_color_hex(0xFFC09F); }

void theme_init(void) {
    // Set screen background color via default theme
    lv_obj_set_style_bg_color(lv_screen_active(), theme_color_bg(), LV_PART_MAIN);

    // --- Card style: white rounded box with soft pink shadow ---
    lv_style_init(&theme_style_card);
    lv_style_set_bg_color(&theme_style_card, theme_color_card());
    lv_style_set_bg_opa(&theme_style_card, LV_OPA_COVER);
    lv_style_set_radius(&theme_style_card, THEME_RADIUS_CARD);
    lv_style_set_pad_all(&theme_style_card, THEME_PADDING_CARD);
    lv_style_set_border_width(&theme_style_card, 1);
    lv_style_set_border_color(&theme_style_card, theme_color_text_light());
    lv_style_set_shadow_color(&theme_style_card, theme_color_primary());
    lv_style_set_shadow_width(&theme_style_card, 8);
    lv_style_set_shadow_ofs_y(&theme_style_card, 2);
    lv_style_set_shadow_opa(&theme_style_card, LV_OPA_30);

    // --- Button primary (accent pink) ---
    lv_style_init(&theme_style_button_primary);
    lv_style_set_bg_color(&theme_style_button_primary, theme_color_accent());
    lv_style_set_bg_opa(&theme_style_button_primary, LV_OPA_COVER);
    lv_style_set_text_color(&theme_style_button_primary, theme_color_card());
    lv_style_set_radius(&theme_style_button_primary, THEME_RADIUS_BUTTON);
    lv_style_set_pad_hor(&theme_style_button_primary, 16);
    lv_style_set_pad_ver(&theme_style_button_primary, 8);
    lv_style_set_border_width(&theme_style_button_primary, 0);

    // --- Button secondary (outlined) ---
    lv_style_init(&theme_style_button_secondary);
    lv_style_set_bg_color(&theme_style_button_secondary, theme_color_card());
    lv_style_set_bg_opa(&theme_style_button_secondary, LV_OPA_COVER);
    lv_style_set_text_color(&theme_style_button_secondary, theme_color_text());
    lv_style_set_radius(&theme_style_button_secondary, THEME_RADIUS_BUTTON);
    lv_style_set_pad_hor(&theme_style_button_secondary, 16);
    lv_style_set_pad_ver(&theme_style_button_secondary, 8);
    lv_style_set_border_width(&theme_style_button_secondary, 2);
    lv_style_set_border_color(&theme_style_button_secondary, theme_color_accent());

    // --- Title text ---
    lv_style_init(&theme_style_title);
    lv_style_set_text_color(&theme_style_title, theme_color_text());
    lv_style_set_text_font(&theme_style_title, &lv_font_montserrat_24);

    // --- Body text ---
    lv_style_init(&theme_style_body);
    lv_style_set_text_color(&theme_style_body, theme_color_text());
    lv_style_set_text_font(&theme_style_body, &lv_font_montserrat_14);

    // --- Caption (small label) ---
    lv_style_init(&theme_style_caption);
    lv_style_set_text_color(&theme_style_caption, theme_color_text());
    lv_style_set_text_font(&theme_style_caption, &lv_font_montserrat_12);

    Serial.println(F("[theme] kawaii pixel art theme loaded"));
}
