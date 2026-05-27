# SETUP.md — Mac + VSCode para CacaOS

Setup zero-to-flash no Mac usando VSCode + PlatformIO. Tempo estimado: **20-30 minutos** (boa parte é download).

> Você prefere Zed/IntelliJ? Veja o apêndice no fim do arquivo.

---

## 1. Pré-requisitos do sistema

### Xcode Command Line Tools (obrigatório)

Fornece compiladores (`clang`), `git`, `make`, etc.

```bash
xcode-select --install
# se já estiver instalado: "xcode-select: error: command line tools are already installed"
```

### Homebrew (se ainda não tiver)

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

---

## 2. VSCode + extensão PlatformIO IDE

### Instala VSCode

```bash
brew install --cask visual-studio-code
```

### Instala a extensão PlatformIO IDE

Abre o VSCode e:

1. `Cmd+Shift+X` → painel de extensões
2. Busca **"PlatformIO IDE"** (publisher: **PlatformIO**)
3. Click em **Install**
4. Aguarda a instalação inicial (baixa o PIO Core, ~5min na primeira vez)
5. **Reinicia o VSCode** quando pedir

Depois de reiniciar, aparece um ícone de formiga 🐜 na sidebar esquerda — é o PIO Home.

> A extensão instala o PlatformIO Core automaticamente em `~/.platformio/`. Mas é útil ter o `pio` no PATH global pra rodar via terminal também (próximo passo).

### Expõe o `pio` no terminal (opcional mas recomendado)

Adiciona no seu `~/.zshrc`:

```bash
export PATH="$PATH:$HOME/.platformio/penv/bin"
```

Recarrega:

```bash
source ~/.zshrc
pio --version
# esperado: PlatformIO Core, version 6.x.x
```

Agora dá pra usar `pio` direto no terminal e ainda ter o GUI da extensão.

---

## 3. Driver CH340 (USB-Serial da CYD)

Sem isso o Mac não enxerga a placa. macOS não vem com o driver.

**Opção A (recomendada) — App Store:**
1. Abre a App Store
2. Busca `CH34xVCPDriver` (autor: WCH)
3. Instala (gratuito)
4. **Reinicia o Mac**

**Opção B — manual:**
Download em https://www.wch-ic.com/downloads/CH34XSER_MAC_ZIP.html, segue o instalador.

Verifica depois de plugar a placa (vai testar quando ela chegar):

```bash
ls /dev/cu.wchusbserial*
# esperado: /dev/cu.wchusbserial14110 (ou similar)
```

---

## 4. Node.js (pra ferramentas de assets LVGL)

Pra converter fontes TTF e imagens pra formato LVGL.

```bash
brew install node

# instala as ferramentas LVGL globalmente
npm install -g lv_font_conv lv_img_conv
```

Verifica:

```bash
lv_font_conv --version
lv_img_conv --help | head -5
```

> Alternativa sem CLI: conversor web em https://lvgl.io/tools (ok pra one-off, ruim pra automatizar).

---

## 5. Ferramenta de pixel art

Pra desenhar sprites, ícones, e o bichinho do Tamagotchi:

