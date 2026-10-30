/* The tutorials of the game, rebuilt from their scripts in qb.pak (four files of steps, each a list of
 * { lesson, script } entries): the on-screen texts (lesson title, instructions, task), the narration clips of
 * STREAMS/streamall in their order and waits, when the highway freezes while the narrator talks, the practice
 * chart of each lesson (SONGS/tutorial_<id>, played on Easy, never failing), the goal that ends it and the feedback
 * lines (the 1st and 4th note hit cheer, the 3rd and 6th miss scold, the 2nd dropped long note brings a hint…).
 * The narration exists in English, French, German, Italian and Spanish ("<lang>_<clip>", English by default); the
 * captions are short Portuguese summaries of what each line teaches, not translations of the audio. Lessons 1A, 1F,
 * 4A, 4B and 4D have no chart: free strums on the frets (1A), the rock meter shown moving (1F), fingering drills
 * timed like the game's (4B, 4D). The Guitar Battle lessons use the charts' battle power phrases (StarBattleMode);
 * an opponent's attacks are not simulated, so 3C asks for notes and 3D for one power used. */
#include "tutorial.h"
#include "app.h"
#include "render.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

enum { OP_END, OP_TITLE, OP_INFO, OP_TASK, OP_LOAD, OP_WAIT, OP_FREEZE, OP_RESUME, OP_SAY, OP_SAYW, OP_WAITSAY, OP_GOAL, OP_BIG, OP_NOBIG, OP_DONE, OP_UNLOAD, OP_METER, OP_GIVESP, OP_WIDGET };
enum { G_NONE, G_TUNE, G_HITS, G_LONG, G_PHRASE, G_SP, G_SPHITS, G_HOPO, G_DEPLOY, G_DRILL_HAMMER, G_DRILL_PULL };
enum { W_NONE, W_FRETS, W_DRILL, W_METER };
typedef struct { int op, a, b; const char *s, *t; } op_t;
#define TITLE(x) { OP_TITLE, 0, 0, x, NULL }
#define INFO(x) { OP_INFO, 0, 0, x, NULL }
#define TASK(x) { OP_TASK, 0, 0, x, NULL }
#define LOAD(x, battle) { OP_LOAD, battle, 0, x, NULL }
#define WAIT(ms) { OP_WAIT, ms, 0, NULL, NULL }
#define FREEZE { OP_FREEZE, 0, 0, NULL, NULL }
#define RESUME { OP_RESUME, 0, 0, NULL, NULL }
#define SAY(clip, txt) { OP_SAY, 0, 0, clip, txt }
#define SAYW(clip, txt) { OP_SAYW, 0, 0, clip, txt }
#define WAITSAY { OP_WAITSAY, 0, 0, NULL, NULL }
#define GOAL(kind, n, lou) { OP_GOAL, kind, n, lou ? "lou" : "god", NULL }
#define BIG(x) { OP_BIG, 0, 0, x, NULL }
#define NOBIG { OP_NOBIG, 0, 0, NULL, NULL }
#define DONE { OP_DONE, 0, 0, NULL, NULL }
#define UNLOAD { OP_UNLOAD, 0, 0, NULL, NULL }
#define METER(v) { OP_METER, v, 0, NULL, NULL }
#define GIVESP(v) { OP_GIVESP, v, 0, NULL, NULL }
#define WIDGET(w) { OP_WIDGET, w, 0, NULL, NULL }
#define END { OP_END, 0, 0, NULL, NULL }
#define COMPLETE WAIT(750), UNLOAD, BIG("Lição concluída")    /* after a goal: the song stops, "Lesson Complete" */
#define CHEER "Comenta a jogada e passa para a próxima lição."

/* ---- 1: Guitar Hero Basics ---- */
static const op_t T1_INTRO[] = { BIG("Tutorial básico"), SAYW("Tutorial_1_Intro_01_God", "Boas-vindas: aqui você aprende a tocar a guitarra do Guitar Hero, das primeiras notas até a tela do jogo."), NOBIG, END };
static const op_t L1A[] = { WIDGET(W_FRETS), SAYW("Tutorial_1_Intro_02_God", "Os cinco botões de traste (verde, vermelho, amarelo, azul e laranja) escolhem a nota; a palheta, para cima ou para baixo, faz a nota soar."),
    TITLE("LIÇÃO 1: AFINANDO A GUITARRA"), INFO("1. SEGURE o traste para ESCOLHER a nota\n2. APERTE a palheta para cima ou para baixo para TOCAR a nota"), TASK("Toque cada nota 3 vezes para continuar"),
    SAYW("Tutorial_1_Intro_03_god", "Segure um traste e palhete."), SAY("tutorial_1a_01_god", "Afine a guitarra: toque cada um dos cinco trastes três vezes, um de cada vez."),
    GOAL(G_TUNE, 15, 0), BIG("Lição concluída"), WAIT(2000), NOBIG, WIDGET(W_NONE), DONE, END };
static const op_t L1B[] = { LOAD("tutorial_1b", 0), TITLE("LIÇÃO 2: TOCANDO NOTAS"), INFO("1. Quando as notas cruzarem a linha, toque-as na guitarra"), WAIT(3600), FREEZE,
    SAYW("Tutorial_1B_01_God", "As notas descem pela pista. Quando uma nota chega à linha dos alvos, segure o traste da mesma cor e palhete; a sequência e os pontos aparecem embaixo."),
    TASK("Toque 8 notas para continuar"), SAY("Tutorial_1B_02_God", "Agora é com você: toque as notas."), WAIT(2000), RESUME, GOAL(G_HITS, 8, 0),
    COMPLETE, SAYW("Tutorial_1B_03_God", CHEER), NOBIG, DONE, END };
static const op_t L1C[] = { LOAD("tutorial_1c", 0), TITLE("LIÇÃO 3: NOTAS DIFERENTES"), WAIT(1000), FREEZE, INFO("1. Combine as cores para tocar notas diferentes"),
    SAYW("Tutorial_1C_01_God", "Agora as notas mudam de cor: aperte o traste da cor de cada nota antes de palhetar."), TASK("Toque notas diferentes: acerte 8 para continuar"), WAIT(2000), RESUME, GOAL(G_HITS, 8, 0),
    COMPLETE, SAYW("Tutorial_1C_02_God", CHEER), NOBIG, DONE, END };
static const op_t L1D[] = { LOAD("tutorial_1d", 0), TITLE("LIÇÃO 4: NOTAS LONGAS"), WAIT(3550), FREEZE, INFO("1. Segure o traste\n2. Palhete\n3. Continue segurando até a nota inteira passar"),
    SAYW("Tutorial_1D_01_God", "Notas longas têm um rastro: palhete e mantenha o traste apertado até o fim do rastro para ganhar mais pontos."), TASK("Toque 8 notas longas para continuar"), WAIT(2000), RESUME, GOAL(G_LONG, 8, 0),
    COMPLETE, SAYW("Tutorial_1D_03_God", CHEER), NOBIG, DONE, END };
static const op_t L1E[] = { LOAD("tutorial_1e", 0), TITLE("LIÇÃO 5: ACORDES"), WAIT(3500), FREEZE, INFO("Duas notas tocadas ao mesmo tempo"),
    SAYW("Tutorial_1E_01_lou", "Acordes: duas notas juntas. Segure os dois trastes ao mesmo tempo e palhete uma vez só."), TASK("Toque 8 acordes para continuar"), WAIT(2000), RESUME, GOAL(G_HITS, 8, 1),
    COMPLETE, WAIT(3000), NOBIG, DONE, END };
