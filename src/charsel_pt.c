/* The character hub's and the store's words in Brazilian Portuguese (charsel.h): the rockers' stories of
 * musician_profiles (by profile name), the guitars' of musician_instrument (by desc_id: the finishes of one guitar
 * mostly share one), and the names the data gives outfits, styles and finishes. What has no entry here is shown as the
 * data has it, in English. Proper names, songs and records, makers and models (Gibson, Les Paul, X-Plorer) and the
 * makers' own finish and line names (Sunburst, Goldtop, Standard, Custom, Signature) stay as they are. */
#include "charsel.h"
#include <string.h>
#include <strings.h>

/* ------------------------------------------------------------------ the rockers */
static const struct { const char *name, *text; } ROCKER[] = {
    { "Axel", "Depois de virar um superastro de verdade, Axel se recolheu às fogueiras da cidade natal e às jams no celeiro. Agora está de volta aos holofotes, pronto para destruir qualquer um ou qualquer coisa sem graça o bastante para ficar no caminho da sua destruição bate-cabeça!" },
    { "Casey", "Finalmente assumindo seu charme feminino, Casey chegou ao posto de 'Deusa do Rock'. Quando um repórter perguntou se ela tinha se vendido, Casey respondeu com um chute giratório na cara. 'Relaxa, eu pago o hospital.'" },
    { "Izzy", "Filhas, tranquem suas mães... o sol ainda não se pôs na Sunset Strip! Izzy volta com o estilo e o cabelão, e não para até seus ouvidos sangrarem rock puro. O glam nunca foi tão gostoso." },
    { "Judy", "Ao perceber que a maioria dos seus fãs eram garotos de 14 anos, Judy largou a gravadora como quem larga um vício. Na estreia solo, 'Punkagothic Rockabillica', Judy traz seu próprio estilo e mostra que sola tão bem quanto os marmanjos, e ainda mais." },
    { "Johnny", "'Eu adoro o cheiro de napalm pela manhã.' Quem disse isso nunca conheceu o Johnny. Virando a noite no thrash e na farra, Johnny não dorme: apaga. 'Eu como disco e c4g0 emo.'" },
    { "Lars", "Depois de uma estreia calorosíssima um ano atrás, Lars precisou de um tempo para esfriar. Preparem os barcos vikings e aumentem o metal. Lars está pronto para reverter o aquecimento global com sua gélida turnê mundial." },
    { "Midori", "Vinda do Japão, Midori é doce e azeda ao mesmo tempo. Com formação clássica em violino e no palco desde os 3 anos, Midori largou o arco, pegou a guitarra e nunca mais olhou para trás. Escola nunca mais!" },
    { "Xavier", "A própria definição de descolado, Xavier maneja o som com efeito hipnótico. As garotas querem ele e os caras querem SER ele. Chamado de 'O Palhetador do Amor', Xavier luta pela paz na Terra e pela boa vontade para com... o Rock 'N Roll." },
    { "Slash", "Como integrante do Guns N' Roses, do Slash's Snakepit e do Velvet Revolver, Slash se firmou como um verdadeiro Guitar Hero. Conhecido pelo seu jeito ardente e bluesy de tocar e pela cartola que é sua marca, Slash é um dos guitarristas mais reconhecíveis do Rock 'N Roll hoje." },
    { "Morello", "Tom Morello ajudou a redefinir do que a guitarra é capaz com seu jeito inovador de tocar ao longo dos anos no Rage Against The Machine, no Audioslave e no seu projeto solo, The Nightwatchman." },
    { "Satan", "Lou é o seu nome, e trocar talento musical por almas é o seu jogo. Encontre-o na encruzilhada e deixe o nervosismo na varanda, ou prepare-se para passar a eternidade afinando as guitarras dele. Gosta mesmo é de cachorro." },
    { "RockGod", "Já foi humano, mas o God of Rock subiu ao mais alto nível do rock e toca toda noite num pequeno bar no Monte Olimpo. De vez em quando, arranja tempo para duelos de guitarra com meros humanos. Também dizem que foi ele quem inventou o alaúde." },
    { "Ripper", "Quando não vasculha as entranhas do Hades atrás da próxima grande banda, o Grim Ripper faz turnês de verão pela Terra: é o seu modus operandi. Ele também curte passeios de canoa." },
};

