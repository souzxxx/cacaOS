# TAMAGOTCHI_SPEC.md

> Detailed specification for the **Tamagotchi app** of CacaOS — supersedes section 5.9 of `PLAN.md`.
> Personalization level: **Nível 2** (full customization + persistence + reset).

---

## 1. Concept

Instead of a fixed mascot, the app starts with an **adoption flow** the first time it's opened. The user picks her pet from 10 bunny variants, names it, picks a background, and *that's* her Tamagotchi. She can change any of those choices later in Settings without losing pet stats.

Story-wise: the device boots, she sees a welcome screen, adopts her bunny, gives it a name (default: "Caca"). From there, it's hers.

---

## 2. UX Flow

### 2.1 First Boot — Adoption Wizard

Trigger: `tamagotchi_state.json` doesn't exist on SD, OR `first_boot: true` in state.

**Screen 1 — Welcome**
- Splash with pixel hearts, primary color background
- Center: animated bunny silhouettes morphing through the 10 variants (2s cycle)
- Bottom: button `"Adotar uma Caca →"`

**Screen 2 — Pick Your Pet**
- Grid 3x4 of pet cards (10 visible + 2 hidden secret slots)
- Each card: bunny `idle.png` first frame (32x32 upscaled to 48x48) centered, on white card with rounded corners
- Tap a card → that card gets pink border + tiny heart icon top-right; bunny in card starts playing its idle animation
- Bottom button: `"Escolher 💕"`
- Locked pets (`secret: true` in manifest): show `?` placeholder until unlocked

**Screen 3 — Name Your Pet**
- Selected bunny appears big (96x96) center-top, playing idle
- Text input field with cursor, default value `"Caca"`
- On-screen keyboard (QWERTY layout, lowercase + uppercase toggle + accents via long-press)
- Max 12 characters
- Bottom button: `"Confirmar 💕"`

**Screen 4 — Pick Your Room**
- Carrousel of 20 backgrounds (swipe left/right)
- Selected bunny appears overlaid on each background as preview (idle animation)
- Background name at top: "Classic Room 1", "Pastel Room", etc.
- Bottom button: `"Pronto! 💕"`

**Screen 5 — Welcome Home**
- Final adoption confirmation: bunny in chosen room, pet name fades in
- Text: `"Olá, [name]! 🐰"` for 3 seconds
- Auto-transitions to main Tamagotchi screen
- Writes `tamagotchi_state.json` with adoption complete

### 2.2 Main Tamagotchi Screen

Layout (top to bottom):

```
┌─────────────────────────────┐
│ ← Caca         ⚙           │  ← header: back + name + settings
├─────────────────────────────┤
│ 🍓 ████████░░  80%          │  ← hunger bar
│ ❤️  ███████░░░  70%          │  ← happiness bar
│ ⚡ █████░░░░░  50%          │  ← energy bar
│ 🧼 ████████░░  80%          │  ← cleanliness bar
├─────────────────────────────┤
│                             │
│      [background]           │
│           🐰 (animated)     │
│                             │
├─────────────────────────────┤
│  🥕    🎀    💤    🪮      │  ← action buttons
└─────────────────────────────┘
```

**Action buttons:**

| Icon | Action | Animation | Effect |
|------|--------|-----------|--------|
| 🥕 | Alimentar | `eat.png` (pulls carrot) | +20 hunger, -2 cleanliness |
| 🎀 | Brincar | `play.png` (jumping) | +15 happiness, -10 energy |
| 💤 | Dormir | `sleep.png` | restores energy over time |
| 🪮 | Escovar | `idle.png` + brush overlay | +15 cleanliness, +5 happiness |

### 2.3 Settings (gear icon)

- **Trocar pet** → re-opens picker (Screen 2), but stats stay the same when changed
- **Renomear** → re-opens naming (Screen 3)
- **Trocar quarto** → re-opens background picker (Screen 4)
- **Aniversário da adoção** — view-only: shows `adopted_at` formatted + days since
- **Resetar tudo** (with confirmation dialog) → wipes state, returns to Adoption Wizard

