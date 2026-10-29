# Guitar Hero III (nativo) para R36S

Port nativo do Guitar Hero III para o R36S e outros portáteis RK3326 com PortMaster (ArkOS, dArkOS, ROCKNIX, AmberELEC).
Não é emulação nem box64: é um player escrito em C que lê os arquivos originais do jogo (versão PC) e desenha
pela GPU do aparelho (SDL2 + GLES2), com visual próprio em pixel art. As regras de acerto das notas (janelas,
hammer-ons, sustains, star power, multiplicador, medidor de rock), a pontuação e as estrelas seguem as do próprio
jogo, como foram levantadas no port para navegador.

## O que precisa

- R36S com PortMaster instalado.
- A pasta `DATA` da sua cópia do Guitar Hero III para PC. São lidas as subpastas:
  - `PAK/` (lista de músicas, carreira e afinação: `qb.pak.xen` + `qb.pab.xen`) - 240 MB
  - `SONGS/` (as partituras: `*_song.pak.xen`) - 380 MB
  - `MUSIC/` (o áudio: `*.fsb.xen` + `*.dat.xen`) - 2,1 GB
  - `ZONES/`, `MOVIES/` e `STREAMS/` não são usadas e podem ficar de fora para poupar cartão.

## Instalação

1. Copie `GH3.sh` para `roms/ports/` (ou `roms2/ports/`) do cartão e a pasta `gh3/` para `roms/ports/gh3/`.
2. Copie a pasta `DATA` do jogo para `roms/ports/gh3/DATA/` (com pelo menos `PAK`, `SONGS` e `MUSIC` dentro).
3. Atualize a lista de jogos (ou reinicie) e abra **Guitar Hero III** no menu Ports.

## Carreira

A lista de músicas é a carreira do jogo, liberada como no original:

- Os 8 sets abrem em ordem; cada set tem 4 músicas, uma batalha em alguns (Tom Morello, Slash, Lou) e um encore.
- Complete **2 músicas no Fácil, 3 no Médio e as 4 no Difícil e no Expert** para liberar a batalha e depois o encore.
- O encore (ou a última batalha) fecha o set e abre o seguinte. O progresso é separado por dificuldade (← → ou L1/R1 na lista).
- As batalhas são tocadas como músicas normais (a trilha do chefe toca junto), sem os ataques do modo batalha.
- Músicas bônus e baixadas ficam sempre abertas. Em Opções dá para liberar tudo de uma vez.

Recordes (pontos, porcentagem, maior sequência e estrelas) ficam em `gh3-recordes.cfg`, por música, parte e dificuldade.
Uma música só conta como completada se não falhar; no modo "sem falhar" nada é gravado.

## Controles (padrão, todos configuráveis em Controles)

| Ação | Botão |
| --- | --- |
| Trastes verde, vermelho, amarelo, azul, laranja | X, Y, B, A, R1 |
| Palhetada | D-pad cima/baixo ou analógico esquerdo |
| Star power | L1 ou Select |
| Alavanca (whammy) | analógico direito ou R2 |
| Pausa | Start |
| Menus | D-pad, A confirma, B volta, X Opções, Y Controles |

Cada ação aceita dois botões (ou direções de analógico). Há três perfis prontos e a tela de controles tem um teste ao vivo.

## Opções (menu Opções, salvas em `gh3.cfg`)

Palhetada normal ou toque livre (a nota toca ao segurar os trastes), canhoto, velocidade da pista (hyperspeed),
sem falhar, efeitos de palco, escurecer o fundo, prévia das músicas na lista, volumes da música e dos efeitos,
atraso do áudio (calibração) e FPS. `port.cfg` só tem as opções do launcher (vsync, clocks, renderização por software).

## O que ainda não tem

- A arte e os cenários 3D do próprio jogo, os vídeos, a loja, o modo batalha com ataques, co-op e multiplayer.
- Os efeitos sonoros são sintetizados (menus, erros, star power); a guitarra silencia ao errar como no jogo.

## Compilar do código

Precisa de um Linux (ou WSL) com `aarch64-linux-gnu-gcc` e um sysroot aarch64 com SDL2 (glibc até 2.30, a do ArkOS):

```
make            # build/gh3 para o PC (SDL2 do sistema), com modos sem janela: --dump, --frames, --screens
make port       # build/gh3.aarch64 e o zip do PortMaster em build/port/
```
