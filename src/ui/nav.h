#pragma once

#include <lvgl.h>

/**
 * Screen router: manages transitions between homescreen and apps.
 *
 * Pattern:
 *   - Each app exports a `show_app_<name>()` function
 *   - When called, it creates its screen and calls nav_push(scr)
 *   - The "back" button calls nav_pop() which returns to the previous screen
 *
 * Screens are NOT cached — when an app is closed (popped), its screen is
 * deleted. This trades a bit of allocation cost for clean state and
 * smaller RAM footprint.
 */

typedef enum {
    NAV_ANIM_NONE,
    NAV_ANIM_SLIDE_LEFT,
    NAV_ANIM_SLIDE_RIGHT,
    NAV_ANIM_FADE,
} nav_anim_t;

void nav_init(void);

/**
 * Push a new screen as the active one.
 * If anim != NAV_ANIM_NONE, animates the transition.
 * Previous screen is kept on stack (max depth 8).
 */
void nav_push(lv_obj_t* new_scr, nav_anim_t anim);

/**
 * Pop the current screen and return to the previous.
 * The current screen is deleted (lv_obj_delete).
 * If stack is empty, no-op.
 */
void nav_pop(nav_anim_t anim);

/**
 * Reset stack to a single root screen (use for "go home" actions).
 */
void nav_reset_to(lv_obj_t* root_scr);

/**
 * Returns the current depth of the navigation stack.
 */
int nav_depth(void);
