# CacaOS — PLAN.md

> Mini-OS rodando na ESP32-2432S028R (Cheap Yellow Display) feito como presente.
> Tema: **kawaii pixel art**, paleta rosa pastel.
> Stack: **PlatformIO + Arduino + LVGL 9.x + TFT_eSPI + XPT2046_Touchscreen**.

---

## 1. Hardware

**Placa:** ESP32-2432S028R (Sunton)

- ESP32-WROOM-32, 240MHz dual-core, 520KB SRAM, 4MB flash (sem PSRAM nessa revisão)
- Display 2.8" TFT 240x320, driver **ILI9341**, RGB565
- Touch resistivo **XPT2046**
- microSD via SPI dedicado
- RGB LED on-board (ativo em LOW)
- LDR (sensor de luz ambiente)
- Speaker passivo no GPIO 26 (via I2S DAC, precisa de tom gerado por software)
- USB-Serial: CH340 (driver `CH34xVCPDriver` na App Store do Mac)

### GPIO pinout (CRÍTICO — não chuta, copia daqui)

**Display (TFT_eSPI HSPI):**
```
MISO=12  MOSI=13  SCLK=14  CS=15  DC=2  RST=-1  BL=21
```

**Touch (XPT2046, VSPI):**
```
MISO=39  MOSI=32  CLK=25  CS=33  IRQ=36
```

**SD Card (SPI compartilhado):**
```
MISO=19  MOSI=23  SCLK=18  CS=5
```

**Periféricos:**
```
RGB LED:     R=GPIO4  G=GPIO16  B=GPIO17  (active LOW)
LDR:         GPIO34
Speaker:     GPIO26
CN1 livre:   GPIO22, GPIO27 (I2C disponível)
```

---

## 2. Tech Stack

### platformio.ini

```ini
[env:cyd]
platform = espressif32@^6.7.0
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed = 921600

board_build.partitions = huge_app.csv

lib_deps =
    lvgl/lvgl@^9.2.0
    bodmer/TFT_eSPI@^2.5.43
    paulstoffregen/XPT2046_Touchscreen@^1.4
    bblanchon/ArduinoJson@^7.1.0

build_flags =
    -D USER_SETUP_LOADED=1
    -D ILI9341_2_DRIVER=1
    -D TFT_WIDTH=240
    -D TFT_HEIGHT=320
    -D TFT_MISO=12
    -D TFT_MOSI=13
    -D TFT_SCLK=14
    -D TFT_CS=15
    -D TFT_DC=2
    -D TFT_RST=-1
    -D TFT_BL=21
    -D TFT_BACKLIGHT_ON=HIGH
    -D LOAD_GLCD=1
    -D LOAD_FONT2=1
    -D LOAD_FONT4=1
    -D SPI_FREQUENCY=55000000
    -D SPI_READ_FREQUENCY=20000000
    -D SPI_TOUCH_FREQUENCY=2500000
    -D LV_CONF_INCLUDE_SIMPLE
    -I include
    -D CORE_DEBUG_LEVEL=0
```

### include/lv_conf.h — settings críticos

```c
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1     // CRÍTICO! TFT_eSPI espera bytes swapped
#define LV_MEM_SIZE (48U * 1024U)
#define LV_USE_FS_STDIO 1
#define LV_FS_STDIO_LETTER 'S' // SD monta em 'S:'
#define LV_USE_TJPGD 1         // JPEG decoder pra galeria
#define LV_USE_LODEPNG 1       // PNG decoder pra sprites
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_USE_DEMO_WIDGETS 0  // off pra economizar flash
```

> ⚠️ Se cores aparecerem invertidas (vermelho/azul trocados), o problema é `LV_COLOR_16_SWAP`. Esse é o erro mais comum no CYD com LVGL.

---

## 3. Estrutura do projeto