/* ------------------------------------------------------------------ the guitars */
#define T_LESPAUL "A Les Paul é a guitarra elétrica mais conhecida, duradoura e reconhecível à primeira vista da história do rock. É a guitarra de alta qualidade com que todo aspirante a deus do rock sonha. Jimmy Page, Zakk Wylde, Slash e um seleto grupo de astros do rock fizeram da Les Paul "
#define T_LESPAUL_END " Suba no palco com esta guitarra e conquiste o seu lugar de direito no panteão da Realeza do Rock!"
#define T_SG "Desde 1961 a SG é a alternativa favorita à Les Paul entre os fãs da Gibson. Mais fina e mais leve que a Les Paul, oferece um som e uma tocabilidade diferentes da sua parente mais robusta. Hoje mais um clássico da Gibson, a SG existe numa variedade de estilos, opções e acabamentos. Entre os grandes nomes da SG estão Jimi Hendrix, Angus Young e Pete Townshend. A SG foi a guitarra preferida de Eric Clapton na época do Cream."
#define T_EXPLORER "Conhecida originalmente como Gibson Explorer, em 1958, depois foi rebatizada de Futura. Como a prima, a Flying V, era uma proposta ousada e original. De início foi um fracasso comercial e saiu de linha em 1959. Em 1975 a Gibson relançou a Explorer e ela virou um clássico instantâneo, provando que algumas ideias estão à frente do seu tempo. The Edge, James Hetfield e Dave Grohl ajudaram a popularizar este instrumento dinâmico."
#define T_EPIPHONE "A Epiphone é a marca de entrada da Gibson. A Epiphone faz réplicas "
static const char ES335[] = "A ES335 é uma guitarra elétrica de corpo oco e tampo arqueado que existe quase sem mudanças desde 1958. Ela é o resultado prático do experimento chamado The Log, a tentativa de Les Paul de criar uma guitarra elétrica amplificada. A outra guitarra que saiu do experimento foi a Les Paul de corpo sólido. A ES335 é compacta e ergonômica, com um timbre maravilhoso. Entre seus músicos famosos estão Alex Lifeson, do Rush, B.B. King e Dave Grohl.",
    SG[] = T_SG, EXPLORER[] = T_EXPLORER,
    FIREBIRD[] = "Outro desenho clássico da Gibson dos anos 1950. Tão ousada à sua maneira quanto a X-plorer e a V-Factor, com sua mão 'invertida' e seu corpo extravagante, lembrava as grandes barbatanas dos carros dos anos 1950. Entre seus donos famosos estão Eric Clapton, Noel Gallagher e Keith Richards.",
    VFACTOR[] = "É difícil acreditar que um ícone do rock como a V possa ter sido algo além de um sucesso enorme. Mas a Flying V, como era chamada no início, foi considerada estranha e extravagante demais para agradar ao grande público. A Flying V foi relançada em 1967 com algumas mudanças estéticas e mecânicas e fez grandes coisas. Entre os músicos conhecidos ligados à Flying V estão Jimi Hendrix (ele tinha uma com pintura psicodélica personalizada), K.K. Downing (Judas Priest), Dave Davis (Kinks), Kirk Hammett, Zakk Wylde, Mike McReady, Joe Perry, Michael Schenker e muitos outros. Esta guitarra tem todo o timbre e a versatilidade que se espera da Gibson e mais presença de palco do que você vai precisar na vida.",
    JUNIOR[] = "Criada como uma guitarra de entrada acessível para estudantes, a Junior era, na prática, um instrumento de alta qualidade sem firulas. Nos anos setenta a Junior ficou muito popular graças ao crunch característico do seu único captador P-90. Hoje as Juniors vintage valem uma bolada entre os colecionadores.",
    LESPAUL[] = T_LESPAUL "a lendária Voz do Rock que ela é hoje." T_LESPAUL_END,
    MARAUDER[] = "De meados dos anos 1970, a Marauder era uma guitarra de entrada que permitia pôr as mãos numa Gibson legítima. A Marauder foi fabricada por apenas cinco anos. Com sua curiosa mistura de corpo esculpido no estilo Les Paul e mão de Flying V, esta guitarra virou hoje um instrumento muito cobiçado, em boa parte pela raridade e pelo visual incomum. Paul Stanley, Thurston Moore e Josh Homme são todos fãs da Marauder.",
    CORVUS[] = "A Corvus foi uma tentativa inovadora da Gibson de apresentar um formato novo a quem compra guitarras. Sua silhueta incomum talvez tenha sido radical demais para muitos compradores diante de tantas opções no mercado, ou talvez a Gibson estivesse, mais uma vez, à frente do seu tempo. Seja qual for o motivo, esta guitarra não fez sucesso comercial na época, mas, como a Marauder, virou um item de colecionador pela raridade e pelo visual incomum. Se você gosta de ser diferente, esta é para você.",
    SUPER400[] = "Uma luxuosa guitarra de corpo grande, oco e de tampo arqueado, com timbre e tocabilidade excelentes, que existe desde 1951. Tem o visual clássico do rock'n'roll dos anos 1950 aliado a uma sonoridade rica e versátil, boa para rock, jazz e country. Quem tem uma dessas belezuras sabe que chegou lá. Scotty Moore, o guitarrista de Elvis, era devoto da Super 400. Mark Knopfler é fã e, claro, o grande Chet Atkins tocava a Super 400-CES.",
    FOCUS[] = "A Focus Fatboy é uma guitarra de entrada poderosa, com o ótimo som, o estilo e a pegada que você já espera da Kramer. Com dois captadores single-coil e um humbucker, suas opções de som são praticamente infinitas.",
    EPI_LESPAUL[] = T_EPIPHONE "da Les Paul para quem tem pouco dinheiro e quer a dinâmica e a ergonomia da maior guitarra de rock do mundo. " T_LESPAUL "a lenda que ela é hoje." T_LESPAUL_END,
    EPI_EXPLORER[] = T_EPIPHONE "da Explorer para quem tem pouco dinheiro e quer o impacto da Explorer original. " T_EXPLORER,
    EPI_SG[] = T_EPIPHONE "da SG para quem tem pouco dinheiro e quer o visual e a tocabilidade da SG original. " T_SG;