static const op_t L1F[] = { TITLE("LIÇÃO 6: A TELA DO JOGO"), SAY("Tutorial_1F_01_god", "O medidor de rock mostra como a plateia vê você: ele sobe quando você acerta e desce quando erra. Se ficar no vermelho, a música acaba. Cada nota tocada soma pontos."),
    WAIT(6000), METER(100), WAIT(9500), INFO("Amarelo = desempenho razoável"), WAIT(5500), METER(160), INFO("Amarelo = desempenho razoável\nVerde = bom desempenho"), WAIT(5000), METER(40),
    INFO("Amarelo = desempenho razoável\nVerde = bom desempenho\nVermelho = mau desempenho"), WAIT(6500), METER(0), INFO("Amarelo = desempenho razoável\nVerde = bom desempenho\nVermelho = mau desempenho\nVermelho piscando = perigo!"),
    WAIT(6500), METER(100), INFO("Amarelo = desempenho razoável\nVerde = bom desempenho\nVermelho = mau desempenho\nVermelho piscando = perigo!\nTocar notas = ganhar pontos"), WAITSAY, WIDGET(W_NONE), DONE, END };
static const op_t T1_OUTRO[] = { BIG("Tutorial básico concluído!"), SAYW("Tutorial_1_outro", "Parabéns, você terminou o básico. Os outros tutoriais ensinam Star Power, batalhas e hammer-ons."), NOBIG, END };
/* ---- 2: Star Power ---- */
static const op_t T2_INTRO[] = { BIG("Tutorial de Star Power"), WAIT(2000), NOBIG, END };
static const op_t L2A[] = { LOAD("tutorial_2b", 0), TITLE("LIÇÃO 1: COMBOS DE ESTRELA"), INFO("Acerte todas as notas de um combo de estrela para ganhar Star Power"), WAIT(3500), FREEZE,
    SAYW("Tutorial_2A_01_lou", "As notas em forma de estrela formam um combo: acerte todas, sem errar nenhuma, e o medidor de Star Power enche um pouco."), TASK("Acerte um combo de estrela inteiro para continuar"), WAIT(2000), RESUME, GOAL(G_PHRASE, 1, 0),
    COMPLETE, WAIT(2000), NOBIG, DONE, END };
static const op_t L2B[] = { LOAD("tutorial_2b", 0), TITLE("LIÇÃO 2: ALAVANCA"), WAIT(1000), FREEZE,
    SAYW("Tutorial_2B_01_lou", "A alavanca: depois de tocar uma nota longa de estrela, mexa a alavanca para dentro e para fora enquanto segura a nota, e o medidor de Star Power enche mais."),
    INFO("Depois de tocar a nota, mexa a alavanca para dentro e para fora para extrair Star Power"), TASK("Encha o medidor de Star Power para continuar"), SAYW("Tutorial_2B_02_lou", "Agora encha o medidor."), RESUME, GOAL(G_SP, 1, 1),
    COMPLETE, WAIT(500), SAYW("Tutorial_2B_04_Lou", CHEER), NOBIG, DONE, END };
static const op_t L2C[] = { LOAD("tutorial_2c", 0), TITLE("LIÇÃO 3: INCLINE PARA O STAR POWER"), WAIT(2000), FREEZE,
    SAYW("Tutorial_2C_01_lou", "Com o medidor pela metade, incline a guitarra para cima e solte o Star Power: os pontos dobram e o medidor de rock sobe mais rápido. Aqui, use o botão de Star Power."),
    INFO("Incline a guitarra para ativar o Star Power (aqui, o botão de Star Power).\nDepois de ativado, pode baixá-la"), TASK("Ative o Star Power e acerte 8 notas para continuar"), GIVESP(100), RESUME, GOAL(G_SPHITS, 8, 0),
    WAIT(1000), UNLOAD, BIG("Tutorial de Star Power concluído!"), SAYW("Tutorial_2C_02_god", "Você aprendeu a ganhar e a usar o Star Power. Use bem: ele salva a música quando o medidor de rock está baixo."), NOBIG, DONE, END };
static const op_t T2_OUTRO[] = { END };
/* ---- 3: Guitar Battle ---- */
static const op_t T3_INTRO[] = { BIG("Tutorial de batalha"), SAYW("Tutorial_3_Intro_01_God", "Nas batalhas de guitarra você enfrenta outro guitarrista: em vez de Star Power, você ganha poderes para atacá-lo."), NOBIG, END };
static const op_t L3A[] = { LOAD("tutorial_3a", 1), TITLE("LIÇÃO 1: GANHANDO PODERES"), INFO("1. Complete uma sequência de poder de batalha para ganhar um poder"), WAIT(1000), FREEZE,
    SAYW("Tutorial_3A_01_God", "As sequências de poder aparecem como notas de estrela: acerte uma sequência inteira para ganhar um poder de ataque."), TASK("Acerte uma sequência de estrela para continuar"), WAIT(2000), RESUME, GOAL(G_PHRASE, 1, 0),
    BIG("Lição concluída"), WAIT(2000), NOBIG, DONE, END };
static const op_t L3B[] = { LOAD("tutorial_3a", 1), TITLE("LIÇÃO 2: USANDO PODERES"), WAIT(1000), FREEZE,
    SAYW("Tutorial_3B_01_Lou", "Com um poder guardado, incline a guitarra para cima para lançá-lo no adversário. Aqui, use o botão de Star Power."),
    TASK("Incline a guitarra (botão de Star Power) para lançar o ataque"), GIVESP(50), WAIT(2000), RESUME, GOAL(G_DEPLOY, 1, 0), COMPLETE, WAIT(2000), NOBIG, DONE, END };
static const op_t L3C[] = { LOAD("tutorial_3c", 1), TITLE("LIÇÃO 3: ATAQUES DIFERENTES"), WAIT(2000), FREEZE,
    SAYW("Tutorial_3C_01_God", "Cada poder é um ataque diferente. Quando o adversário quebra uma das suas cordas, aperte várias vezes o traste da corda quebrada para consertá-la."),
    INFO("No jogo: aperte rápido o traste da corda quebrada.\n(Os ataques do adversário não são simulados aqui.)"), TASK("Acerte 8 notas para continuar"), WAIT(1000), RESUME, GOAL(G_HITS, 8, 0),
    COMPLETE, WAIT(2000), NOBIG, DONE, END };
static const op_t L3D[] = { LOAD("tutorial_3d", 1), TITLE("LIÇÃO 4: VÁRIOS ATAQUES"), WAIT(1000), FREEZE,
    SAYW("Tutorial_3D_01_Lou", "Junte poderes e use os ataques um atrás do outro para causar mais dano ao adversário."),
    TASK("Ganhe um poder e lance o ataque (botão de Star Power)"), WAIT(2000), RESUME, GOAL(G_DEPLOY, 1, 1), DONE, END };
static const op_t T3_OUTRO[] = { WAIT(750), UNLOAD, BIG("Tutorial de batalha concluído!"), SAYW("Tutorial_3_outro", "Agora você sabe ganhar e usar os poderes: está pronto para os duelos da carreira."), NOBIG, END };
/* ---- 4: Hammer-ons and pull-offs ---- */
static const op_t T4_INTRO[] = { BIG("Hammer-ons e pull-offs"), WAIT(3000), NOBIG, END };
static const op_t L4A[] = { TITLE("LIÇÃO 1: NOÇÕES DAS CORDAS"), WIDGET(W_FRETS),
    SAYW("Tutorial_4A_01_God", "Hammer-ons e pull-offs tocam notas sem palhetar, só com os dedos nos trastes. As notas que permitem isso têm um brilho a mais na pista."), WIDGET(W_NONE), DONE, END };
