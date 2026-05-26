# CLAUDE.md

> Instructions for Claude Code working on **CacaOS** — an ESP32-2432S028R (Cheap Yellow Display) mini-OS built as a personal gift. Read `PLAN.md` for full project spec. This file covers **how** to work on the codebase, not **what** to build.

---

## Project at a glance

- **Target hardware:** ESP32-2432S028R (CYD), no PSRAM revision
- **Stack:** PlatformIO + Arduino framework + LVGL 9.x + TFT_eSPI + XPT2046_Touchscreen
- **Theme:** kawaii pixel art, pastel pink palette
- **Host:** macOS (Apple Silicon), CH340 USB-serial driver required
- **Owner:** Leonardo (souzxxx)

For pinout, library versions, paleta, app specs, and roadmap → see `PLAN.md`.

---

## Development environment

```
host:       macOS (Apple Silicon)
editor:     VSCode + PlatformIO extension (or pio CLI)
serial:     /dev/cu.wchusbserial* (CH340)
shell:      zsh
git:        always create feature branches, never push to main directly
```

Required setup steps before any build:

```bash
# install CH340 driver (App Store: "CH34xVCPDriver") — manual one-time step
# install PlatformIO CLI
brew install platformio

# clone & configure
git clone <repo> && cd cacaos
cp src/config.example.h src/config.h
# fill in WIFI_SSID, WIFI_PASS, OPENWEATHER_API_KEY, START_DATE, HER_NAME
```

---

## Common commands

```bash
# build only
pio run

# build + flash
pio run -t upload

# build + flash + open serial monitor
pio run -t upload -t monitor

# serial monitor only
pio device monitor -b 115200

# upload LittleFS (fonts, embedded icons)
pio run -t uploadfs

# clean build artifacts
pio run -t clean

# list connected devices
pio device list
```

**Never run `pio run -t upload` unless the user confirms the placa is connected.** Building is fine without hardware; flashing is not.

---

## Code conventions

### Language

