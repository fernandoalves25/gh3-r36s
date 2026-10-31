# Guitar Hero III (nativo) para R36S

Port nativo do Guitar Hero III para o R36S e outros portáteis RK3326 com PortMaster (ArkOS, dArkOS, ROCKNIX, AmberELEC).
Não é emulação nem box64: é um player escrito em C que lê os arquivos originais do jogo (versão PC) e desenha
pela GPU do aparelho (SDL2 + GLES2), com os menus, a arte e as fontes do próprio jogo e os palcos, a banda e a plateia
do jogo em 3D atrás da pista. As regras de acerto das notas (janelas,
hammer-ons, sustains, star power, multiplicador, medidor de rock), a pontuação e as estrelas seguem as do próprio
jogo, como foram levantadas no port para navegador.

## O que precisa

- R36S com PortMaster instalado.
- A pasta `DATA` da sua cópia do Guitar Hero III para PC. São lidas as subpastas:
  - `PAK/` (lista de músicas, carreira e afinação: `qb.pak.xen` + `qb.pab.xen`) - 240 MB
  - `SONGS/` (as partituras: `*_song.pak.xen`) - 380 MB
  - `MUSIC/` (o áudio: `*.fsb.xen` + `*.dat.xen`) - 2,1 GB
  - `STREAMS/` (a música do menu e os riffs de guitarra: `streamall.fsb.xen`) - 85 MB, opcional
  - `MOVIES/R36S/` (os vídeos do jogo em MPEG-1, `*.mpg`) - 200 MB, opcional. São feitos no PC a partir dos MP4 de
    `MOVIES/WEB/` com `tools/convert_movies.sh DATA` (precisa do ffmpeg). Só essa subpasta é lida; sem ela os vídeos são pulados.
    Os quatro vídeos bônus (`brettmichaels`, `sexpistols`, `slash`, `tommorello`) são 115 MB desse total.
  - `ZONES/global_sfx.pak.xen` (os efeitos sonoros do jogo: menus, notas erradas, star power, ataques, plateia) - 6 MB, opcional.
    Sem ele os efeitos são sintetizados.
  - `ZONES/global.pak.xen` + `global.pab.xen` (a arte 2D do jogo: notas, trastes, HUD, fundos dos menus e fontes) - 83 MB, opcional.
    Sem eles o jogo funciona com um visual liso de reserva.
  - `ZONES/` (os cenários 3D, `z_*.pak.xen`) - 225 MB. Sem eles o jogo mostra um palco 2D no lugar. O resto de
    `MOVIES/` não é usado e pode ficar de fora para poupar cartão.

## Instalação

1. Copie `GH3.sh` para `roms/ports/` (ou `roms2/ports/`) do cartão e a pasta `gh3/` para `roms/ports/gh3/`.
2. Copie a pasta `DATA` do jogo para `roms/ports/gh3/DATA/` (com pelo menos `PAK`, `SONGS`, `MUSIC` e `ZONES` dentro).
3. Atualize a lista de jogos (ou reinicie) e abra **Guitar Hero III** no menu Ports.

Quem tinha a versão anterior pode apagar `GH3 3D.sh` de `roms/ports/` e `gh3-3d.aarch64` e `log-3d.txt` de `gh3/`:
agora o `GH3.sh` já abre o jogo em 3D. Os saves continuam os mesmos.

## O palco 3D

Atrás da pista o jogo desenha o cenário do próprio jogo em 3D, lido de `DATA/ZONES/z_*.pak.xen`: as malhas e texturas
do palco do set (o bar, o inferno, o Budokan…), com as câmeras do jogo cortando pela lista de cortes de cada música
(`<música>_cameras_notes`) e as de vitória e derrota no fim. Nos menus, o último palco carregado passeia pelas tomadas
gerais do jogo. No palco:

- **A banda do jogo** (**Opções > Vídeo > Banda 3D**), com o roqueiro, o visual e a guitarra escolhidos em "Seu
  roqueiro", as poses de vitória e derrota no fim da música e os ataques e reações nas batalhas. O vocalista segue a
  música (sem vocalista nas instrumentais); os modelos que faltarem na sua cópia do jogo são trocados pelo padrão.
- **A plateia do jogo**, que muda com o medidor de rock, levanta os braços no star power e acende os isqueiros nas
  músicas que pedem.
- **O show de luzes** (feixes, cor, estrobo, fogo), que segue a música e ilumina o palco, a banda e a plateia.

**Opções > Vídeo > Palco 3D** desliga o cenário (volta ao palco 2D), **Efeitos de palco** desliga as luzes, o
estrobo e a plateia, e `Escurecer o fundo` continua valendo sobre o cenário. **Opções > Vídeo > Resolução do palco 3D**
escolhe o tamanho em que o cenário é desenhado antes de ser ampliado atrás da pista: Alta (640x480), **Média**
(480x360, o padrão) ou Baixa (320x240, a mais leve); a pista e o placar ficam sempre nítidos. No R36S, na Média, ele
roda a 58,5 quadros por segundo (o painel). Na primeira vez que um palco abre, as texturas são comprimidas (ETC1) e
guardadas em `cache-3d/` na pasta do port; das outras vezes ele abre mais rápido. Apagar `cache-3d/` só faz o
próximo carregamento refazê-lo. Enquanto uma música toca, o `log.txt` ganha a cada 10 s uma linha `r3d perf:` com
quadros por segundo, tempo médio e máximo do quadro, tempo do cenário, chamadas de desenho, triângulos e memória de
texturas. Sem `ZONES/`, ou quando o aparelho desenha por software (`SOFT_RENDER=1`), o jogo usa o palco 2D sozinho.

## Menu principal

Igual ao do jogo: tela de título (aperte Start, toca um riff de guitarra) e o menu com **Carreira**, Carreira co-op,
**Jogo rápido**, Multijogador, **Treino** e **Opções**. Co-op e multijogador aparecem, mas precisam de dois
jogadores e não abrem no R36S. Nos menus tocam as 11 músicas de menu do próprio jogo, lidas de `STREAMS/` (sem essa
pasta os menus ficam em silêncio).

- **Jogo rápido**: qualquer música já liberada na carreira, em qualquer dificuldade. O recorde conta, a carreira não muda.
- **Treino > Treinar música**: escolha as seções ("Começar em" e "Terminar em") e a velocidade, como no jogo: Normal,
  Devagar (80%), Mais devagar (67%) e Bem devagar (50%). Abaixo da velocidade normal a música toca mais lenta no mesmo
  tom, a pista corre mais devagar e as janelas de acerto ficam do mesmo tamanho em tempo de música (mais largas no
  relógio). Não falha, não tem Star Power e nada é salvo; no fim, "Mudar o treino" volta à escolha do trecho.
- **Treino > Tutoriais**: as lições narradas do jogo (básico, Star Power, batalha, hammer-ons e pull-offs), refeitas a
  partir dos roteiros do próprio jogo: textos, falas do Deus do Rock e de Lou na ordem certa, a pista que congela
  enquanto eles explicam, a partitura de treino e o objetivo de cada lição. As falas são em inglês (ou francês, alemão,
  italiano, espanhol, à escolha na lista); as legendas em português resumem o que cada fala ensina. Os ataques do
  adversário da lição de batalha não são simulados. As lições concluídas ficam marcadas em `gh3-tutoriais.cfg`.

## Carreira

A lista de músicas é a carreira do jogo, liberada como no original:

- Os 8 sets abrem em ordem; cada set tem 4 músicas, uma batalha em alguns (Tom Morello, Slash, Lou) e um encore.
- Complete **2 músicas no Fácil, 3 no Médio e as 4 no Difícil e no Expert** para liberar a batalha e depois o encore.
- O encore (ou a última batalha) fecha o set e abre o seguinte. O progresso é separado por dificuldade (← → na lista).
- A lista tem as três abas do jogo, cada uma o seu caderno: **Setlist** (os sets da carreira, depois os encores do
  co-op e o resto), **Bônus** e **Downloads**. L1 e R1 passam de uma aba para a do lado.