static const op_t L4B[] = { TITLE("LIÇÃO 2: DEDILHADO DO HAMMER-ON"), WIDGET(W_DRILL),
    SAYW("Tutorial_4B_01_God", "Hammer-on: palhete o verde e, com ele ainda soando, aperte o vermelho e depois o amarelo sem palhetar de novo."),
    INFO("1. Toque o verde normalmente (palhetando)\n2. Hammer-on no vermelho (sem palhetar)\n3. Hammer-on no amarelo (sem palhetar)"), TASK("Faça 3 sequências de hammer-on para continuar"),
    SAY("Tutorial_4B_02_God", "Agora faça você."), GOAL(G_DRILL_HAMMER, 3, 0), BIG("Lição concluída"), SAYW("Tutorial_4B_03_God", CHEER), NOBIG, WIDGET(W_NONE), DONE, END };
static const op_t L4C[] = { LOAD("tutorial_4c", 0), TITLE("LIÇÃO 3: HAMMER-ONS"), INFO("1. Pratique os hammer-ons"), WAIT(1000), FREEZE,
    SAYW("Tutorial_4c_01_God", "Na música: palhete a primeira nota e toque as seguintes só apertando os trastes."), TASK("Acerte 8 notas com hammer-on para continuar"), WAIT(1000), RESUME, GOAL(G_HOPO, 8, 0),
    COMPLETE, SAYW("Tutorial_4B_03_God", CHEER), NOBIG, DONE, END };
static const op_t L4D[] = { TITLE("LIÇÃO 4: DEDILHADO DO PULL-OFF"), WIDGET(W_DRILL),
    SAYW("Tutorial_4d_01_Lou", "Pull-off: segure verde, vermelho e amarelo, palhete e solte o amarelo para soar o vermelho; depois solte o vermelho para soar o verde."),
    INFO("1. Toque o amarelo normalmente (palhetando)\n2. Pull-off para o vermelho (sem palhetar)\n3. Pull-off para o verde (sem palhetar)"), TASK("Faça 3 sequências de pull-off para continuar"),
    GOAL(G_DRILL_PULL, 3, 1), BIG("Lição concluída"), SAYW("Tutorial_4D_02_Lou", CHEER), NOBIG, WIDGET(W_NONE), DONE, END };
static const op_t L4E[] = { LOAD("tutorial_4e", 0), TITLE("LIÇÃO 5: PULL-OFFS"), INFO("1. Pratique os pull-offs"), WAIT(1000), FREEZE, TASK("Acerte 8 notas com pull-off para continuar"),
    SAYW("Tutorial_4e_01_Lou", "Agora os pull-offs na música."), RESUME, GOAL(G_HOPO, 8, 1), COMPLETE, SAYW("Tutorial_4e_02_Lou", CHEER), NOBIG, DONE, END };
static const op_t T4_OUTRO[] = { BIG("Técnicas avançadas concluídas!"), SAYW("Tutorial_4_Outro", "Fim dos tutoriais de técnica: misture hammer-ons e pull-offs para tocar os solos rápidos."), NOBIG, END };

typedef struct { const char *id, *name; const op_t *ops; } lesson_t;
typedef struct { const char *name, *desc; const op_t *intro; const lesson_t *ls; int n; const op_t *outro; } tut_t;
static const lesson_t LS1[] = { { "1a", "Afinando a guitarra", L1A }, { "1b", "Tocando notas", L1B }, { "1c", "Notas diferentes", L1C }, { "1d", "Notas longas", L1D }, { "1e", "Acordes", L1E }, { "1f", "A tela do jogo", L1F } };
static const lesson_t LS2[] = { { "2a", "Combos de estrela", L2A }, { "2b", "Alavanca", L2B }, { "2c", "Incline para o Star Power", L2C } };
static const lesson_t LS3[] = { { "3a", "Ganhando poderes", L3A }, { "3b", "Usando poderes", L3B }, { "3c", "Ataques diferentes", L3C }, { "3d", "Vários ataques", L3D } };
static const lesson_t LS4[] = { { "4a", "Noções das cordas", L4A }, { "4b", "Dedilhado do hammer-on", L4B }, { "4c", "Hammer-ons", L4C }, { "4d", "Dedilhado do pull-off", L4D }, { "4e", "Pull-offs", L4E } };
#define TUT_N 4
static const tut_t TUT[TUT_N] = {
    { "Tutorial básico", "Com o Deus do Rock: trastes e palhetada, notas, notas longas, acordes e a tela do jogo.", T1_INTRO, LS1, 6, T1_OUTRO },
    { "Star Power", "Com Lou: combos de estrela, a alavanca e como soltar o Star Power.", T2_INTRO, LS2, 3, T2_OUTRO },
    { "Batalha de guitarra", "Poderes de batalha e como lançá-los. Os ataques do adversário não são simulados neste port.", T3_INTRO, LS3, 4, T3_OUTRO },
    { "Hammer-ons e pull-offs", "Técnicas avançadas: notas tocadas sem palhetar, primeiro no dedilhado, depois na música.", T4_INTRO, LS4, 5, T4_OUTRO } };
#define LANG_N 5
static const char *LANG[LANG_N] = { "en", "fr", "gr", "it", "sp" }, *LANG_PT[LANG_N] = { "Inglês", "Francês", "Alemão", "Italiano", "Espanhol" };
#define LEAD 1000.0

static struct {
    int run, t, seg, ip, opBegun; double opAt, lastNow;
    char title[96], info[300], task[160], big[96]; int widget;
    int loaded, frozen, battle; char chart[32]; double fakeSong; int headless, bot;
    uint8_t *clip; double clipEnd, capUntil; int capNote; char capWho[24], cap[400];
    int goal, need, lou, count, lastHits, lastMisses, misses, lastPhr, lastHopo, drops, help, spArmed; double goalAt; unsigned char *seen; int nseen;
    int held, tuned[5], tunedN, drill; double drillAt, strumAt, flash[5], botAt;
    double meter, meterTarget;
    int paused, pauseRow; double pausedAt, againUntil;
    int lang, cursor, scroll; unsigned char done[TUT_N][8]; char msg[120]; double msgAt;
} T;

/* ---- saved state: the lessons done and the narration language, next to gh3.cfg ---- */
static void path(char *out, size_t n) { snprintf(out, n, "%s/gh3-tutoriais.cfg", settings_home()); }
static void save(void) {
    char p[600]; path(p, sizeof p); FILE *f = fopen(p, "w"); if (!f) return;
    fprintf(f, "# tutoriais do Guitar Hero III: idioma da narracao e licoes concluidas\nidioma=%s\n", LANG[T.lang]);
    for (int t = 0; t < TUT_N; t++) for (int l = 0; l < TUT[t].n; l++) if (T.done[t][l]) fprintf(f, "%s=1\n", TUT[t].ls[l].id);
    fclose(f);
}
static void load(void) {
    char p[600], line[120]; path(p, sizeof p); FILE *f = fopen(p, "r"); if (!f) return;
    while (fgets(line, sizeof line, f)) { char *eq = strchr(line, '='); if (!eq || line[0] == '#') continue; *eq = 0; char *v = eq + 1; v[strcspn(v, "\r\n")] = 0;
        if (!strcmp(line, "idioma")) { for (int i = 0; i < LANG_N; i++) if (!strcmp(v, LANG[i])) T.lang = i; continue; }
        for (int t = 0; t < TUT_N; t++) for (int l = 0; l < TUT[t].n; l++) if (!strcmp(line, TUT[t].ls[l].id)) T.done[t][l] = atoi(v) != 0; }
    fclose(f);
}

