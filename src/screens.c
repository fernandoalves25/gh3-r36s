/* The menus, laid out like the game's own: title, main menu (Career, Co-op Career, Quickplay, Multiplayer, Training,
 * Options), the set list of each mode, song setup, options by category (Audio, Calibrate Lag, Controller, Video,
 * Top Rockers, Cheats), controls, pause, results and error, in Portuguese. */
#include "app.h"
#include "render.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include "fx.h"
#include "practice.h"
#include "store.h"
#include "battle.h"
#include "cheats.h"
#include "tutorial.h"
#include "orig_setup.h"
#include "orig_setlist.h"
#include "orig_loading.h"
#include "original.h"
#include "orig_results.h"
#include "original.h"
#include "orig_options.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const char *DIFF_PT[4] = { "Fácil", "Médio", "Difícil", "Expert" };
const char *diff_name_pt(int d) { return DIFF_PT[d < 0 ? 0 : d > 3 ? 3 : d]; }
static void fmt_num(long n, char *out, size_t cap) { /* 1.234.567 */
    char raw[32]; snprintf(raw, sizeof raw, "%ld", n < 0 ? -n : n); int L = (int)strlen(raw), o = 0; if (n < 0) out[o++] = '-';
    for (int i = 0; i < L && o + 1 < (int)cap; i++) { out[o++] = raw[i]; int left = L - 1 - i; if (left && left % 3 == 0 && o + 1 < (int)cap) out[o++] = '.'; } out[o] = 0;
}
static void song_title(const gh3_song *s, char *out, size_t cap) {
    if (s->isBoss) { const char *p = strstr(s->title, "vs."); snprintf(out, cap, "Batalha %s", p ? p : s->title); } else snprintf(out, cap, "%s", s->title);
}
static void stars_str(int n, char *out) { out[0] = 0; for (int i = 0; i < 5; i++) strcat(out, i < n ? "★" : "☆"); }
static const char *part_name(const gh3_song *s, int part) { return part == 1 ? (s->hasRhythm ? "Baixo" : "Guitarra base") : "Guitarra solo"; }

/* ------------------------------------------------------------------ rows */
void screens_build_rows(void) {
    gh3_library *lib = A.lib; A.nrows = 0; A.rows = realloc(A.rows, (lib->nsongs + lib->ntiers + 1) * sizeof *A.rows);
    for (int t = 0; t < lib->ntiers; t++) { int head = A.nrows; A.rows[A.nrows].kind = 0; A.rows[A.nrows].idx = t; A.rows[A.nrows].tier = t; A.nrows++;
        for (int k = 0; k < lib->tiers[t].nsongs; k++) { const gh3_song *s = &lib->songs[lib->tiers[t].songIdx[k]];
            if (!strncmp(s->name, "tutorial", 8)) continue;
            A.rows[A.nrows].kind = 1; A.rows[A.nrows].idx = lib->tiers[t].songIdx[k]; A.rows[A.nrows].tier = t; A.nrows++; }
        if (A.nrows == head + 1) A.nrows = head; }
    if (A.cursor >= A.nrows || A.cursor < 0) A.cursor = 0;
    while (A.cursor < A.nrows && A.rows[A.cursor].kind == 0) A.cursor++;
}
static void move_cursor(int d) { int c = A.cursor; do { c += d; if (c < 0 || c >= A.nrows) return; } while (A.rows[c].kind == 0); A.cursor = c; }
static int row_lock(int i) { return app_song_lock(A.rows[i].tier, A.rows[i].idx); }

/* ------------------------------------------------------------------ title and main menu */
static void draw_logo(int cx, int y, int big) {   /* the game's name in the pixel title font: GUITAR HERO over a large III */
    int s1 = big ? 6 : 3, s2 = big ? 9 : 4;
    ui_title(cx - gfx_text_w("GUITAR HERO", s1) / 2, y, s1, "GUITAR HERO", C_INK, C_SHADOW, big ? 4 : 2);
    ui_title(cx - gfx_text_w("III", s2) / 2, y + s1 * 13, s2, "III", C_AMP, C_SHADOW, big ? 5 : 3);
    gfx_text_sh_center(cx, y + s1 * 13 + s2 * 12 + 4, big ? 2 : 1, "LEGENDS OF ROCK", big ? C_INK : C_MUTED, C_SHADOW);
}
static void draw_title(double now) {
    render_backdrop(now, SET.stageFx); int W = A.W, H = A.H; double T = (now - A.titleAt) / 1000; float k = T < 0.7 ? (float)(T / 0.7) : 1;
    ui_dim(W, H, 70);
    draw_logo(W / 2, 60 - (int)(30 * (1 - k) * (1 - k)), 1);
    gfx_sprite *tu = art_get(ART_TGT_UP); int tw = gfx_sprite_w(tu), th = gfx_sprite_h(tu);   /* the five frets under the logo */
    for (int f = 0; f < 5; f++) { float b = (float)(0.5 + 0.5 * sin(T * 3 - f * 0.7)); gfx_blit(tu, 0, 0, tw, th, W / 2 - 130 + f * 52, 312, 48, 48.0f * th / tw, FRET_COLORS[f], 0.55f + 0.45f * b); }
    if (fmod(T, 1.1) < 0.7) gfx_text_sh_center(W / 2, H - 92, 2, "APERTE START", C_INK, C_SHADOW);
    gfx_text_center(W / 2, H - 30, 1, "port nativo para R36S • Select + Start fecha o jogo", C_LINE);
}
enum { MM_CAREER, MM_COOP, MM_QUICK, MM_MULTI, MM_TRAINING, MM_OPTIONS, MM_EXIT, MM_N };
static const char *MM_LABEL[MM_N] = { "CARREIRA", "CARREIRA CO-OP", "JOGO RÁPIDO", "MULTIJOGADOR", "TREINO", "OPÇÕES", "SAIR" };
static const int MM_OFF[MM_N] = { 0, 1, 0, 1, 0, 0, 0 };   /* two-player modes: shown as in the game, not playable here */
static const char *MM_HELP[MM_N] = {
    "Viva a vida de um astro do rock: toque os sets em ordem, libere músicas novas, vença as batalhas e feche cada set com o encore.",
    "Dois jogadores numa banda, guitarra solo e baixo, dividindo o medidor de rock. Precisa de dois controles: não dá no R36S.",
    "Toque qualquer música que você já liberou na carreira, em qualquer dificuldade. O recorde conta, a carreira não muda.",
    "Face-off, Pro Face-off e Batalha, dois jogadores um contra o outro. Precisa de dois controles: não dá no R36S.",
    "Tutoriais e treino de música: comece em qualquer seção, sem falhar, para pegar aquele solo.",
    "Áudio, calibração do atraso, controle, vídeo, Top Rockers e trapaças.",
    "Fecha o jogo e volta ao menu do aparelho." };
static void draw_note(double now) {   /* a short line over any menu ("Precisa de dois controles", …) */
    if (A.noteAt <= 0 || now - A.noteAt > 2600) return; float a = now - A.noteAt > 2100 ? (float)(1 - (now - A.noteAt - 2100) / 500) : 1;
    int w = gfx_text_w(A.note, 1) + 40; ui_panel(A.W / 2 - w / 2, A.H - 132, w, 30, UI_PANEL_DARK); gfx_text_sh_center(A.W / 2, A.H - 122, 1, A.note, fb_alpha(C_AMP, a), C_SHADOW);
}
static void note(double now, const char *msg) { snprintf(A.note, sizeof A.note, "%s", msg); A.noteAt = now; }
static void draw_menu_list(const char *const *labels, const int *off, int n, int sel, int x, int y, int step) {
    for (int i = 0; i < n; i++) { int yy = y + i * step, on = i == sel; uint32_t c = off[i] ? RGB(110, 104, 122) : on ? C_INK : RGB(210, 204, 196);
        if (on) { ui_panel(x - 14, yy - 7, gfx_text_w(labels[i], 3) + 52, step - 4, off[i] ? UI_PANEL_DARK : UI_PANEL_AMBER); gfx_sprite_at(art_get(ART_PICK), x - 8, yy - 1, 0.6f, RGB(255, 255, 255), 1); }
        ui_title(x + 16, yy, 3, labels[i], c, C_SHADOW, on ? 2 : 1); }
}
static void draw_main(double now) {
    render_backdrop(now, SET.stageFx); int W = A.W, H = A.H; char b[120];
    ui_dim(W, H, 60); draw_logo(176, 6, 0);
    draw_menu_list(MM_LABEL, MM_OFF, MM_N, A.mainRow, 46, 130, 36);
    /* the career at a glance, like the game's status board */
    int px = W - 228, py = 132; ui_panel(px, py, 208, 150, UI_PANEL_FLAT);
    gfx_text_sh(px + 14, py + 12, 1, "SUA CARREIRA", C_AMP, C_SHADOW);
    int ntier = 0; for (int t = 0; t < A.lib->ntiers; t++) ntier += A.lib->tiers[t].career;
    static int doneBy[4]; static double statsAt = -1e9;   /* recounted once a second, not every frame */
    if (now - statsAt > 1000 || now < statsAt) { statsAt = now; tierstate_t *ts = malloc(A.lib->ntiers * sizeof *ts); for (int d = 0; d < 4; d++) { career_eval(A.lib, d, SET.unlockAll, ts); doneBy[d] = career_tiers_done(A.lib, ts); } free(ts); }
    for (int d = 0; d < 4; d++) { int done = doneBy[d];
        snprintf(b, sizeof b, "%s", diff_name_pt(d)); gfx_text(px + 14, py + 36 + d * 22, 1, b, C_MUTED);
        snprintf(b, sizeof b, "%d/%d sets", done, ntier); gfx_text_sh_right(px + 194, py + 36 + d * 22, 1, b, done == ntier ? C_GOOD : C_INK, C_SHADOW);
        ui_bar(px + 70, py + 38 + d * 22, 60, 5, ntier ? (float)done / ntier : 0, done == ntier ? C_GOOD : C_AMP); }
    { gfx_sprite *st = fx_star(1, SET.theme >= 1); gfx_blit(st, 0, 0, gfx_sprite_w(st), gfx_sprite_h(st), px + 12, py + 120, 20, 20, RGB(255, 255, 255), 1); } snprintf(b, sizeof b, "%d estrelas", records_count_stars()); gfx_text_sh(px + 38, py + 126, 1, b, C_INK, C_SHADOW);
    ui_panel(14, H - 86, W - 28, 50, UI_PANEL_FLAT); gfx_text_box(26, H - 78, W - 52, 1, MM_HELP[A.mainRow], C_MUTED);
    static const char *bt[2] = { "A", "B" }, *tx[2] = { "Escolher", "Voltar" }; ui_hints(H - 24, W, bt, tx, 2);
    if (A.confirmQuit) { ui_dim(W, H, 150); ui_panel(W / 2 - 150, H / 2 - 50, 300, 100, UI_PANEL_DARK); gfx_text_sh_center(W / 2, H / 2 - 34, 2, "Sair do jogo?", C_INK, C_SHADOW); static const char *qb[2] = { "A", "B" }, *qt[2] = { "Sim, sair", "Ficar" }; ui_hints(H / 2 + 14, W, qb, qt, 2); }
}
enum { TR_TUTORIAL, TR_PRACTICE, TR_N };
static const char *TR_LABEL[TR_N] = { "TUTORIAIS", "TREINAR MÚSICA" };
static const int TR_OFF[TR_N] = { 0, 0 };
static const char *TR_HELP[TR_N] = {
    "As lições narradas do jogo, com o Deus do Rock e Lou: o básico, Star Power, batalhas, hammer-ons e pull-offs.",
    "Escolha uma música já liberada e a seção onde começar. A música não falha e nada é salvo: é só para treinar." };
