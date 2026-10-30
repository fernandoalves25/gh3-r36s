# Guitar Hero III nativo para R36S

Player de Guitar Hero III escrito em C para o R36S e outros portáteis RK3326 com PortMaster (ArkOS, dArkOS,
ROCKNIX, AmberELEC). Não é emulação: ele lê os arquivos da versão PC do jogo (músicas, partituras, carreira e
afinação) e desenha com SDL2 + GLES2, em visual próprio. As regras de acerto, a pontuação, as estrelas e a ordem
da carreira seguem as do jogo original.

| Pixel art | HD suave |
| --- | --- |
| ![Tema pixel art](docs/pixel.png) | ![Tema HD suave](docs/hd.png) |
| ![Lista de músicas da carreira](docs/setlist.png) | ![Tela de resultados](docs/results.png) |

## O que tem

- Carreira liberada como no original (sets, batalhas, encores), por dificuldade, com recordes e estrelas salvos.
- Dinheiro da carreira pela tabela do jogo e a Loja: músicas bônus e vídeos bônus com os preços originais; o coro do encore.
- Batalhas contra Tom Morello, Slash e Lou com os ataques do jogo, o chefe na pista ao lado e o Dreno Mortal.
- Hammer-ons, sustains, star power, multiplicador e medidor de rock com as janelas de tempo do jogo.
- Controles iguais ao Guitar Hero III de PS2 por padrão (trastes em L2, L1, R1, R2 e B), configuráveis, com cinco perfis.
- Alavanca nos dois analógicos, que abaixa o tom das notas longas como no PS2.
- O visual do próprio jogo (notas, trastes, pista, HUD, menus e fontes lidos de `ZONES/global`), calibração do atraso do som, menus em português, prévia das músicas.
- **Dois executáveis na mesma pasta**: `GH3` (palco em pixel art) e `GH3 3D`, que desenha o cenário do próprio jogo
  em 3D atrás da pista (os `.scn`/`.tex` de `ZONES/`, com as câmeras do jogo cortando no ritmo da música). Os dois leem o
  mesmo `DATA` e os mesmos arquivos de save; o 3D é experimental e pode ser desligado em Opções > Vídeo > Palco 3D.

## Instalação

Este repositório não traz nenhum arquivo do jogo: é preciso a pasta `DATA` da sua cópia do Guitar Hero III para PC.
O passo a passo, os controles e as opções estão em [port/README.md](port/README.md).

## Compilar

Num Linux (ou WSL) com SDL2, `aarch64-linux-gnu-gcc` e um sysroot aarch64 com SDL2 (glibc até 2.30):

```
make            # build/gh3 (2D) e build/gh3-3d (-DGH3_3D) para o PC, com modos sem janela: --dump, --frames, --screens, --stretchtest, --r3dtest
make port       # build/gh3.aarch64, build/gh3-3d.aarch64 e o zip do PortMaster em build/port/
```

O binário 3D é compilado dos mesmos fontes mais `src/r3d/` (leitor de cenas `.scn`, bancos `.tex`/DDS, câmeras e luzes do
`.qb` da zona, e o desenho em GLES2 cru dentro da janela do SDL_Renderer, antes da camada 2D). Sem `-DGH3_3D` nada de
`r3d/` entra no binário 2D. `build/gh3-3d --r3dtest z_dive --out DIR` carrega uma zona numa janela escondida, salva
quadros e imprime malhas, texturas, triângulos, memória de vídeo e ms por quadro nas três resoluções do palco
(`R3D_NODXT=1` força o caminho do Mali: texturas decodificadas e comprimidas em ETC1).

O decodificador de MP3 é o [minimp3](https://github.com/lieff/minimp3) (CC0) e o de vídeo (MPEG-1 + MP2) é o
[pl_mpeg](https://github.com/phoboslab/pl_mpeg) (MIT), ambos em `third_party/`.
A pasta `tools/fixes/` guarda correções de outros ports usados no mesmo aparelho.

## Licença

O código deste projeto é livre, sob a [licença MIT](LICENSE). As bibliotecas em `third_party/` têm as suas próprias
licenças (minimp3: CC0; pl_mpeg: MIT). A licença vale só para o código: os arquivos do jogo, as músicas e os vídeos
continuam sendo da Activision e não fazem parte deste repositório.

Guitar Hero é marca da Activision. Este projeto não é afiliado à Activision nem à RedOctane.
