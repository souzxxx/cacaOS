/**
 * @file pixel_icons.cpp
 * @brief 16x16 silhouette-style pixel art icons for the homescreen grid.
 *
 * Each icon is hand-drawn as a 16-row char grid. The render path:
 *   1. Allocate an ARGB8888 buffer at the requested display size (16 × scale)
 *   2. Create an lv_canvas backed by that buffer
 *   3. Walk the grid: '#' becomes a pixel_size × pixel_size block in the
 *      requested colour; ' ' stays transparent
 *   4. Attach an LV_EVENT_DELETE hook that frees the buffer when the canvas
 *      is destroyed (so callers don't have to manage memory)
 *
 * Design choices follow skills/pixel-art-sprites: silhouette first, 2 colours
 * total per icon, hard edges, integer scaling. All artwork is constrained to
 * rows 2..13 (top/bottom 2 rows reserved as padding) so the displayed icon
 * leaves comfortable space above the label below the card.
 */

#include "pixel_icons.h"

#include <stdint.h>
#include <string.h>

namespace {

constexpr int W = 16;
constexpr int H = 16;

struct IconArt {
    const char* rows[H];
};

// clang-format off
const IconArt ICONS[PIX_COUNT] = {
    // PIX_GALLERY — photo frame with sun (top-left) + mountain (bottom)
    {{
        "                ",
        "                ",
        " ############## ",
        " #            # ",
        " #  ##        # ",
        " # ####       # ",
        " #  ##        # ",
        " #            # ",
        " #     #      # ",
        " #    ###     # ",
        " #   #####    # ",
        " #  #######   # ",
        " # #########  # ",
        " ############## ",
        "                ",
        "                ",
    }},
    // PIX_DAILY_CARD — envelope with V flap
    {{
        "                ",
        "                ",
        " ############## ",
        " ##          ## ",
        " # ##      ## # ",
        " #  ##    ##  # ",
        " #   ##  ##   # ",
        " #    ####    # ",
        " #     ##     # ",
        " #            # ",
        " #            # ",
        " #            # ",
        " #            # ",
        " ############## ",
        "                ",
        "                ",
    }},
    // PIX_COUNTER — two interlocked wedding rings ("alianças")
    {{
        "                ",
        "                ",
        "                ",
        "   ####  ####   ",
        "  ##  ####  ##  ",
        " ##    ##    ## ",
        " #     ##     # ",
        " #     ##     # ",
        " ##    ##    ## ",
        "  ##  ####  ##  ",
        "   ####  ####   ",
        "                ",
        "                ",
        "                ",
        "                ",
        "                ",
    }},
    // PIX_OPEN_WHEN — paper sheet with text lines
    {{
        "                ",
        "                ",
        "                ",
        "   ##########   ",
        "   #        #   ",
        "   # ###### #   ",
        "   #        #   ",
        "   # ###### #   ",
        "   #        #   ",
        "   # ###### #   ",
        "   #        #   ",
        "   # ###    #   ",
        "   #        #   ",
        "   ##########   ",
        "                ",
        "                ",
    }},
    // PIX_MEMORY — brain (two hemispheres with wrinkle hints)
    {{
        "                ",
        "                ",
        "    ########    ",
        "   ##########   ",
        "  ## ##  ## ##  ",
        "  ############  ",
        "  ## ##  ## ##  ",
        "  ############  ",
        "  ## ##  ## ##  ",
        "   ##########   ",
        "    ########    ",
        "                ",
        "                ",
        "                ",
        "                ",
        "                ",
    }},
    // PIX_POMODORO — tomato body (#) with green leaf and stem (+)
    {{
        "                ",
        "                ",
        "      +++       ",
        "     +++        ",
        "      +         ",
        "     ######     ",
        "    ########    ",
        "   ##########   ",
        "  ############  ",
        "  ############  ",
        "   ##########   ",
        "    ########    ",
        "                ",
        "                ",
        "                ",
        "                ",
    }},
    // PIX_MOOD — outlined smile face
    {{
        "                ",
        "                ",
        "     ######     ",
        "   ##      ##   ",
        "  #          #  ",
        "  #  ##  ##  #  ",
        " #            # ",
        " #            # ",
        " #            # ",
        " #  #      #  # ",
        " #   ######   # ",
        "  #          #  ",
        "   ##      ##   ",
        "     ######     ",
        "                ",
        "                ",
    }},
    // PIX_PET — filled house silhouette with window and door cutouts
    {{
        "                ",
        "                ",
        "       ##       ",
        "      ####      ",
        "     ######     ",
        "    ########    ",
        "   ##########   ",
        "  ############  ",
        "  ############  ",
        "  ##  ####  ##  ",
        "  ##  ####  ##  ",
        "  ############  ",
        "  ####    ####  ",
        "  ####    ####  ",
        "                ",
        "                ",
    }},
    // PIX_SETTINGS — gear with 4 teeth + center hole (symmetric)
    {{
        "                ",
        "                ",
        "      ####      ",
        "      ####      ",
        "   ##########   ",
        "  ############  ",
        " ## ######## ## ",
        " #####    ##### ",
        " #####    ##### ",
        " ## ######## ## ",
        "  ############  ",
        "   ##########   ",
        "      ####      ",
        "      ####      ",
        "                ",
        "                ",
    }},
    // PIX_LOCK — padlock: inverted-U shackle over a solid body with a keyhole
    {{
        "                ",
        "                ",
        "                ",
        "     ######     ",
        "     ##  ##     ",
        "     ##  ##     ",
        "     ##  ##     ",
        "   ##########   ",
        "   ##########   ",
        "   ####  ####   ",
        "   ####  ####   ",
        "   ##########   ",
        "   ##########   ",
        "   ##########   ",
        "                ",
        "                ",
    }},
    // PIX_STAT_HUNGER — apple with stem + top dimple (stat: fome)
    {{
        "                ",
        "                ",
        "        #       ",
        "       #        ",
        "     ## ##      ",
        "    #######     ",
        "   #########    ",
        "   #########    ",
        "   #########    ",
        "   #########    ",
        "   #########    ",
        "    #######     ",
        "    ##   ##     ",
        "                ",
        "                ",
        "                ",
    }},
    // PIX_STAT_HAPPY — heart (stat: feliz)
    {{
        "                ",
        "                ",
        "                ",
        "   ###    ###   ",
        "  #####  #####  ",
        "  ############  ",
        "  ############  ",
        "   ##########   ",
        "    ########    ",
        "     ######     ",
        "      ####      ",
        "       ##       ",
        "                ",
        "                ",
        "                ",
        "                ",
    }},
    // PIX_STAT_ENERGY — lightning bolt (stat: energia)
    {{
        "                ",
        "                ",
        "        ####    ",
        "       ###      ",
        "      ###       ",
        "     ###        ",
        "    ########    ",
        "       ###      ",
        "      ###       ",
        "     ###        ",
        "    ###         ",
        "   ##           ",
        "                ",
        "                ",
        "                ",
        "                ",
    }},
    // PIX_STAT_CLEAN — water drop (stat: limpeza)
    {{
        "                ",
        "       ##       ",
        "       ##       ",
        "      ####      ",
        "      ####      ",
        "     ######     ",
        "     ######     ",
        "    ########    ",
        "   ##########   ",
        "  ############  ",
        "  ############  ",
        "  ############  ",
        "   ##########   ",
        "    ########    ",
        "                ",
        "                ",
    }},
    // PIX_MOOD_HAPPY — eyes + SOLID rounded smile (filled, no tips/point)
    {{
        "                ",
        "                ",
        "                ",
        "                ",
        "                ",
        "    ##    ##    ",
        "    ##    ##    ",
        "                ",
        "                ",
        "                ",
        "   ##########   ",
        "    ########    ",
        "     ######     ",
        "                ",
        "                ",
        "                ",
    }},
    // PIX_MOOD_OK — eyes + gentle small smile
    {{
        "                ",
        "                ",
        "                ",
        "                ",
        "                ",
        "    ##    ##    ",
        "    ##    ##    ",
        "                ",
        "                ",
        "                ",
        "                ",
        "    #      #    ",
        "     ######     ",
        "                ",
        "                ",
        "                ",
    }},
    // PIX_MOOD_MEH — eyes + flat mouth
    {{
        "                ",
        "                ",
        "                ",
        "                ",
        "                ",
        "    ##    ##    ",
        "    ##    ##    ",
        "                ",
        "                ",
        "                ",
        "                ",
        "    ########    ",
        "    ########    ",
        "                ",
        "                ",
        "                ",
    }},
    // PIX_MOOD_SAD — eyes + SOLID rounded frown (vertical mirror of the smile)
    {{
        "                ",
        "                ",
        "                ",
        "                ",
        "                ",
        "    ##    ##    ",
        "    ##    ##    ",
        "                ",
        "                ",
        "                ",
        "     ######     ",
        "    ########    ",
        "   ##########   ",
        "                ",
        "                ",
        "                ",
    }},
    // PIX_MOOD_ANGRY — angled brows + eyes + frown
    {{
        "                ",
        "                ",
        "                ",
        "  ##        ##  ",
        "    ##    ##    ",
        "    ##    ##    ",
        "    ##    ##    ",
        "                ",
        "                ",
        "                ",
        "     ######     ",
        "    ##    ##    ",
        "   ##      ##   ",
        "                ",
        "                ",
        "                ",
    }},
};
// clang-format on

void canvas_free_buf_cb(lv_event_t* e) {
    void* buf = lv_event_get_user_data(e);
    if (buf) lv_free(buf);
}

} // namespace

