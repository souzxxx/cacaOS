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
#ifdef CACAOS_SIM
  /* SDL native backend doesn't want byte-swapped RGB565. */
  #define LV_COLOR_16_SWAP     0
#else
  #define LV_COLOR_16_SWAP     1
#endif
#define LV_COLOR_SCREEN_TRANSP 1

/*=========================
   STDLIB WRAPPER SETTINGS
 *=========================*/
/* Use the system allocator on both host and device. On the ESP32 (WROOM, no
 * PSRAM) the BUILTIN pool is reserved as a fixed static BSS block of
 * LV_MEM_SIZE regardless of actual use; at 64KB that overflowed dram0_0_seg.
 * CLIB lets LVGL allocate from the general heap (~200KB) on demand, which is
 * where the keyboard/picker peaks live comfortably. */
#define LV_USE_STDLIB_MALLOC   LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING   LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF  LV_STDLIB_BUILTIN

/* Only consulted by the BUILTIN allocator (now unused — see CLIB above). Kept
 * for reference; changing it has no effect while LV_USE_STDLIB_MALLOC is CLIB. */
#define LV_MEM_SIZE            (64U * 1024U)
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
#ifdef CACAOS_SIM
  #define LV_USE_LOG       1
  #define LV_LOG_LEVEL     LV_LOG_LEVEL_WARN   /* assert/warn only — quiet */
  #define LV_LOG_PRINTF    1
#else
  #define LV_USE_LOG       0          /* 1 to enable, eats flash */
#endif
/* LV_LOG_LEVEL left undefined when LV_USE_LOG=0; lv_conf_internal.h picks
 * a sane default. Defining both was triggering a "redefined" warning. */

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
#ifdef CACAOS_SIM
  #define LV_FS_STDIO_PATH     "./sd_card"   /* host filesystem mirror */
#else
  #define LV_FS_STDIO_PATH     "/sd"          /* ESP32 Arduino SD library mounts here */
#endif
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

/*==================
 * SIMULATOR-ONLY: SDL display + input
 *================*/
#ifdef CACAOS_SIM
  #define LV_USE_SDL              1
  #define LV_SDL_INCLUDE_PATH     <SDL2/SDL.h>
  /* PARTIAL with 2 buffers matches the device-side configuration and lets
   * lv_screen_load_anim() composite old + new screens during transitions.
   * DIRECT mode was freezing on screen pop / state change. */
  #define LV_SDL_RENDER_MODE      LV_DISPLAY_RENDER_MODE_PARTIAL
  #define LV_SDL_BUF_COUNT        2
  #define LV_SDL_FULLSCREEN       0
  #define LV_SDL_MOUSEWHEEL_MODE  LV_SDL_MOUSEWHEEL_MODE_ENCODER
  #define LV_USE_LINUX_FBDEV      0
  #define LV_USE_NUTTX            0
#endif

#endif /* LV_CONF_H */
