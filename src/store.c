/* Career money, the Guitar Hero store and the encore chant, from the game's own data (qb.pak, read into lib->shop):
 * - a Career result pays by the stars it earned and the stars that song was already paid for on that difficulty (the
 *   game's table, newstars x oldstars: Easy 3/4/5 stars pay $75/$175/$300, Medium $150/$350/$600, Hard $300/$700/$1200,
 *   Expert $450/$1050/$1800; a later 5 over a 3 pays the difference, on Expert $1350). Under 3 stars, or a repeat that
 *   does not beat the stars already paid, pays nothing; Quickplay, Practice, No Fail and the bot pay nothing. Encores
 *   pay by their stars like any other song. A boss battle has no stars, but Progression_SongWon sets 5 for a boss beaten
 *   (Progression_IsBossSong) before Progression_AwardCash: the first win pays the 5-star row ($300/$600/$1200/$1800),
 *   later ones nothing (the devil: once his song is finished, after the battle).
 * - the store sells the bonus songs (store_song_data: $500 each; the one with no price, Through the Fire and Flames,
 *   opens when a career is finished on any difficulty, as in the game) and the four bonus videos (bonus_videos, $1500).
 *   A bonus song is locked in every mode until bought; one with a record (played before the store existed) is owned;
 *   the "Liberar todas as músicas" cheat opens them all. It also sells what charsel.c reads: the secret rockers, the
 *   guitars and special guitars, and the finishes (a finish once its guitar is yours), at the data's prices; from the
 *   character hub the store opens on the locked item and B goes back there.
 * - the encore: the crowd chants for it first (STREAMS/streamall's encore_build_<venue>, the venue being the tier's
 *   level in levelzones), over an ENCORE! banner on the setup screen, then the song starts.
 * Money and purchases are kept in gh3-carreira.cfg next to gh3.cfg. */
#include "app.h"
#include "store.h"
#include "charsel.h"
#include "render.h"
#include "ui.h"
#include "audio.h"
#include "fx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static long cash; static int lastGain, careerDone, anyPrice, noSave;
typedef struct { char key[80]; int stars; } paid_t;           /* "song/diff": the stars already paid for */
static paid_t *paid; static int npaid, paidCap;
static char (*buys)[72]; static int nbuys, buyCap;            /* song names, and "video:<id>" */
static unsigned char *owned; static int nowned;               /* per song: bought, or played before */

static void cfg_path(char *out, size_t n) { snprintf(out, n, "%s/gh3-carreira.cfg", settings_home()); }
static int bought(const char *name) { for (int i = 0; i < nbuys; i++) if (!strcmp(buys[i], name)) return 1; return 0; }
static void add_buy(const char *name) { if (!*name || bought(name)) return; if (nbuys == buyCap) { buyCap = buyCap ? buyCap * 2 : 32; buys = realloc(buys, buyCap * sizeof *buys); } snprintf(buys[nbuys++], sizeof *buys, "%s", name); }
static paid_t *paid_key(const char *k) {
    for (int i = 0; i < npaid; i++) if (!strcmp(paid[i].key, k)) return &paid[i];
    if (npaid == paidCap) { paidCap = paidCap ? paidCap * 2 : 128; paid = realloc(paid, paidCap * sizeof *paid); }
    paid_t *p = &paid[npaid++]; snprintf(p->key, sizeof p->key, "%s", k); p->stars = 0; return p;
}
static paid_t *paid_of(const char *song, int d) { char k[80]; snprintf(k, sizeof k, "%s/%d", song, d); return paid_key(k); }
static void save(void) {
    if (noSave) return; char p[560]; cfg_path(p, sizeof p); FILE *f = fopen(p, "w"); if (!f) return;
    fprintf(f, "# Guitar Hero III (R36S) - o dinheiro da carreira e as compras na loja\n# estrelas=musica/dificuldade,N: as estrelas daquela musica que ja foram pagas\ndinheiro=%ld\n", cash);
    for (int i = 0; i < nbuys; i++) fprintf(f, "compra=%s\n", buys[i]);
    for (int i = 0; i < npaid; i++) if (paid[i].stars > 0) fprintf(f, "estrelas=%s,%d\n", paid[i].key, paid[i].stars);
    fclose(f);
}
static void fmt_cash(long n, char *out, size_t cap) {   /* $1.234 */
    char raw[24]; snprintf(raw, sizeof raw, "%ld", n < 0 ? -n : n); int L = (int)strlen(raw), o = 0; if (n < 0 && o + 1 < (int)cap) out[o++] = '-'; if (o + 1 < (int)cap) out[o++] = '$';
    for (int i = 0; i < L && o + 1 < (int)cap; i++) { out[o++] = raw[i]; int left = L - 1 - i; if (left && left % 3 == 0 && o + 1 < (int)cap) out[o++] = '.'; } out[o] = 0;
}
long store_cash(void) { return cash; }
const char *store_cash_str(void) { static char b[32]; fmt_cash(cash, b, sizeof b); return b; }
int store_last_gain(void) { return lastGain; }

