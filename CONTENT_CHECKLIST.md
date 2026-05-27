# Content checklist

Coisas que **precisam de você** antes da placa chegar (ou enquanto chega).
Código tá tudo no lugar — só falta conteúdo pessoal.

---

## 🔴 Bloqueia experiência principal

### `src/config.h`
Já existe localmente (gitignored). Confirma se tá preenchido:

- [ ] `WIFI_SSID` / `WIFI_PASS` — rede de casa (placa só vai online com isso)
- [ ] `OPENWEATHER_API_KEY` — pega grátis em [openweathermap.org/api](https://openweathermap.org/api)
- [ ] `OPENWEATHER_CITY` / `OPENWEATHER_COUNTRY` — default é "Sao Paulo" / "BR"
- [ ] `RELATIONSHIP_START` — formato YYYY-MM-DD (usado no app **Counter**)
- [ ] `HER_NAME` — aparece em vários lugares
- [ ] `DEFAULT_PET_NAME` — nome inicial do tamagotchi (ela pode trocar depois)

> Pro **simulador** (sem placa): config.h não precisa estar correto, mas tem
> que existir. `cp src/config.example.h src/config.h` e segue.

---

## 🟡 Bloqueia algumas apps

### `sd_card/open_when/*.txt` — 7 cartinhas

Estado atual: **título pronto, corpo é GUIA, não carta**.

- [ ] `01_triste.txt` — quando triste
- [ ] `02_saudade.txt` — quando com saudade
- [ ] `03_ansiosa.txt` — quando ansiosa
- [ ] `04_prova.txt` — antes de prova
- [ ] `05_dia_bom.txt` — quando dia bom
- [ ] `06_entediada.txt` — quando entediada
- [ ] `07_bonita.txt` — quando se sentir bonita

Cada arquivo tem um GUIA dentro com estrutura sugerida. Apaga o guia e
escreve a carta de verdade.

### `sd_card/memory_pairs/` — 8 imagens 60×60

Estado atual: **vazio**. Sem essas, o jogo da memória não roda.

- [ ] `01.png` ... `08.png` — escolhe 8 fotos reconhecíveis dela / de vocês
- [ ] Resize pra 60×60 (instruções em `sd_card/memory_pairs/README.txt`)

---

## 🟢 Opcional / nice-to-have

### `sd_card/photos/` — galeria
- ✅ 15 fotos já presentes (240×320 jpg). Quer mais? Adiciona seguindo
  `01.jpg, 02.jpg, ...`. Limite confortável ~30.

### `sd_card/messages.json` — daily card
- ✅ 50 mensagens carinhosas já no arquivo. Adiciona mais se quiser
  (limite no código: 64 — em `src/apps/daily_card/daily_card.cpp:18`).

### Fotos do tamagotchi
- ✅ Sprites do pet já em `sd_card/tamagotchi_sprites/`. Funciona out-of-box.

### Backgrounds do tamagotchi
- Olha `src/apps/tamagotchi/` se quiser trocar o cenário.

---

## Como testar antes da placa

Veja a seção **Simulador (SDL)** em [SETUP.md](SETUP.md).
Resumo: `brew install sdl2 && pio run -e sim -t exec`.

O sim usa o folder `sd_card/` local como se fosse o SD físico —
então tudo que você preencher acima já aparece no sim.