```
cacaos/
├── platformio.ini
├── README.md
├── include/
│   └── lv_conf.h
├── src/
│   ├── main.cpp                  # setup + loop + LVGL tick handler
│   ├── config.h                  # WiFi, API key, data início (gitignored)
│   ├── config.example.h          # template
│   ├── system/
│   │   ├── display.cpp/h         # init TFT_eSPI + flush callback LVGL
│   │   ├── touch.cpp/h           # init XPT2046 + read callback LVGL
│   │   ├── sdcard.cpp/h          # mount + helpers FS
│   │   ├── wifi_mgr.cpp/h        # connect + reconnect + NTP
│   │   ├── weather.cpp/h         # OpenWeather client
│   │   └── storage.cpp/h         # NVS wrapper pra prefs/state
│   ├── ui/
│   │   ├── theme.cpp/h           # paleta + estilos LVGL globais
│   │   ├── homescreen.cpp/h      # grid de apps + clock + clima
│   │   ├── nav.cpp/h             # router entre apps + back button
│   │   └── widgets/
│   │       ├── kawaii_button.cpp/h
│   │       ├── pixel_icon.cpp/h
│   │       └── icons/            # arrays C de ícones
│   └── apps/
│       ├── gallery/
│       ├── daily_card/
│       ├── counter/
│       ├── open_when/
│       ├── memory_game/
│       ├── pomodoro/
│       ├── mood_tracker/
│       └── tamagotchi/
├── data/                         # LittleFS (fonts, ícones essenciais)
└── sd_card/                      # espelho do que vai no SD físico
    ├── photos/
    ├── messages.json
    ├── open_when/
    ├── memory_pairs/
    ├── tamagotchi_sprites/
    ├── moods.json
    └── tamagotchi_state.json
```

---

## 4. Fases de implementação

**Cada fase é shippable.** Em qualquer ponto dá pra parar e apresentar — só fica com menos apps. Sugiro presentear no fim da Fase 4 e ir adicionando apps depois via OTA ou reflash.

### Fase 0 — Hello World (dia 1, ~2h) — *aguardando hardware*
- [ ] CH340 driver instalado no Mac
- [ ] Placa detectada em `/dev/cu.wchusbserial*`
- [ ] Sketch básico TFT_eSPI mostra `"CacaOS 💕"` em rosa centralizado
- [ ] Touch printa coords no Serial quando tocado

**Critério de aceite:** vê o texto rosa e o Serial cospe `(x=120, y=160)` ao tocar.

### Fase 1 — Foundation (dia 2-3) — ✅ pronto
- [x] LVGL inicializado com display flush + touch read callbacks
- [x] `theme.cpp` aplica paleta + estilos globais (cards rounded, sombra rosa, fonte Montserrat)
- [x] Homescreen estática: relógio + grid 3x3 de ícones
- [x] Router (`nav.cpp`): tap em ícone abre tela do app, botão back volta
- [x] Splash screen ao boot: card branco arredondado + sprite branquinha 4x + título "CacaOS"

**Critério de aceite:** dá pra navegar entre todas as 8 telas vazias dos apps usando touch — *validar com placa*.

### Fase 2 — Network & Time (dia 4) — 🟡 código pronto, aguardando placa
- [x] WiFi connect via `config.h` (SSID + senha) — código em `system/wifi_mgr.cpp`
- [x] NTP sync, timezone São Paulo — código em `system/wifi_mgr.cpp`
- [x] OpenWeather (free tier `weather` endpoint), cache 30min — código em `system/weather.cpp`
- [x] Homescreen mostra hora real + temp + ícone WiFi
- [x] Reconnect automático se cair WiFi

**Critério de aceite:** desliga e religa, hora certa em <10s, clima aparece — *validar com placa + WiFi real*.

### Fase 3 — Galeria + Cartinha (dia 5-7) — ✅ pronto
- [x] SD montado em `'S:'` via LVGL FS_STDIO (path `/sd`)
- [x] **Gallery:** lista `/photos/*.jpg`, navegação prev/next, render via `lv_image_set_src`
- [x] **Daily card:** lê `messages.json` do SD, índice = day-of-year, botão "outra" mostra próxima
- [ ] Swipe horizontal na gallery (tem prev/next por enquanto)
- [ ] Autoplay 5s/foto
- [ ] Gradiente rosa no daily_card (uso cartão branco com texto centrado)

**Critério de aceite:** 15 fotos preparadas em `sd_card/photos/` (01..15.jpg), 30 mensagens — *validar render com placa*.

### Fase 4 — Contador + Open When (dia 8-9) — ✅ pronto
- [x] **Counter:** data início em `config.h`, calcula dias/horas/minutos em tempo real (refresh 30s)
- [x] **Open When:** lista `open_when/*.txt` do SD, cada arquivo vira um envelope clicável
- [ ] Confete pixel a cada 100 dias (visual polish)
- [ ] Animação de abrir envelope (3 frames)

