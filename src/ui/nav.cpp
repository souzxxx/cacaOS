/**
 * @file nav.cpp
 * @brief Simple screen stack with animated transitions.
 */

#include "nav.h"

#include <Arduino.h>

static constexpr int NAV_MAX_DEPTH = 8;

static lv_obj_t* s_stack[NAV_MAX_DEPTH] = {};
static int s_depth = 0;

static lv_screen_load_anim_t to_lvgl_anim(nav_anim_t a) {
    switch (a) {
        case NAV_ANIM_SLIDE_LEFT:  return LV_SCR_LOAD_ANIM_MOVE_LEFT;
        case NAV_ANIM_SLIDE_RIGHT: return LV_SCR_LOAD_ANIM_MOVE_RIGHT;
        case NAV_ANIM_FADE:        return LV_SCR_LOAD_ANIM_FADE_IN;
        default:                   return LV_SCR_LOAD_ANIM_NONE;
    }
}

void nav_init(void) {
    s_depth = 0;
    for (int i = 0; i < NAV_MAX_DEPTH; ++i) s_stack[i] = nullptr;
}

void nav_push(lv_obj_t* new_scr, nav_anim_t anim) {
    if (!new_scr) return;
    if (s_depth >= NAV_MAX_DEPTH) {
        Serial.println(F("[nav] stack overflow!"));
        return;
    }

    // The current active screen goes on the stack
    s_stack[s_depth++] = lv_screen_active();

    // time=0 with NONE anim takes LVGL's "immediate load" shortcut; otherwise
    // even NONE creates a 300ms dummy animation that collides with a follow-up
    // load (the wizard does pop+show_next back-to-back).
    uint32_t time = (anim == NAV_ANIM_NONE) ? 0 : 300;
    lv_screen_load_anim(new_scr, to_lvgl_anim(anim), time, 0, false);
}

static void nav_delete_async_cb(void* obj) {
    if (obj) lv_obj_delete((lv_obj_t*)obj);
}

void nav_pop(nav_anim_t anim) {
    if (s_depth == 0) {
        Serial.println(F("[nav] pop on empty stack"));
        return;
    }

    lv_obj_t* current = lv_screen_active();
    lv_obj_t* prev = s_stack[--s_depth];
    s_stack[s_depth] = nullptr;

    // Use the opposite direction of push for a natural feel
    nav_anim_t back_anim = anim;
    if (anim == NAV_ANIM_SLIDE_LEFT)  back_anim = NAV_ANIM_SLIDE_RIGHT;
    if (anim == NAV_ANIM_SLIDE_RIGHT) back_anim = NAV_ANIM_SLIDE_LEFT;

    // Two deletion strategies depending on anim type:
    //  - NONE: shortcut path (time=0) swaps the screen synchronously, then
    //    we async-delete `current` because the event callback that called
    //    nav_pop still lives on `current`.
    //  - Animated: let LVGL auto-delete on animation completion. Doing
    //    async-delete here would fire before the 300ms anim finishes and
    //    delete the still-active screen.
    if (back_anim == NAV_ANIM_NONE) {
        lv_screen_load_anim(prev, to_lvgl_anim(back_anim), 0, 0, false);
        lv_async_call(nav_delete_async_cb, current);
    } else {
        lv_screen_load_anim(prev, to_lvgl_anim(back_anim), 300, 0, true);
    }
}

void nav_reset_to(lv_obj_t* root_scr) {
    // Delete all stacked screens
    for (int i = 0; i < s_depth; ++i) {
        if (s_stack[i]) lv_obj_delete(s_stack[i]);
        s_stack[i] = nullptr;
    }
    s_depth = 0;
    lv_screen_load_anim(root_scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
}

int nav_depth(void) {
    return s_depth;
}