static void draw_training(double now) {
    render_backdrop(now, SET.stageFx); int W = A.W, H = A.H; ui_header(W, "TREINO", NULL);
    draw_menu_list(TR_LABEL, TR_OFF, TR_N, A.trainRow, 46, 110, 44);
    ui_panel(14, H - 86, W - 28, 50, UI_PANEL_FLAT); gfx_text_box(26, H - 78, W - 52, 1, TR_HELP[A.trainRow], C_MUTED);
    static const char *bt[2] = { "A", "B" }, *tx[2] = { "Escolher", "Voltar" }; ui_hints(H - 24, W, bt, tx, 2);
}

/* ------------------------------------------------------------------ set list */
static void draw_setlist(double now) {
    if (SET.theme == 2 && orig_ready() && orig_setlist_draw(now)) return;
    render_backdrop(now, SET.stageFx); int W = A.W, H = A.H; gh3_library *lib = A.lib; char b[200], b2[64];
    int done = career_tiers_done(lib, A.tiers), ntier = 0, career = A.mode == MODE_CAREER; for (int t = 0; t < lib->ntiers; t++) ntier += lib->tiers[t].career;
    if (career) snprintf(b, sizeof b, "%s • %d/%d sets • ★ %d • %s", diff_name_pt(A.selDiff), done, ntier, records_count_stars(), store_cash_str()); else snprintf(b, sizeof b, "★ %d", records_count_stars());
    ui_header(W, career ? "CARREIRA" : A.mode == MODE_QUICK ? "JOGO RÁPIDO" : "TREINO", b);
    snprintf(b, sizeof b, career ? "Dificuldade da carreira:  ← %s →" : "Dificuldade:  ← %s →", diff_name_pt(A.selDiff)); gfx_text_sh(16, 66, 1, b, C_MUTED, C_SHADOW);
    gfx_text_right(W - 16, 66, 1, "L1 / R1 ou ← → mudam", C_LINE);
    int rowH = 22, top = 84, vis = (H - top - 34) / rowH;
    if (A.cursor < A.scroll) A.scroll = A.cursor; if (A.cursor >= A.scroll + vis) A.scroll = A.cursor - vis + 1; if (A.scroll < 0) A.scroll = 0;
    for (int i = A.scroll, y = top; i < A.nrows && i < A.scroll + vis; i++, y += rowH) {
        if (A.rows[i].kind == 0) { const gh3_tier *tr = &lib->tiers[A.rows[i].idx]; const tierstate_t *ts = &A.tiers[A.rows[i].idx];
            gfx_rect(10, y + 2, W - 20, rowH - 4, RGBA(60, 40, 15, 120)); gfx_rect(10, y + rowH - 3, W - 20, 1, C_AMP_DARK);
            const char *title = tr->tab == 1 ? "Músicas bônus" : tr->tab == 2 ? "Músicas baixadas" : tr->tab == 3 ? "Mais músicas" : tr->title;
            gfx_text_sh(18, y + 6, 1, title, ts->open || !career ? C_AMP : C_MUTED, C_SHADOW);
            if (tr->career && career) { if (!ts->open) snprintf(b, sizeof b, "bloqueado: termine o set anterior"); else if (ts->done) snprintf(b, sizeof b, "set concluído ✓"); else if (ts->regDone < ts->required) snprintf(b, sizeof b, "%d de %d músicas para liberar %s", ts->regDone, ts->required, tr->bossIdx >= 0 ? "a batalha" : "o encore"); else if (tr->bossIdx >= 0 && !ts->bossDone) snprintf(b, sizeof b, "vença a batalha para liberar o encore"); else snprintf(b, sizeof b, "toque o encore para fechar o set");
                gfx_text_right(W - 18, y + 6, 1, b, ts->done ? C_GOOD : C_MUTED); }
            else if (tr->tab == 1 && !SET.unlockAll) gfx_text_right(W - 18, y + 6, 1, career ? "à venda na Loja: aperte Y" : "à venda na Loja da carreira", C_MUTED);
            continue; }
        const gh3_song *s = &lib->songs[A.rows[i].idx]; int sel = i == A.cursor, lock = row_lock(i); const record_t *rc = records_get(s->name, A.selDiff, 0); const record_t *rc2 = records_get(s->name, A.selDiff, 1); if (!rc || (rc2 && rc2->score > rc->score)) rc = rc2;
        if (sel) { ui_panel(10, y - 1, W - 20, rowH, lock ? UI_PANEL_DARK : UI_PANEL_AMBER); gfx_sprite_at(art_get(ART_PICK), 16, y + 3, 0.5f, RGB(255, 255, 255), 1); }
        song_title(s, b, sizeof b); uint32_t tc = lock ? RGB(110, 104, 122) : sel ? C_INK : RGB(210, 204, 196);
        gfx_text_sh(34, y + 1, 2, b, tc, C_SHADOW);
        if (s->isBoss) gfx_sprite_at(art_get(ART_SKULL), 34 + gfx_text_w(b, 2) + 8, y + 1, 0.9f, RGB(255, 255, 255), lock ? 0.5f : 1);
        int tw = gfx_text_w(b, 2);
        if (lock) { const char *why = lock >= LOCK_BUY ? store_lock_text(A.rows[i].idx, career) : !career ? "libere na carreira" : lock == LOCK_TIER ? "bloqueado" : lock == LOCK_BOSS ? "batalha bloqueada" : "encore bloqueado"; gfx_text_right(W - 20, y + 6, 1, why, lock == LOCK_BUY ? C_AMP : RGB(110, 104, 122)); }
        else if (rc) { stars_str(rc->stars, b2); fmt_num(rc->score, b, sizeof b); gfx_text_right(W - 20, y + 6, 1, b, sel ? C_INK : C_MUTED); gfx_text_right(W - 20 - gfx_text_w(b, 1) - 12, y + 3, 2, b2, C_AMP); }
        else { snprintf(b, sizeof b, "%s%s%s", s->artist, *s->year ? " • " : "", s->year); if (gfx_text_w(b, 1) + tw + 60 < W) gfx_text_right(W - 20, y + 6, 1, b, sel ? C_MUTED : C_LINE); }
    }
    static const char *bt[4] = { "A", "B", "← →", "Y" }, *tx[4] = { "Escolher", "Voltar", "Dificuldade", "Loja" };
    ui_hints(H - 24, W, bt, tx, career ? 4 : 3);
    if (A.unlockAt > 0 && now - A.unlockAt < 4500) { float a = now - A.unlockAt > 3500 ? (float)(1 - (now - A.unlockAt - 3500) / 1000) : 1; int w = gfx_text_w(A.unlockMsg, 2) + 40; ui_panel(W / 2 - w / 2, H / 2 - 30, w, 60, UI_PANEL_AMBER); gfx_sprite_at(art_get(ART_CROWN), W / 2 - w / 2 + 10, H / 2 - 20, 1, RGB(255, 255, 255), a); gfx_text_sh_center(W / 2 + 10, H / 2 - 11, 2, A.unlockMsg, fb_alpha(C_INK, a), C_SHADOW); }
}
/* ------------------------------------------------------------------ setup */
enum { SR_DIFF, SR_PART, SR_SECTION, SR_STRUM, SR_HYPER, SR_PLAY, SR_END, SR_SPEED };
static int setup_rows(int *out) { int n = 0; out[n++] = SR_DIFF; out[n++] = SR_PART; if (A.mode == MODE_PRACTICE) { out[n++] = SR_SECTION; out[n++] = SR_END; out[n++] = SR_SPEED; } out[n++] = SR_STRUM; out[n++] = SR_HYPER; out[n++] = SR_PLAY; return n; }
static void section_name(int k, char *out, size_t n) { if (k <= 0 || k > A.S.sd.nmarkers) snprintf(out, n, "Do começo"); else { const char *t = app_section_at(A.S.sd.markers[k - 1].t); snprintf(out, n, "%d. %s", k, *t ? t : "Seção"); } }
/* practice: the last section runs from the first one chosen to the one before the last ("Até o fim" covers that) */
static void end_change(int d) { int lo = A.practiceSec > 1 ? A.practiceSec : 1, hi = A.S.sd.nmarkers - 1, cnt = 1 + (hi >= lo ? hi - lo + 1 : 0);
    int i = A.practiceEnd >= lo && A.practiceEnd <= hi ? A.practiceEnd - lo : cnt - 1; i = (i + cnt + d) % cnt; A.practiceEnd = i == cnt - 1 ? 0 : lo + i; }