/* ---- narration ---- */
static void say(const char *sound, const char *text, double now, int note) {
    char nm[64]; snprintf(nm, sizeof nm, "%s_%s.mp3", LANG[T.lang], sound); for (char *c = nm; *c; c++) *c = (char)tolower((unsigned char)*c);
    nm[30] = 0;   /* the bank keeps 30 characters of a name: "en_tutorial_god_positive_01.mp" */
    size_t n = 0; uint8_t *b = app_stream_load(nm, &n);
    audio_voice(b, n, 0); free(T.clip); T.clip = b;   /* the mixer is done with the old clip once audio_voice returns */
    double dur = b ? n / 16.0 : 0;   /* the clips are 128 kbit/s MP3 */
    if (!b) dur = text ? 900 + strlen(text) * 45.0 : 1500;   /* no clip (no streamall): the time to read the caption */
    T.clipEnd = now + dur; T.capUntil = T.clipEnd + 1200; T.capNote = note;
    snprintf(T.capWho, sizeof T.capWho, "%s", strstr(nm, "_lou") ? "Lou" : "Deus do Rock"); snprintf(T.cap, sizeof T.cap, "%s", text ? text : "");
    if (T.headless) printf("%8.2f s  fala %-30s %s(%.1f s)\n", now / 1000, nm, b ? "" : "[sem o clipe] ", dur / 1000);
}
static int speaking(double now) { return audio_has_device() && T.clip ? audio_voice_on() : now < T.clipEnd; }
static void hush(double now) { audio_voice(NULL, 0, 0); free(T.clip); T.clip = NULL; T.clipEnd = now; if (T.capUntil > now) T.capUntil = now; }
static void feedback(int good, int lou, int pick, double now) {   /* the cheering and scolding lines; they never cut the narration */
    if (speaking(now)) return; char nm[48]; snprintf(nm, sizeof nm, "Tutorial_%s_%s_%02d", lou ? "Lou" : "God", good ? "Positive" : "Negative", pick);
    say(nm, good ? "Elogia a jogada." : "Reclama dos erros.", now, 1);
}
static void cheer(int lou, double now) { feedback(1, lou, rand() % 10 < 5 ? 1 : 2, now); }   /* the 1st and the 4th: Positive_01 or _02, half and half */
static void scold(int lou, double now) { feedback(0, lou, lou ? 1 + rand() % 2 : 1 + rand() % 6, now); }

/* ---- the chart of a lesson ---- */
static double song_clock(void) { return T.headless ? T.fakeSong : audio_song_ms() - SET.offsetMs; }
static void battle_phrases(const char *name) {   /* the battle power phrases (<song>_easy_starbattlemode) in place of the star ones */
    char nm[96], key[96]; size_t len; snprintf(nm, sizeof nm, "%s_song.pak.xen", name); uint8_t *pak = app_read_data("songs", nm, &len); if (!pak) return;
    snprintf(key, sizeof key, "%s_easy_starbattlemode", name); for (char *c = key; *c; c++) *c = (char)tolower((unsigned char)*c);
    uint32_t k = gh3_qbkey(key), KQB = gh3_qbkey(".qb"); gh3_pakent *ents; int ne = gh3_pak_parse(pak, len, NULL, 0, &ents); gh3_chart *c = &A.S.chart;
    for (int e = 0; e < ne; e++) { if (ents[e].type != KQB || !ents[e].data) continue; qbsection *s; int ns = gh3_qb_parse(ents[e].data, ents[e].len, &s); if (ns < 0) continue;
        for (int i = 0; i < ns; i++) { const qbv *a = s[i].v; if (s[i].id != k || !a || a->kind != QB_ARRAY || !a->narr) continue;
            free(c->phrases); c->phrases = calloc(a->narr + 1, sizeof *c->phrases); c->nphrases = 0; for (int j = 0; j < c->nnotes; j++) c->notes[j].star = -1;
            for (int p = 0; p < a->narr; p++) { const qbv *ph = a->arr[p]; if (!ph || ph->kind != QB_ARRAY || ph->narr < 2) continue; int st = (int)qb_num(ph->arr[0], 0), sl = (int)qb_num(ph->arr[1], 0);
                gh3_phrase *g = &c->phrases[c->nphrases]; g->t = st; g->end = st + sl; for (int j = 0; j < c->nnotes; j++) if (c->notes[j].t >= st && c->notes[j].t < st + sl) { c->notes[j].star = c->nphrases; g->total++; }
                c->nphrases++; } }
        gh3_qb_free_sections(s, ns); }
    free(ents); free(pak);
    if (T.headless) printf("          %s: %d frases de poder de batalha\n", name, c->nphrases);
}
static void unload(void) { if (T.loaded) app_session_free(); T.loaded = 0; audio_set_bend(0); }
static int load_chart(const char *name, int battle, double now) {
    char err[300]; unload();
    if (app_load_song_named(name, 0, T.bot, !T.headless, err, sizeof err)) { snprintf(T.msg, sizeof T.msg, "%s", err); T.msgAt = now; SDL_Log("tutorial: %s", err); return -1; }
    A.S.run.noFail = 1; if (battle) battle_phrases(name);
    T.loaded = 1; T.battle = battle; snprintf(T.chart, sizeof T.chart, "%s", name); T.fakeSong = -LEAD;
    T.lastHits = T.lastMisses = T.lastPhr = T.lastHopo = 0; free(T.seen); T.nseen = A.S.chart.nnotes; T.seen = calloc(T.nseen + 1, 1);
    if (!T.headless) { app_apply_volumes(); audio_play(0, LEAD); if (T.frozen) audio_pause(1); }
    return 0;
}