static const struct { uint32_t id; const char *text; } GUITAR[] = {
    { 0xbe9144acu, ES335 }, { 0x55d750e4u, ES335 }, { 0xfc9052aau, ES335 }, { 0x96c50497u, ES335 }, { 0x44a203b8u, ES335 },
    { 0xa5d7760au, SG }, { 0x406a7e76u, SG }, { 0xe5c59398u, SG },
    { 0x4405d7d2u, "Uma SG mais leve e mais potente, com um acabamento de cor vibrante." },
    { 0x44d14c11u, EXPLORER }, { 0xbcad0ff7u, EXPLORER }, { 0x8f9b3b9du, EXPLORER }, { 0xa96ce4d3u, EXPLORER }, { 0x152b2a66u, EXPLORER },
    { 0x05eda2feu, FIREBIRD }, { 0xa40607d9u, FIREBIRD }, { 0x713af979u, FIREBIRD }, { 0x3bcac6ddu, FIREBIRD }, { 0x7b38a2a7u, FIREBIRD },
    { 0xb7e8c0f1u, VFACTOR }, { 0xc093d88au, VFACTOR }, { 0xb94778ccu, VFACTOR }, { 0xe312daefu, VFACTOR }, { 0x0b91d16du, VFACTOR },
    { 0x117d629fu, "Jimi Hendrix pintou à mão este instrumento quase mítico de 1967. Foi comprado em 1967 e usado nas turnês pela Europa em 1967 e 1968. jimihendrix.com authentichendrix.com" },
    { 0x1d22fe64u, JUNIOR }, { 0x842bafdeu, JUNIOR }, { 0xf32c9f48u, JUNIOR },
    { 0x6d480aebu, "Esta edição Signature é uma cópia fiel da própria Junior 1956 de Billie Joe. Ela traz um captador H-90 de bobina dupla empilhada, feito especialmente para ela, com um som enorme e agressivo." },
    { 0x662dd01fu, LESPAUL }, { 0xd708f871u, LESPAUL }, { 0x97467512u, LESPAUL }, { 0x1c428ffdu, LESPAUL }, { 0x91b3a689u, LESPAUL },
    { 0xaed0a2f4u, T_LESPAUL "a lenda que ela é hoje." T_LESPAUL_END },
    { 0xc1466672u, "Uma Les Paul Custom linda e inconfundível, com grafismo em preto e branco antigo, captadores EMG e ferragens douradas. O braço é de bordo sem acabamento, com escala de ébano e marcações em bloco de madrepérola. Uma guitarra à altura de um mestre do metal." },
    { 0x1d765602u, "A Les Paul Camo de Zakk Wylde é uma LP Custom dos anos 80 modificada, com braço de bordo sem acabamento, escala de bordo e captadores EMG. Esta máquina tem tampo de bordo entalhado, fundo de mogno maciço e ferragens douradas." },
    { 0x012d33d4u, T_LESPAUL "a lenda que ela é hoje." T_LESPAUL_END " Esta versão digital de ponta derruba limites e restrições para o guitarrista." },
    { 0x4c7622e1u, MARAUDER }, { 0x73d5303au, MARAUDER }, { 0x2e8e3592u, MARAUDER },
    { 0x44106645u, CORVUS }, { 0x5d5006e2u, CORVUS }, { 0xd67e15b5u, CORVUS },
    { 0x13a200d7u, SUPER400 }, { 0xd81d8868u, SUPER400 }, { 0x5bc6dd6bu, SUPER400 }, { 0x354287fcu, SUPER400 }, { 0x68198254u, SUPER400 }, { 0x9b902b3eu, SUPER400 },
    { 0x51382683u, FOCUS }, { 0xc8317739u, FOCUS },
    { 0xd9bcbf4bu, "As guitarras de braço de alumínio da Kramer foram revolucionárias. Tinham um sustain enorme e um som monstruoso. A Kramer fez vários formatos de corpo em madeiras lindíssimas. Essas guitarras ficavam ótimas no palco e rendiam demais no estúdio." },
    { 0x7720d892u, "Ned Steinberger projetou o L2 nos anos 70. Três protótipos foram vendidos a John Entwistle, Tony Levin e Andy West. Eram feitos de um composto de carbono e grafite, sem cabeça e com as tarraxas na ponte. Quando chegaram às lojas no início dos anos 80, foram recebidos com euforia. Entre as antecessoras da Synapse estão a Série L e a Série M (projetada com a ajuda de Mike Rutherford). Com seu visual sem cabeça de virar cabeças, a Synapse é a mais nova da tradição Steinberger e tem a tocabilidade, o sustain e a sonoridade das antecessoras." },
    { 0xf61e2935u, EPI_LESPAUL }, { 0x6f17788fu, EPI_LESPAUL }, { 0x1979c181u, EPI_EXPLORER }, { 0x8070903bu, EPI_EXPLORER }, { 0xe6a4c081u, EPI_SG }, { 0x7fad913bu, EPI_SG },
    { 0x836ffc1eu, "Na mitologia grega, Apolo era o deus do sol, da música, da cura, da profecia e da poesia." },
    { 0x21b323b5u, "Na mitologia grega, Atena era a deusa da sabedoria, dos ofícios, da guerra defensiva e a padroeira de Atenas." },
    { 0xefd998d0u, "Lembra do final, quando ele teve que convencer a bomba a não explodir? Faça-se a luz? Faça-se o rock!" },
    { 0xead2abc3u, "Esta guitarra toca o hino dos Estados Unidos, não importa como você a afine." },
    { 0x4d9d0e03u, "Xavier Stone encomendou a Metal Starfish durante o Verão do Amor. Em 1967 os equinodermos eram a última moda. Quem não tinha um instrumento com tema marinho estava definitivamente fora da onda." },
    { 0x03852cb8u, "Pela Inglaterra e por São Jorge! God save the Queen! Anarchy in the UK! Babacas!" },
    { 0xf2029081u, "Chega de ficar largado no fundo do poço: você vai adorar sair do Inferno do Lou com esta navegadora sônica. Ótima para fritar." },
    { 0xd279d327u, "A favorita dos surfistas. Não morda mais do que consegue mastigar. O tubarão não é um mamífero." },
    { 0x901f87b7u, "Esta bem pode ser a guitarra que você tocaria se tivesse um medo mórbido de ir para a cadeia... e você tem." },
    { 0xf09ceb45u, "Hoje em dia todo mundo adora um pirata... até achar cópias ilegais do seu último disco circulando na internet, é claro. O genial e insano luthier Philbert 'meio pirado' Phunngalzoft fez esta especialmente para você." },
    { 0x796bd545u, "Se existisse uma guitarra robô samurai de plástico, seria esta. A escala é de um novo bioplástico transparente, que dá a impressão de que as cordas flutuam sobre o nada. Não coma." },
    { 0x464c6a43u, "Mais loucura TIKI: junte a coleção completa e ganhe um prêmio fantástico! Esta guitarra Tiki vai fazer você sonhar com o lindo Pacífico, a vida tranquila e feliz na ilha e o amor que ficou para trás. Bons tempos. A vida era um paraíso. O sol brilhava, os dias não acabavam nunca e você tinha tudo de sobra. Aí você foi inventar de entrar numa banda..." },
    { 0x8c2d85dbu, "Ficar de bobeira demais na praia enfurece os espíritos da ilha. Mas, se essa é a sua praia, esta guitarra Tiki irritadiça é para você." },
    { 0x7703672cu, "Chegou a hora de mostrar suas cartas, mas não se precipite." },
    { 0x9659c9bcu, "Alguma coisa sobre dinamite e raios laser? O que é que isso quer dizer? E quem sou eu para negar?" },
    { 0x44cef723u, "O lendário luthier Philbert 'meio pirado' Phungalzoft pegou a tampa da privada e foi em frente, acrescentando um rato gigante empalhado e envernizado à obra-prima pronta. Fãs de guitarra do mundo inteiro adorariam sujar as mãos com esta obra-prima." },
    { 0x1c8dd771u, "Com esta guitarra, todo riff vira uma arma nas suas mãos. A guitarra é mais poderosa que a espada." },
    { 0x0df9cba6u, "Inspirada nos primeiros filmes de ficção científica, esta peça virou objeto de desejo desde que revelaram que a coisa toda foi forjada num galpão no deserto." },
    { 0x1b31a7cbu, "Com um braço macio e sedoso assim, você vai disparar frases ligeiras como mercúrio." },
    { 0x350f7251u, "É uma espécie de perna de mulher que, de algum jeito, virou guitarra. Junte a coleção completa. Pode custar os olhos da cara (e as pernas)." },
    { 0x21e907d1u, "Alguns dizem que este foi o instrumento que a própria Cleópatra deu a César, como seu maior sacrifício de amor. Outros dizem que pode ser o alaúde sagrado do rei Nebkheperure Tutancâmon, filho de Amenófis III. Há ainda quem imagine que seja a antiga lira de ouro maciço dada à rainha Vitória por Sir Samuel Baker, o famoso explorador do século 19. Na verdade é uma bugiganga barata de plástico de Las Vegas." },
    { 0x01f4a732u, "Daquele pessoal adorável da Axe, uma guitarra maravilhosa e perfeitamente equilibrada, com uma escala precisa e rápida. E ainda acaba com o cecê do seu sovaco." },
    { 0xf0ec8ac4u, "Uma espécie de bebê alienígena que virou instrumento musical numa bizarra implosão quântica de nanotempo da sétima dimensão. Este bebê alienígena vivo e que respira faz sons doces para você tocar músicas loucas e cheias de groove. Não entre em pânico!" },
    { 0x1369ab7bu, "Esta é a guitarra preferida de Slash para gravar em estúdio. Ela aparece nos discos multimilionários do Guns 'n' Roses, como 'Appetite for Destruction' e 'Lies'. É a guitarra 'Santo Graal' de Slash e fica trancada num cofre quando não está em uso." },
    { 0xad6a7f90u, "Um instrumento morto-vivo digno apenas do próprio mestre do mal. A guitarra favorita do grandalhão vermelho Lou canta como uma coisa possuída. Porque ela é." },
    { 0xd3011132u, "A guitarra de Tom tem corpo personalizado de estilo tradicional, braço Kramer Carrera, alavanca Floyd Rose e captadores EMG. Esta peça lendária foi usada no Rage Against the Machine e no Audioslave e tem arte feita por esse grande guitar hero." },
    { 0x695729e5u, "Uma guitarra digna apenas do próprio God of Rock: quando você é o God of Rock, só uma Gibson serve, de preferência feita de plasma, luz laser e ouro maciço." },
    { 0xc110a1aeu, "O Grim Ripper usa pantufa e pé de pato e come arenque no prato." },
};

