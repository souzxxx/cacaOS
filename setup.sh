#!/usr/bin/env bash
# ============================================================
# CacaOS - Mac dev environment setup
# Run with: bash setup.sh
# ============================================================
# Automates ~80% of the SETUP.md installation.
# Manual steps remaining at end (printed clearly).
#
# Safe to re-run: each install command checks if the tool is
# already present and skips if so.
# ============================================================

set -euo pipefail

# ----- visual helpers -----
PINK='\033[38;5;212m'
BOLD='\033[1m'
GREEN='\033[32m'
YELLOW='\033[33m'
RED='\033[31m'
DIM='\033[2m'
RESET='\033[0m'

heart() { printf "${PINK}♡${RESET}"; }
step() { printf "\n${PINK}${BOLD}[$1/$TOTAL_STEPS]${RESET} ${BOLD}$2${RESET}\n"; }
info() { printf "${DIM}  → $1${RESET}\n"; }
ok()   { printf "${GREEN}  ✓ $1${RESET}\n"; }
warn() { printf "${YELLOW}  ⚠ $1${RESET}\n"; }
err()  { printf "${RED}  ✗ $1${RESET}\n"; }

TOTAL_STEPS=8

# ----- banner -----
clear
cat <<'BANNER'

   ____             ___  ____
  / ___|__ _  ___ __ \ \/ /  _ \  ___
 | |   / _` |/ __/ _` |\  /| | | |/ __|
 | |__| (_| | (_| (_| |/  \| |_| |\__ \
  \____\__,_|\___\__,_/_/\_\____/|___/

   Setup automatizado pro Mac

BANNER

printf "${DIM}Esse script vai instalar: Xcode CLI tools, Homebrew, VSCode, Node, PlatformIO Core, e ferramentas LVGL.${RESET}\n"
printf "${DIM}Tempo estimado: 15-20 minutos (depende da sua internet).${RESET}\n\n"

read -p "$(printf "${PINK}Bora? [s/N]${RESET} ")" -n 1 -r REPLY
echo
if [[ ! $REPLY =~ ^[Ss]$ ]]; then
    echo "Beleza, cancelado."
    exit 0
fi

# ============================================================
# STEP 1: Xcode Command Line Tools
# ============================================================
step 1 "Xcode Command Line Tools"

if xcode-select -p >/dev/null 2>&1; then
    ok "Já instalado em $(xcode-select -p)"
else
    info "Disparando instalação (vai abrir popup do sistema)"
    xcode-select --install
    warn "Aceita o popup e espera terminar antes de continuar."
    read -p "  Apertou e instalou? Pressiona ENTER pra continuar..." -r
fi

# ============================================================
# STEP 2: Homebrew
# ============================================================
step 2 "Homebrew"

if command -v brew >/dev/null 2>&1; then
    ok "Já instalado ($(brew --version | head -1))"
else
    info "Instalando Homebrew (vai pedir senha)"
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

    # Apple Silicon: add to PATH
    if [[ -d /opt/homebrew ]]; then
        info "Apple Silicon detectado, configurando PATH"
        echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> ~/.zshrc
        eval "$(/opt/homebrew/bin/brew shellenv)"
    fi
    ok "Homebrew instalado"
fi

# ============================================================
# STEP 3: VSCode
# ============================================================
step 3 "Visual Studio Code"

if [[ -d "/Applications/Visual Studio Code.app" ]] || command -v code >/dev/null 2>&1; then
    ok "Já instalado"
else
    info "Instalando VSCode via Homebrew"
    brew install --cask visual-studio-code
    ok "VSCode instalado"
fi

# Garante que o comando `code` tá no PATH
if ! command -v code >/dev/null 2>&1; then
    warn "Comando 'code' não tá no PATH. Vou tentar adicionar."
    CODE_BIN="/Applications/Visual Studio Code.app/Contents/Resources/app/bin"
    if [[ -d "$CODE_BIN" ]]; then
        echo "export PATH=\"\$PATH:$CODE_BIN\"" >> ~/.zshrc
        export PATH="$PATH:$CODE_BIN"
        ok "Comando 'code' adicionado ao PATH"
    fi
fi

# ============================================================
# STEP 4: Node.js
# ============================================================
step 4 "Node.js"

if command -v node >/dev/null 2>&1; then
    ok "Já instalado ($(node --version))"
else
    info "Instalando Node via Homebrew"
    brew install node
    ok "Node $(node --version) instalado"
fi

# ============================================================
# STEP 5: Ferramentas LVGL (lv_font_conv, lv_img_conv)
# ============================================================
step 5 "Ferramentas LVGL"

if command -v lv_font_conv >/dev/null 2>&1; then
    ok "lv_font_conv já instalado"
else
    info "Instalando lv_font_conv globalmente"
    npm install -g lv_font_conv
    ok "lv_font_conv instalado"
fi

if command -v lv_img_conv >/dev/null 2>&1; then
    ok "lv_img_conv já instalado"
else
    info "Instalando lv_img_conv globalmente"
    npm install -g lv_img_conv
    ok "lv_img_conv instalado"
fi

# ============================================================
# STEP 6: PlatformIO Core
# ============================================================
step 6 "PlatformIO Core"

if command -v pio >/dev/null 2>&1; then
    ok "Já instalado ($(pio --version))"
else
    info "Instalando PlatformIO via Homebrew"
    brew install platformio
    ok "PlatformIO instalado"
fi

# Garante que pio tá no PATH no zshrc
if ! grep -q 'platformio/penv/bin' ~/.zshrc 2>/dev/null; then
    info "Adicionando pio ao PATH"
    echo 'export PATH="$PATH:$HOME/.platformio/penv/bin"' >> ~/.zshrc
fi

# ============================================================
# STEP 7: Verifica driver CH340 (manual)
# ============================================================
step 7 "Driver CH340 (USB-Serial)"

if kextstat 2>/dev/null | grep -qi "wch.ch34" || \
   ls /System/Library/Extensions 2>/dev/null | grep -qi ch34 || \
   ls /Library/Extensions 2>/dev/null | grep -qi ch34; then
    ok "Driver CH340 já parece instalado"
else
    warn "Driver CH340 NÃO encontrado — instalação manual necessária"
    info "1. Abre a App Store"
    info "2. Busca 'CH34xVCPDriver' (autor: WCH)"
    info "3. Instala (grátis)"
    info "4. Reinicia o Mac"
    info ""
    info "Sem isso o Mac não enxerga a placa CYD."
fi

# ============================================================
# STEP 8: Resumo + verificação
# ============================================================
step 8 "Verificação final"

printf "\n"
printf "${BOLD}Versões instaladas:${RESET}\n"
xcode-select -p >/dev/null 2>&1 && printf "  Xcode CLI:    ${GREEN}OK${RESET} ($(xcode-select -p))\n" || printf "  Xcode CLI:    ${RED}FALTANDO${RESET}\n"
command -v brew >/dev/null && printf "  Homebrew:     ${GREEN}$(brew --version | head -1)${RESET}\n" || printf "  Homebrew:     ${RED}FALTANDO${RESET}\n"
command -v code >/dev/null && printf "  VSCode:       ${GREEN}$(code --version | head -1)${RESET}\n" || printf "  VSCode:       ${YELLOW}instalado mas comando 'code' não acessível ainda (reinicia terminal)${RESET}\n"
command -v node >/dev/null && printf "  Node:         ${GREEN}$(node --version)${RESET}\n" || printf "  Node:         ${RED}FALTANDO${RESET}\n"
command -v pio  >/dev/null && printf "  PlatformIO:   ${GREEN}$(pio --version)${RESET}\n" || printf "  PlatformIO:   ${YELLOW}instalado mas comando 'pio' não acessível ainda (reinicia terminal ou: source ~/.zshrc)${RESET}\n"
command -v lv_font_conv >/dev/null && printf "  lv_font_conv: ${GREEN}OK${RESET}\n" || printf "  lv_font_conv: ${RED}FALTANDO${RESET}\n"
command -v lv_img_conv  >/dev/null && printf "  lv_img_conv:  ${GREEN}OK${RESET}\n" || printf "  lv_img_conv:  ${RED}FALTANDO${RESET}\n"

# ============================================================
# Próximos passos manuais
# ============================================================
printf "\n${PINK}${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}\n"
printf "${PINK}${BOLD}  Próximos passos manuais ($(heart))${RESET}\n"
printf "${PINK}${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}\n\n"

cat <<'MANUAL'
  1. RECARREGA o terminal (ou roda):
     source ~/.zshrc

  2. NO VSCODE — instala a extensão PlatformIO IDE:
     - Abre o VSCode (comando: code)
     - Cmd+Shift+X → busca "PlatformIO IDE" (publisher: PlatformIO)
     - Click Install
     - Reinicia o VSCode quando pedir
     - Aguarda o setup inicial (~5 min, baixa o PIO Core)

  3. DRIVER CH340 (se ainda não tiver):
     - App Store → busca "CH34xVCPDriver" → Install
     - Reinicia o Mac

  4. PIXEL ART (escolha uma):
     - Piskel (grátis, web): https://www.piskelapp.com
     - Aseprite (App Store, $20): https://www.aseprite.org

  5. (Opcional) EEZ Studio pra prototipar telas LVGL:
     https://www.envox.eu/studio/

  6. Cria conta no OpenWeatherMap (grátis) pra pegar API key:
     https://openweathermap.org/api → "Free" tier
     (usado pelo widget de clima do CacaOS)

  7. Clona o repo do CacaOS:
     git clone <seu-repo> && cd cacaos
     cp src/config.example.h src/config.h
     # edita config.h com WiFi, API key, data início, nome

  8. Primeiro build (sem hardware):
     pio run
     # se aparecer SUCCESS, tá tudo pronto!

  Quando a placa chegar:
     pio run -t upload -t monitor

MANUAL

printf "${PINK}${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}\n"
printf "  Setup completo! $(heart) Boa sorte com o CacaOS.\n"
printf "${PINK}${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}\n\n"