/* ------------------------------------------------------------------ the pay */
static const int CUM[4][3] = { { 75, 175, 300 }, { 150, 350, 600 }, { 300, 700, 1200 }, { 450, 1050, 1800 } };   /* the game's table (0 -> 3/4/5 stars), if qb.pak lacks it */
int store_cash_for(const gh3_library *lib, int d, int ns, int os) {
    if (d < 0 || d > 3) return 0; if (ns > 5) ns = 5; if (os > 5) os = 5; if (os < 3) os = 0; if (ns < 3 || ns <= os) return 0;
    if (lib && lib->shop.hasCash) { const int (*c)[6] = lib->shop.cash[d]; if (c[ns][os] > 0) return c[ns][os]; if (c[ns][0] > 0) { int v = c[ns][0] - (os ? c[os][0] : 0); return v > 0 ? v : 0; } }
    return CUM[d][ns - 3] - (os ? CUM[d][os - 3] : 0);
}
int store_result(const gh3_song *s, int d, int stars, int pays) {
    lastGain = 0; if (!pays || !s) return 0;
    if (s->isBoss) stars = 5;   /* Progression_SongWon: a boss beaten counts as 5 stars (Progression_IsBossSong), paid once from the table */
    paid_t *p = paid_of(s->name, d); int g = store_cash_for(A.lib, d, stars, p->stars); if (stars > p->stars) p->stars = stars;
    cash += g; lastGain = g; save(); return g;
}
void store_load(const gh3_library *lib) {
    char p[560]; cfg_path(p, sizeof p); FILE *f = fopen(p, "r"); cash = 0; npaid = nbuys = 0;
    if (f) { char line[300];
        while (fgets(line, sizeof line, f)) { line[strcspn(line, "\r\n")] = 0; char *eq = strchr(line, '='); if (line[0] == '#' || !eq) continue; *eq = 0; char *v = eq + 1;
            if (!strcmp(line, "dinheiro")) cash = atol(v); else if (!strcmp(line, "compra")) add_buy(v);
            else if (!strcmp(line, "estrelas")) { char *c = strrchr(v, ','); if (c) { *c = 0; paid_key(v)->stars = atoi(c + 1); } } }
        fclose(f); }
    else {   /* the first time: the career already played pays what it would have earned */
        for (int si = 0; si < lib->nsongs; si++) for (int d = 0; d < 4; d++) { int st = 0;
            for (int part = 0; part < 2; part++) { const record_t *r = records_get(lib->songs[si].name, d, part); if (r && r->career && r->stars > st) st = r->stars; if (r && r->career && lib->songs[si].isBoss) st = 5; }   /* (a battle's record: it was won) */
            if (st >= 3) { cash += store_cash_for(lib, d, st, 0); paid_of(lib->songs[si].name, d)->stars = st; } }
        save(); }
    store_refresh(lib);
}
void store_refresh(const gh3_library *lib) {
    if (nowned < lib->nsongs) { owned = realloc(owned, lib->nsongs); nowned = lib->nsongs; } memset(owned, 0, nowned); anyPrice = 0;
    for (int t = 0; t < lib->ntiers; t++) if (lib->tiers[t].tab == 1) for (int k = 0; k < lib->tiers[t].nsongs; k++) { int si = lib->tiers[t].songIdx[k]; const char *n = lib->songs[si].name; int o = bought(n);
        for (int d = 0; d < 4 && !o; d++) for (int part = 0; part < 2 && !o; part++) if (records_get(n, d, part)) o = 1;
        owned[si] = o; if (lib->shop.songPrice[si] > 0) anyPrice = 1; }
    int ntier = 0; for (int t = 0; t < lib->ntiers; t++) ntier += lib->tiers[t].career;
    tierstate_t *ts = malloc((lib->ntiers + 1) * sizeof *ts); careerDone = 0;
    for (int d = 0; d < 4 && ntier; d++) { career_eval(lib, d, 0, ts); if (career_tiers_done(lib, ts) == ntier) careerDone = 1; }
    free(ts);
}
int store_song_lock(const gh3_library *lib, int t, int si) {
    if (t < 0 || t >= lib->ntiers || lib->tiers[t].tab != 1 || SET.unlockAll || !anyPrice || si < 0 || si >= nowned || owned[si]) return 0;
    return lib->shop.songPrice[si] > 0 ? LOCK_BUY : careerDone ? 0 : LOCK_FINALE;
}
const char *store_lock_text(int si, int career) {
    static char b[80]; char c[24]; int pr = si >= 0 && si < A.lib->nsongs ? A.lib->shop.songPrice[si] : 0;
    if (pr <= 0) return "termine a carreira para liberar"; fmt_cash(pr, c, sizeof c); snprintf(b, sizeof b, "%s", c); (void)career; return b;   /* the price tag: the set header says where */
}