/* ------------------------------------------------------------------ the names */
static const char *const OUTFIT[][2] = {   /* outfits ("name" of each profile's outfits) */
    { "Shorts", "Bermuda" }, { "Leather", "Couro" }, { "Mo' Leather", "Mais Couro" }, { "The Rockstar", "O Astro do Rock" }, { "The Peacock", "O Pavão" },
    { "Mohawk", "Moicano" }, { "Puffball", "Pompom" }, { "Xavier Classic", "Xavier Clássico" }, { "Captain X", "Capitão X" }, { NULL, NULL } };
static const char *const STYLE[][2] = {    /* and their styles */
    { "Black 'N Blue", "Preto e Azul" }, { "Black 'N Red", "Preto e Vermelho" }, { "Stonewashed", "Estonado" }, { "Red Leather", "Couro Vermelho" },
    { "Black Vest", "Colete Preto" }, { "Brown Vest", "Colete Marrom" }, { "White Vest", "Colete Branco" }, { "Stripes", "Listras" },
    { "Basic Black", "Preto Básico" }, { "Silver 'N Black", "Prata e Preto" }, { "Crimson", "Carmim" }, { "Camo", "Camuflado" },
    { "Black 'N White", "Preto e Branco" }, { "Vast Purple", "Roxo Imenso" }, { "Fiery", "Flamejante" },
    { "Leopard", "Leopardo" }, { "Lizard", "Lagarto" }, { "Menagerie", "Zoológico" }, { "Fantasy", "Fantasia" }, { "Florid", "Florido" }, { "Starry Night", "Noite Estrelada" },
    { "Shocking Pink", "Rosa-Choque" }, { "Tornado Blue", "Azul Tornado" }, { "Jet Black", "Preto Azeviche" }, { "Flame Orange", "Laranja Chama" },
    { "Purple Plaid", "Xadrez Roxo" }, { "Dirty Yellow", "Amarelo Sujo" }, { "Riot", "Baderna" },
    { "Bloodshot", "Injetado" }, { "Pea Soup", "Sopa de Ervilha" }, { "Spew", "Vômito" }, { "Skeleton", "Esqueleto" },
    { "Spinach", "Espinafre" }, { "Pumpkin", "Abóbora" }, { "Two Tone", "Duas Cores" }, { "Eggplant", "Berinjela" },
    { "Dyed Black", "Tingido de Preto" }, { "Bleach Blonde", "Loiro Oxigenado" }, { "Phantom", "Fantasma" }, { "Thunda", "Trovão" },
    { "Barbaric", "Bárbaro" }, { "Icy", "Gélido" }, { "Freedom", "Liberdade" },
    { "Amethyst", "Ametista" }, { "Ruby", "Rubi" }, { "Candy", "Doce" }, { "Emerald", "Esmeralda" }, { "Aqua", "Água-Marinha" }, { "Maroon", "Bordô" }, { "Cream", "Creme" },
    { "Orchid", "Orquídea" }, { "Tangerine", "Tangerina" }, { "Azure", "Azul-Celeste" }, { NULL, NULL } };
