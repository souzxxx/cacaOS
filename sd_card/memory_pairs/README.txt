Pares pro app Memory Game (jogo da memória 4x4).

Quantidade: EXATAMENTE 8 imagens. Cada uma vira UM par no tabuleiro
            (8 pares × 2 = 16 cards).

Formato:    PNG (recomendado por causa do alpha) ou JPG
Resolução:  60x60 px — fixo. Vai aparecer sem resize.
Nome:       01.png ... 08.png (ordem importa pro layout fixo)

Escolha das imagens — IMPORTANTE pra ela reconhecer rápido:
- ✓ Selfie nítida em close (rosto ocupa boa parte do quadro)
- ✓ Foto de comida que vocês comeram juntos (algo identificável)
- ✓ Lugar marcante (placa, fachada, vista — algo que ela bata o olho)
- ✓ Pet / objeto / desenho que ela ama
- ✗ Foto de paisagem pequena demais
- ✗ Print de tela / texto (não dá pra ler em 60x60)
- ✗ Selfie escura ou de longe

Pra redimensionar 60x60 quadrado no Mac:

  sips -Z 60 -c 60 60 input.jpg --out output.png

Pra fazer em lote (com 8 imagens em ~/memory_originais/):

  cd ~/memory_originais
  i=1
  for f in *.jpg; do
    sips -Z 60 -c 60 60 "$f" --out "$(printf '%02d.png' $i)"
    i=$((i+1))
  done

Confere visual:
  open *.png   # abre tudo no Preview pra ver se dá pra reconhecer

NOTA: app espera os 8 arquivos. Se faltar algum, ele degrada graciosamente
mas o tabuleiro fica com card faltando. Não coloca 9+ (vai ignorar).