/* ------------------------------------------------------------------ the store screen */
enum { NCAT = 2 + CS_CATS };   /* bonus songs, bonus videos, then charsel.c's rockers, guitars and finishes */
static int cat, sel[NCAT], top, confirm, back = SC_SETLIST; static int list[64], nlist;
static void note(double now, const char *m) { snprintf(A.note, sizeof A.note, "%s", m); A.noteAt = now; }
static void make_list(void) { nlist = 0; const gh3_library *lib = A.lib;
    for (int t = 0; t < lib->ntiers; t++) if (lib->tiers[t].tab == 1) for (int k = 0; k < lib->tiers[t].nsongs && nlist < 64; k++) { int si = lib->tiers[t].songIdx[k]; if (strncmp(lib->songs[si].name, "tutorial", 8)) list[nlist++] = si; } }
static int count_of(int c) { return c == 0 ? nlist : c == 1 ? A.lib->shop.nvideos : charsel_store_n(c - 2); }
static int count(void) { return count_of(cat); }
int store_owns(const char *key) { return bought(key); }
static int video_owned(const gh3_video *v) { char k[48]; snprintf(k, sizeof k, "video:%s", v->id); return bought(k); }
/* an item of the shelf shown: info (its text), why (it cannot be bought yet) and key (what a purchase keeps) when asked */
static void item_x(int i, const char **title, const char **sub, int *price, int *have, const char **info, const char **why, char *key, size_t cap) {
    const gh3_shop *sh = &A.lib->shop; const char *in_ = NULL, *wy = NULL; char k_[72];
    if (cat == 0) { const gh3_song *g = &A.lib->songs[list[i]]; *title = g->title; *sub = g->artist; *price = sh->songPrice[list[i]]; *have = owned[list[i]]; in_ = sh->songInfo[list[i]]; snprintf(k_, sizeof k_, "%s", g->name); }
    else if (cat == 1) { const gh3_video *v = &sh->videos[i]; *title = v->name; *sub = ""; *price = v->price; *have = video_owned(v); in_ = v->info; snprintf(k_, sizeof k_, "video:%s", v->id); }
    else charsel_store_item(cat - 2, i, title, sub, price, have, &in_, &wy, k_, sizeof k_);
    if (info) *info = in_; if (why) *why = wy; if (key) snprintf(key, cap, "%s", k_);
}
static void item(int i, const char **title, const char **sub, int *price, int *have) { item_x(i, title, sub, price, have, NULL, NULL, NULL, 0); }
/* a bonus video, from MOVIES/R36S (tools/convert_movies.sh makes them) */
static void watch_video(const gh3_video *v, double now) {
    if (app_movie_exists(v->id)) { audio_sfx(SFX_OK); app_play_movie(v->id, SC_STORE); return; }
    char b[160]; snprintf(b, sizeof b, "Falta o vídeo \"%s\" em DATA/MOVIES/R36S.", v->name); note(now, b); audio_sfx(SFX_WARN);
}
void store_open(double now) { make_list(); store_refresh(A.lib); A.screen = SC_STORE; confirm = 0; back = SC_SETLIST; if (cat > 1) cat = 0; app_preview(-1); audio_sfx(SFX_OK); audio_sfx(SFX_WHOOSH); (void)now; }
void store_open_at(int c, const char *key, int backTo) {
    make_list(); store_refresh(A.lib); A.screen = SC_STORE; confirm = 0; back = backTo; cat = c >= 0 && c < NCAT ? c : 0; top = 0; app_preview(-1); audio_sfx(SFX_WHOOSH);
    for (int i = 0, n = count(); key && *key && i < n; i++) { const char *t, *s, *in_, *wy; int p, h; char k[72]; item_x(i, &t, &s, &p, &h, &in_, &wy, k, sizeof k); if (!strcmp(k, key)) sel[cat] = i; }
}
void store_draw(double now) {
    render_backdrop(now, SET.stageFx); int W = A.W, H = A.H; const gh3_library *lib = A.lib; char b[320], c[32];
    snprintf(b, sizeof b, "Seu dinheiro: %s", store_cash_str()); ui_header(W, "LOJA", b);
    static const char *CAT[NCAT] = { "MÚSICAS BÔNUS", "VÍDEOS BÔNUS", "ROQUEIROS", "GUITARRAS", "ACABAMENTOS" };
    { int nc = 0, tw = 0; for (int k = 0; k < NCAT; k++) if (k == 0 || count_of(k)) { nc++; tw += gfx_text_w(CAT[k], 1) + 18; } int big = nc <= 2, gap = big ? 6 : 4, x = 12;   /* the tabs: big when only the songs and videos are sold */
      if (!big && tw + gap * (nc - 1) > W - 24) gap = 0;
      for (int k = 0; k < NCAT; k++) { if (k && !count_of(k)) continue; int w = gfx_text_w(CAT[k], big ? 2 : 1) + (big ? 28 : 18); if (k == cat) ui_panel(x, 64, w, big ? 28 : 22, UI_PANEL_AMBER);
          gfx_text_sh(x + (big ? 14 : 9), big ? 71 : 70, big ? 2 : 1, CAT[k], k == cat ? C_INK : C_MUTED, C_SHADOW); x += w + gap; }
      if (big) gfx_text_right(W - 16, 76, 1, "L1 / R1 ou ← →", C_LINE); }
    int pH = cat >= 2 ? 140 : 94, py = H - 52 - pH;   /* the item's panel over the hints: the guitars' longest text is 7 lines, and a line under it says where they are chosen */
    int n = count(), rowH = 24, y0 = 102, vis = (py - 16 - y0) / rowH, *s = &sel[cat];
    if (*s >= n) *s = n ? n - 1 : 0; if (*s < top) top = *s; if (*s >= top + vis) top = *s - vis + 1; if (top < 0) top = 0;
    for (int i = top, y = y0; i < n && i < top + vis; i++, y += rowH) { const char *title, *sub; int price, have, on = i == *s; uint32_t sc;
        item(i, &title, &sub, &price, &have);
        if (have) { snprintf(b, sizeof b, cat == 1 ? "✓ Assistir" : cat == 0 ? "✓ comprada" : "✓ seu"); sc = C_GOOD; } else if (price > 0) { fmt_cash(price, b, sizeof b); sc = cash >= price ? C_AMP : C_BAD; }
        else { snprintf(b, sizeof b, careerDone || SET.unlockAll ? "liberada" : "termine a carreira"); sc = careerDone || SET.unlockAll ? C_GOOD : C_MUTED; }
        if (on) { ui_panel(10, y - 3, W - 20, rowH, UI_PANEL_AMBER); gfx_sprite_at(art_get(ART_PICK), 16, y + 1, 0.5f, RGB(255, 255, 255), 1); }
        gfx_text_sh(34, y + 1, 2, title, on ? C_INK : have ? C_MUTED : RGB(210, 204, 196), C_SHADOW);
        int tw = gfx_text_w(title, 2), rw = gfx_text_w(b, cat || have || price <= 0 ? 1 : 2);
        if (*sub && 34 + tw + 14 + gfx_text_w(sub, 1) < W - 40 - rw) gfx_text(34 + tw + 14, y + 6, 1, sub, on ? C_INK : C_MUTED);
        if (price > 0 && !have) gfx_text_sh_right(W - 22, y + 1, 2, b, sc, C_SHADOW); else gfx_text_sh_right(W - 22, y + 6, 1, b, sc, C_SHADOW); }
    if (top > 0) gfx_text_center(W / 2, y0 - 13, 1, "↑", C_MUTED); if (top + vis < n) gfx_text_center(W / 2, y0 + vis * rowH, 1, "↓", C_MUTED);   /* clear of the rows' highlight, above the panel */
    ui_panel(14, py, W - 28, pH, UI_PANEL_FLAT);
    if (n) { const char *title, *sub, *info, *why; int price, have, tx0 = 26; item_x(*s, &title, &sub, &price, &have, &info, &why, NULL, 0);
        if (cat == 0) { const gh3_song *g = &lib->songs[list[*s]]; snprintf(b, sizeof b, "%s • %s%s%s", g->title, g->artist, *g->year ? " • " : "", g->year); }
        else if (cat >= 2 && *sub) snprintf(b, sizeof b, "%s • %s", title, sub); else snprintf(b, sizeof b, "%s", title);
        if (cat == 2) { char k[72]; const char *t, *u, *in_, *wy; int p, h; item_x(*s, &t, &u, &p, &h, &in_, &wy, k, sizeof k); gfx_sprite *ph = charsel_photo(k + 9);   /* "roqueiro:<name>": the rocker's photo beside the text */
            if (ph) { gfx_blit(ph, 0, 0, gfx_sprite_w(ph), gfx_sprite_h(ph), 20, py + 5, 84, 84, RGB(255, 255, 255), 1); tx0 = 114; } }
        gfx_text_sh(tx0, py + 8, 1, b, C_AMP, C_SHADOW); gfx_text_box(tx0, py + 26, W - 26 - tx0, 1, why ? why : info && *info ? info : cat ? "" : "Uma das músicas bônus do jogo.", why ? C_BAD : C_MUTED); }
    else gfx_text_box(26, py + 10, W - 52, 1, "Os itens da loja não estão no qb.pak desta cópia do jogo.", C_MUTED);
    if (cat >= 2) { gfx_rect(26, py + pH - 22, W - 52, 1, C_LINE); gfx_text_center(W / 2, py + pH - 17, 1, back == SC_CHARSEL ? "B volta à escolha do roqueiro e da guitarra." : "Escolha o roqueiro e a guitarra ao entrar na Carreira ou no Jogo rápido.", C_MUTED); }   /* inside the panel, clear of the hints */
    { const char *title, *sub; int price = 0, have = 0; if (n) item(*s, &title, &sub, &price, &have);
      static const char *bt[3] = { "A", "← →", "B" }; const char *tx[3] = { have && cat == 1 ? "Assistir" : "Comprar", "Categoria", "Voltar" }; ui_hints(H - 22, W, bt, tx, 3); }
    if (confirm && n) { const char *title, *sub; int price, have; item(*s, &title, &sub, &price, &have);
        ui_dim(W, H, 160); ui_panel(W / 2 - 240, H / 2 - 62, 480, 124, UI_PANEL_DARK);
        snprintf(b, sizeof b, "Comprar \"%s\"?", title); gfx_text_sh_center(W / 2, H / 2 - 46, gfx_text_w(b, 2) < 440 ? 2 : 1, b, C_INK, C_SHADOW);
        fmt_cash(price, c, sizeof c); char r[32]; fmt_cash(cash - price, r, sizeof r); snprintf(b, sizeof b, "Preço %s  •  você fica com %s", c, r); gfx_text_sh_center(W / 2, H / 2 - 10, 1, b, C_AMP, C_SHADOW);
        static const char *qb[2] = { "A", "B" }, *qt[2] = { "Comprar", "Cancelar" }; ui_hints(H / 2 + 26, W, qb, qt, 2); }
}
static void buy(double now) {
    const char *title, *sub, *info, *why; int price, have; char k[72]; item_x(sel[cat], &title, &sub, &price, &have, &info, &why, k, sizeof k); confirm = 0; if (have || why || price <= 0 || cash < price) return;
    cash -= price; add_buy(k); save(); store_refresh(A.lib); audio_sfx(SFX_CASH);   /* the store's Cash_Sound */
    note(now, cat == 0 ? "Comprada! Já está na lista de músicas." : cat == 1 ? "Comprado! Aperte A para assistir." : back == SC_CHARSEL ? "Comprado! Aperte B e escolha." : "Comprado! Escolha ao entrar na Carreira ou no Jogo rápido.");
}
void store_event(int in, double now) {
    int n = count(), *s = &sel[cat];
    if (confirm) { if (in == IN_OK) buy(now); else if (in == IN_BACK || in == IN_X || in == IN_Y) { confirm = 0; audio_sfx(SFX_BACK); } return; }
    if (in == IN_UP || in == IN_DOWN) { if (n) { *s = (*s + n + (in == IN_UP ? -1 : 1)) % n; audio_sfx(SFX_MOVE); } }
    else if (in == IN_LEFT || in == IN_RIGHT || in == IN_L || in == IN_R) { int d = in == IN_RIGHT || in == IN_R ? 1 : -1; do cat = (cat + NCAT + d) % NCAT; while (cat && !count_of(cat)); top = 0; audio_sfx(SFX_MOVE); }
    else if (in == IN_BACK || in == IN_Y) { A.screen = back; audio_sfx(SFX_BACK); if (back == SC_SETLIST) app_preview(A.cursor < A.nrows ? A.rows[A.cursor].idx : -1); }
    else if (in == IN_OK && n) { const char *title, *sub, *info, *why; int price, have; item_x(*s, &title, &sub, &price, &have, &info, &why, NULL, 0); char b[120], c[24];
        if (have) { if (cat == 1) watch_video(&A.lib->shop.videos[*s], now); else { audio_sfx(SFX_WARN); note(now, cat == 0 ? "Essa já é sua: toque na lista de músicas." : "Esse já é seu."); } return; }
        if (why) { audio_sfx(SFX_WARN); note(now, why); return; }
        if (price <= 0) { audio_sfx(SFX_WARN); note(now, careerDone || SET.unlockAll ? "Essa já está liberada na lista de músicas." : "Termine a carreira (em qualquer dificuldade) para liberar esta."); return; }
        if (cash < price) { audio_sfx(SFX_WARN); fmt_cash(price - cash, c, sizeof c); snprintf(b, sizeof b, "Faltam %s: tire 3 estrelas ou mais na carreira.", c); note(now, b); return; }
        confirm = 1; audio_sfx(SFX_OK); }
}
void store_draw_result(int W, int y, double T) {
    if (A.mode != MODE_CAREER || A.S.run.failed || A.S.run.autoPlay || SET.noFail || T < 1.2) return; char b[64], c[32];
    float k = (float)fmin(1, (T - 1.2) / 0.3);
    if (lastGain > 0) { fmt_cash(lastGain, c, sizeof c); snprintf(b, sizeof b, "+%s", c); ui_title(W / 2 + 296 - gfx_text_w(b, 3), y - (int)(8 * (1 - k)), 3, b, C_GOOD, C_SHADOW, 2); }
    else gfx_text_right(W / 2 + 294, y + 8, 1, "sem dinheiro novo", C_MUTED);
    snprintf(b, sizeof b, "total %s", store_cash_str()); gfx_text_right(W / 2 + 294, y + 30, 1, b, C_MUTED);
}