static void draw_setup(double now) {
    if (SET.theme == 2 && orig_ready()) { int r_[8], n_ = setup_rows(r_); if (orig_setup_draw(now, r_, n_)) return; }
    render_backdrop(now, SET.stageFx); int W = A.W, H = A.H; const gh3_song *s = &A.lib->songs[A.selSong]; char b[200], t[160];
    song_title(s, t, sizeof t); ui_header(W, "PREPARAR", NULL);
    gfx_text_sh(16, 70, 3, t, C_INK, C_SHADOW); snprintf(b, sizeof b, "%s%s%s", s->artist, *s->year ? " • " : "", s->year); gfx_text_sh(16, 106, 1, b, C_MUTED, C_SHADOW);
    int rows[8], nr = setup_rows(rows), step = nr > 6 ? 32 : nr > 5 ? 34 : 40, y0 = nr > 6 ? 122 : 132; char hv[16], sec[80], endv[80], spv[40];
    if (SET.hyper) snprintf(hv, sizeof hv, "+%d", SET.hyper); else snprintf(hv, sizeof hv, "Normal"); section_name(A.practiceSec, sec, sizeof sec);
    if (A.practiceEnd > 0) section_name(A.practiceEnd, endv, sizeof endv); else snprintf(endv, sizeof endv, "Até o fim");
    int spk = A.practiceSpeed >= 0 && A.practiceSpeed < PRACTICE_SPEEDS ? A.practiceSpeed : 0; snprintf(spv, sizeof spv, "%s (%d%%)", PRACTICE_SPEED_NAME[spk], (int)lround(PRACTICE_SPEED[spk] * 100));
    int lock = app_song_lock(A.selTier, A.selSong);
    for (int i = 0; i < nr; i++) { int y = y0 + i * step, r = rows[i];
        switch (r) { case SR_DIFF: ui_option(30, y, W - 60, "Dificuldade", diff_name_pt(A.selDiff), A.setupRow == i, 1); break; case SR_PART: ui_option(30, y, W - 60, "Parte", part_name(s, A.selPart), A.setupRow == i, 1); break;
            case SR_SECTION: ui_option(30, y, W - 60, "Começar em", sec, A.setupRow == i, 1); break; case SR_STRUM: ui_option(30, y, W - 60, "Palhetada", SET.tap ? "Toque livre" : "Normal", A.setupRow == i, 1); break;
            case SR_END: ui_option(30, y, W - 60, "Terminar em", endv, A.setupRow == i, 1); break; case SR_SPEED: ui_option(30, y, W - 60, "Velocidade", spv, A.setupRow == i, 1); break;
            case SR_HYPER: ui_option(30, y, W - 60, "Velocidade da pista", hv, A.setupRow == i, 1); break;
            default: if (A.setupRow == i) ui_panel(22, y - 8, W - 44, 34, lock ? UI_PANEL_DARK : UI_PANEL_AMBER); gfx_text_sh_center(W / 2, y, 2, lock ? "Bloqueada nesta dificuldade" : A.mode == MODE_PRACTICE ? "♪  Treinar!  ♪" : "♪  Tocar!  ♪", lock ? C_MUTED : (A.setupRow == i ? C_INK : C_AMP), C_SHADOW); } }
    /* the record and the chart */
    const record_t *rc = records_get(s->name, A.selDiff, A.selPart); int y = nr > 6 ? 376 : 344;
    ui_panel(16, y, W - 32, 62, UI_PANEL_FLAT);
    if (A.mode == MODE_PRACTICE) { gfx_text_sh(28, y + 10, 1, "Treino: não falha, sem Star Power, nada é salvo.", C_SP, C_SHADOW);
        if (rc) { char n[32], st[32]; fmt_num(rc->score, n, sizeof n); stars_str(rc->stars, st); snprintf(b, sizeof b, "Seu recorde: %s pontos  %s", n, st); gfx_text(28, y + 28, 1, b, C_MUTED); }
        const gh3_partdiff *pd = A.S.sd.parts[A.selPart][A.selDiff].raw ? &A.S.sd.parts[A.selPart][A.selDiff] : &A.S.sd.parts[A.selPart ? 0 : 1][A.selDiff];   /* the stretch chosen: its notes and how long it takes */
        double end = A.S.sd.nfret ? A.S.sd.fretbars[A.S.sd.nfret - 1] : 0, from, to; practice_range(&A.S.sd, A.practiceSec, A.practiceEnd, end, &from, &to); int cnt = 0;
        for (int i = 0; i + 2 < pd->nraw; i += 3) cnt += pd->raw[i] >= from - 1 && pd->raw[i] < to;
        int sec2 = (int)((to - from) / PRACTICE_SPEED[spk] / 1000 + 0.5); snprintf(b, sizeof b, "Trecho: %d notas • %d:%02d a %d%% da velocidade", cnt, sec2 / 60, sec2 % 60, (int)lround(PRACTICE_SPEED[spk] * 100)); gfx_text(28, y + 44, 1, b, C_LINE); }
    else if (rc) { char n[32], st[32]; fmt_num(rc->score, n, sizeof n); stars_str(rc->stars, st); snprintf(b, sizeof b, "Seu recorde: %s pontos  %s", n, st); gfx_text_sh(28, y + 10, 1, b, C_AMP, C_SHADOW);
        snprintf(b, sizeof b, "%d%% das notas • maior sequência %d • tocada %d %s", rc->pct, rc->streak, rc->plays, rc->plays == 1 ? "vez" : "vezes"); gfx_text(28, y + 28, 1, b, C_MUTED); }
    else { gfx_text_sh(28, y + 10, 1, "Ainda sem recorde nesta parte e dificuldade.", C_MUTED, C_SHADOW); }
    int charted = A.S.sd.parts[A.selPart][A.selDiff].raw != NULL; int notes = charted ? A.S.sd.parts[A.selPart][A.selDiff].nraw / 3 : (A.S.sd.parts[A.selPart ? 0 : 1][A.selDiff].raw ? A.S.sd.parts[A.selPart ? 0 : 1][A.selDiff].nraw / 3 : 0);
    snprintf(b, sizeof b, "%s em %s: %d notas%s", part_name(s, A.selPart), diff_name_pt(A.selDiff), notes, charted ? "" : " (esta parte usa a partitura da outra)"); if (A.mode != MODE_PRACTICE) gfx_text(28, y + 44, 1, b, C_LINE);
    static const char *bt[3] = { "A", "B", "← →" }, *tx[3] = { "Tocar", "Voltar", "Mudar" }; ui_hints(H - 24, W, bt, tx, 3);
    encore_draw(now);
}
/* ------------------------------------------------------------------ options */
enum { OPT_VOLB, OPT_VOLG, OPT_VOLS, OPT_MENUMUS, OPT_PREVIEW, OPT_OFFSET, OPT_LEFTY, OPT_STRUM, OPT_BUTTONS, OPT_THEME, OPT_STAGE, OPT_DIM, OPT_FPS, OPT_HYPER, OPT_NOFAIL, OPT_UNLOCK, OPT_INTRO, OPT_STAGE3D, OPT_BAND3D, OPT_RES3D, OPT_N };   /* OPT_STAGE3D, OPT_BAND3D, OPT_RES3D: listed in the gh3-3d build only */
static const char *OPT_LABEL[OPT_N] = { "Volume da banda", "Volume da guitarra", "Volume dos efeitos", "Música do menu", "Prévia das músicas", "Atraso do áudio", "Canhoto (lefty flip)", "Palhetada", "Botões…",
    "Tema visual", "Efeitos de palco", "Escurecer o fundo", "Mostrar FPS", "Hyperspeed", "Sem falhar (No Fail)", "Liberar todas as músicas", "Vídeos de abertura", "Palco 3D", "Banda 3D", "Resolução do palco 3D" };