- **Aseprite** — padrão da indústria, $20 na App Store. Tem timeline de animação, paleta indexada, exporta spritesheet. **Recomendado.**
- **Piskel** — grátis, roda no browser (https://www.piskelapp.com). Bom o suficiente pra começar.
- **Pixelorama** — grátis, open source, desktop. Meio-termo.

Pra esse projeto Piskel já resolve.

---

## 6. EEZ Studio (opcional)

GUI visual pra montar telas LVGL. Você arrasta widgets, ele gera código C compatível. Bom pra prototipar rápido antes de implementar na mão.

Download: https://www.envox.eu/studio/studio-introduction/ (tem build Mac arm64).

Se você curte escrever LVGL direto no código, pula.

---

## 7. Setup do projeto no VSCode

```bash
# clone do projeto
git clone <seu-repo-cacaos> && cd cacaos

# template de config
cp src/config.example.h src/config.h
# edita src/config.h:
#   - WIFI_SSID, WIFI_PASS
#   - OPENWEATHER_API_KEY (gera em openweathermap.org → free tier)
#   - START_DATE "2024-XX-XX"
#   - HER_NAME

# abre no VSCode
code .
```

No VSCode:

1. A extensão PIO detecta o `platformio.ini` automaticamente
2. Primeira abertura: a extensão baixa o toolchain ESP32 + libs (~5min, observa o status na barra de baixo)
3. Quando aparecer "PlatformIO: Project Initialization Done" na notificação, tá pronto

### Onde ficam os controles

Barra inferior do VSCode (esquerda pra direita):

- **🏠 PIO Home** — dashboard de libs, boards, exemplos
- **✓ Build** — compila
- **→ Upload** — flasha (precisa da placa conectada)
- **🗑️ Clean** — limpa build cache
- **🔌 Serial Monitor** — abre o monitor serial
- **🔍 Find in Files** — search global
- **📤 Upload Filesystem Image** — manda LittleFS (assets embutidos)
- **⚙️ Terminal** — terminal PIO com env corretamente configurada

Resumo: 99% do tempo você só clica em ✓ pra build e → pra flash.

### Configurações úteis do projeto

**`.vscode/settings.json`** (criado automaticamente pela extensão, mas pode customizar)

```json
{
  "files.associations": {
    "*.ino": "cpp",
    "*.h": "c"
  },
  "C_Cpp.default.cppStandard": "c++17",
  "editor.formatOnSave": true,
  "editor.tabSize": 2,
  "terminal.integrated.env.osx": {
    "PATH": "${env:HOME}/.platformio/penv/bin:${env:PATH}"
  }
}
```

**`.clang-format`** na raiz (estilo do código)

```yaml
---
BasedOnStyle: LLVM
IndentWidth: 2
TabWidth: 2
UseTab: Never
ColumnLimit: 100
PointerAlignment: Left
DerivePointerAlignment: false
AllowShortFunctionsOnASingleLine: Inline
AllowShortIfStatementsOnASingleLine: false
SpaceBeforeParens: ControlStatements
IncludeBlocks: Regroup
SortIncludes: true
```

**`.gitignore` essencial**

```gitignore
.pio/
.vscode/c_cpp_properties.json
.vscode/launch.json
.vscode/ipch
build/
src/config.h
*.swp
.DS_Store
```

> O PlatformIO gera `c_cpp_properties.json` com paths absolutos da sua máquina, então não comita. Mas pode versionar o `.vscode/settings.json` e `.vscode/extensions.json` se quiser compartilhar settings entre máquinas.

---

## 8. Verificação final (sem hardware ainda)

Roda esse script pra confirmar tudo:

```bash
echo "=== Toolchain ===" && \
xcode-select -p && \
echo "" && \
echo "=== Brew ===" && \
brew --version | head -1 && \
echo "" && \
echo "=== VSCode ===" && \
code --version | head -1 && \
echo "" && \
echo "=== PlatformIO ===" && \
pio --version && \
echo "" && \
echo "=== Node + LVGL tools ===" && \
node --version && \
lv_font_conv --version && \
echo "" && \
echo "=== Git ===" && \
git --version && \
echo "" && \
echo "✅ Tudo pronto"
```

Se algum comando falhar, é onde focar.

### Teste de build (sem hardware)

Dentro do projeto:

```bash
pio run
```

Esperado: download de toolchain + libs na primeira vez, depois "SUCCESS" no fim. Se der erro de partition size, confirma que `huge_app.csv` tá no `platformio.ini`. Se der erro de lib não encontrada, roda `pio pkg install`.

---

## 8b. Simulador no Mac (sem placa)

Pra testar a UI no Mac antes da placa chegar — janela SDL 240×320 que roda o
boot completo, splash, homescreen e todas as apps. Lê do folder local
`sd_card/` como se fosse o cartão físico, então tudo que você adicionar lá
aparece também no sim.

### Pré-requisitos do sim

```bash
brew install sdl2
```

(O resto — toolchain, PlatformIO — você já tem dos passos anteriores.)

### Rodar

```bash
# build + executa (abre janela SDL)
pio run -e sim -t exec

# só build
pio run -e sim
.pio/build/sim/program     # roda separado

# Ctrl+C no terminal fecha a janela limpinho
```

### O que o sim cobre vs não cobre

✅ **Cobre:**
- Toda a UI LVGL renderizada em 240×320 nativo
- Touch via mouse (clique e arrasta)
- Persistência (tamagotchi, mood, settings) em `~/.cacaos_sim_prefs.json`
- Conteúdo do SD (fotos, mensagens, cartinhas) do folder `sd_card/`
- Daily card, gallery, open_when, memory_game, counter, pomodoro, settings,
  tamagotchi, mood_tracker

❌ **Não cobre — só na placa:**
- Cores byte-swapped (`LV_COLOR_16_SWAP`) — bugs de R↔B só aparecem lá
- Calibração de touch (raw → tela)
- Timing real de JPG decode (~500ms na placa, instantâneo no Mac)
- WiFi + OpenWeather real (sim usa weather mockado: 23°C, parcialmente nublado)
- Brilho via PWM (LDR + backlight são no-ops no sim)
- Som do piezo (pomodoro chime é silencioso)
- Boot time real, consumo de RAM real

### Trocar conteúdo no sim

O sim lê de `./sd_card/` relativo ao cwd. Override com env var:

```bash
CACAOS_SD_ROOT=/tmp/teste pio run -e sim -t exec
```

### Limpar estado persistido do sim

```bash
rm ~/.cacaos_sim_prefs.json
```

---

## 9. Setup adicional quando a placa chegar

### Driver CH340 funcionando?

```bash
# antes de plugar
ls /dev/cu.wch* 2>/dev/null
# (provavelmente vazio)

# plugar a placa via USB

# depois
ls /dev/cu.wch*
# esperado: /dev/cu.wchusbserial14110 (ou número similar)
```

Se não aparecer nada após plugar:
1. Verifica o cabo (alguns cabos micro-USB são só de carga, sem dados — testa com outro)
2. Reinstala o driver e reinicia o Mac
3. Tenta outra porta USB
4. Se ainda assim nada, a placa pode estar defeituosa (raro, mas acontece com AliExpress)

### Primeira flashada

No VSCode, com a placa conectada:

1. Click no ✓ na barra inferior — confirma que builda
2. Click no → — flasha
3. Click no 🔌 — abre serial monitor
4. Aperta o botão **RESET** na placa (ao lado do USB)
5. Deve aparecer "CacaOS 💕" na tela e logs no monitor serial

### Formatar microSD

```bash
# lista discos pra achar o SD
diskutil list

# digamos que o SD é /dev/disk4 (CONFIRMA antes ou apaga o disco errado!)
# pra cartão ≤32GB:
diskutil eraseDisk MS-DOS "CACAOS" MBR /dev/disk4

# pra cartão >32GB (precisa forçar FAT32):
sudo diskutil eraseDisk FAT32 CACAOS MBRFormat /dev/disk4
```

> ⚠️ `eraseDisk` apaga **tudo** no disco selecionado. Conferir 3 vezes qual é o `/dev/diskN` correto antes de rodar.

---

## 10. TL;DR

```bash
# 1. system
xcode-select --install

# 2. ferramentas
brew install --cask visual-studio-code
brew install node
npm install -g lv_font_conv lv_img_conv

# 3. dentro do VSCode
# → Cmd+Shift+X → busca "PlatformIO IDE" → Install → reinicia

# 4. driver
# → App Store, busca "CH34xVCPDriver", instala, reinicia o Mac

# 5. pixel art
# → Piskel (web) ou Aseprite (App Store $20)

# 6. abre projeto
code cacaos
# → barra inferior: ✓ Build, → Upload, 🔌 Monitor
```

Pronto. Quando a placa chegar, só clicar → na barra inferior e tá rolando.

---

## Apêndice: usar Zed ou outra IDE

Se você quiser usar **Zed** (ou outro editor sem extensão de PlatformIO) ao invés de VSCode:

**Trade-off:** sem GUI pra build/flash, sem serial monitor integrado, sem dashboard PIO. Tudo via terminal/tasks. Funciona, mas adiciona fricção.

**Setup mínimo pro Zed:**

```bash
# instala PIO global
brew install platformio

# no projeto, gera intellisense pro clangd do Zed
pio run -t compiledb
```

Cria `.zed/tasks.json` no projeto:

```json
[
  { "label": "PIO: Build", "command": "pio run", "cwd": "$ZED_WORKTREE_ROOT", "reveal": "always", "shell": "system" },
  { "label": "PIO: Upload", "command": "pio run -t upload", "cwd": "$ZED_WORKTREE_ROOT", "reveal": "always", "shell": "system" },
  { "label": "PIO: Upload + Monitor", "command": "pio run -t upload -t monitor", "cwd": "$ZED_WORKTREE_ROOT", "reveal": "always", "shell": "system" },
  { "label": "PIO: Monitor", "command": "pio device monitor -b 115200", "cwd": "$ZED_WORKTREE_ROOT", "reveal": "always", "shell": "system" },
  { "label": "PIO: Upload LittleFS", "command": "pio run -t uploadfs", "cwd": "$ZED_WORKTREE_ROOT", "reveal": "always", "shell": "system" },
  { "label": "PIO: Regenerate compile_commands.json", "command": "pio run -t compiledb", "cwd": "$ZED_WORKTREE_ROOT", "reveal": "always", "shell": "system" }
]
```

Cria `.zed/settings.json`:

```json
{
  "lsp": { "clangd": { "binary": { "path_lookup": true } } },
  "languages": {
    "C++": { "format_on_save": "on", "tab_size": 2 },
    "C": { "format_on_save": "on", "tab_size": 2 }
  }
}
```

Pra disparar: `Cmd+Shift+P` → "task: spawn" → escolhe.

**Lembrete:** sempre que adicionar uma lib nova no `platformio.ini`, roda `pio run -t compiledb` de novo, senão o clangd do Zed fica desatualizado.

**Sugestão prática:** usa VSCode pro CacaOS (e qualquer projeto embedded futuro) e mantém Zed pros teus outros projetos (Next.js, Python, FastAPI). Não tem regra de "uma IDE só".

💕