/* ------------------------------------------------------------------ the encore chant */
static uint8_t *chant; static size_t chantN; static double chantAt; static int chantOn, chantFading;
static uint8_t *chant_load(size_t *n) {   /* encore_build_<venue>.mp3: z_party -> party, z_artdeco -> deco */
    const char *z = A.selTier >= 0 && A.selTier < A.lib->ntiers ? A.lib->shop.tierZone[A.selTier] : ""; if (!strncmp(z, "z_", 2)) z += 2; char nm[64]; *n = 0; if (!*z) return NULL;
    snprintf(nm, sizeof nm, "encore_build_%s.mp3", z); uint8_t *b = app_stream_load(nm, n); if (b) return b;
    static const char *V[] = { "budokan", "deco", "dive", "hell", "party", "prison", "video", "wikker", NULL };
    for (int i = 0; V[i]; i++) if (strstr(z, V[i])) { snprintf(nm, sizeof nm, "encore_build_%s.mp3", V[i]); return app_stream_load(nm, n); }
    return NULL;
}
static void chant_end(int start) { chantOn = 0; if (start) app_begin_song(); else if (audio_preview_on()) audio_preview_stop(0); /* app_begin_song stops it too */ free(chant); chant = NULL; chantN = 0; }
void encore_begin(double now) {
    const gh3_tier *tr = &A.lib->tiers[A.selTier];
    if (A.mode != MODE_CAREER || A.selSong != tr->encoreIdx) { app_begin_song(); return; }
    free(chant); chant = chant_load(&chantN); app_preview(-1); if (audio_preview_on()) audio_preview_stop(0);
    if (chant && audio_has_device()) audio_preview(chant, chantN, -1, 0);   /* through the menu music's stream: the band volume */
    chantOn = 1; chantFading = 0; chantAt = now; audio_sfx(SFX_WHOOSH);   /* the ENCORE! banner flies in */
}
int encore_event(int in, double now) {
    if (!chantOn) return 0;
    if (in == IN_OK) chant_end(1); else if (in == IN_BACK) { chant_end(0); audio_sfx(SFX_BACK); }
    return 1;
}
void encore_draw(double now) {
    if (!chantOn) return; double T = (now - chantAt) / 1000; int W = A.W, H = A.H;
    /* the chant builds for ~15 s in the game's clips: about 9 s of it, faded out, then the song (A skips) */
    if (T > 9.2 && !chantFading) { chantFading = 1; audio_preview_stop(0.8); }
    if ((T > 3.5 && !audio_preview_on()) || T > 10.5) { chant_end(1); return; }
    ui_dim(W, H, 170 + (int)(50 * fmin(1, T))); gfx_rect(0, H - 40, W, 40, RGBA(10, 8, 16, 255));   /* over the setup screen, its hints hidden */
    float pulse = (float)(0.5 + 0.5 * sin(T * 6.2)); int sc = T < 0.4 ? 4 + (int)(T / 0.1) : 8; const char *E = "ENCORE!";
    ui_title(W / 2 - gfx_text_w(E, sc) / 2, H / 2 - 92, sc, E, fb_alpha(C_AMP, 0.75f + 0.25f * pulse), C_SHADOW, 4);
    gfx_text_sh_center(W / 2, H / 2 + 6, 2, "A plateia quer mais uma!", C_INK, C_SHADOW);
    const gh3_song *s = &A.lib->songs[A.selSong]; char b[320]; snprintf(b, sizeof b, "%s • %s", s->title, s->artist); gfx_text_sh_center(W / 2, H / 2 + 40, 1, b, C_MUTED, C_SHADOW);
    for (int i = 0; i < 5; i++) { float a = (float)(0.35 + 0.65 * fmax(0, sin(T * 6.2 - i * 0.9))); { gfx_sprite *st = fx_star(1, SET.theme >= 1); gfx_blit(st, 0, 0, gfx_sprite_w(st), gfx_sprite_h(st), W / 2 - 116 + i * 50, H / 2 + 66, 32, 32, RGB(255, 255, 255), a); } }
    static const char *bt[2] = { "A", "B" }, *tx[2] = { "Tocar já", "Voltar" }; ui_hints(H - 24, W, bt, tx, 2);
}
int store_encore_offer(void) {   /* the tier's encore once the career opened it by its own rules (not by the cheat), until it is played */
    if (A.mode != MODE_CAREER || !A.S.loaded || A.S.run.failed || !A.tiers || A.selTier < 0 || A.selTier >= A.lib->ntiers) return -1;
    const gh3_tier *tr = &A.lib->tiers[A.selTier]; const tierstate_t *ts = &A.tiers[A.selTier];
    if (!tr->career || tr->encoreIdx < 0 || A.selSong == tr->encoreIdx || ts->encoreDone || !ts->encoreOpen || ts->regDone < ts->required || (tr->bossIdx >= 0 && !ts->bossDone)) return -1;
    return tr->encoreIdx;
}

