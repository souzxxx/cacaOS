Cartinhas "Open When" (app: Open When).

Cada arquivo .txt vira um envelope no app. Formato OBRIGATÓRIO:

  Título da ocasião
  ---
  Corpo da carta (pode ter várias linhas,
  parágrafos, listas, o que quiser).
  Lê até o final do arquivo.

Regras:
- A primeira linha é o título do envelope (aparece na listagem).
- A segunda linha tem que ser exatamente "---" (3 hifens).
- Tudo depois disso é o corpo, mostrado quando o envelope abre.
- UTF-8 sem BOM. Acentos PT-BR são suportados.
- Ordem dos envelopes = ordem alfabética dos nomes (por isso o prefixo
  numérico 01_, 02_, ...).

Dica:
- Pode ter até ~10 envelopes confortavelmente. Mais que isso fica
  rolagem chata na placa.
- Mantém cada carta entre 5-12 linhas. Lê em uns 30-45 segundos.
- O título aparece em fonte média; corpo em fonte um pouco menor.
  Evita títulos muito longos (> 22 caracteres) ou cortam.

NOTA pro Leonardo: os arquivos atuais (01_triste.txt etc.) têm GUIAS
no corpo, não conteúdo real. Apaga o guia e escreve a carta de verdade
antes de copiar pro SD físico. Esse README.txt é seguro deixar — o app
ignora qualquer arquivo chamado "README.txt" (case-insensitive). Veja
src/apps/open_when/open_when.cpp.