static const char *OPT_HELP[OPT_N] = {
    "O volume da banda (as outras faixas da música) e da música do menu.", "O volume da parte que você toca. Ela silencia quando você erra, como no jogo.", "Volume dos sons do menu, erros e star power.",
    "As músicas do menu do próprio jogo tocam nos menus.", "Na lista de músicas, toca um trecho da música selecionada no lugar da música do menu.",
    "Compensa o atraso do som do aparelho, em milissegundos. Se as notas passam pela linha antes de você ouvir, aumente. O melhor é medir em Calibrar atraso.",
    "Espelha a pista para quem toca com a mão esquerda: o verde fica à direita.",
    "Normal: palhete com o direcional, como na guitarra. Toque livre: como o GH3 no controle comum do PS2, apertar o botão do traste já é a palhetada.",
    "Troque os botões de cada traste, da palhetada, do star power e da alavanca.",
    "Pixel art: o visual retrô desenhado pixel a pixel. HD suave: formas lisas com antialiasing. Original: a arte 2D do próprio jogo (precisa de DATA/ZONES/global.pak.xen e global.pab.xen).", "Plateia, luzes e feixes atrás da pista. Desligue se o aparelho engasgar.",
    "Quanto mais escuro o palco atrás da pista, mais fácil enxergar as notas.", "Quadros por segundo no alto da tela.",
    "Notas mais espaçadas e rápidas na pista. Não muda a dificuldade.", "O medidor de rock nunca encerra a música. A pontuação aparece, mas não vale recorde nem carreira.",
    "Abre todos os sets sem passar pela carreira. Os recordes continuam valendo.",
    "Os logotipos e a abertura do jogo ao ligar. A ou B pula um vídeo, Start pula todos.",
    "O palco do próprio jogo em 3D atrás da pista (o cenário do set, com as câmeras do jogo). Desligue para o palco em pixel art.",
    "Os músicos do jogo no palco 3D: o guitarrista, o baixista (ou o chefe numa batalha), o baterista e o vocalista, tocando com a música. Vale a partir da próxima música. Desligue se o aparelho engasgar.",
    "O tamanho em que o cenário 3D é desenhado antes de ser ampliado atrás da pista: Alta (a tela inteira), Média (3/4, mais leve) ou Baixa (metade, a mais leve). A pista e o placar continuam nítidos." };
enum { CAT_AUDIO, CAT_CALIB, CAT_CONTROL, CAT_VIDEO, CAT_TOP, CAT_CHEATS, CAT_RESET, CAT_N };
static const char *CAT_LABEL[CAT_N] = { "ÁUDIO", "CALIBRAR ATRASO", "CONTROLE", "VÍDEO", "TOP ROCKERS", "TRAPAÇAS", "RESTAURAR PADRÕES" };
static const char *CAT_HELP[CAT_N] = {
    "Volumes da banda, da guitarra e dos efeitos, a música do menu, as prévias e o atraso do áudio.",
    "Toca um clique e você palheta no ritmo: o jogo mede o atraso do som e ajusta as notas e o julgamento. Faça antes de jogar.",
    "Canhoto, modo de palhetada e os botões de cada ação.", "Palco 3D e a sua resolução (no GH3 3D), efeitos de palco, escurecer o fundo, FPS e os vídeos de abertura.",
    "Os seus melhores resultados em cada música.", "As trapaças do jogo: toque os códigos nos trastes ou ligue cada uma direto.", "Volta todas as opções e controles ao padrão (recordes e carreira ficam)." };
#ifdef GH3_3D
#define VIDEO_ITEMS { OPT_STAGE3D, OPT_RES3D, OPT_BAND3D, OPT_STAGE, OPT_DIM, OPT_FPS, OPT_INTRO, -1 }
#else
#define VIDEO_ITEMS { OPT_STAGE, OPT_DIM, OPT_FPS, OPT_INTRO, -1 }
#endif
static const int CAT_ITEMS[CAT_N][8] = { { OPT_VOLB, OPT_VOLG, OPT_VOLS, OPT_MENUMUS, OPT_PREVIEW, OPT_OFFSET, -1 }, { -1 }, { OPT_LEFTY, OPT_STRUM, OPT_BUTTONS, -1 },
    VIDEO_ITEMS, { -1 }, { OPT_HYPER, OPT_NOFAIL, OPT_UNLOCK, -1 }, { -1 } };