lv_obj_t* pixel_icon_create(lv_obj_t* parent, PixelIconId id,
                            uint32_t primary_hex, uint32_t secondary_hex,
                            int pixel_size) {
    if (id < 0 || id >= PIX_COUNT) return nullptr;
    if (pixel_size < 1) pixel_size = 1;

    const int dw = W * pixel_size;
    const int dh = H * pixel_size;
    const size_t buf_size = (size_t)dw * dh * 4 + 32;   // ARGB8888 + slack

    uint8_t* buf = (uint8_t*)lv_malloc(buf_size);
    if (!buf) return nullptr;
    memset(buf, 0, buf_size);

    lv_obj_t* canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(canvas, buf, dw, dh, LV_COLOR_FORMAT_ARGB8888);
    lv_canvas_fill_bg(canvas, lv_color_hex(0), LV_OPA_TRANSP);

    lv_color_t primary   = lv_color_hex(primary_hex);
    lv_color_t secondary = lv_color_hex(secondary_hex);
    const IconArt& art = ICONS[id];

    for (int row = 0; row < H; ++row) {
        const char* line = art.rows[row];
        for (int col = 0; col < W; ++col) {
            char c = line[col];
            if (c != '#' && c != '+') continue;
            lv_color_t color = (c == '+') ? secondary : primary;
            for (int dy = 0; dy < pixel_size; ++dy) {
                for (int dx = 0; dx < pixel_size; ++dx) {
                    lv_canvas_set_px(canvas,
                                     col * pixel_size + dx,
                                     row * pixel_size + dy,
                                     color, LV_OPA_COVER);
                }
            }
        }
    }

    // Free the backing buffer when the canvas object is deleted.
    lv_obj_add_event_cb(canvas, canvas_free_buf_cb, LV_EVENT_DELETE, buf);
    return canvas;
}