- As batalhas são como no jogo (na Carreira e no Jogo rápido, na guitarra solo): o chefe toca a guitarra base na pista
  da direita. Completar uma frase de notas de batalha dá um ataque (até 3); o botão de star power lança o de cima.
  Ataques: amp sobrecarregado (a pista treme e as notas piscam), corda arrebentada (aperte o traste quebrado até
  consertar), dificuldade maior, notas dobradas, inversão canhota, alavanca (mexa a alavanca até liberar os trastes) e
  roubo de poder. Os medidores só mexem durante um ataque; vence quem fizer o outro falhar. Contra o Lou, perto do
  fim vem o Dreno Mortal (é preciso derrubá-lo antes), e depois de vencido ele ainda precisa ser "acabado" no resto
  da música. Antes da batalha toca a fala do chefe (de `STREAMS/`); A pula.
- Antes do encore a plateia pede bis (o coro "encore" do próprio jogo, de cada palco); A começa a música na hora.
- **Dinheiro**: a carreira paga pelas estrelas, com a tabela do jogo (3/4/5 estrelas: Fácil $75/$175/$300, Médio
  $150/$350/$600, Difícil $300/$700/$1200, Expert $450/$1050/$1800). Repetir uma música só paga a diferença quando você
  tira mais estrelas do que já tinha tirado nela; jogo rápido, treino e "sem falhar" não pagam.
- **Loja** (Y na lista da carreira): as 24 músicas bônus ($500 cada; ficam trancadas em todos os modos até comprar) e os
  4 vídeos bônus ($1500). Through the Fire and Flames não se compra: abre quando você termina a carreira. Guitarras,
  acabamentos e roqueiros ficaram de fora (precisam dos modelos 3D). Quem já tinha tocado uma música bônus continua com ela.
- Músicas baixadas ficam sempre abertas. Em Opções > Trapaças dá para liberar tudo de uma vez.
- Os vídeos da carreira tocam como no jogo: a abertura na primeira vez que a carreira é aberta e o vídeo de cada set
  ao completá-lo (o último é o final). Também há os logotipos e a abertura do jogo ao ligar (Opções > Vídeo > Vídeos de
  abertura). A ou B pula um vídeo, Start pula todos os da abertura. Precisam de `MOVIES/R36S/` (veja acima).

Recordes (pontos, porcentagem, maior sequência e estrelas) ficam em `gh3-recordes.cfg`, por música, parte e dificuldade.
O dinheiro e as compras ficam em `gh3-carreira.cfg` (na primeira vez, a carreira já jogada paga o que teria pago).
Uma música só conta como completada se não falhar; no modo "sem falhar" nada é gravado.

## Controles (padrão igual ao PS2, todos configuráveis em Opções > Controle > Botões)

O perfil padrão segue o Guitar Hero III de PS2 com o controle comum (DualShock 2): os trastes ficam nos botões de ombro e
o laranja no X, que no R36S é o botão de baixo (B). O perfil "R36S" põe os trastes
na ordem L1 L2 R2 R1, que ficou mais natural nos ombros do aparelho; o perfil "PS2" é o do manual ao pé da letra, botão
por botão:

| Ação | PS2 (DualShock 2) | R36S (padrão) | perfil PS2 |
| --- | --- | --- | --- |
| Traste verde | L2 | L1 | L2 |
| Traste vermelho | L1 | L2 | L1 |
| Traste amarelo | R1 | R2 | R1 |
| Traste azul | R2 | R1 | R2 |
| Traste laranja | X | B (botão de baixo) | B |
| Palhetada | direcional cima/baixo | direcional cima/baixo | igual |
| Star power | Select | Select ou X (botão de cima) | igual |
| Alavanca (whammy) | analógico esquerdo | qualquer um dos dois analógicos | igual |
| Pausa | Start | Start | igual |
| Sair do jogo | | Select + Start juntos, em qualquer tela | igual |