static const char *const FINISH[][2] = {   /* a guitar's finish (and its line, where the line repeats it) */
    { "Cherry", "Cereja" }, { "Trans Black", "Preto Translúcido" }, { "Vintage Sunburst", "Sunburst Vintage" }, { "Worn Brown", "Marrom Desgastado" }, { "Worn Red", "Vermelho Desgastado" },
    { "Ebony", "Ébano" }, { "Alpine White", "Branco Alpino" }, { "Creamy", "Creme" }, { "Blue Mist", "Azul Névoa" }, { "Sparkle", "Cintilante" },
    { "TV Yellow", "Amarelo TV" }, { "Camo", "Camuflada" }, { "Blue", "Azul" }, { "Cherry Red", "Vermelho Cereja" }, { "Black", "Preta" }, { "White", "Branca" },
    { "Wine Red", "Vermelho Vinho" }, { "Midnight Black", "Preto Meia-Noite" }, { "Candy Apple Red", "Vermelho Maçã do Amor" }, { "Red", "Vermelha" },
    { "Deity Red", "Vermelho Divino" }, { "Abundantly Blue", "Azul Abundante" }, { "Inevitably Dark", "Inevitavelmente Escura" }, { "Stars 'n' Stripes", "Estrelas e Listras" },
    { "Red on White", "Vermelho no Branco" }, { "Fortune", "Fortuna" }, { "Fury", "Fúria" }, { "Special", "Especial" }, { NULL, NULL } };