/* ---- the goals ---- */
static int hopo_done(const gh3_run *r) { if (!r->autoPlay) return r->hammers + r->pulls; int n = 0; for (int i = 0; i < r->nnotes; i++) n += r->notes[i].hit && r->notes[i].hopo; return n; }   /* the bot never hammers: its hit HOPO notes count */
static int goal_met(void) {
    const gh3_run *r = &A.S.run;
    switch (T.goal) { case G_SP: return T.loaded && (r->sp >= 0.5 || r->spOn); case G_DEPLOY: return T.loaded && r->spOn; default: return T.count >= T.need; }
}
static void goal_track(double now) {   /* what the run did since the last frame, and the lines the game plays for it */
    if (!T.loaded || T.goal == G_NONE) return; gh3_run *r = &A.S.run;
    int dh = r->hits - T.lastHits, dm = r->misses - T.lastMisses; T.lastHits = r->hits; T.lastMisses = r->misses;
    int phr = 0; for (int i = 0; i < r->chart->nphrases; i++) phr += r->chart->phrases[i].done; int dp = phr - T.lastPhr; T.lastPhr = phr;
    int ho = hopo_done(r), dho = ho - T.lastHopo; T.lastHopo = ho;
    if (T.battle && dp > 0 && !r->spOn && r->sp < 0.5) r->sp = 0.5;   /* a battle power sequence hands a power over */
    switch (T.goal) {
    case G_HITS: for (int i = 0; i < dh; i++) { T.count++; if (T.count == 1 || T.count == 4) cheer(T.lou, now); }
        for (int i = 0; i < dm; i++) { T.misses++; if (T.misses == 3 || T.misses == 6) scold(T.lou, now); } break;
    case G_SPHITS: if (r->spOn) { T.spArmed = 1; for (int i = 0; i < dh; i++) { T.count++; if (T.count == 1 || T.count == 4) cheer(0, now); } }
        else if (T.spArmed && r->sp < 0.5) { r->sp = 1; T.spArmed = 0; } break;   /* ran out before the 8 notes: the game fills it again */
    case G_LONG: for (int i = 0; i < r->nnotes && i < T.nseen; i++) { const gh3_note *n = &r->notes[i]; if (T.seen[i] || !n->sus) continue;
            if (n->dropped) { T.seen[i] = 2; if (++T.drops == 2) say("Tutorial_1D_02_God", "Não solte o traste antes do fim da nota longa: segure até o rastro acabar.", now, 0); }
            else if (n->hit && r->now >= n->t + n->sus) { static const int P[6] = { 1, 2, 3, 5, 6, 7 }; T.seen[i] = 1; T.count++; if (T.count == 1 || T.count == 4) feedback(1, 0, P[rand() % 6], now); } } break;
    case G_PHRASE: T.count += dp; break;
    case G_HOPO: if (dho > 0 && T.count == 0) feedback(1, T.lou, T.lou ? 5 : 7, now); T.count += dho; break;
    case G_SP: if (!T.help && !T.frozen && now - T.goalAt > 20000 && r->sp < 0.25) { T.help = 1; say("Tutorial_2B_03_lou", "Mexa a alavanca durante as notas longas de estrela para encher o medidor.", now, 0); } break; }
}
/* 1A: each fret strummed alone three times */
static void tune_strum(int held, double now) {
    int f = -1, n = 0; for (int k = 0; k < 5; k++) if (held >> k & 1) { f = k; n++; }
    if (n != 1 || T.tuned[f] >= 3) return; T.tuned[f]++; T.count++; T.flash[f] = now;
    if (T.tuned[f] == 3) { T.tunedN++; if (T.tunedN == 1 || T.tunedN == 4) cheer(0, now); }
}
/* 4B / 4D: the fingering drills, each step within a second (60 frames) as in the game */
static int top_fret(int held) { for (int f = 4; f >= 0; f--) if (held >> f & 1) return f; return -1; }
static void drill_ok(double now) {
    T.count++; T.drill = 0; for (int f = 0; f < 5; f++) T.flash[f] = now;
    if (T.count < T.need) { static const int GOD[5] = { 2, 4, 5, 6, 9 }, LOU[5] = { 2, 3, 4, 6, 7 }; feedback(1, T.lou, T.lou ? LOU[rand() % 5] : GOD[rand() % 5], now); }
}
static void drill_fail(void) { T.drill = 0; }
static void drill_strum(double now) {
    if (T.goal == G_DRILL_HAMMER) { T.drill = T.held & 1 ? 1 : 0; T.drillAt = now; }
    else if (T.goal == G_DRILL_PULL) { T.drill = top_fret(T.held) == 2 ? 1 : 0; T.drillAt = now; }
}
static void drill_frets(double now) {
    int h = T.held;
    if (T.goal == G_DRILL_HAMMER) {
        if (T.drill == 1) { if (h & 0x1C) drill_fail(); else if (h & 2) { T.drill = 2; T.drillAt = now; } }
        else if (T.drill == 2) { if (h & 0x18) drill_fail(); else if (h & 4) drill_ok(now); } }
    else if (T.goal == G_DRILL_PULL) { int top = top_fret(h);
        if (T.drill == 1 && top != 2) { if (top == 1) { T.drill = 2; T.drillAt = now; } else drill_fail(); }
        else if (T.drill == 2 && top != 1) { if (top == 0) drill_ok(now); else drill_fail(); } }
}

/* ---- running the steps ---- */
static const op_t *seg_ops(void) { const tut_t *tu = &TUT[T.t]; return T.seg < 0 ? tu->intro : T.seg < tu->n ? tu->ls[T.seg].ops : T.seg == tu->n ? tu->outro : NULL; }
static const char *OPN[] = { "fim", "título", "instrução", "tarefa", "partitura", "espera", "congela", "solta", "fala", "fala", "espera a fala", "objetivo", "letreiro", "-letreiro", "concluída", "para a música", "medidor", "star power", "quadro" };
static void finish(double now, const char *msg);
static void begin_op(const op_t *o, double now) {
    gh3_run *r = &A.S.run;
    if (T.headless && o->op != OP_SAY && o->op != OP_SAYW && o->op != OP_END) { printf("%8.2f s  %-14s %s", now / 1000, OPN[o->op], o->op == OP_GOAL ? "" : o->s ? o->s : ""); if (o->op == OP_GOAL || o->op == OP_WAIT) printf("%d", o->op == OP_GOAL ? o->b : o->a); printf("\n"); }
    switch (o->op) {
    case OP_TITLE: snprintf(T.title, sizeof T.title, "%s", o->s); T.info[0] = T.task[0] = 0; break;
    case OP_INFO: snprintf(T.info, sizeof T.info, "%s", o->s); break;
    case OP_TASK: snprintf(T.task, sizeof T.task, "%s", o->s); break;
    case OP_LOAD: if (load_chart(o->s, o->a, now)) finish(now, NULL); break;   /* no chart (SONGS/tutorial_*): back to the list, which shows why */
    case OP_FREEZE: T.frozen = 1; if (T.loaded && !T.headless) audio_pause(1); break;
    case OP_RESUME: T.frozen = 0; if (T.loaded) { if (!T.headless) audio_pause(0); game_set_frets(r, T.held, song_clock(), now); } break;
    case OP_SAY: case OP_SAYW: say(o->s, o->t, now, 0); break;
    case OP_GOAL: T.goal = o->a; T.need = o->b; T.lou = o->s && !strcmp(o->s, "lou"); T.count = T.misses = T.drops = T.help = T.spArmed = T.drill = T.tunedN = 0; memset(T.tuned, 0, sizeof T.tuned); T.goalAt = now; T.botAt = now;
        if (T.loaded) { T.lastHits = r->hits; T.lastMisses = r->misses; T.lastHopo = hopo_done(r); T.lastPhr = 0; for (int i = 0; i < r->chart->nphrases; i++) T.lastPhr += r->chart->phrases[i].done; }
        if (T.goal == G_PHRASE && T.loaded && !r->chart->nphrases) { T.goal = G_HITS; T.need = 8; snprintf(T.task, sizeof T.task, "Acerte 8 notas para continuar"); }   /* a chart without the phrases */
        if (T.goal == G_DEPLOY && T.loaded && !r->chart->nphrases && r->sp < 0.5) r->sp = 0.5;
        break;
    case OP_BIG: snprintf(T.big, sizeof T.big, "%s", o->s); break;
    case OP_NOBIG: T.big[0] = 0; break;
    case OP_DONE: if (T.seg >= 0 && T.seg < TUT[T.t].n) { T.done[T.t][T.seg] = 1; if (!T.headless) save(); } break;
    case OP_UNLOAD: unload(); T.frozen = 0; break;
    case OP_METER: if (T.widget != W_METER) { T.widget = W_METER; T.meter = 1; } T.meterTarget = o->a / 100.0; break;
    case OP_GIVESP: if (T.loaded && r->sp < o->a / 100.0) r->sp = o->a / 100.0; break;
    case OP_WIDGET: T.widget = o->a; if (o->a == W_FRETS || o->a == W_DRILL) memset(T.tuned, 0, sizeof T.tuned); break; }
}
static int op_done(const op_t *o, double now) {
    switch (o->op) { case OP_WAIT: return now - T.opAt >= o->a; case OP_SAYW: case OP_WAITSAY: return !speaking(now); case OP_GOAL: return goal_met(); default: return 1; }
}
static void finish(double now, const char *msg) {
    unload(); hush(now); T.frozen = 0; T.run = 0; T.goal = G_NONE; T.paused = 0; T.widget = W_NONE; T.big[0] = 0;
    if (msg) { snprintf(T.msg, sizeof T.msg, "%s", msg); T.msgAt = now; }
}
static void run_ops(double now) {
    for (int guard = 0; guard < 64 && T.run; guard++) {
        const op_t *ops = seg_ops(); if (!ops) { finish(now, "Tutorial concluído!"); if (!T.headless) audio_sfx(SFX_FANFARE); return; }
        const op_t *o = &ops[T.ip];
        if (!T.opBegun) { T.opBegun = 1; T.opAt = now; begin_op(o, now); }
        if (!op_done(o, now)) return;
        if (o->op == OP_GOAL) { if (T.headless) printf("%8.2f s  objetivo cumprido (%d/%d)\n", now / 1000, T.count, T.need); T.goal = G_NONE; }
        if (o->op == OP_END) { T.seg++; T.ip = 0; } else T.ip++;
        T.opBegun = 0;
    }
}
static void start(int t, int l, double now) {
    app_preview(-1); if (audio_preview_on()) audio_preview_stop(0);
    memset(T.title, 0, sizeof T.title); T.info[0] = T.task[0] = T.big[0] = 0; T.cap[0] = 0; T.capUntil = 0; T.widget = W_NONE; T.goal = G_NONE; T.held = 0;
    T.run = 1; T.t = t; T.seg = l < 0 ? -1 : l; T.ip = 0; T.opBegun = 0; T.paused = 0; T.frozen = 0; T.lastNow = now; T.againUntil = 0;
    if (!T.headless) T.bot = A.autoPlay;   /* gh3 --auto: the bot plays the lessons too */
    A.mode = MODE_PRACTICE; input_reset_axes(); input_action_buttons(ACT_STAR, A.starKeys, sizeof A.starKeys);
}
static void restart_lesson(double now) {
    unload(); hush(now); T.frozen = 0; T.goal = G_NONE; T.big[0] = 0; T.widget = W_NONE; T.ip = 0; T.opBegun = 0; T.title[0] = T.info[0] = T.task[0] = 0;
}
static void tick(double now) {
    if (!T.run) return; double dt = now - T.lastNow; if (dt < 0) dt = 0; if (dt > 100) dt = 100; T.lastNow = now;
    if (T.paused) return;
    gh3_run *r = &A.S.run;
    if (T.loaded && !T.frozen) { if (T.headless) T.fakeSong += dt; double st = song_clock(); game_step(r, st, now); audio_set_bend(r->sus && !r->autoPlay ? r->whammy : 0); }
    if (T.widget == W_METER) { double d = T.meterTarget - T.meter, s = 1.8 * dt / 1000; T.meter += fabs(d) < s ? d : d > 0 ? s : -s; }
    if (T.goal == G_TUNE && T.bot && now - T.botAt > 350) { T.botAt = now; for (int f = 0; f < 5; f++) if (T.tuned[f] < 3) { tune_strum(1 << f, now); break; } }
    if ((T.goal == G_DRILL_HAMMER || T.goal == G_DRILL_PULL) && T.bot && now - T.botAt > 1200) { T.botAt = now; drill_ok(now); }
    if (T.drill && now - T.drillAt > 1000) drill_fail();   /* too slow: the sequence starts over */
    goal_track(now);
    if (T.loaded && r->finished && T.goal != G_NONE && !goal_met()) {   /* the song ended first: once more, from the top (the count stays) */
        if (T.headless) printf("%8.2f s  a música acabou antes do objetivo (%d/%d): de novo\n", now / 1000, T.count, T.need);
        char nm[32]; snprintf(nm, sizeof nm, "%s", T.chart); scold(T.lou, now); load_chart(nm, T.battle, now); T.againUntil = now + 2500; }
    run_ops(now);
}

