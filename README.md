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

- [ ] Fase 0 — Hello World (display + touch verificados) — *aguardando hardware*
- [x] Fase 1 — Foundation (LVGL + theme + homescreen + nav + splash)
- [~] Fase 2 — Network & Time (WiFi/NTP/OpenWeather codificado, falta testar com placa)
- [x] Fase 3 — Galeria + Cartinha (gallery renderiza JPEG do SD via LVGL FS)
- [x] Fase 4 — Contador + Open When
- [x] Fase 5 — Memory Game (com placeholder de cores enquanto não há fotos personalizadas em `memory_pairs/`)
- [x] Fase 6 — Pomodoro (com beep no GPIO 26)
- [x] Fase 7 — Mood Tracker (5 humores + heatmap 7d)
- [x] Fase 8 — Tamagotchi (core + wizard 4 telas: pet/naming/bg, sad textbox quando stat zera, reset menu)
- [x] Bonus — Settings app (brilho + sobre + reset pet + recalibrar touch)
- [x] Bonus — Counter confete a cada 100 dias

Legenda: `[x]` pronto · `[~]` parcial · `[ ]` não começado.

Build atual: **RAM 37.2%** (122 KB/320 KB) · **Flash 45.2%** (1.42 MB/3.0 MB).

## O que está pronto vs. depende de hardware

**Funcionando no build (verificado por CI + build local):**
- Compilação limpa, sem warnings críticos
- Estrutura de UI completa: splash → homescreen → 8 apps com navegação back
- Persistência (NVS) para humor, recorde do memory, estado do tamagotchi
- Leitura SD para mensagens, open whens, fotos, sprites
- LVGL FS driver registrado em `S:` apontando pra `/sd`
- Decoders JPEG + PNG habilitados

**Depende de placa pra validar:**
- Cores corretas (pode precisar inverter `LV_COLOR_16_SWAP`)
- Touch calibration roda automática no 1º boot (4 toques nos cantos rosa)
- WiFi conectar com SSID real
- Beep do pomodoro audível no GPIO 26
- RGB LED, LDR (não usados ainda)

## Próximos passos sugeridos (antes da placa)

1. Editar `src/config.h` com seus valores reais (já criado a partir do example)
2. Personalizar `sd_card/messages.json` (já tem 30 mensagens-exemplo)
3. Preencher os `.txt` de `sd_card/open_when/` (atualmente placeholders)
4. Instalar driver CH340 (App Store → "CH34xVCPDriver") e a extensão PlatformIO IDE no VSCode

## Próximos passos sugeridos (depois da placa)

1. Verificar Fase 0 (Hello World + touch funcionando)
2. Confirmar que a touch calibration ficou precisa (cantos rosa no 1º boot)
3. Verificar render real das fotos JPEG + sprites PNG via LVGL FS
4. Settings menu (brightness, reset, sobre)
5. Background picker pro tamagotchi
6. On-screen keyboard pro tamagotchi rename
7. Animações finas: confete no counter a cada 100 dias, fade-in no daily_card, flip nos cards do memory_game

## License

Personal use only. Sprite assets from [ToffeeCraft's Pet Mobile Pixel Asset Pack](https://toffeecraft.itch.io/pet-virtual-mobile-pixel-asset) (purchased license).

Made with 💕