---

## 3. State Schema

### 3.1 `tamagotchi_state.json` (on SD card)

```json
{
  "version": 1,
  "first_boot": false,
  "pet_slug": "white",
  "pet_name": "Caca",
  "background": "classic/02.png",
  "adopted_at": "2026-06-15T14:23:00",
  "stats": {
    "hunger": 80,
    "happiness": 80,
    "energy": 80,
    "cleanliness": 80
  },
  "last_update_unix": 1734567890,
  "achievements": [],
  "total_care_days": 0,
  "consecutive_care_days": 0
}
```

### 3.2 Stats decay model

- All stats clamp `[0, 100]`
- Decay applied **on boot** (catches up missed time) and **every 15 min** while running
- Rates:
  - `hunger`: -1 / hour
  - `happiness`: -1 / hour
  - `energy`: -2 / hour while awake (i.e. `play.png` action was recent), -0 while sleeping (sleep restores +5/h up to 100)
  - `cleanliness`: -1 / 2 hours

- If any stat reaches `0`, pet plays `sad.png` and shows "needs attention" indicator
- Stat changes save to NVS immediately (avoid SD write thrashing)

### 3.3 NVS keys (mirror critical state for fast boot)

```
tama_pet_slug    (string, max 16)
tama_pet_name    (string, max 16)
tama_bg          (string, max 24)
tama_hunger      (uint8)
tama_happiness   (uint8)
tama_energy      (uint8)
tama_cleanliness (uint8)
tama_last_unix   (uint32)
```

NVS is the source of truth for stats; JSON on SD is for snapshot/recovery + the things that don't change often (slug, name, bg, adoption date).

---

## 4. Asset Loading

### 4.1 Path resolution

```
Pet sprite:      /pets/{pet_slug}/{anim}.png
Background:      /backgrounds/{theme}/{NN}.png
Manifest read:   /pets_manifest.json (on boot, cache in heap)
```

### 4.2 Sprite sheets

All animations are **single-row sprite sheets** of 12 frames × 32x32 each (384x32 total). LVGL displays one frame at a time via cropped image source.

Frame timing recommendation:
- `idle`: 200ms/frame (5 FPS, slow ambient)
- `eat`: 100ms/frame (faster, more excited)
- `play`: 80ms/frame (energetic)
- `sleep`: 250ms/frame (very slow)

### 4.3 Memory

- 10 pets × 11 animations × ~1KB PNG ≈ 110KB total on SD (cheap)
- In RAM: only current pet + only current animation loaded → ~1KB working set
- Lazy load: when user opens picker, load `idle.png` for all 10 pets concurrently (~10KB total) and animate them in parallel — fits comfortably

---

## 5. Pet Manifest (`/pets_manifest.json`)

Defines available pets, default selection, display names, and metadata for the picker UI:

```json
{
  "version": 1,
  "default": "white",
  "pets": [
    {
      "slug": "white",
      "display_name": "Branquinha",
      "color_hex": "#FFFFFF",
      "default": true
    },
    {
      "slug": "fantasy",
      "display_name": "Fantasia",
      "color_hex": "#A78BCC"
    },
    ...
    {
      "slug": "demonic",
      "display_name": "Diabinha",
      "color_hex": "#8B0000",
      "secret": true,
      "unlock_hint": "Cuide do pet por 7 dias seguidos"
    }
  ],
  "animations": ["idle.png","sleep.png","happy.png","rest.png","play.png",
                 "run.png","eat.png","scratch.png","sad.png","sad_run.png","ko.png"],
  "animation_frame_size": 32,
  "animation_frame_count": 12
}
```

---

## 6. Backgrounds Manifest (`/backgrounds_manifest.json`)

```json
{
  "version": 1,
  "default": "classic/02.png",
  "themes": {
    "classic": {
      "name": "Clássico",
      "count": 20
    },
    "xmas": {
      "name": "Natal",
      "count": 5,
      "seasonal": true,
      "available_months": [11, 12, 1]
    }
  }
}
```