static int cat_count(int c) { int n = 0; while (n < 8 && CAT_ITEMS[c][n] >= 0) n++; return n; }
static int cat_off(int c) { return A.optionsFrom == SC_PAUSE && (c == CAT_CALIB || c == CAT_TOP || c == CAT_RESET); }
static void opt_value(int i, char *out, size_t n) {
    switch (i) { case OPT_THEME: snprintf(out, n, "%s", render_theme_name(SET.theme)); break; case OPT_STRUM: snprintf(out, n, "%s", SET.tap ? "Toque livre" : "Normal"); break; case OPT_LEFTY: snprintf(out, n, "%s", SET.lefty ? "Sim" : "Não"); break;
    case OPT_HYPER: if (SET.hyper) snprintf(out, n, "+%d", SET.hyper); else snprintf(out, n, "Desligada"); break; case OPT_NOFAIL: snprintf(out, n, "%s", SET.noFail ? "Ligado" : "Desligado"); break;
    case OPT_STAGE: snprintf(out, n, "%s", SET.stageFx ? "Ligados" : "Desligados"); break; case OPT_DIM: snprintf(out, n, "%d%%", SET.bgDim); break; case OPT_PREVIEW: snprintf(out, n, "%s", SET.preview ? "Ligada" : "Desligada"); break;
    case OPT_MENUMUS: snprintf(out, n, "%s", SET.menuMusic ? "Ligada" : "Desligada"); break;
    case OPT_VOLB: snprintf(out, n, "%d%%", SET.volMusic); break; case OPT_VOLG: snprintf(out, n, "%d%%", SET.volGuitar); break; case OPT_VOLS: snprintf(out, n, "%d%%", SET.volSfx); break; case OPT_OFFSET: snprintf(out, n, "%+d ms", SET.offsetMs); break;
    case OPT_FPS: snprintf(out, n, "%s", SET.showFps ? "Sim" : "Não"); break; case OPT_UNLOCK: snprintf(out, n, "%s", SET.unlockAll ? "Ligado" : "Desligado"); break;
    case OPT_INTRO: snprintf(out, n, "%s", SET.introMovies ? "Ligados" : "Desligados"); break; case OPT_STAGE3D: snprintf(out, n, "%s", SET.stage3d ? "Ligado" : "Desligado"); break; case OPT_BAND3D: snprintf(out, n, "%s", SET.band3d ? "Ligada" : "Desligada"); break;
    case OPT_RES3D: { static const char *RN[3] = { "Alta", "Média", "Baixa" }; snprintf(out, n, "%s", RN[SET.stage3dRes < 0 || SET.stage3dRes > 2 ? 1 : SET.stage3dRes]); break; } default: out[0] = 0; }
}
static int clamp100(int v) { return v < 0 ? 0 : v > 100 ? 100 : v; }
static void opt_change(int i, int d) {
    switch (i) { case OPT_THEME: SET.theme = (SET.theme + THEME_N + d) % THEME_N; break; case OPT_STRUM: SET.tap = !SET.tap; break; case OPT_LEFTY: SET.lefty = !SET.lefty; break; case OPT_HYPER: SET.hyper = (SET.hyper + 6 + d) % 6; break; case OPT_NOFAIL: SET.noFail = !SET.noFail; break;
    case OPT_STAGE: SET.stageFx = !SET.stageFx; break; case OPT_DIM: SET.bgDim = clamp100(SET.bgDim + d * 10); break; case OPT_PREVIEW: SET.preview = !SET.preview; if (!SET.preview) app_preview(-1); break;
    case OPT_MENUMUS: SET.menuMusic = !SET.menuMusic; break;
    case OPT_VOLB: SET.volMusic = clamp100(SET.volMusic + d * 10); app_apply_volumes(); break; case OPT_VOLG: SET.volGuitar = clamp100(SET.volGuitar + d * 10); app_apply_volumes(); break;
    case OPT_VOLS: SET.volSfx = clamp100(SET.volSfx + d * 10); app_apply_volumes(); break;
    case OPT_OFFSET: SET.offsetMs += d * 5; if (SET.offsetMs < -300) SET.offsetMs = -300; if (SET.offsetMs > 300) SET.offsetMs = 300; break;
    case OPT_FPS: SET.showFps = !SET.showFps; break; case OPT_UNLOCK: SET.unlockAll = !SET.unlockAll; app_refresh_career(0); break; case OPT_INTRO: SET.introMovies = !SET.introMovies; break; case OPT_STAGE3D: SET.stage3d = !SET.stage3d; break; case OPT_BAND3D: SET.band3d = !SET.band3d; break; case OPT_RES3D: SET.stage3dRes = (SET.stage3dRes + 3 + d) % 3; break; }
}
static void draw_options(double now) {
    if (SET.theme == 2 && orig_ready() && orig_options_draw(now, CAT_LABEL, CAT_HELP, CAT_ITEMS, CAT_N, OPT_LABEL, OPT_HELP, opt_value, cat_off)) return;
    if (A.optionsFrom == SC_PAUSE) { hud_info hi = { A.S.song->title, A.S.song->artist, A.S.section, 0, SET.lefty, SET.stageFx, SET.bgDim, 0, SET.noFail, NULL }; render_game(&A.S.run, now, &hi); ui_dim(A.W, A.H, 200); } else render_backdrop(now, SET.stageFx);
    int W = A.W, H = A.H; char v[40];
    if (A.optCat < 0) {   /* the categories, as the game's Options menu lists them */
        ui_header(W, "OPÇÕES", NULL); int off[CAT_N]; for (int c = 0; c < CAT_N; c++) off[c] = cat_off(c);
        draw_menu_list(CAT_LABEL, off, CAT_N, A.optRow, 46, 72, 38);
        ui_panel(14, H - 86, W - 28, 50, UI_PANEL_FLAT); gfx_text_box(26, H - 78, W - 52, 1, cat_off(A.optRow) ? "Não dá para abrir durante a música." : CAT_HELP[A.optRow], C_MUTED);
        static const char *bt[2] = { "A", "B" }, *tx[2] = { "Abrir", "Voltar" }; ui_hints(H - 24, W, bt, tx, 2); return; }
    ui_header(W, CAT_LABEL[A.optCat], "salvas ao sair");
    int n = cat_count(A.optCat), rowH = 30, top = 80; if (A.optRow >= n) A.optRow = n - 1;   /* a row past the category (one that lost an item) */
    for (int k = 0; k < n; k++) { int i = CAT_ITEMS[A.optCat][k], y = top + k * rowH, sel = k == A.optRow; opt_value(i, v, sizeof v);
        if (sel) { ui_panel(14, y - 7, W - 28, rowH - 2, UI_PANEL_AMBER); gfx_sprite_at(art_get(ART_PICK), 20, y - 3, 0.5f, RGB(255, 255, 255), 1); }
        gfx_text_sh(36, y, 2, OPT_LABEL[i], sel ? C_INK : C_MUTED, C_SHADOW);
        if (v[0]) { char b[60]; if (sel) snprintf(b, sizeof b, "← %s →", v); else snprintf(b, sizeof b, "%s", v); gfx_text_sh_right(W - 30, y + 2, 1, b, sel ? C_AMP : C_INK, C_SHADOW); }
        if (i == OPT_VOLB || i == OPT_VOLG || i == OPT_VOLS || i == OPT_DIM) ui_bar(W - 30 - 150 - 70, y + 4, 60, 8, (i == OPT_VOLB ? SET.volMusic : i == OPT_VOLG ? SET.volGuitar : i == OPT_VOLS ? SET.volSfx : SET.bgDim) / 100.0f, sel ? C_AMP : C_MUTED); }
    ui_panel(14, H - 96, W - 28, 60, UI_PANEL_FLAT); gfx_text_box(26, H - 88, W - 52, 1, OPT_HELP[CAT_ITEMS[A.optCat][A.optRow]], C_MUTED);
    static const char *bt[3] = { "← →", "A", "B" }, *tx[3] = { "Mudar", "Abrir", "Voltar" }; ui_hints(H - 24, W, bt, tx, 3);
}
/* ------------------------------------------------------------------ top rockers */
static void draw_toprock(double now) {
    render_backdrop(now, SET.stageFx); int W = A.W, H = A.H; gh3_library *lib = A.lib; char b[160], b2[64];
    snprintf(b, sizeof b, "← %s →", diff_name_pt(A.topDiff)); ui_header(W, "TOP ROCKERS", b);
    int rowH = 22, top = 70, vis = (H - top - 34) / rowH, cur = A.topScroll;
    int first = cur - vis / 2; if (first > A.nrows - vis) first = A.nrows - vis; if (first < 0) first = 0;
    for (int i = first, y = top; i < A.nrows && i < first + vis; i++, y += rowH) {
        if (A.rows[i].kind == 0) { const gh3_tier *tr = &lib->tiers[A.rows[i].idx]; gfx_rect(10, y + 2, W - 20, rowH - 4, RGBA(60, 40, 15, 120)); gfx_text_sh(18, y + 6, 1, tr->tab == 1 ? "Músicas bônus" : tr->tab == 2 ? "Músicas baixadas" : tr->tab == 3 ? "Mais músicas" : tr->title, C_AMP, C_SHADOW); continue; }
        const gh3_song *s = &lib->songs[A.rows[i].idx]; const record_t *rc = records_get(s->name, A.topDiff, 0), *rc2 = records_get(s->name, A.topDiff, 1); if (!rc || (rc2 && rc2->score > rc->score)) rc = rc2;
        if (i == cur) ui_panel(10, y - 1, W - 20, rowH, UI_PANEL_AMBER);
        song_title(s, b, sizeof b); gfx_text_sh(24, y + 6, 1, b, rc ? C_INK : C_MUTED, C_SHADOW);
        if (rc) { stars_str(rc->stars, b2); fmt_num(rc->score, b, sizeof b); gfx_text_sh_right(W - 24, y + 6, 1, b, C_INK, C_SHADOW); gfx_text_right(W - 120, y + 3, 2, b2, C_AMP); snprintf(b, sizeof b, "%d%%", rc->pct); gfx_text_right(W - 236, y + 6, 1, b, C_MUTED); }
        else gfx_text_right(W - 24, y + 6, 1, "—", C_LINE); }
    static const char *bt[3] = { "↑↓", "← →", "B" }, *tx[3] = { "Rolar", "Dificuldade", "Voltar" }; ui_hints(H - 24, W, bt, tx, 3);
}
/* ------------------------------------------------------------------ controls */
#define CTL_ROWS (ACT_N + 2)   /* profile, actions, reset */
static void draw_controls(double now) {
    if (SET.theme == 2 && orig_ready() && orig_controls_draw(now)) return;
    render_backdrop(now, SET.stageFx); int W = A.W, H = A.H; char b[80];
    ui_header(W, "CONTROLES", "A: trocar   X: apagar   ← →: coluna");
    gfx_text(200, 62, 1, "1º", C_LINE); gfx_text(345, 62, 1, "2º", C_LINE); gfx_text(490, 62, 1, "3º", C_LINE);
    int top = 74, rowH = 20;
    for (int r = 0; r < CTL_ROWS; r++) { int y = top + r * rowH; int sel = r == A.ctlRow;
        if (sel) ui_panel(12, y - 4, W - 24, rowH, UI_PANEL_AMBER);
        if (r == 0) { gfx_text_sh(24, y, 1, "Perfil", sel ? C_INK : C_MUTED, C_SHADOW); snprintf(b, sizeof b, sel ? "← %s →" : "%s", input_preset_name(A.preset)); gfx_text_sh_right(W - 24, y, 1, b, sel ? C_AMP : C_INK, C_SHADOW); continue; }
        if (r == CTL_ROWS - 1) { gfx_text_sh_center(W / 2, y, 1, "Restaurar o perfil escolhido", sel ? C_INK : C_MUTED, C_SHADOW); continue; }
        int a = r - 1; gfx_text_sh(24, y, 1, input_action_name(a), sel ? C_INK : C_MUTED, C_SHADOW);
        if (a < 5) gfx_rect(14, y + 2, 6, 7, FRET_COLORS[a]);
        for (int s = 0; s < BIND_SLOTS; s++) { input_bind_name(&SET.binds[a][s], b, sizeof b); int x = 200 + s * 145; int hot = sel && A.ctlSlot == s;
            if (hot) gfx_rect(x - 6, y - 3, 138, rowH - 2, RGBA(255, 255, 255, 30)); gfx_text_sh(x, y, 1, b, hot ? C_AMP : SET.binds[a][s].type ? C_INK : C_LINE, C_SHADOW); } }
    /* the live test: the five targets, the strum, star power and the whammy */
    int ty = H - 86; ui_panel(12, ty - 10, W - 24, 60, UI_PANEL_FLAT); gfx_text(24, ty - 2, 1, "Teste: aperte os botões", C_MUTED);
    gfx_sprite *tu = art_get(ART_TGT_UP), *td = art_get(ART_TGT_DOWN); int tw = gfx_sprite_w(tu), th = gfx_sprite_h(tu);
    for (int f = 0; f < 5; f++) { int down = A.testHeld & (1 << f); float x = 40 + f * 44; gfx_blit(down ? td : tu, 0, 0, tw, th, x, (float)ty + 10, 40, 40.0f * th / tw, FRET_COLORS[f], 1); }
    float sa = (float)(now - A.testStrumAt); int strumOn = sa < 150; gfx_text_sh(270, ty + 16, 2, A.testStrum < 0 ? "↑" : "↓", strumOn ? C_INK : C_LINE, C_SHADOW); gfx_text(290, ty + 20, 1, "palhetada", strumOn ? C_INK : C_LINE);
    gfx_sprite_at(art_get(ART_BOLT), 370, ty + 12, 0.7f, A.testStar ? C_SP : C_LINE, 1); gfx_text(386, ty + 20, 1, "star power", A.testStar ? C_SP : C_LINE);
    gfx_text(470, ty + 8, 1, "alavanca", C_MUTED); ui_bar(470, ty + 22, 140, 10, A.testWhammy, C_AMP);
    if (A.capturing) { ui_dim(W, H, 170); ui_panel(W / 2 - 200, H / 2 - 50, 400, 100, UI_PANEL_SP); snprintf(b, sizeof b, "%s", input_action_name(A.ctlRow - 1)); gfx_text_sh_center(W / 2, H / 2 - 36, 2, b, C_INK, C_SHADOW);
        gfx_text_center(W / 2, H / 2 - 6, 1, "Aperte um botão ou mova um analógico", C_SP); char cb[60]; snprintf(cb, sizeof cb, "(sem apertar nada, cancela em %d s)", (int)ceil((A.capAt + 5000 - now) / 1000)); gfx_text_center(W / 2, H / 2 + 14, 1, cb, C_MUTED);
        if (now - A.capAt > 5000) A.capturing = 0; }
    static const char *bt[3] = { "A", "X", "B" }, *tx[3] = { "Trocar", "Apagar", "Voltar" }; ui_hints(H - 22, W, bt, tx, 3);
}
/* ------------------------------------------------------------------ pause / results / error */
static void draw_pause(double now) {
    hud_info hi = { A.S.song->title, A.S.song->artist, A.S.section, 0, SET.lefty, SET.stageFx, SET.bgDim, 0, SET.noFail }; if (BTL.on) render_battle(now, 0); else render_game(&A.S.run, now, &hi);
    int W = A.W, H = A.H; ui_dim(W, H, 170); ui_panel(W / 2 - 140, 120, 280, 230, UI_PANEL_DARK);
    ui_title(W / 2 - gfx_text_w("PAUSA", 4) / 2, 134, 4, "PAUSA", C_INK, C_SHADOW, 3);
    const char *it[4] = { "Continuar", "Reiniciar", "Opções", "Sair da música" };
    for (int i = 0; i < 4; i++) { int y = 196 + i * 34; if (i == A.pauseRow) { ui_panel(W / 2 - 120, y - 7, 240, 30, UI_PANEL_AMBER); gfx_sprite_at(art_get(ART_PICK), W / 2 - 112, y - 3, 0.5f, RGB(255, 255, 255), 1); } gfx_text_sh_center(W / 2 + 6, y, 2, it[i], i == A.pauseRow ? C_INK : C_MUTED, C_SHADOW); }
}
static void draw_results(double now) {
    if (SET.theme == 2 && orig_ready() && orig_results_draw(now)) return;
    render_backdrop(now, SET.stageFx); gh3_run *r = &A.S.run; char b[120], n[32]; int W = A.W, H = A.H; double T = (now - A.resultsAt) / 1000;
    ui_header(W, r->failed ? "QUE PENA…" : "VOCÊ ARRASOU!", NULL);
    song_title(A.S.song, b, sizeof b); gfx_text_sh_center(W / 2, 70, 2, b, C_INK, C_SHADOW); snprintf(b, sizeof b, "%s • %s • %s", A.S.song->artist, part_name(A.S.song, A.S.part), diff_name_pt(A.S.diff)); gfx_text_center(W / 2, 96, 1, b, C_MUTED);
    fmt_num((long)r->score, n, sizeof n); ui_title(W / 2 - gfx_text_w(n, 5) / 2, 120, 5, n, r->failed ? C_MUTED : C_AMP, C_SHADOW, 4); gfx_text_center(W / 2, 180, 1, "PONTOS", C_MUTED);
    if (BTL.on) battle_result_lines(W / 2, 194);   /* a battle: who won and the attacks */
    else if (!r->failed) render_stars(W / 2, 200, A.resultStars, 3, T);
    else { gfx_sprite_draw(art_get(ART_SKULL), W / 2, 216, 1.5f, RGB(255, 255, 255), 1); gfx_text_center(W / 2, 236, 1, "A plateia vaiou… tente de novo!", C_BAD); }
    int total = r->nnotes, hit = r->hits; int y = 256;
    ui_panel(W / 2 - 220, y - 8, 440, 96, UI_PANEL_FLAT);
    snprintf(b, sizeof b, "Notas acertadas"); gfx_text(W / 2 - 200, y, 1, b, C_MUTED); snprintf(b, sizeof b, "%d / %d  (%d%%)", hit, total, A.resultPct); gfx_text_sh_right(W / 2 + 200, y - 3, 2, b, C_INK, C_SHADOW);
    gfx_text(W / 2 - 200, y + 26, 1, "Maior sequência", C_MUTED); snprintf(b, sizeof b, "%d", r->best); gfx_text_sh_right(W / 2 + 200, y + 23, 2, b, C_INK, C_SHADOW);
    gfx_text(W / 2 - 200, y + 52, 1, "Palhetadas erradas", C_MUTED); snprintf(b, sizeof b, "%d", r->overstrums); gfx_text_sh_right(W / 2 + 200, y + 49, 2, b, r->overstrums ? C_BAD : C_GOOD, C_SHADOW);
    ui_bar(W / 2 - 200, y + 14, 180, 6, A.resultPct / 100.0f, A.resultPct >= 95 ? C_GOOD : A.resultPct >= 80 ? C_AMP : C_BAD);
    if (A.newRecord && !r->failed && T > 1.9) { float k = (float)fmin(1, (T - 1.9) / 0.3); int w = 230; ui_panel(W / 2 - w / 2, 356 - (int)(10 * (1 - k)), w, 32, UI_PANEL_AMBER); gfx_sprite_at(art_get(ART_CROWN), W / 2 - w / 2 + 10, 362, 1, RGB(255, 255, 255), 1); gfx_text_sh(W / 2 - w / 2 + 40, 362, 2, "NOVO RECORDE!", C_INK, C_SHADOW); }
    if (A.mode == MODE_PRACTICE) { int k = A.practiceSpeed > 0 && A.practiceSpeed < PRACTICE_SPEEDS ? A.practiceSpeed : 0; snprintf(b, sizeof b, "Treino a %d%% da velocidade: nada é salvo.", (int)lround(PRACTICE_SPEED[k] * 100)); gfx_text_center(W / 2, 362, 1, b, C_SP); }
    else if (SET.noFail && !r->failed) gfx_text_center(W / 2, 362, 1, "Modo sem falhar: não vale para recordes nem para a carreira.", C_MUTED);
    else if (!cheats_records_ok(A.mode) && !r->failed) gfx_text_center(W / 2, 362, 1, "Easy Expert: não vale para recordes.", C_MUTED);
    else if (A.mode == MODE_QUICK && !r->failed && !A.newRecord) gfx_text_center(W / 2, 362, 1, "Jogo rápido: o recorde conta, a carreira não muda.", C_MUTED);
    store_draw_result(W, 200, T);
    const char *it[2] = { "Tocar de novo", A.mode == MODE_PRACTICE ? "Mudar o treino" : store_encore_offer() >= 0 ? "Encore!" : "Lista de músicas" }; int by = 404;
    for (int i = 0; i < 2; i++) { int x = W / 2 - 236 + i * 244; if (i == A.resRow) ui_panel(x, by - 7, 228, 30, UI_PANEL_AMBER); gfx_text_sh_center(x + 114, by, 2, it[i], i == A.resRow ? C_INK : C_MUTED, C_SHADOW); }
    static const char *bt[2] = { "A", "← →" }, *tx[2] = { "Confirmar", "Escolher" }; ui_hints(H - 22, W, bt, tx, 2);
}
static void draw_calib(double now) {
    if (SET.theme == 2 && orig_ready() && orig_calib_draw(now)) return;
    int W = A.W, H = A.H; char b[120]; double raw = audio_song_ms(); int med = A.calN >= 4 ? app_calib_result() : SET.offsetMs;
    render_calib(raw - med, 500, now, A.calN, A.calLastDiff, A.calLastTapAt);
    ui_header(W, "CALIBRAR ATRASO", NULL);
    ui_panel(14, 66, W - 28, 64, UI_PANEL_FLAT);
    gfx_text_box(26, 74, W - 52, 1, "Palhete (direcional ↑ ou ↓) junto com cada clique, de preferência ouvindo e sem olhar a pista. Depois de uns 10 toques o atraso medido fica estável.", C_INK);
    snprintf(b, sizeof b, "Atual: %+d ms", SET.offsetMs); gfx_text_sh(26, 142, 1, b, C_MUTED, C_SHADOW);
    if (A.calN >= 4) { snprintf(b, sizeof b, "Medido: %+d ms", med); int bw = gfx_text_w(b, 2) + 24; ui_panel(W / 2 - bw / 2, 134, bw, 30, UI_PANEL_AMBER); gfx_text_sh_center(W / 2, 140, 2, b, C_INK, C_SHADOW); }
    snprintf(b, sizeof b, "%d toques", A.calN); gfx_text_sh_right(W - 26, 142, 1, b, C_MUTED, C_SHADOW);
    if (A.calN > 0 && now - A.calLastTapAt < 600) { snprintf(b, sizeof b, "%+d ms", (int)A.calLastDiff); gfx_text_sh_center(W / 2, H - 70, 2, b, fabs(A.calLastDiff) < 35 ? C_GOOD : C_AMP, C_SHADOW); }
    static const char *bt[3] = { "↑↓", "A", "B" }, *tx[3] = { "Palhetar no clique", "Salvar", "Cancelar" }; ui_hints(H - 24, W, bt, tx, 3);
}
static void draw_error(double now) { render_backdrop(now, SET.stageFx); int W = A.W, H = A.H; ui_header(W, "OPA…", NULL); ui_panel(30, 120, W - 60, 160, UI_PANEL_DARK); gfx_text_sh(46, 134, 2, "Algo deu errado", C_BAD, C_SHADOW); gfx_text_box(46, 170, W - 92, 1, A.errMsg, C_INK); static const char *bt[1] = { "A" }, *tx[1] = { "Continuar" }; ui_hints(H - 24, W, bt, tx, 1); }