/* ---- drawing ---- */
static int text_h(int w, const char *s) { return gfx_text_box(-4000, -4000, w, 1, s, RGBA(0, 0, 0, 0)); }   /* measured off screen */
static void draw_frets(int cy, double now) {
    int W = A.W; gfx_sprite *tu = art_get(ART_TGT_UP), *td = art_get(ART_TGT_DOWN); int tw = gfx_sprite_w(tu), th = gfx_sprite_h(tu); char b[16];
    for (int f = 0; f < 5; f++) { int down = T.held >> f & 1; float x = W / 2 - 150 + f * 62, k = now - T.flash[f] < 300 ? (float)(1 - (now - T.flash[f]) / 300) : 0;
        if (k > 0) gfx_rect(x - 4, cy - 4, 64, 64.0f * th / tw + 8, fb_alpha(FRET_COLORS[f], 0.35f * k));
        gfx_blit(down ? td : tu, 0, 0, tw, th, x, cy, 56, 56.0f * th / tw, FRET_COLORS[f], 1);
        if (T.goal == G_TUNE || T.widget == W_FRETS) { if (T.tuned[f] >= 3) gfx_text_sh_center((int)x + 28, cy + 50, 2, "✓", C_GOOD, C_SHADOW); else if (T.goal == G_TUNE) { snprintf(b, sizeof b, "%d/3", T.tuned[f]); gfx_text_sh_center((int)x + 28, cy + 54, 1, b, C_INK, C_SHADOW); } } }
    if (now - T.strumAt < 160) gfx_text_sh_center(W / 2, cy - 26, 2, "♪", C_INK, C_SHADOW);
}
static void draw_drill(int cy, double now) {
    draw_frets(cy, now); int W = A.W, pull = T.goal == G_DRILL_PULL || (T.seg == 3 && T.t == 3);
    static const char *const HAM[3] = { "Verde: palhete", "Vermelho: sem palhetar", "Amarelo: sem palhetar" }, *const PUL[3] = { "Amarelo: palhete", "Solte: vermelho", "Solte: verde" };
    for (int i = 0; i < 3; i++) { const char *s = pull ? PUL[i] : HAM[i]; int x = W / 2 - 195 + i * 130, on = T.drill == i && T.goal != G_NONE, past = T.drill > i;
        ui_panel(x, cy + 70, 124, 26, on ? UI_PANEL_AMBER : UI_PANEL_FLAT); gfx_text_sh_center(x + 62, cy + 78, 1, s, past ? C_GOOD : on ? C_INK : C_MUTED, C_SHADOW); }
}
static void draw_meter(int cy, double now) {
    int W = A.W, x = W / 2 - 160, w = 320; double v = T.meter / 2; if (v < 0) v = 0; if (v > 1) v = 1;
    ui_panel(x - 14, cy - 30, w + 28, 76, UI_PANEL_DARK); gfx_text_sh(x, cy - 22, 1, "MEDIDOR DE ROCK", C_MUTED, C_SHADOW);
    gfx_rect(x, cy, w / 3, 18, C_BAD); gfx_rect(x + w / 3, cy, w / 3, 18, RGB(230, 200, 60)); gfx_rect(x + 2 * w / 3, cy, w - 2 * (w / 3), 18, C_GOOD);
    if (v < 0.12 && fmod(now, 500) < 250) gfx_rect(x - 4, cy - 4, w + 8, 26, fb_alpha(C_BAD, 0.5f));
    int nx = x + (int)(v * w); gfx_rect(nx - 2, cy - 6, 5, 30, C_INK);
}
static void draw_list(double now) {
    render_backdrop(now, SET.stageFx); int W = A.W, H = A.H; char b[160];
    snprintf(b, sizeof b, "narração: %s", LANG_PT[T.lang]); ui_header(W, "TUTORIAIS", b);
    int nrow = 1; for (int t = 0; t < TUT_N; t++) nrow += 1 + TUT[t].n;
    int rowH = 19, top = 66, vis = (H - top - 104) / rowH;
    if (T.cursor < T.scroll) T.scroll = T.cursor; if (T.cursor >= T.scroll + vis) T.scroll = T.cursor - vis + 1;
    const char *help = "";
    for (int i = 0, k = 0; k < nrow; k++) { int t = -1, l = -1, j = k - 1; if (k > 0) for (t = 0; t < TUT_N; t++) { if (j <= TUT[t].n) { l = j - 1; break; } j -= TUT[t].n + 1; }
        int sel = k == T.cursor; if (sel) help = k == 0 ? "O jogo não tem narração em português: escolha a língua das falas. As legendas em português resumem o que cada fala ensina (não são traduções)." : l < 0 ? TUT[t].desc : "Começa nesta lição e segue até o fim do tutorial.";
        if (k < T.scroll || k >= T.scroll + vis) continue; int y = top + (i++) * rowH;
        if (sel) { ui_panel(10, y - 4, W - 20, rowH, UI_PANEL_AMBER); gfx_sprite_at(art_get(ART_PICK), 16, y - 1, 0.45f, RGB(255, 255, 255), 1); }
        if (k == 0) { snprintf(b, sizeof b, sel ? "Idioma da narração:  ← %s →" : "Idioma da narração:  %s", LANG_PT[T.lang]); gfx_text_sh(34, y + 1, 1, b, sel ? C_INK : C_MUTED, C_SHADOW); continue; }
        if (l < 0) { int d = 0; for (int q = 0; q < TUT[t].n; q++) d += T.done[t][q]; snprintf(b, sizeof b, "%d. %s", t + 1, TUT[t].name);
            if (!sel) { gfx_rect(10, y - 2, W - 20, rowH - 3, RGBA(60, 40, 15, 120)); gfx_rect(10, y + rowH - 6, W - 20, 1, C_AMP_DARK); } gfx_text_sh(34, y + 1, 1, b, sel ? C_INK : C_AMP, C_SHADOW);
            snprintf(b, sizeof b, "%d/%d lições%s", d, TUT[t].n, d == TUT[t].n ? " ✓" : ""); gfx_text_sh_right(W - 22, y + 2, 1, b, d == TUT[t].n ? C_GOOD : C_MUTED, C_SHADOW); continue; }
        snprintf(b, sizeof b, "%s  %s", TUT[t].ls[l].id, TUT[t].ls[l].name); b[0] = (char)toupper((unsigned char)b[0]); b[1] = (char)toupper((unsigned char)b[1]);
        gfx_text_sh(58, y + 1, 1, b, sel ? C_INK : RGB(210, 204, 196), C_SHADOW);
        if (T.done[t][l]) gfx_text_sh_right(W - 22, y + 1, 1, "✓ concluída", C_GOOD, C_SHADOW); }
    ui_panel(14, H - 96, W - 28, 60, UI_PANEL_FLAT); gfx_text_box(26, H - 88, W - 52, 1, help, C_MUTED);
    static const char *bt[3] = { "A", "B", "← →" }, *tx[3] = { "Começar", "Voltar", "Idioma" }; ui_hints(H - 24, W, bt, tx, 3);
    if (now - T.msgAt < 3000 && T.msg[0]) { int w = gfx_text_w(T.msg, 2) + 40; ui_panel(W / 2 - w / 2, H / 2 - 24, w, 44, UI_PANEL_AMBER); gfx_text_sh_center(W / 2, H / 2 - 11, 2, T.msg, C_INK, C_SHADOW); }
}
static void draw_lesson(double now) {
    int W = A.W, H = A.H; char b[160]; const tut_t *tu = &TUT[T.t];
    if (T.loaded) { hud_info hi = { tu->name, "", "", 0, SET.lefty, SET.stageFx, SET.bgDim, 0, 0, A.starKeys }; render_game(&A.S.run, now, &hi); }
    else { render_backdrop(now, SET.stageFx); ui_dim(W, H, 110); gfx_text_sh(14, 10, 2, tu->name, C_INK, C_SHADOW); }
    if (T.widget == W_FRETS) draw_frets(212, now); else if (T.widget == W_DRILL) draw_drill(212, now); else if (T.widget == W_METER) draw_meter(226, now);   /* the lessons without a chart: under the boxes, over the caption */
    if (T.title[0]) { int w = 216, h = 26 + (T.info[0] ? text_h(w - 20, T.info) + 6 : 0); ui_panel(8, 44, w, h, UI_PANEL_DARK); gfx_text_sh(18, 52, 1, T.title, C_AMP, C_SHADOW); if (T.info[0]) gfx_text_box(18, 70, w - 20, 1, T.info, C_INK); }
    if (T.task[0] && T.goal != G_NONE) { int w = 212, x = W - w - 8, h = 34 + text_h(w - 20, T.task) + 16;
        ui_panel(x, 66, w, h, UI_PANEL_SP); gfx_text_sh(x + 10, 74, 1, "TAREFA", C_SP, C_SHADOW); int th = gfx_text_box(x + 10, 92, w - 20, 1, T.task, C_INK);
        const char *what = T.goal == G_TUNE ? "Notas afinadas" : T.goal == G_LONG ? "Notas longas" : T.goal == G_HOPO ? "Sem palhetar" : T.goal == G_SPHITS ? (A.S.run.spOn ? "Com Star Power" : "Ative o Star Power!") : T.goal == G_DRILL_HAMMER || T.goal == G_DRILL_PULL ? "Sequências" : T.goal == G_PHRASE ? "Combos" : T.goal == G_SP ? "Star Power" : T.goal == G_DEPLOY ? "Poder lançado" : "Notas acertadas";
        if (T.goal == G_SP) snprintf(b, sizeof b, "%s: %d%%", what, (int)(A.S.run.sp * 100 + 0.5)); else if (T.goal == G_DEPLOY) snprintf(b, sizeof b, "%s", A.S.run.sp >= 0.5 ? "Poder pronto: lance!" : "Ganhe um poder"); else snprintf(b, sizeof b, "%s: %d / %d", what, T.count, T.need);
        gfx_text_sh(x + 10, 96 + th, 1, b, C_AMP, C_SHADOW); }
    else if (T.task[0]) { int w = 212, x = W - w - 8, h = 30 + text_h(w - 20, T.task); ui_panel(x, 66, w, h, UI_PANEL_FLAT); gfx_text_sh(x + 10, 74, 1, "TAREFA", C_MUTED, C_SHADOW); gfx_text_box(x + 10, 92, w - 20, 1, T.task, C_MUTED); }
    if (T.big[0]) { int s = gfx_text_w(T.big, 3) > W - 60 ? 2 : 3, w = gfx_text_w(T.big, s) + 48; ui_panel(W / 2 - w / 2, 150, w, 16 * s + 22, UI_PANEL_AMBER); ui_title(W / 2 - gfx_text_w(T.big, s) / 2, 160, s, T.big, C_INK, C_SHADOW, 2); }
    if (now < T.againUntil) gfx_text_sh_center(W / 2, 214, 2, "A música acabou: de novo!", C_BAD, C_SHADOW);
    if (now < T.capUntil && T.cap[0]) { int side = T.capNote && T.loaded, w = side ? 212 : 440, x = side ? W - w - 8 : W / 2 - w / 2, th = text_h(w - 28, T.cap), h = th + (T.capNote ? 34 : 50), y = side ? 200 : T.loaded ? 236 : 322;   /* the cheering goes by the task, off the highway */
        if (y + h > H - 30) y = H - 30 - h;
        ui_panel(x, y, w, h, UI_PANEL_DARK); gfx_text_sh(x + 14, y + 8, 1, T.capWho, C_AMP, C_SHADOW); if (speaking(now)) gfx_text_sh(x + 14 + gfx_text_w(T.capWho, 1) + 8, y + 8, 1, "♪", C_AMP, C_SHADOW);
        gfx_text_box(x + 14, y + 24, w - 28, 1, T.cap, C_INK); if (!T.capNote) gfx_text_right(x + w - 14, y + h - 16, 1, "resumo da fala, não é tradução", C_LINE); }
    if (T.paused) { ui_dim(W, H, 170); ui_panel(W / 2 - 150, 120, 300, 230, UI_PANEL_DARK); ui_title(W / 2 - gfx_text_w("PAUSA", 4) / 2, 134, 4, "PAUSA", C_INK, C_SHADOW, 3);
        const char *it[4] = { "Continuar", "Pular a fala", "Recomeçar a lição", "Sair do tutorial" };
        for (int i = 0; i < 4; i++) { int y = 196 + i * 34, off = i == 1 && !speaking(now); if (i == T.pauseRow) { ui_panel(W / 2 - 130, y - 7, 260, 30, off ? UI_PANEL_DARK : UI_PANEL_AMBER); gfx_sprite_at(art_get(ART_PICK), W / 2 - 122, y - 3, 0.5f, RGB(255, 255, 255), 1); }
            gfx_text_sh_center(W / 2 + 6, y, 2, it[i], off ? RGB(110, 104, 122) : i == T.pauseRow ? C_INK : C_MUTED, C_SHADOW); } }
}
void tutorial_frame(double now) { tick(now); if (T.run) draw_lesson(now); else draw_list(now); }
int tutorial_busy(void) { return T.run; }

