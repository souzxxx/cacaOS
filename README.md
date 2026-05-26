# CacaOS 🐰💕

Mini-OS personalizado rodando em ESP32-2432S028R (Cheap Yellow Display), feito como presente. Tema kawaii pixel art em paleta rosa pastel, com 8 apps: galeria de fotos, cartinha do dia, contador de namoro, cartinhas "open when", memory game, pomodoro, mood tracker e tamagotchi customizável.

![CacaOS](docs/preview.png)

## Stack

- **Hardware:** ESP32-WROOM-32 (no PSRAM revision), display TFT 2.8" 320x240 ILI9341, touch resistivo XPT2046, microSD
- **Framework:** PlatformIO + Arduino-ESP32
- **GUI:** LVGL 9.x + TFT_eSPI
- **Persistence:** NVS + microSD (JSON + sprite assets)

## Estrutura

```
cacaos/
├── platformio.ini          # build config
├── include/lv_conf.h       # LVGL config (critical: LV_COLOR_16_SWAP = 1)
├── src/
│   ├── main.cpp            # entry: setup() + loop() + LVGL tick
│   ├── config.example.h    # template — copy to config.h with your values
│   ├── system/             # hardware abstraction (display, touch, sd, wifi, weather)
│   ├── ui/                 # theme + nav + homescreen
│   └── apps/               # 8 apps, one folder each
├── data/                   # LittleFS partition (embedded fonts/assets)
├── sd_card/                # mirror of what goes on physical SD
└── docs/
```

Specs detalhados:
- **PLAN.md** — o quê construir (8 fases, 8 apps)
- **CLAUDE.md** — como o agente trabalha (convenções, gotchas)
- **SETUP.md** — como rodar na sua máquina (Mac + VSCode)
- **TAMAGOTCHI_SPEC.md** — spec do app de pet customizável

## Quickstart

```bash
# 1. Setup do ambiente (Mac)
bash setup.sh

# 2. Configurar secrets
cp src/config.example.h src/config.h
# edita src/config.h com WiFi, OpenWeather API key, etc.

# 3. Build (sem hardware)
pio run

# 4. Flash + monitor (com placa)
pio run -t upload -t monitor
```

## Pinout (ESP32-2432S028R)

| Função  | GPIO |
|---------|------|
| TFT MISO/MOSI/SCLK/CS/DC/BL | 12 / 13 / 14 / 15 / 2 / 21 |
| Touch MISO/MOSI/CLK/CS/IRQ  | 39 / 32 / 25 / 33 / 36 |
| SD MISO/MOSI/SCLK/CS        | 19 / 23 / 18 / 5 |
| RGB LED (R/G/B, active LOW) | 4 / 16 / 17 |
| LDR                         | 34 |
| Speaker                     | 26 |

## Status das fases

- [ ] Fase 0 — Hello World (display + touch verificados)
- [ ] Fase 1 — Foundation (LVGL + theme + homescreen + nav)
- [ ] Fase 2 — Network & Time (WiFi + NTP + OpenWeather)
- [ ] Fase 3 — Galeria + Cartinha
- [ ] Fase 4 — Contador + Open When
- [ ] Fase 5 — Memory Game
- [ ] Fase 6 — Pomodoro
- [ ] Fase 7 — Mood Tracker
- [ ] Fase 8 — Tamagotchi

## License

Personal use only. Sprite assets from [ToffeeCraft's Pet Mobile Pixel Asset Pack](https://toffeecraft.itch.io/pet-virtual-mobile-pixel-asset) (purchased license).

Made with 💕
