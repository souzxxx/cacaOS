Coloca aqui as fotos pra galeria.

Formato: JPG (RECOMENDADO) ou PNG
Tamanho ideal: 240x320 (mesmo da tela, evita resize em runtime)
Nome: qualquer ordem alfabética (01.jpg, 02.jpg, ...)

Pra converter no Mac:
  cd ~/fotos_originais
  mkdir resized
  sips -Z 320 -c 320 240 *.jpg --out resized
  # depois copia resized/*.jpg pra /photos/ no SD
