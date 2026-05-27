Fotos pra galeria (app: Gallery).

Formato:    JPG (recomendado — JPG decode é ~500ms na placa, PNG é mais lento)
Resolução:  240x320 (= resolução da tela; evita resize em runtime)
Orientação: retrato (portrait)
Nome:       ordem alfabética: 01.jpg, 02.jpg, ...
Quantidade: 10-30 fotos. Mais que 30 enche o SD rápido + boot demora pra listar.

Pra converter / redimensionar no Mac (sips já vem instalado):

  cd ~/fotos_originais
  mkdir resized
  # -Z = encaixa no maior lado; -c = corta pro tamanho exato (240x320)
  sips -Z 320 -c 320 240 *.jpg --out resized
  # remove EXIF pra ficar mais leve:
  for f in resized/*.jpg; do sips -s formatOptions 75 "$f" --out "$f"; done
  # depois copia pra cá:
  cp resized/*.jpg /caminho/para/cacaOS/sd_card/photos/

Dica: antes de copiar pro SD físico, dá uma olhada nas miniaturas em
sd_card/photos/ pra garantir que tá no zoom certo (sem cabeça cortada).

NOTA: deixa esse README aqui — ele é ignorado pelo app (que filtra só
.jpg/.png). Pode pre-stagear no SD físico também sem problema.