**Critério de aceite:** contador atualiza vivo, envelopes abrem — *validar com placa*.

**👉 PONTO DE ENTREGA SUGERIDO 👈**
Aqui o presente já é absurdo. As próximas fases dá pra empurrar depois.

### Fase 5 — Memory Game (dia 10-12) — ✅ pronto (com placeholder)
- [ ] Asset pipeline: 8 fotos 60x60 PNG em `sd_card/memory_pairs/` (atualmente usa 8 **cores**)
- [x] Grid 4x4 com pares
- [x] Contador de tentativas + cronômetro
- [x] Recorde salvo no NVS (`memory` namespace, key `best_s`)
- [x] Mensagem de "novo recorde!" no fim
- [ ] Flip animation (rotateY 180°) — atualmente é reveal instantâneo
- [ ] Tela de vitória com confete

### Fase 6 — Pomodoro (dia 13-14) — ✅ pronto
- [x] Timer 25/5 (cycles automáticos focus → break → focus...)
- [x] Estado "focada" / "descanso" indicado no label
- [x] Beep no GPIO 26 (`tone()`): 880Hz no fim do focus, 660Hz no fim do break
- [x] Contador de pomodoros completos por sessão
- [ ] Bichinho pixel central com idle animation
- [ ] Anel de progresso visual
- [ ] Timers customizáveis (15/3, 25/5, 50/10) — atualmente fixo

### Fase 7 — Mood Tracker (dia 15-16) — ✅ pronto
- [x] Tela diária: 5 botões de humor (cores diferentes)
- [x] Tap salva no NVS (`mood` namespace, key = "YYYYMMDD")
- [x] Permite editar o humor do dia (basta tocar de novo)
- [x] Histórico: heatmap horizontal dos últimos 7 dias
- [ ] Histórico mensal em calendário 7x5 (atualmente só 7 dias)
- [ ] Sprite-art emojis em vez de texto `:D / :)` etc

### Fase 8 — Tamagotchi (dia 17-20) — 🟡 core + wizard mínimo prontos
- [x] Stats: hunger/happiness/energy/cleanliness (0-100 cada) no NVS namespace `tama`
- [x] Decay model: -1/h hunger, -1/h happiness, -2/h energy, -1/(2h) cleanliness
- [x] Catch-up decay no boot (cap em 24h pra não auto-matar)
- [x] 4 ações (feed/play/sleep/brush) com efeitos conforme TAMAGOTCHI_SPEC.md
- [x] Sprite atlas: PNG 384x32 (12 frames) animado a 200ms/frame
- [x] Anim escolhido pelo estado: idle / sad / sleep / happy
- [x] Background `classic/02.png` atrás do pet
- [x] Adoption wizard mínimo (welcome + pet picker 3x3 com 9 variantes)
- [x] Pet picker — escolha persistida no NVS (`slug`), reflete no sprite
- [ ] Nome customizado (atualmente usa `DEFAULT_PET_NAME` do config.h — falta keyboard on-screen)
- [ ] Background picker com 20 quartos
- [ ] Settings menu (gear icon → renomear, trocar pet/bg, reset)
- [ ] Achievements (desbloqueia `demonic` por exemplo)
- [ ] Pet sad textbox quando algum stat zera

---

## 5. Especificação dos apps

### 5.1 Homescreen
- **Topo:** hora grande (fonte pixel 32px) + data pequena ao lado
- **Linha de status:** ícone clima + temp + ícone WiFi
- **Grid 3x3** de ícones com label embaixo: Galeria, Cartinha, Contador, Open When, Memory, Pomodoro, Mood, Tama, (slot vago)
- **Animação:** bounce suave no ícone tocado (scale 0.95 → 1.1 → 1.0 em 200ms)

### 5.2 Galeria
- **Estado:** `current_index`, `autoplay_on`, `photo_list`
- **Gestos:** swipe left/right pra navegar, tap longo pra autoplay
- **Footer:** "12 / 47" + ícone play/pause
- **Otimização:** preload next photo em task separada (core 0)
- **Asset:** `/photos/*.jpg` no SD, redimensionados 240x320

