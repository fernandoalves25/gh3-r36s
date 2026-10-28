# Guitar Hero III (nativo) para R36S

Port nativo do Guitar Hero III para o R36S e outros portáteis RK3326 com PortMaster (ArkOS, dArkOS, ROCKNIX, AmberELEC).
Não é emulação nem box64: é um player escrito em C que lê os arquivos originais do jogo (versão PC) e desenha
pela GPU do aparelho (SDL2 + GLES2). As regras de acerto das notas (janelas, hammer-ons, sustains, star power,
multiplicador, medidor de rock) seguem as do próprio jogo, como foram levantadas no port para navegador.

## O que precisa

- R36S com PortMaster instalado.
- A pasta `DATA` da sua cópia do Guitar Hero III para PC. São lidas as subpastas:
  - `PAK/` (lista de músicas e afinação: `qb.pak.xen` + `qb.pab.xen`) - 240 MB
  - `SONGS/` (as partituras: `*_song.pak.xen`) - 380 MB
  - `MUSIC/` (o áudio: `*.fsb.xen` + `*.dat.xen`) - 2,1 GB
  - `ZONES/`, `MOVIES/` e `STREAMS/` ainda não são usadas e podem ficar de fora para poupar cartão.

## Instalação

1. Copie `GH3.sh` para `roms/ports/` (ou `roms2/ports/`) do cartão e a pasta `gh3/` para `roms/ports/gh3/`.
2. Copie a pasta `DATA` do jogo para `roms/ports/gh3/DATA/` (com pelo menos `PAK`, `SONGS` e `MUSIC` dentro).
3. Atualize a lista de jogos (ou reinicie) e abra **Guitar Hero III** no menu Ports.

A primeira tela é a lista de músicas (set list, bônus e downloads lidos do próprio jogo). Escolha a música, depois
dificuldade, parte (guitarra ou baixo/ritmo) e se quer tocar com ou sem palhetada.

## Controles

| Ação | Botão |
| --- | --- |
| Trastes verde, vermelho, amarelo, azul, laranja | X, Y, B, A, R1 |
| Palhetada (strum) | D-pad cima/baixo (ou analógico esquerdo) |
| Star power | L1 ou Select |
| Whammy | analógico direito ou R2 |
| Pausa | Start |
| Menus | D-pad, A confirma, B volta; B na lista de músicas sai do jogo |

Modo **TAP** (em `port.cfg`, `TAP_MODE=1`, ou na tela da música): as notas tocam assim que os trastes certos
estão apertados, sem palhetada. Bom para começar no portátil.

## Opções (`ports/gh3/port.cfg`)

| Opção | Padrão | O que faz |
| --- | --- | --- |
| `TAP_MODE` | 0 | 1 = sem palhetada |
| `VOLUME` | 100 | volume geral |
| `LEFTY` | 0 | 1 = espelha a highway para canhotos |
| `VSYNC` | 1 | 0 = não espera o refresh da tela |
| `PERF_GOV` | 1 | clocks de CPU/GPU/memória no máximo enquanto joga |
| `SOFT_RENDER` | 0 | 1 = desenha por software (só se a imagem sair errada na GPU) |

## O que ainda não tem

- A arte do próprio jogo (texturas da highway, gemas, fontes, menus): por enquanto o visual é próprio, limpo e rápido.
- Efeitos sonoros do jogo, plateia, vídeos, cenários 3D, carreira, batalhas contra chefes, multiplayer, prática em velocidade reduzida.
- O áudio da guitarra silencia ao errar (como no jogo), mas não há eco de star power nem pitch caindo ao falhar.

## Compilar do código

Precisa de um Linux (ou WSL) com `aarch64-linux-gnu-gcc` e um sysroot aarch64 com SDL2 (glibc até 2.30, a do ArkOS):

```bash
make                                   # build/gh3 para testar no PC (SDL2 do sistema)
SYSROOT=/caminho/sysroot-arm64 bash scripts/build_arm64.sh   # build/gh3.aarch64
bash scripts/make_port.sh              # build/port/gh3-r36s.zip (sem material do jogo)
```

Testes sem tela: `./build/gh3 --data DATA --dump --song barracuda`, `--audiotest`, `--frames 1500 --song barracuda --out dir`, `--screens dir`.

## Créditos

- Regras do jogo e formatos dos arquivos levantados a partir do *Guitar Hero Browser Edition* (port para navegador de Chris Chan).
- Decodificação de MP3: [minimp3](https://github.com/lieff/minimp3) (domínio público / CC0).
- Guitar Hero III é da Activision / Neversoft. Este projeto não traz nenhum material do jogo: use a sua cópia.