- **Code, comments, docstrings, commit messages:** English
- **User-facing content** (mensagens.json, open_when/*.txt, app labels visible on device): **Portuguese (PT-BR)**

### Naming

- C files: `snake_case.cpp` / `snake_case.h`
- C functions: `snake_case`
- C++ classes: `PascalCase`
- C++ methods: `camelCase`
- Constants/macros: `UPPER_SNAKE_CASE`
- LVGL widget pointers: prefix with `lv_` (e.g., `lv_obj_t *lv_main_screen`)

### File organization

Every app lives in `src/apps/<app_name>/` with:
- `<app_name>.cpp` — implementation
- `<app_name>.h` — public interface (typically just `void <app_name>_show(void)` and `void <app_name>_hide(void)`)
- Internal helpers stay `static` in the .cpp

System modules in `src/system/`, UI primitives in `src/ui/widgets/`.

### Commits

Conventional commits, always in English:
- `feat(gallery): add swipe-to-navigate`
- `fix(touch): correct Y-axis inversion`
- `chore(deps): bump lvgl to 9.2.1`
- `refactor(theme): extract card style to helper`

One logical change per commit. Don't mix refactors with features.

---

## Critical hardware constraints

These bite people on this board. Watch for them:

1. **`LV_COLOR_16_SWAP` must be `1`** in `include/lv_conf.h`. If colors look inverted (red/blue swapped), this is the cause 95% of the time.

2. **No PSRAM** on this revision. SRAM is 520KB total, ~200KB usable for app heap. Do not assume PSRAM is available. Don't decode multiple large images concurrently — stream from SD instead.

3. **Touch needs calibration.** Raw XPT2046 readings are not display coordinates. Apply offset + scale from calibration step (stored in NVS). Default `Touch_calibrate()` from TFT_eSPI works as starting point.

4. **SPI bus is shared.** Touch, display, and SD card all use SPI but on different controllers/CS pins. Don't initialize them in conflicting orders. Init sequence: display → touch → SD.

5. **GPIO pinout is fixed.** Refer to `PLAN.md` section 1 for exact pins. **Never guess GPIO numbers** — copy from PLAN.

6. **Speaker is a passive piezo on GPIO 26.** No I2S codec onboard. Use `ledcWriteTone()` or the `tone()` polyfill for beeps. Don't try to play MP3/WAV without external DAC.

7. **`huge_app.csv` partition is required.** LVGL + Arduino framework barely fits in default 1.4MB. Configured in platformio.ini already — don't change without checking.

8. **Flash before adding new libraries.** Each lib_deps addition can push the binary over the partition limit. After adding a dep, run `pio run` and confirm size before assuming it works.

---

## LVGL specifics

- **LVGL 9.x API** (not 8.x). Many tutorials online still show 8.x — verify before copy-pasting.
- **Tick handler:** LVGL needs `lv_tick_inc(ms)` called periodically. Use a hardware timer or `millis()` delta in loop. Configured in `main.cpp`.
- **Memory:** LVGL has its own allocator (`LV_MEM_SIZE`). Bumping it eats SRAM. Default 48KB is tight but works for our apps.
- **Screens:** prefer `lv_scr_load_anim()` for transitions between apps (slide, fade). Default 300ms.
- **Don't recreate screens every show.** Cache them; just show/hide via `lv_scr_load()`. Memory-wise this matters.
- **Images from SD:** path format is `S:/photos/name.jpg` (LVGL uses driver letter prefix, not Unix paths).
- **Custom fonts:** generate with `lv_font_conv`, register via `LV_FONT_DECLARE(my_font)`. Don't include the .c font files in build flags — let the compiler pick them up from `src/`.

---

## Memory + performance budget

- **Heap:** ~200KB after LVGL + WiFi + SD init. Anything bigger than 80KB allocation should be questioned.
- **Stack:** Arduino loop runs in an 8KB stack. Don't put large arrays as local vars in deep call chains.
- **Frame rate target:** 30 FPS for animations, 60 FPS for transitions when possible.
- **JPG decode:** ~500-800ms for full-screen photo. Always preload in a task on core 0 when navigating.
- **Boot target:** <5s from power-on to homescreen interactive.

If a feature requires more memory or compute than the budget, surface that to the user before implementing — don't silently degrade quality.

---

## Communication style with user (Leonardo)

- Respond in **casual Portuguese (PT-BR)**, direct, no excessive formality.
- Code blocks, file paths, library names stay in English.
- Don't over-explain when he asks for something specific — he's a CS student, embedded-comfortable. Skip the basics unless he asks.
- When suggesting changes, **show the diff** or the specific file/lines. Don't paraphrase what you're about to do.
- If a task is ambiguous, ask **one** focused question, not a checklist.
- He gravitates toward polish — when there's a nicer animation, transition, or visual flourish worth adding, mention it as an option (not as default work).

---

## When to ask vs. when to just do it

**Just do it** (no permission needed):
- Implementing what's spec'd in PLAN.md for the current phase
- Adding logging/serial prints for debugging
- Refactoring within a file for clarity
- Fixing obvious bugs introduced in the same session

**Ask first:**
- Adding a new lib_dep (partition risk)
- Changing GPIO assignments
- Changing partition table or build flags
- Modifying `lv_conf.h` settings beyond what PLAN specifies
- Skipping or reordering phases from PLAN
- Any `pio run -t upload` (need confirmation hardware is connected)
- Anything touching `config.h` (contains personal secrets)

---

## Things to never do

- **Never commit `src/config.h`.** It's gitignored for a reason — contains WiFi password and API keys.
- **Never push to `main` directly.** Always branch → PR → merge.
- **Never use `delay()` longer than 50ms** in callbacks — blocks LVGL tick and touch.
- **Never assume the SD card is mounted.** Check return of `SD.begin()` and degrade gracefully.
- **Never hardcode the girlfriend's name or relationship date** outside `config.h`. The codebase should be reusable (in theory).
- **Never include `<Arduino.h>` in headers** — only in .cpp. Keeps compile times reasonable.
- **Never use `String` (Arduino class) for anything performance-sensitive.** Use `const char*` or `std::string` with reserve.
- **Never call LVGL functions from interrupts or non-LVGL tasks.** Use `lv_async_call()` or a queue.

---

## Debugging workflow

When something doesn't work on hardware:

1. **Check Serial first.** `pio device monitor -b 115200`. If nothing on serial, it's a flash/driver issue, not code.
2. **Isolate the layer.** Display issue? Run a TFT_eSPI-only sketch. Touch issue? Run XPT2046 example. LVGL issue? Run LVGL hello world.
3. **Add prints, not breakpoints.** No JTAG on CYD by default. `Serial.printf("[gallery] loading %s\n", path);` is your friend.
4. **Check heap before/after suspicious code.** `ESP.getFreeHeap()` — log it.
5. **When stuck:** state the symptom + last 3 things tried in PT-BR and ask Leonardo for input. Don't spiral on the same approach.

---

## File layout (recap)

```
include/lv_conf.h           # LVGL config — change with care
src/main.cpp                # entry: setup() + loop() + LVGL tick
src/config.h                # GITIGNORED — secrets
src/config.example.h        # template
src/system/                 # hardware abstraction
src/ui/theme.{cpp,h}        # palette + global LVGL styles
src/ui/homescreen.{cpp,h}   # main launcher
src/ui/nav.{cpp,h}          # app router
src/ui/widgets/             # reusable LVGL widgets
src/apps/<name>/<name>.{cpp,h}  # one folder per app
data/                       # LittleFS partition contents
sd_card/                    # mirror of what goes on physical SD
```

---

## Reference docs

- LVGL 9.x: https://docs.lvgl.io/9.2/
- TFT_eSPI: https://github.com/Bodmer/TFT_eSPI
- XPT2046_Touchscreen: https://github.com/PaulStoffregen/XPT2046_Touchscreen
- CYD community repo: https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display
- ESP32 Arduino core: https://docs.espressif.com/projects/arduino-esp32/

---

## Quick "what's the status?" command

Before starting work, run:

```bash
git status && git branch --show-current && pio run --silent 2>&1 | tail -5
```

This shows: uncommitted changes, current branch, and whether the project still builds. Always start from a green build.

💕