void store_demo(long c) { noSave = 1; cash = c; }

/* ------------------------------------------------------------------ --moneytest */
int store_selftest(const gh3_library *lib) {
    int bad = 0; noSave = 1; long cash0 = cash; int np = npaid;
    { int own = 0; for (int i = 0; i < nowned; i++) own += owned[i]; printf("loaded: %s, %d bought, %d paid entries, %d bonus songs owned, career finished: %s\n", store_cash_str(), nbuys, npaid, own, careerDone ? "yes" : "no"); }
    printf("cash table %s qb.pak:\n", lib->shop.hasCash ? "from" : "NOT in (built-in values)");
    for (int d = 0; d < 4; d++) { printf("  %-6s", GH3_DIFF_NAME[d]); for (int ns = 3; ns <= 5; ns++) for (int os = 0; os < ns; os++) if (os == 0 || os >= 3) printf("  %d<-%d $%d", ns, os, store_cash_for(lib, d, ns, os)); printf("\n"); }
    /* a song on Expert: 3 stars, the same again, then 5, then 4: pays 450, 0, 1350, 0 */
    gh3_song fake, boss; memset(&fake, 0, sizeof fake); snprintf(fake.name, sizeof fake.name, "__moneytest__"); boss = fake; snprintf(boss.name, sizeof boss.name, "__moneytest_boss__"); boss.isBoss = 1; cash = 0;
    /* then a boss battle (no stars: it counts as 5): won on Medium, again, on Expert, a battle lost or out of Career: pays 600, 0, 1800, 0, 0 */
    struct { int d, stars, pays, boss; long want; } C[] = { { 3, 3, 1, 0, 450 }, { 3, 3, 1, 0, 0 }, { 3, 5, 1, 0, 1350 }, { 3, 4, 1, 0, 0 }, { 3, 5, 0, 0, 0 }, { 0, 2, 1, 0, 0 }, { 0, 4, 1, 0, 175 }, { 0, 5, 1, 0, 125 }, { 1, 5, 0, 0, 0 }, { 2, 5, 1, 0, 1200 },
        { 1, 0, 1, 1, 600 }, { 1, 0, 1, 1, 0 }, { 3, 0, 1, 1, 1800 }, { 2, 0, 0, 1, 0 } };
    long total = 0;
    for (unsigned i = 0; i < sizeof C / sizeof *C; i++) { int g = store_result(C[i].boss ? &boss : &fake, C[i].d, C[i].stars, C[i].pays); total += C[i].want;
        char what[24]; if (C[i].boss) snprintf(what, sizeof what, C[i].pays ? "boss battle won" : "boss battle"); else snprintf(what, sizeof what, "%d stars", C[i].stars);
        printf("  %-6s %s%s: +$%d (want $%ld)%s\n", GH3_DIFF_NAME[C[i].d], what, C[i].pays ? "" : C[i].boss ? " (lost, or not Career)" : " (not Career)", g, C[i].want, g == C[i].want ? "" : "  <-- WRONG"); bad += g != C[i].want; }
    printf("  total $%ld (want $%ld)%s\n", cash, total, cash == total ? "" : "  <-- WRONG"); bad += cash != total;
    /* the store: bonus songs priced, the one without a price, the videos */
    int priced = 0, free_ = 0; for (int t = 0; t < lib->ntiers; t++) if (lib->tiers[t].tab == 1) for (int k = 0; k < lib->tiers[t].nsongs; k++) { int si = lib->tiers[t].songIdx[k]; if (lib->shop.songPrice[si] > 0) priced++; else { free_++; printf("  bonus song without a price: %s\n", lib->songs[si].name); } }
    printf("  bonus songs: %d for sale, %d opened by finishing a career; videos:", priced, free_); for (int i = 0; i < lib->shop.nvideos; i++) printf(" %s $%d", lib->shop.videos[i].id, lib->shop.videos[i].price); printf("\n");
    for (int t = 0; t < lib->ntiers; t++) if (lib->tiers[t].career) { int sv = A.selTier; A.selTier = t; size_t n; uint8_t *b = chant_load(&n);
        printf("  tier %d %-30s venue %-10s encore %-20s chant %u bytes\n", t + 1, lib->tiers[t].title, lib->shop.tierZone[t], lib->tiers[t].encoreIdx >= 0 ? lib->songs[lib->tiers[t].encoreIdx].name : "-", (unsigned)n); int had = b != NULL; free(b); A.selTier = sv;
        if (lib->tiers[t].encoreIdx >= 0 && !had) { printf("  <-- no chant for that encore\n"); bad++; } }
    npaid = np; cash = cash0; noSave = 0; printf(bad ? "money test: %d FAILED\n" : "money test: all passed\n", bad); return bad;
}