void screens_draw(double now) {
    switch (A.screen) { case SC_TITLE: draw_title(now); break; case SC_MAIN: draw_main(now); break; case SC_TRAINING: draw_training(now); break; case SC_TOPROCK: draw_toprock(now); break;
        case SC_SETLIST: draw_setlist(now); break; case SC_SETUP: draw_setup(now); break; case SC_OPTIONS: draw_options(now); break; case SC_CONTROLS: draw_controls(now); break;
        case SC_PAUSE: draw_pause(now); break; case SC_STORE: store_draw(now); break; case SC_CHEATS: cheats_draw(now); break; case SC_TUTORIAL: tutorial_frame(now); break; case SC_CALIB: draw_calib(now); break; case SC_RESULTS: draw_results(now); break; case SC_BATTLE: battle_screen_draw(now); break; case SC_LOADING: loading_draw(now); break; default: draw_error(now); }
    draw_note(now);
}

/* ------------------------------------------------------------------ input */
static void go_options(int from) { A.optionsFrom = from; A.screen = SC_OPTIONS; A.optCat = -1; A.optRow = 0; }
static void to_setlist(double now) {
    if (A.moviePending[0] && A.mode == MODE_CAREER) { char m[48]; snprintf(m, sizeof m, "%s", A.moviePending); A.moviePending[0] = 0; app_play_movie(m, SC_SETLIST); return; }   /* a tier's cutscene first */
    A.moviePending[0] = 0; A.screen = SC_SETLIST; app_preview(A.cursor < A.nrows ? A.rows[A.cursor].idx : -1); if (A.unlockPending && A.mode == MODE_CAREER) { A.unlockPending = 0; A.unlockAt = now; audio_sfx(SFX_FANFARE); } }