Cada ação aceita até três botões. Há seis perfis prontos (R36S, PS2, o do primeiro Guitar Hero com os botões de face, em linha, invertido e gatilhos) e a tela de controles
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
- **Controle**: canhoto (lefty flip), palhetada normal (o direcional, como na guitarra) ou toque livre (como o GH3 no controle comum do PS2: apertar o botão do traste já é a palhetada, com as mesmas regras e o mesmo som de nota errada), **Calibrar alavanca** (solte o analógico e aperte A, empurre até o fim e aperte A: o jogo guarda onde ele descansa, a zona morta, e o fim do curso), a **sensibilidade** da alavanca (50% a 200%) e os botões.
- O cartaz de opções tem as doze faixas do jogo; as que não servem no R36S (calibrar imagem, gerenciar banda,
  salvar/carregar, vídeos, a loja) ficam em cinza e o cursor passa por cima delas. A faixa do login automático do PC
  ficou com **Restaurar padrões**.
- **Vídeo**: palco 3D, a resolução do palco 3D e a banda 3D, efeitos de palco, escurecer o fundo, FPS, vídeos de abertura.

O visual é o do próprio jogo, lido de `ZONES/global.pak.xen` + `global.pab.xen`: as notas, os trastes, as chamas, a pista com
os seus trilhos, o medidor de rock com as válvulas do star power, a caixa do placar com a válvula do multiplicador e os
dígitos do jogo, e nos menus os fundos (o salão do menu principal, a mesa de madeira da lista, a parede de cartazes), a
moldura e o padrão do menu de pausa, as pílulas dos botões e a fonte dos títulos. Os textos miúdos continuam na fonte pixel.
Sem esses dois arquivos o jogo desenha tudo num visual liso de reserva.
- **Top Rockers**: os seus melhores resultados em cada música, por dificuldade.
- **Trapaças**: as do jogo, com os mesmos códigos (em "Tocar um código", segure os trastes de cada acorde e palhete):
  Hyperspeed, No Fail, Easy Expert (janelas de acerto dobradas; não vale na carreira nem para recordes), Precision Mode
  (janelas pela metade), liberar todas as músicas e liberar tudo. Cada uma também liga e desliga direto. Air Guitar
  (a banda sem instrumentos) e Performance Mode (sem pista nem placar, a música toca sozinha) mudam a banda 3D; Bret
  Michaels precisa de `PAK/Singer_Brett.pak.xen`, que nem toda cópia do jogo tem.

`port.cfg` só tem as opções do launcher (vsync, clocks, renderização por software).

## O que ainda não tem

- Co-op e multiplayer.
- Os efeitos sonoros são os do próprio jogo (`ZONES/global_sfx.pak.xen`, pela tabela de eventos de som do jogo: menus,
  notas erradas, multiplicador perdido, sequências, star power, ataques das batalhas, loja, resultado), com a plateia do
  palco de cada set (o murmúrio que acompanha o medidor de rock, vaias e aplausos, as palmas no star power, o grito no
  fim). Sem esse arquivo os efeitos são sintetizados. A guitarra silencia ao errar como no jogo.

## Compilar do código

Precisa de um Linux (ou WSL) com `aarch64-linux-gnu-gcc` e um sysroot aarch64 com SDL2 (glibc até 2.30, a do ArkOS):

```
make            # build/gh3-3d (o jogo) e build/gh3 (o mesmo sem o 3D, para os testes) para o PC, com modos sem janela: --dump, --frames, --screens, --r3dtest
make port       # build/gh3.aarch64 (com o 3D) e o zip do PortMaster em build/port/
```

O jogo é compilado com `-DGH3_3D` mais `src/r3d/` (sem a define nada do 3D é compilado). Ele precisa de um
SDL2 com `SDL_RenderFlush` (2.0.10+, resolvido em tempo de execução) e do renderer GLES2/OpenGL do SDL; por software
cai no palco 2D.
