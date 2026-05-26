/**
 * @file lv_conf.h
 * @brief LVGL 9.x configuration for CacaOS on ESP32-2432S028R
 *
 * CRITICAL: LV_COLOR_16_SWAP must be 1 for TFT_eSPI (RGB565 byte order).
 * If colors look inverted (red/blue swapped), check this first.
 */

#ifndef LV_CONF_H
#define LV_CONF_H

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH       16
#define LV_COLOR_16_SWAP     1
#define LV_COLOR_SCREEN_TRANSP 1

/*=========================
   STDLIB WRAPPER SETTINGS
 *=========================*/
#define LV_USE_STDLIB_MALLOC   LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING   LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF  LV_STDLIB_BUILTIN

#define LV_MEM_SIZE            (32U * 1024U)
#define LV_MEM_POOL_INCLUDE    <stdlib.h>

/*====================
   HAL SETTINGS
 *====================*/
#define LV_DEF_REFR_PERIOD   16     /* ~60 FPS target (won't always hit) */
#define LV_DPI_DEF           130    /* matches 2.8" 240x320 */
#define LV_TICK_CUSTOM       0

/*================
 * RENDERING
 *================*/
#define LV_DRAW_SW_COMPLEX               1
#define LV_DRAW_SW_SHADOW_CACHE_SIZE     0
#define LV_DRAW_SW_CIRCLE_CACHE_SIZE     4
#define LV_USE_DRAW_SW_ASM               LV_DRAW_SW_ASM_NONE

/*================
 * LOGGING
 *================*/
#define LV_USE_LOG       0          /* 1 to enable, eats flash */
/* LV_LOG_LEVEL left undefined here; lv_conf_internal.h picks a sane default
 * based on LV_USE_LOG. Defining both was triggering a "redefined" warning. */

/*=================
 * ASSERTS
 *=================*/
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/*-------------
 * Others
 *-----------*/
#define LV_USE_PERF_MONITOR       0
#define LV_USE_MEM_MONITOR        0
#define LV_USE_REFR_DEBUG         0

/*==================
 * FONTS
 *==================*/
#define LV_FONT_MONTSERRAT_8        0
#define LV_FONT_MONTSERRAT_10       0
#define LV_FONT_MONTSERRAT_12       1
#define LV_FONT_MONTSERRAT_14       1
#define LV_FONT_MONTSERRAT_16       1
#define LV_FONT_MONTSERRAT_18       1
#define LV_FONT_MONTSERRAT_20       0
#define LV_FONT_MONTSERRAT_22       0
#define LV_FONT_MONTSERRAT_24       1
#define LV_FONT_MONTSERRAT_28       0
#define LV_FONT_MONTSERRAT_32       1
#define LV_FONT_DEFAULT             &lv_font_montserrat_14

/*==================
 *  WIDGETS
 *================*/
#define LV_USE_ANIMIMG     1
#define LV_USE_ARC         1
#define LV_USE_BAR         1
#define LV_USE_BUTTON      1
#define LV_USE_BUTTONMATRIX 1
#define LV_USE_CALENDAR    1
#define LV_USE_CANVAS      1
#define LV_USE_CHART       1
#define LV_USE_CHECKBOX    1
#define LV_USE_DROPDOWN    1
#define LV_USE_IMAGE       1
#define LV_USE_IMAGEBUTTON 1
#define LV_USE_KEYBOARD    1
#define LV_USE_LABEL       1
#define LV_USE_LED         0
#define LV_USE_LINE        1
#define LV_USE_LIST        1
#define LV_USE_MENU        0
#define LV_USE_MSGBOX      1
#define LV_USE_ROLLER      1
#define LV_USE_SCALE       0
#define LV_USE_SLIDER      1
#define LV_USE_SPAN        0
#define LV_USE_SPINBOX     1
#define LV_USE_SPINNER     1
#define LV_USE_SWITCH      1
#define LV_USE_TABLE       1
#define LV_USE_TABVIEW     1
#define LV_USE_TEXTAREA    1
#define LV_USE_TILEVIEW    1
#define LV_USE_WIN         0

/*==================
 * FILE SYSTEM (SD card)
 *================*/
#define LV_USE_FS_STDIO        1
#define LV_FS_STDIO_LETTER     'S'
#define LV_FS_STDIO_PATH       ""
#define LV_FS_STDIO_CACHE_SIZE 0

#define LV_USE_FS_POSIX        0
#define LV_USE_FS_WIN32        0
#define LV_USE_FS_FATFS        0

/*==================
 * IMAGE DECODERS
 *================*/
#define LV_USE_TINY_TTF       0
#define LV_USE_BMP            0
#define LV_USE_LODEPNG        1   /* PNG support for sprites */
#define LV_USE_LIBPNG         0
#define LV_USE_TJPGD          1   /* JPEG support for photos */
#define LV_USE_LIBJPEG_TURBO  0
#define LV_USE_GIF            0
#define LV_USE_QRCODE         0
#define LV_USE_BARCODE        0
#define LV_USE_FREETYPE       0

/*==================
 * EXTRA THEMES
 *================*/
#define LV_USE_THEME_DEFAULT  1
#define LV_THEME_DEFAULT_DARK 0
#define LV_THEME_DEFAULT_GROW 1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80

#define LV_USE_THEME_SIMPLE   0
#define LV_USE_THEME_MONO     0

/*==================
 * LAYOUTS
 *================*/
#define LV_USE_FLEX  1
#define LV_USE_GRID  1

/*==================
 * DEMOS / EXAMPLES
 *================*/
#define LV_BUILD_EXAMPLES   0
#define LV_USE_DEMO_WIDGETS 0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0
#define LV_USE_DEMO_BENCHMARK 0
#define LV_USE_DEMO_STRESS 0
#define LV_USE_DEMO_MUSIC 0

#endif /* LV_CONF_H */