### 5.3 Cartinha do dia
- **Lógica:** `index = day_of_year % messages.length`
- **JSON:** `{"messages": ["frase 1", "frase 2", ...]}`
- **Visual:** fundo gradiente rosa→branco, texto centralizado em fonte pixel 18, coraçãozinho pulsando embaixo
- **Botão "outra":** mostra próxima da lista (não persiste, só pra rolê)

### 5.4 Contador
- **Config:** `START_DATE` em `config.h` (formato ISO)
- **Display:** 3 cards grandes — DIAS / HORAS / MINUTOS
- **Animação:** dígitos rolam tipo odômetro quando incrementam
- **Easter egg:** a cada 100 dias, fogos pixel art

### 5.5 Open When
- **Estrutura:** cada `.txt` tem `Título\n---\nConteúdo...`
- **Lista:** envelopes rosa em scroll vertical
- **Animação:** envelope abre (3 frames) → texto fade-in
- **Sugestão de cenários:** triste, saudade, entediada, ansiosa, querendo dormir, prova chegando, dia bom

### 5.6 Memory Game
- **Setup:** 8 pares = 16 cards, grid 4x4
- **State machine:** `idle → first_flip → second_flip → check (match? || hide)`
- **Visual:** verso dos cards com padrão pixel rosa, frente com as fotos
- **NVS keys:** `memory_best_time`, `memory_games_played`

### 5.7 Pomodoro
- **Estados:** `focus`, `break`, `paused`, `done`
- **Visual:** anel de progresso ao redor do bichinho, timer mm:ss embaixo
- **Botões:** play/pause, skip, reset
- **Som:** `tone()` no GPIO 26 — sine 880Hz no fim do focus, 660Hz no fim do break

### 5.8 Mood Tracker
- **Tela 1 (hoje):** "Como tá se sentindo hoje?" + 5 emojis grandes
- **Tela 2 (histórico):** calendário 7x5 com células coloridas por mood
- **Cores:** verde (😊), verde-claro (😌), cinza (😐), azul (😢), vermelho (😡)
- **Stats no rodapé:** "média da semana: 😌"

### 5.9 Tamagotchi
- **Estado:** `{hunger, happiness, energy, last_update, name}` em `tamagotchi_state.json` + NVS backup
- **Tick:** decay aplicado no boot baseado em `last_update`, depois a cada hora
- **Ações:**
  - **Alimentar:** +20 hunger, anima 2s (mastigando)
  - **Brincar:** +15 happiness, -10 energy, mini-game de tap rápido
  - **Dormir:** restaura energy até 100 em 8h (tela escurece, pet ronca)
- **Visual:** sprite central 80x80 + barras de status no topo

---

## 6. Asset Pipeline

### Fotos (galeria)
```bash
# no Mac, terminal:
cd ~/fotos_originais
mkdir resized
sips -Z 320 -c 320 240 *.jpg --out resized  # crop + resize
# copia resized/* pro SD em /photos/
```

### Sprites pixel art
1. **Cria** em Aseprite (pago) ou **Piskel** (grátis, web)
2. **Exporta** PNG (transparência ok)
3. **Pra ícones do homescreen** (uso frequente): converte com `lv_img_conv` → array C em `src/ui/widgets/icons/`
4. **Pra sprites grandes** (tamagotchi, memory): deixa como PNG no SD

### Mensagens
```json
{
  "messages": [
    "Você é meu lugar favorito 💕",
    "Saudade tem nome e é seu",
    "Hoje tô grato por ter você",
    "..."
  ]
}
```
Mínimo 30 mensagens pra não repetir mês.

### Open When
Cada cenário é um `.txt` em `/open_when/`:
```
Quando estiver triste
---
[conteúdo da cartinha...]
```

---

## 7. theme.h — paleta + estilos

```c
#pragma once
#include "lvgl.h"

// Paleta kawaii rosa pastel
#define COLOR_BG          lv_color_hex(0xFFE5EC)
#define COLOR_CARD        lv_color_hex(0xFFFFFF)
#define COLOR_PRIMARY     lv_color_hex(0xFF8FAB)
#define COLOR_ACCENT      lv_color_hex(0xFB6F92)
#define COLOR_TEXT        lv_color_hex(0xC9184A)
#define COLOR_TEXT_LIGHT  lv_color_hex(0xFFC2D1)
#define COLOR_SUCCESS     lv_color_hex(0xB5E48C)
#define COLOR_WARN        lv_color_hex(0xFFC09F)

// Métricas
#define RADIUS_CARD       12
#define RADIUS_BUTTON     20
#define SHADOW_OFFSET     4
#define SHADOW_BLUR       8
#define PADDING_CARD      12

void theme_init(void);
extern lv_style_t style_card;
extern lv_style_t style_button_primary;
extern lv_style_t style_text_title;
```