static void open_mode(int mode, double now) { A.mode = mode; app_refresh_career(0); screens_build_rows(); audio_sfx(SFX_OK); audio_sfx(SFX_WHOOSH);
    if (mode == MODE_CAREER && !SET.careerIntro && app_movie_exists(A.lib->careerMovie)) { SET.careerIntro = 1; settings_save(); app_play_movie(A.lib->careerMovie, SC_SETLIST); return; }   /* the career's opening movie, the first time */
    to_setlist(now); }
static void to_main(void) { A.screen = SC_MAIN; A.confirmQuit = 0; app_preview(-1); }
static void leave_options(void) { settings_save(); app_apply_volumes(); if (A.optionsFrom == SC_PAUSE) A.screen = SC_PAUSE; else { A.screen = SC_MAIN; A.mainRow = MM_OPTIONS; } }
void screens_enter(int screen, double now) {
    if (screen == SC_SETLIST) to_setlist(now); else if (screen == SC_MAIN) to_main(); else if (screen == SC_TITLE) { A.screen = SC_TITLE; A.titleAt = now; } else A.screen = screen; }
static int menu_move(int in, int *row, int n) { if (in == IN_UP) { *row = (*row + n - 1) % n; audio_sfx(SFX_MOVE); return 1; } if (in == IN_DOWN) { *row = (*row + 1) % n; audio_sfx(SFX_MOVE); return 1; } return 0; }
static void setup_change(int r, int d) { const gh3_song *s = &A.lib->songs[A.selSong];
    switch (r) { case SR_DIFF: A.selDiff = (A.selDiff + 4 + d) % 4; app_refresh_career(0); break; case SR_PART: A.selPart = (s->hasRhythm || A.S.sd.parts[1][A.selDiff].raw) ? !A.selPart : 0; break;
        case SR_SECTION: A.practiceSec = (A.practiceSec + A.S.sd.nmarkers + 1 + d) % (A.S.sd.nmarkers + 1); if (A.practiceEnd && A.practiceEnd < A.practiceSec) A.practiceEnd = A.practiceSec < A.S.sd.nmarkers ? A.practiceSec : 0; break;
        case SR_END: end_change(d); break; case SR_SPEED: A.practiceSpeed = (A.practiceSpeed + PRACTICE_SPEEDS + d) % PRACTICE_SPEEDS; break; case SR_STRUM: SET.tap = !SET.tap; break; case SR_HYPER: SET.hyper = (SET.hyper + 6 + d) % 6; break; } }