static const char *const LINE[][2] = {     /* its line, the special guitars' nicknames among them (Gibson's own, "Special", "Standard", stay) */
    { "Silver Sparkle", "Prata Cintilante" }, { "Aluminum Neck", "Braço de Alumínio" }, { "Apollo", "Apolo" }, { "Athena", "Atena" }, { "The Patriot", "O Patriota" }, { "Pirates", "Piratas" }, { "Messenger", "Mensageiro" },
    { "Risk Assessment", "Análise de Risco" }, { "Slash's", "do Slash" }, { "Lou's", "do Lou" }, { "Tom Morello's", "do Tom Morello" }, { "Scythe", "Foice" }, { NULL, NULL } };
static const char *const MAKER[][2] = { { "Special", "Especial" }, { NULL, NULL } };   /* its maker: the special guitars have none */

const char *charsel_pt_rocker(const char *name) { for (size_t i = 0; name && i < sizeof ROCKER / sizeof *ROCKER; i++) if (!strcasecmp(ROCKER[i].name, name)) return ROCKER[i].text; return NULL; }
const char *charsel_pt_guitar(uint32_t id) { for (size_t i = 0; i < sizeof GUITAR / sizeof *GUITAR; i++) if (GUITAR[i].id == id) return GUITAR[i].text; return NULL; }
const char *charsel_pt_name(int kind, const char *en) {
    const char *const (*t)[2] = kind == CS_PT_OUTFIT ? OUTFIT : kind == CS_PT_STYLE ? STYLE : kind == CS_PT_FINISH ? FINISH : kind == CS_PT_LINE ? LINE : MAKER;
    for (int i = 0; en && t[i][0]; i++) if (!strcasecmp(t[i][0], en)) return t[i][1]; return en;
}