**Fontes recomendadas:** Press Start 2P (chunky retro) ou Pixelify Sans (mais legível). Converter via `lv_font_conv` em 14px e 18px.

---

## 8. Build & Deploy

```bash
# clone
git clone <repo>
cd cacaos

# setup config
cp src/config.example.h src/config.h
# edita src/config.h:
#   - WIFI_SSID, WIFI_PASS
#   - OPENWEATHER_API_KEY (https://openweathermap.org/api → free tier)
#   - START_DATE "2024-XX-XX" (data início namoro)
#   - HER_NAME

# build + flash
pio run -t upload

# monitor pra debug
pio device monitor

# upload LittleFS (fonts/ícones embarcados)
pio run -t uploadfs
```

---

## 9. Setup do cartão SD

1. Formatar **FAT32** (Disk Utility no Mac: Erase → MS-DOS FAT)
2. Criar estrutura:
   ```
   /photos/
   /open_when/
   /memory_pairs/
   /tamagotchi_sprites/
   messages.json
   moods.json          → {"moods":[]}
   tamagotchi_state.json → {"hunger":80,"happiness":80,"energy":80,"last_update":0,"name":"Caca"}
   ```
3. Copiar conteúdo de `sd_card/` do repo

---

## 10. Riscos & gotchas conhecidos

| Sintoma | Causa provável | Fix |
|---|---|---|
| Cores invertidas (azul/vermelho trocados) | `LV_COLOR_16_SWAP` errado | Inverte no lv_conf.h |
| Touch invertido / impreciso | Calibração XY | Roda calibration na Fase 0, salva offsets no NVS |
| Galeria trava entre fotos | JPG decode síncrono | Preload em task no core 0 |
| Reboot ao decodificar PNG grande | Falta de heap | Diminui tamanho do sprite ou usa JPG |
| NTP não sincroniza | DNS / firewall | Usa pool.ntp.org com fallback br.pool.ntp.org |
| Build falha em `lv_conf.h not found` | Include path | Confirma `-I include` no platformio.ini + `LV_CONF_INCLUDE_SIMPLE` |
| 4MB partition apertada com LVGL | Partições default | Usa `huge_app.csv` (já tá no platformio.ini) |

---

## 11. v2 (depois do MVP)

- **Mahjong solitaire mini** (40-60 peças)
- **Config app** pra trocar WiFi sem reflashar
- **OTA updates** via WiFi
- **Notificações push** via webhook (ex: você manda mensagem de outro lugar, aparece na tela dela)
- **Integração FinanceHub** — mostra gasto compartilhado do mês
- **Modo soneca** — usa LDR pra escurecer tela quando luz baixa
- **Múltiplos perfis** (caso queira fazer um pra outras pessoas)
- **Mood-aware** — Luna (do FinanceHub) sugere mensagem baseado no mood do dia

---

## 12. Convenções de código

- **Idioma:** código + comentários em inglês, conteúdo (mensagens, open when) em PT-BR
- **Naming:** snake_case pra C, camelCase pra C++ classes
- **Arquivos por app:** `app_name.cpp` + `app_name.h`, função de entrada `void app_name_show(void)`
- **Commits:** conventional commits (`feat:`, `fix:`, `chore:`)
- **Branch:** `main` sempre buildando, features em `feat/nome-app`

---

## 13. Acceptance final

CacaOS v1 está pronto quando:

- [ ] Boot em <5s do power-on até homescreen funcional
- [ ] Todos 8 apps acessíveis e funcionais
- [ ] WiFi reconecta sozinho se cair
- [ ] Hora certa após desligar/religar
- [ ] 30+ mensagens, 20+ fotos, 7+ open whens preenchidos
- [ ] Memory game tem 8 fotos personalizadas
- [ ] Tamagotchi sobrevive 24h sem crash
- [ ] Sem reboot espontâneo em 48h de uso
- [ ] Case de acrílico montada e apresentável

💕
