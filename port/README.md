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
  - `STREAMS/` (a música do menu e os riffs de guitarra: `streamall.fsb.xen`) - 85 MB, opcional
  - `ZONES/` e `MOVIES/` não são usadas e podem ficar de fora para poupar cartão.

## Instalação

1. Copie `GH3.sh` para `roms/ports/` (ou `roms2/ports/`) do cartão e a pasta `gh3/` para `roms/ports/gh3/`.
2. Copie a pasta `DATA` do jogo para `roms/ports/gh3/DATA/` (com pelo menos `PAK`, `SONGS` e `MUSIC` dentro).
3. Atualize a lista de jogos (ou reinicie) e abra **Guitar Hero III** no menu Ports.

## Menu principal

Igual ao do jogo: tela de título (aperte Start, toca um riff de guitarra) e o menu com **Carreira**, Carreira co-op,
**Jogo rápido**, Multijogador, **Treino** e **Opções**. Co-op e multijogador aparecem, mas precisam de dois
jogadores e não abrem no R36S. Nos menus tocam as 11 músicas de menu do próprio jogo, lidas de `STREAMS/` (sem essa
pasta os menus ficam em silêncio).

- **Jogo rápido**: qualquer música já liberada na carreira, em qualquer dificuldade. O recorde conta, a carreira não muda.
- **Treino > Treinar música**: escolha a seção onde começar ("Começar em"). A música não falha e nada é salvo.
  Os tutoriais narrados aparecem, mas ainda não foram portados.

## Carreira

A lista de músicas é a carreira do jogo, liberada como no original:

- Os 8 sets abrem em ordem; cada set tem 4 músicas, uma batalha em alguns (Tom Morello, Slash, Lou) e um encore.
- Complete **2 músicas no Fácil, 3 no Médio e as 4 no Difícil e no Expert** para liberar a batalha e depois o encore.
- O encore (ou a última batalha) fecha o set e abre o seguinte. O progresso é separado por dificuldade (← → ou L1/R1 na lista).
- As batalhas são como no jogo (na Carreira e no Jogo rápido, na guitarra solo): o chefe toca a guitarra base na pista
  da direita. Completar uma frase de notas de batalha dá um ataque (até 3); o botão de star power lança o de cima.
  Ataques: amp sobrecarregado (a pista treme e as notas piscam), corda arrebentada (aperte o traste quebrado até
  consertar), dificuldade maior, notas dobradas, inversão canhota, alavanca (mexa a alavanca até liberar os trastes) e
  roubo de poder. Os medidores só mexem durante um ataque; vence quem fizer o outro falhar. Contra o Lou, perto do
  fim vem o Dreno Mortal (é preciso derrubá-lo antes), e depois de vencido ele ainda precisa ser "acabado" no resto
  da música. Antes da batalha toca a fala do chefe (de `STREAMS/`); A pula.
- Músicas bônus e baixadas ficam sempre abertas. Em Opções > Trapaças dá para liberar tudo de uma vez.

Recordes (pontos, porcentagem, maior sequência e estrelas) ficam em `gh3-recordes.cfg`, por música, parte e dificuldade.
Uma música só conta como completada se não falhar; no modo "sem falhar" nada é gravado.

## Controles (padrão igual ao PS2, todos configuráveis em Opções > Controle > Botões)

O perfil padrão é o do manual do Guitar Hero III de PS2 para o controle comum (DualShock 2): os trastes ficam nos
botões de ombro e no X, que no R36S é o botão de baixo (B):

| Ação | PS2 (DualShock 2) | R36S |
| --- | --- | --- |
| Traste verde | L2 | L2 |
| Traste vermelho | L1 | L1 |
| Traste amarelo | R1 | R1 |
| Traste azul | R2 | R2 |
| Traste laranja | X | B (botão de baixo) |
| Palhetada | direcional cima/baixo | direcional cima/baixo |
| Star power | Select | Select ou X (botão de cima) |
| Alavanca (whammy) | analógico esquerdo | qualquer um dos dois analógicos |
| Pausa | Start | Start |
| Sair do jogo | | Select + Start juntos, em qualquer tela |

Cada ação aceita até três botões. Há cinco perfis prontos (PS2, o do primeiro Guitar Hero com os botões de face, em linha, invertido e gatilhos) e a tela de controles
tem um teste ao vivo. Para cancelar a troca de um botão é só esperar 5 segundos sem apertar nada.

A alavanca funciona como no PS2: segurando uma nota longa, ela abaixa o tom da guitarra (até três quartos de
tom no fundo), a cauda da nota ondula, e numa frase de star power mexer a alavanca enche o medidor. O star power pode
ser usado com o medidor na metade (a marca no meio da barra); o botão aparece na tela quando está pronto.

## Atraso do som (importante)

O áudio do R36S chega aos ouvidos um pouco depois do que o jogo calcula. Antes de jogar, entre em
**Opções > Calibrar atraso**: toca um clique e você palheta no ritmo; o jogo mede o atraso e ajusta as notas e o
julgamento. Sem isso, as notas parecem certas na tela e mesmo assim saem atrasadas.

## Opções (salvas em `gh3.cfg`)

Organizadas como no jogo:

- **Áudio**: volume da banda, da guitarra e dos efeitos, música do menu, prévia das músicas na lista, atraso do áudio.
- **Calibrar atraso**: a medição descrita acima.
- **Controle**: canhoto (lefty flip), palhetada normal ou toque livre (a nota toca ao segurar os trastes), botões.
- **Vídeo**: tema visual, efeitos de palco, escurecer o fundo, FPS.
- **Top Rockers**: os seus melhores resultados em cada música, por dificuldade.
- **Trapaças**: hyperspeed, sem falhar, liberar todas as músicas.

`port.cfg` só tem as opções do launcher (vsync, clocks, renderização por software).

## O que ainda não tem

- A arte e os cenários 3D do próprio jogo, os vídeos, a loja, o modo batalha com ataques, co-op e multiplayer, os tutoriais
  narrados e o treino em velocidade reduzida.
- Os efeitos sonoros são sintetizados (menus, erros, star power); a guitarra silencia ao errar como no jogo.

## Compilar do código

Precisa de um Linux (ou WSL) com `aarch64-linux-gnu-gcc` e um sysroot aarch64 com SDL2 (glibc até 2.30, a do ArkOS):

```
make            # build/gh3 para o PC (SDL2 do sistema), com modos sem janela: --dump, --frames, --screens
make port       # build/gh3.aarch64 e o zip do PortMaster em build/port/
```