/* ---- input ---- */
static int list_rows(void) { int n = 1; for (int t = 0; t < TUT_N; t++) n += 1 + TUT[t].n; return n; }
static void pause_set(int on, double now) {
    if (on) { T.paused = 1; T.pauseRow = 0; T.pausedAt = now; if (T.loaded && !T.frozen && !T.headless) audio_pause(1); audio_sfx(SFX_BACK); return; }
    double d = now - T.pausedAt; T.paused = 0; T.opAt += d; T.clipEnd += d; T.capUntil += d; T.drillAt += d; T.goalAt += d; T.botAt += d; T.lastNow = now;
    if (T.loaded && !T.frozen && !T.headless) { audio_pause(0); input_reset_axes(); }
}
void tutorial_open(double now) { static int loaded; if (!loaded) { load(); loaded = 1; } T.run = 0; T.paused = 0; A.screen = SC_TUTORIAL; (void)now; }
void tutorial_event(int in, double now) {
    if (T.run) {
        if (!T.paused) return;
        if (in == IN_UP) { T.pauseRow = (T.pauseRow + 3) % 4; audio_sfx(SFX_MOVE); } else if (in == IN_DOWN) { T.pauseRow = (T.pauseRow + 1) % 4; audio_sfx(SFX_MOVE); }
        else if (in == IN_BACK) pause_set(0, now);
        else if (in == IN_OK) { audio_sfx(SFX_OK);
            if (T.pauseRow == 0) pause_set(0, now);
            else if (T.pauseRow == 1) { if (speaking(now)) { hush(now); pause_set(0, now); } }
            else if (T.pauseRow == 2) { pause_set(0, now); restart_lesson(now); }
            else finish(now, NULL); }
        return; }
    int n = list_rows();
    if (in == IN_UP) { T.cursor = (T.cursor + n - 1) % n; audio_sfx(SFX_MOVE); } else if (in == IN_DOWN) { T.cursor = (T.cursor + 1) % n; audio_sfx(SFX_MOVE); }
    else if (in == IN_LEFT || in == IN_RIGHT || in == IN_L || in == IN_R) { T.lang = (T.lang + LANG_N + (in == IN_RIGHT || in == IN_R ? 1 : -1)) % LANG_N; save(); audio_sfx(SFX_MOVE); }
    else if (in == IN_BACK) { save(); A.screen = SC_TRAINING; audio_sfx(SFX_BACK); }
    else if (in == IN_OK) { if (T.cursor == 0) { T.lang = (T.lang + 1) % LANG_N; save(); audio_sfx(SFX_MOVE); return; }
        int j = T.cursor - 1; for (int t = 0; t < TUT_N; t++) { if (j <= TUT[t].n) { audio_sfx(SFX_OK); start(t, j - 1, now); return; } j -= TUT[t].n + 1; } }
}
int tutorial_raw_event(const SDL_Event *ev, double now) {
    if (!T.run || T.paused) return 0;
    inev_t evs[4]; int n = input_translate(ev, evs, 4); if (!n) return 0;
    gh3_run *r = &A.S.run; int live = T.loaded && !T.frozen; double te = song_clock() - (now - ev->common.timestamp);
    for (int i = 0; i < n; i++) { int a = evs[i].action, pr = evs[i].pressed;
        if (a <= ACT_FRET4) { if (pr) T.held |= 1 << a; else T.held &= ~(1 << a); if (live) game_set_frets(r, T.held, te, now); drill_frets(now); }
        else if (a == ACT_STRUM_UP || a == ACT_STRUM_DOWN) { if (!pr) continue; T.strumAt = now; if (live) game_strum(r, te, now); if (T.goal == G_TUNE) tune_strum(T.held, now); drill_strum(now); }
        else if (a == ACT_STAR) { if (pr && live) game_star(r, now); }
        else if (a == ACT_WHAMMY) { if (live) game_whammy(r, evs[i].value, now); }
        else if (a == ACT_PAUSE && pr) pause_set(1, now); }
    return 1;
}