`seasonal` + `available_months` mean the xmas theme is **only shown in the picker during Nov/Dec/Jan**. Outside that window, the theme is hidden (but if she already picked it, it stays applied — we don't remove user choice).

---

## 7. Achievements (foundation, V1 ships with 3)

| ID | Trigger | Reward |
|----|---------|--------|
| `first_meal` | First time feeding | Toast: "Primeira refeição! 🥕" |
| `week_streak` | 7 consecutive care days | Unlocks `demonic` pet variant |
| `name_master` | Renamed pet 3+ times | Toast: "Indecisa né? 😄" |

Achievements stored as array of slug strings in `tamagotchi_state.json`. Easy to extend later.

---

## 8. Edge Cases

| Case | Behavior |
|------|----------|
| User boots device 3 days after last use | Apply decay capped at `max_decay_hours = 24` so pet doesn't auto-die. Show "Senti sua falta!" toast |
| SD card not present | Adoption wizard still works; state lives in NVS only; degrade gracefully (no background, solid color fallback) |
| Pet manifest JSON corrupted | Fall back to hardcoded list with `white` as default |
| User picks `xmas/03.png` in July (locale-permitting) | Allow if already saved in state. Hide from picker unless seasonal months |
| Reset → wizard, but user backs out mid-wizard | Keep `first_boot: true` until completion; show wizard again next boot |

---

## 9. Implementation Order (within Phase 8 of PLAN.md)

1. **Manifests load + cache** → confirm reads on boot
2. **Main screen rendering** with hardcoded `white` + `classic/02.png`, no interactions yet
3. **Stat bars + decay loop**
4. **Action buttons** (feed/play/sleep/brush) with state mutations
5. **Settings menu skeleton**
6. **Pet picker** (full flow)
7. **Name picker** (with keyboard)
8. **Background picker** (with preview)
9. **Adoption wizard** (chains 6→7→8 on first boot)
10. **Achievements** + secret pet unlock
11. **Seasonal theme gating**

Each step independently testable.

---

## 10. SD card layout (after `organize_assets.py`)

```
sd_card/
├── pets/
│   ├── white/
│   │   ├── idle.png       (384x32, 12 frames)
│   │   ├── sleep.png
│   │   ├── happy.png      (was "Liking")
│   │   ├── rest.png       (was "LieDown")
│   │   ├── play.png       (was "Jumping")
│   │   ├── run.png
│   │   ├── eat.png        (was "Attack" - pulls carrot!)
│   │   ├── scratch.png
│   │   ├── sad.png        (was "HurtIdle")
│   │   ├── sad_run.png
│   │   └── ko.png         (was "Death" - just sad expression)
│   ├── fantasy/ (same files)
│   ├── lightbrown/
│   ├── grey/
│   ├── brown2color/
│   ├── brown/
│   ├── brownwhite/
│   ├── blackwhite/
│   ├── black/
│   └── demonic/           (unlock-only)
├── backgrounds/
│   ├── classic/01.png ... 20.png
│   └── xmas/01.png ... 05.png
├── items/
│   ├── food/      (carrots, flowers, grass_wood)
│   ├── beds/      (beds.png)
│   ├── toys/      (toys, balls)
│   ├── bowls/     (food/water, full/empty)
│   ├── care/      (brush, vitamins, vitamin_paste)
│   └── home/      (cages, carry_box)
├── pets_manifest.json
├── backgrounds_manifest.json
└── tamagotchi_state.example.json
```

Total: **153 files, 186 KB**. Fits in <0.1% of a 32GB SD.

---

## 11. Communication style on the screen

- All user-facing text in **PT-BR**
- Casual, warm tone — never "Erro: pet inválido". Prefer "Ops, não achei essa Caca 🥺"
- Encouragement when stats are good: "Caca tá feliz! 💕"
- Gentle nudges when stats are low: "Caca tá com fominha..."

💕