void screens_event(int in, double now) {
    if (A.screen == SC_BATTLE) { battle_input(in, now); return; }
    if (A.screen == SC_TITLE) { if (in == IN_OK || in == IN_X || in == IN_Y) { audio_sfx(SFX_OK); audio_sfx(SFX_WHOOSH); app_guitar_lick(); A.screen = SC_MAIN; } else if (in == IN_BACK) { A.screen = SC_MAIN; A.mainRow = MM_EXIT; A.confirmQuit = 1; } }
    else if (A.screen == SC_MAIN) {
        if (A.confirmQuit) { if (in == IN_OK) app_quit(); else if (in == IN_BACK || in == IN_X || in == IN_Y) { A.confirmQuit = 0; audio_sfx(SFX_BACK); } return; }
        if (menu_move(in, &A.mainRow, MM_N)) return;
        if (in == IN_BACK) { A.screen = SC_TITLE; A.titleAt = now; audio_sfx(SFX_BACK); return; }
        if (in != IN_OK) return;
        switch (A.mainRow) { case MM_CAREER: open_mode(MODE_CAREER, now); break; case MM_QUICK: open_mode(MODE_QUICK, now); break;
            case MM_COOP: case MM_MULTI: audio_sfx(SFX_WARN); note(now, "Precisa de dois jogadores: não dá no R36S."); break;
            case MM_TRAINING: A.screen = SC_TRAINING; A.trainRow = TR_PRACTICE; audio_sfx(SFX_OK); audio_sfx(SFX_WHOOSH); break; case MM_OPTIONS: go_options(SC_MAIN); audio_sfx(SFX_OK); audio_sfx(SFX_WHOOSH); break;
            default: A.confirmQuit = 1; audio_sfx(SFX_BACK); }
    } else if (A.screen == SC_TRAINING) {
        if (menu_move(in, &A.trainRow, TR_N)) return;
        if (in == IN_BACK) { A.screen = SC_MAIN; audio_sfx(SFX_BACK); }
        else if (in == IN_OK) { if (A.trainRow == TR_TUTORIAL) { audio_sfx(SFX_OK); app_preview(-1); tutorial_open(now); } else { A.practiceSec = A.practiceEnd = 0; open_mode(MODE_PRACTICE, now); } }
    } else if (A.screen == SC_SETLIST) {
        if (in == IN_UP || in == IN_DOWN) { int was = A.cursor; move_cursor(in == IN_UP ? -1 : 1); if (A.cursor != was) { audio_sfx(SFX_MOVE); app_preview(A.rows[A.cursor].idx); } }
        else if (in == IN_LEFT || in == IN_RIGHT || in == IN_L || in == IN_R) { A.selDiff = (A.selDiff + 4 + ((in == IN_RIGHT || in == IN_R) ? 1 : -1)) % 4; app_refresh_career(0); audio_sfx(SFX_MOVE); }
        else if (in == IN_OK && A.cursor < A.nrows) { int lock = row_lock(A.cursor); if (lock) { audio_sfx(SFX_WARN); snprintf(A.unlockMsg, sizeof A.unlockMsg, "%s", lock == LOCK_BUY ? (A.mode == MODE_CAREER ? "Compre esta música na Loja (Y)" : "Compre esta música na Loja da carreira") : lock == LOCK_FINALE ? "Termine a carreira para liberar esta" : A.mode != MODE_CAREER ? "Libere esta música na carreira primeiro" : lock == LOCK_TIER ? "Termine o set anterior primeiro" : lock == LOCK_BOSS ? "Complete mais músicas do set para a batalha" : "Complete o set (e a batalha) para o encore"); A.unlockAt = now; return; }
            A.selSong = A.rows[A.cursor].idx; A.selTier = A.rows[A.cursor].tier; A.practiceSec = A.practiceEnd = 0; if (app_prepare_song(A.selSong) == 0) { int rows[8]; A.setupRow = setup_rows(rows) - 1; A.screen = SC_SETUP; audio_sfx(SFX_OK); audio_sfx(SFX_WHOOSH); } }
        else if (in == IN_Y && A.mode == MODE_CAREER) store_open(now);
        else if (in == IN_BACK) { to_main(); audio_sfx(SFX_BACK); }
    } else if (A.screen == SC_STORE) store_event(in, now);
    else if (A.screen == SC_SETUP) { if (encore_event(in, now)) return; int rows[8], nr = setup_rows(rows);
        if (menu_move(in, &A.setupRow, nr)) return;
        if (in == IN_LEFT || in == IN_RIGHT) { audio_sfx(SFX_MOVE); setup_change(rows[A.setupRow], in == IN_RIGHT ? 1 : -1); }
        else if (in == IN_OK) { if (rows[A.setupRow] != SR_PLAY && rows[A.setupRow] != SR_PART) { setup_change(rows[A.setupRow], 1); audio_sfx(SFX_MOVE); return; } if (rows[A.setupRow] == SR_PART) { setup_change(SR_PART, 1); audio_sfx(SFX_MOVE); return; }
            if (app_song_lock(A.selTier, A.selSong)) { audio_sfx(SFX_WARN); return; } settings_save(); encore_begin(now); }
        else if (in == IN_BACK) { app_session_free(); A.screen = SC_SETLIST; audio_sfx(SFX_BACK); app_preview(A.rows[A.cursor].idx); }
    } else if (A.screen == SC_OPTIONS) {
        if (A.optCat < 0) {
            if (menu_move(in, &A.optRow, CAT_N)) return;
            if (in == IN_BACK) { leave_options(); audio_sfx(SFX_BACK); return; }
            if (in != IN_OK) return;
            if (cat_off(A.optRow)) { audio_sfx(SFX_WARN); return; }
            if (A.optRow == CAT_CALIB) { audio_sfx(SFX_OK); app_calib_start(SC_OPTIONS); }
            else if (A.optRow == CAT_TOP) { A.screen = SC_TOPROCK; A.topDiff = A.selDiff; A.topScroll = 0; while (A.topScroll < A.nrows - 1 && A.rows[A.topScroll].kind == 0) A.topScroll++; audio_sfx(SFX_OK); }
            else if (A.optRow == CAT_CHEATS) { cheats_open(); audio_sfx(SFX_OK); }
            else if (A.optRow == CAT_RESET) { int seen = SET.careerIntro; settings_defaults(); SET.careerIntro = seen; A.preset = 0; app_apply_volumes(); app_refresh_career(0); audio_sfx(SFX_OK); note(now, "Opções e controles voltaram ao padrão."); }
            else { A.optCat = A.optRow; A.optRow = 0; audio_sfx(SFX_OK); audio_sfx(SFX_WHOOSH); }
            return; }
        int n = cat_count(A.optCat), it = CAT_ITEMS[A.optCat][A.optRow];
        if (menu_move(in, &A.optRow, n)) return;
        if (in == IN_LEFT || in == IN_RIGHT) { if (it != OPT_BUTTONS) { opt_change(it, in == IN_RIGHT ? 1 : -1); audio_sfx(SFX_MOVE); } }
        else if (in == IN_OK) { if (it == OPT_BUTTONS) { A.screen = SC_CONTROLS; A.ctlRow = 1; A.ctlSlot = 0; audio_sfx(SFX_OK); } else { opt_change(it, 1); audio_sfx(SFX_MOVE); } }
        else if (in == IN_BACK) { A.optRow = A.optCat; A.optCat = -1; settings_save(); audio_sfx(SFX_BACK); }
    } else if (A.screen == SC_TOPROCK) {
        if (in == IN_UP || in == IN_DOWN) { int c = A.topScroll, d = in == IN_UP ? -1 : 1; do { c += d; } while (c >= 0 && c < A.nrows && A.rows[c].kind == 0); if (c >= 0 && c < A.nrows) { A.topScroll = c; audio_sfx(SFX_MOVE); } }
        else if (in == IN_LEFT || in == IN_RIGHT || in == IN_L || in == IN_R) { A.topDiff = (A.topDiff + 4 + ((in == IN_RIGHT || in == IN_R) ? 1 : -1)) % 4; audio_sfx(SFX_MOVE); }
        else if (in == IN_BACK || in == IN_OK) { A.screen = SC_OPTIONS; A.optCat = -1; A.optRow = CAT_TOP; audio_sfx(SFX_BACK); }
    } else if (A.screen == SC_CONTROLS) {
        if (A.capturing) return;
        if (in == IN_UP) { A.ctlRow = (A.ctlRow + CTL_ROWS - 1) % CTL_ROWS; audio_sfx(SFX_MOVE); } else if (in == IN_DOWN) { A.ctlRow = (A.ctlRow + 1) % CTL_ROWS; audio_sfx(SFX_MOVE); }
        else if (in == IN_LEFT || in == IN_RIGHT) { if (A.ctlRow == 0) { A.preset = (A.preset + PRESET_N + (in == IN_RIGHT ? 1 : -1)) % PRESET_N; input_apply_preset(A.preset); } else A.ctlSlot = (A.ctlSlot + BIND_SLOTS + (in == IN_RIGHT ? 1 : -1)) % BIND_SLOTS; audio_sfx(SFX_MOVE); }
        else if (in == IN_OK) { if (A.ctlRow == 0) { input_apply_preset(A.preset); audio_sfx(SFX_OK); } else if (A.ctlRow == CTL_ROWS - 1) { input_apply_preset(A.preset); audio_sfx(SFX_OK); } else { A.capturing = 1; A.capAt = now; input_reset_axes(); audio_sfx(SFX_OK); } }
        else if (in == IN_X) { if (A.ctlRow >= 1 && A.ctlRow < CTL_ROWS - 1) { memset(&SET.binds[A.ctlRow - 1][A.ctlSlot], 0, sizeof(bind_t)); audio_sfx(SFX_BACK); } }
        else if (in == IN_BACK) { settings_save(); A.screen = SC_OPTIONS; audio_sfx(SFX_BACK); A.testHeld = 0; }
    } else if (A.screen == SC_PAUSE) {
        if (in == IN_UP) { A.pauseRow = (A.pauseRow + 3) % 4; audio_sfx(SFX_MOVE); } else if (in == IN_DOWN) { A.pauseRow = (A.pauseRow + 1) % 4; audio_sfx(SFX_MOVE); }
        else if (in == IN_OK) { audio_sfx(SFX_OK); if (A.pauseRow == 0) app_resume_play(); else if (A.pauseRow == 1) app_begin_song(); else if (A.pauseRow == 2) go_options(SC_PAUSE); else { app_session_free(); app_refresh_career(0); to_setlist(now); } }
        else if (in == IN_BACK) app_resume_play();
    } else if (A.screen == SC_RESULTS) {
        if (in == IN_LEFT || in == IN_RIGHT) { A.resRow = !A.resRow; audio_sfx(SFX_MOVE); }
        else if (in == IN_OK && A.resRow == 1 && store_encore_offer() >= 0) { int enc = store_encore_offer(); app_session_free(); A.selSong = enc; A.practiceSec = A.practiceEnd = 0;   /* sent to the encore: its setup, the chant, the song */
            for (int i = 0; i < A.nrows; i++) if (A.rows[i].kind && A.rows[i].idx == enc && A.rows[i].tier == A.selTier) A.cursor = i;
            if (app_prepare_song(enc) == 0) { int rows[8]; A.setupRow = setup_rows(rows) - 1; A.screen = SC_SETUP; encore_begin(now); } }
        else if (in == IN_OK) { audio_sfx(SFX_OK); if (A.resRow == 0) { if (app_song_lock(A.selTier, A.selSong)) { app_session_free(); to_setlist(now); } else app_begin_song(); }
            else if (A.mode == MODE_PRACTICE) { if (app_prepare_song(A.selSong) == 0) { int rows[8]; A.setupRow = setup_rows(rows) - 1; A.screen = SC_SETUP; } }   /* other sections or speed, same song */
            else { app_session_free(); to_setlist(now); } }
        else if (in == IN_BACK) { app_session_free(); audio_sfx(SFX_BACK); to_setlist(now); }
    } else if (A.screen == SC_CALIB) {
        if (in == IN_OK) { if (A.calN >= 6) { SET.offsetMs = app_calib_result(); settings_save(); audio_sfx(SFX_FANFARE); } else audio_sfx(SFX_BACK); app_calib_stop(); A.screen = SC_OPTIONS; A.optCat = -1; A.optRow = CAT_CALIB; }
        else if (in == IN_BACK) { app_calib_stop(); A.screen = SC_OPTIONS; A.optCat = -1; A.optRow = CAT_CALIB; audio_sfx(SFX_BACK); }
    } else if (A.screen == SC_CHEATS) cheats_event(in, now);
    else if (A.screen == SC_TUTORIAL) tutorial_event(in, now);
    else if (A.screen == SC_ERROR) { if (in == IN_OK || in == IN_BACK) { app_session_free(); if (A.libOk) to_main(); else app_quit(); } }
}
int screens_raw_event(const SDL_Event *ev, double now) {
    if (A.screen == SC_CALIB) { inev_t evs[4]; int n = input_translate(ev, evs, 4);
        for (int i = 0; i < n; i++) if ((evs[i].action == ACT_STRUM_UP || evs[i].action == ACT_STRUM_DOWN) && evs[i].pressed) {
            double te = audio_song_ms() - (SDL_GetTicks() - ev->common.timestamp); long k = lround(te / 500); if (k < 0) continue; double d = te - k * 500.0; if (fabs(d) > 240) continue;
            if (A.calN == 24) { memmove(A.calDiff, A.calDiff + 1, 23 * sizeof(double)); A.calN = 23; } A.calDiff[A.calN++] = d; A.calLastDiff = d; A.calLastTapAt = now; }
        return n > 0 && (evs[0].action == ACT_STRUM_UP || evs[0].action == ACT_STRUM_DOWN); }
    if (A.screen == SC_CHEATS) return cheats_raw_event(ev, now);
    if (A.screen == SC_TUTORIAL) return tutorial_raw_event(ev, now);
    if (A.screen != SC_CONTROLS) return 0;
    if (A.capturing) { bind_t b; if (now - A.capAt < 250) return 1; /* the A press that opened the capture is not the answer */ if (input_capture(ev, A.ctlRow - 1, &b)) { int a = A.ctlRow - 1; if (a >= 0 && a < ACT_N) {
            for (int x = 0; x < ACT_N; x++) for (int s = 0; s < BIND_SLOTS; s++) if (SET.binds[x][s].type == b.type && SET.binds[x][s].id == b.id && SET.binds[x][s].dir == b.dir) memset(&SET.binds[x][s], 0, sizeof b); /* one button, one job */
            SET.binds[a][A.ctlSlot] = b; } A.capturing = 0; audio_sfx(SFX_OK); return 1; } return 1; }
    inev_t evs[4]; int n = input_translate(ev, evs, 4);
    for (int i = 0; i < n; i++) { int a = evs[i].action; if (a <= ACT_FRET4) { if (evs[i].pressed) A.testHeld |= 1 << a; else A.testHeld &= ~(1 << a); } else if (a == ACT_STRUM_UP || a == ACT_STRUM_DOWN) { if (evs[i].pressed) { A.testStrum = a == ACT_STRUM_UP ? -1 : 1; A.testStrumAt = now; } } else if (a == ACT_STAR) A.testStar = evs[i].pressed; else if (a == ACT_WHAMMY) A.testWhammy = evs[i].value; }
    return 0; /* the menu still sees the event (D-pad moves the list) */
}