/* ---- headless: the bot plays one lesson (or a whole tutorial: "2"), frames saved every `every` */
int tutorial_headless(const char *id, int frames, int every, void (*savef)(const char *name)) {
    int t = -1, l = -1;
    for (int a = 0; a < TUT_N; a++) { if (id[0] == '1' + a && !id[1]) { t = a; l = -1; } for (int b = 0; b < TUT[a].n; b++) if (!strcasecmp(id, TUT[a].ls[b].id)) { t = a; l = b; } }
    if (t < 0) { fprintf(stderr, "no lesson %s (1a..4e, or 1..4 for a whole tutorial)\n", id); return 1; }
    T.headless = 1; T.bot = 1; double now = 1000; start(t, l, now); int saved = 0, f = 0;
    printf("tutorial %s: %s%s%s\n", id, TUT[t].name, l >= 0 ? " / " : "", l >= 0 ? TUT[t].ls[l].name : "");
    for (; f < frames && T.run; f++, now += 1000.0 / 60) {
        tick(now); if (l >= 0 && T.seg > l && T.seg < TUT[t].n) { printf("%8.2f s  lição %s concluída\n", now / 1000, TUT[t].ls[l].id); break; }
        if (every > 0 && f % every == 0) { char nm[48]; gfx_begin(); draw_lesson(now); gfx_end(); snprintf(nm, sizeof nm, "tutorial_%s_%04d", id, f); savef(nm); saved++; } }
    gfx_begin(); if (T.run) draw_lesson(now); else draw_list(now); gfx_end(); { char nm[48]; snprintf(nm, sizeof nm, "tutorial_%s_end", id); savef(nm); }
    printf("%d frames (%.1f s), %d saved; lesson %s; run: hits %d, score %ld\n", f, f / 60.0, saved + 1, T.run && !(l >= 0 && T.seg > l) ? "NOT finished" : "finished", A.S.run.hits, (long)A.S.run.score);
    return T.run && !(l >= 0 && T.seg > l) ? 2 : 0;
}
