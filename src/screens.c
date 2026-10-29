/* The menus: set list (career), song setup, options, controls, pause, results and error, in Portuguese. */
#include "app.h"
#include "render.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include <stdio.h>
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
static int row_lock(int i) { return career_song_lock(A.lib, A.tiers, A.rows[i].tier, A.rows[i].idx); }

/* ------------------------------------------------------------------ set list */
static void draw_setlist(double now) {
    render_backdrop(now, SET.stageFx); int W = A.W, H = A.H; gh3_library *lib = A.lib; char b[200], b2[64];
    int done = career_tiers_done(lib, A.tiers), ntier = 0; for (int t = 0; t < lib->ntiers; t++) ntier += lib->tiers[t].career;
    snprintf(b, sizeof b, "Carreira %s • %d/%d sets • ★ %d", diff_name_pt(A.selDiff), done, ntier, records_count_stars());
    ui_header(W, "GUITAR HERO III", b);
    snprintf(b, sizeof b, "Dificuldade da carreira:  ← %s →", diff_name_pt(A.selDiff)); gfx_text_sh(16, 66, 1, b, C_MUTED, C_SHADOW);
    gfx_text_right(W - 16, 66, 1, "L1 / R1 ou ← → mudam", C_LINE);
    int rowH = 22, top = 84, vis = (H - top - 34) / rowH;
    if (A.cursor < A.scroll) A.scroll = A.cursor; if (A.cursor >= A.scroll + vis) A.scroll = A.cursor - vis + 1; if (A.scroll < 0) A.scroll = 0;
    for (int i = A.scroll, y = top; i < A.nrows && i < A.scroll + vis; i++, y += rowH) {
        if (A.rows[i].kind == 0) { const gh3_tier *tr = &lib->tiers[A.rows[i].idx]; const tierstate_t *ts = &A.tiers[A.rows[i].idx];
            gfx_rect(10, y + 2, W - 20, rowH - 4, RGBA(60, 40, 15, 120)); gfx_rect(10, y + rowH - 3, W - 20, 1, C_AMP_DARK);
            const char *title = tr->tab == 1 ? "Músicas bônus" : tr->tab == 2 ? "Músicas baixadas" : tr->tab == 3 ? "Mais músicas" : tr->title;
            gfx_text_sh(18, y + 6, 1, title, ts->open ? C_AMP : C_MUTED, C_SHADOW);
            if (tr->career) { if (!ts->open) snprintf(b, sizeof b, "bloqueado: termine o set anterior"); else if (ts->done) snprintf(b, sizeof b, "set concluído ✓"); else if (ts->regDone < ts->required) snprintf(b, sizeof b, "%d de %d músicas para liberar %s", ts->regDone, ts->required, tr->bossIdx >= 0 ? "a batalha" : "o encore"); else if (tr->bossIdx >= 0 && !ts->bossDone) snprintf(b, sizeof b, "vença a batalha para liberar o encore"); else snprintf(b, sizeof b, "toque o encore para fechar o set");
                gfx_text_right(W - 18, y + 6, 1, b, ts->done ? C_GOOD : C_MUTED); }
            continue; }
        const gh3_song *s = &lib->songs[A.rows[i].idx]; int sel = i == A.cursor, lock = row_lock(i); const record_t *rc = records_get(s->name, A.selDiff, 0); const record_t *rc2 = records_get(s->name, A.selDiff, 1); if (!rc || (rc2 && rc2->score > rc->score)) rc = rc2;
        if (sel) { ui_panel(10, y - 1, W - 20, rowH, lock ? UI_PANEL_DARK : UI_PANEL_AMBER); gfx_sprite_at(art_get(ART_PICK), 16, y + 3, 0.5f, RGB(255, 255, 255), 1); }
        song_title(s, b, sizeof b); uint32_t tc = lock ? RGB(110, 104, 122) : sel ? C_INK : RGB(210, 204, 196);
        gfx_text_sh(34, y + 1, 2, b, tc, C_SHADOW);
        if (s->isBoss) gfx_sprite_at(art_get(ART_SKULL), 34 + gfx_text_w(b, 2) + 8, y + 1, 0.9f, RGB(255, 255, 255), lock ? 0.5f : 1);
        int tw = gfx_text_w(b, 2);
        if (lock) { const char *why = lock == LOCK_TIER ? "bloqueado" : lock == LOCK_BOSS ? "batalha bloqueada" : "encore bloqueado"; gfx_text_right(W - 20, y + 6, 1, why, RGB(110, 104, 122)); }
        else if (rc) { stars_str(rc->stars, b2); fmt_num(rc->score, b, sizeof b); gfx_text_right(W - 20, y + 6, 1, b, sel ? C_INK : C_MUTED); gfx_text_right(W - 20 - gfx_text_w(b, 1) - 12, y + 3, 2, b2, C_AMP); }
        else { snprintf(b, sizeof b, "%s%s%s", s->artist, *s->year ? " • " : "", s->year); if (gfx_text_w(b, 1) + tw + 60 < W) gfx_text_right(W - 20, y + 6, 1, b, sel ? C_MUTED : C_LINE); }
    }
    static const char *bt[5] = { "A", "B", "X", "Y", "Start" }, *tx[5] = { "Escolher", "Sair", "Opções", "Controles", "Tocar" };
    ui_hints(H - 24, W, bt, tx, 4);
    if (A.unlockAt > 0 && now - A.unlockAt < 4500) { float a = now - A.unlockAt > 3500 ? (float)(1 - (now - A.unlockAt - 3500) / 1000) : 1; int w = gfx_text_w(A.unlockMsg, 2) + 40; ui_panel(W / 2 - w / 2, H / 2 - 30, w, 60, UI_PANEL_AMBER); gfx_sprite_at(art_get(ART_CROWN), W / 2 - w / 2 + 10, H / 2 - 20, 1, RGB(255, 255, 255), a); gfx_text_sh_center(W / 2 + 10, H / 2 - 11, 2, A.unlockMsg, fb_alpha(C_INK, a), C_SHADOW); }
    if (A.confirmQuit) { ui_dim(W, H, 150); ui_panel(W / 2 - 150, H / 2 - 50, 300, 100, UI_PANEL_DARK); gfx_text_sh_center(W / 2, H / 2 - 34, 2, "Sair do jogo?", C_INK, C_SHADOW); static const char *qb[2] = { "A", "B" }, *qt[2] = { "Sim, sair", "Ficar" }; ui_hints(H / 2 + 14, W, qb, qt, 2); }
}
/* ------------------------------------------------------------------ setup */
static void draw_setup(double now) {
    render_backdrop(now, SET.stageFx); int W = A.W, H = A.H; const gh3_song *s = &A.lib->songs[A.selSong]; char b[200], t[160];
    song_title(s, t, sizeof t); ui_header(W, "PREPARAR", NULL);
    gfx_text_sh(16, 70, 3, t, C_INK, C_SHADOW); snprintf(b, sizeof b, "%s%s%s", s->artist, *s->year ? " • " : "", s->year); gfx_text_sh(16, 106, 1, b, C_MUTED, C_SHADOW);
    const char *labels[5] = { "Dificuldade", "Parte", "Palhetada", "Velocidade da pista", "Tocar!" }; const char *val[4]; char hv[16];
    val[0] = diff_name_pt(A.selDiff); val[1] = part_name(s, A.selPart); val[2] = SET.tap ? "Toque livre" : "Normal";
    if (SET.hyper) snprintf(hv, sizeof hv, "+%d", SET.hyper); else snprintf(hv, sizeof hv, "Normal"); val[3] = hv;
    int lock = career_song_lock(A.lib, A.tiers, A.selTier, A.selSong);
    for (int i = 0; i < 5; i++) { int y = 136 + i * 40; if (i < 4) ui_option(30, y, W - 60, labels[i], val[i], A.setupRow == i, 1);
        else { if (A.setupRow == i) ui_panel(22, y - 8, W - 44, 34, lock ? UI_PANEL_DARK : UI_PANEL_AMBER); gfx_text_sh_center(W / 2, y, 2, lock ? "Bloqueada nesta dificuldade" : "♪  Tocar!  ♪", lock ? C_MUTED : (A.setupRow == i ? C_INK : C_AMP), C_SHADOW); } }
    /* the record and the chart */
    const record_t *rc = records_get(s->name, A.selDiff, A.selPart); int y = 344;
    ui_panel(16, y, W - 32, 62, UI_PANEL_FLAT);
    if (rc) { char n[32], st[32]; fmt_num(rc->score, n, sizeof n); stars_str(rc->stars, st); snprintf(b, sizeof b, "Seu recorde: %s pontos  %s", n, st); gfx_text_sh(28, y + 10, 1, b, C_AMP, C_SHADOW);
        snprintf(b, sizeof b, "%d%% das notas • maior sequência %d • tocada %d %s", rc->pct, rc->streak, rc->plays, rc->plays == 1 ? "vez" : "vezes"); gfx_text(28, y + 28, 1, b, C_MUTED); }
    else { gfx_text_sh(28, y + 10, 1, "Ainda sem recorde nesta parte e dificuldade.", C_MUTED, C_SHADOW); }
    int charted = A.S.sd.parts[A.selPart][A.selDiff].raw != NULL; int notes = charted ? A.S.sd.parts[A.selPart][A.selDiff].nraw / 3 : (A.S.sd.parts[A.selPart ? 0 : 1][A.selDiff].raw ? A.S.sd.parts[A.selPart ? 0 : 1][A.selDiff].nraw / 3 : 0);
    snprintf(b, sizeof b, "%s em %s: %d notas%s", part_name(s, A.selPart), diff_name_pt(A.selDiff), notes, charted ? "" : " (esta parte usa a partitura da outra)"); gfx_text(28, y + 44, 1, b, C_LINE);
    static const char *bt[3] = { "A", "B", "← →" }, *tx[3] = { "Tocar", "Voltar", "Mudar" }; ui_hints(H - 24, W, bt, tx, 3);
}
/* ------------------------------------------------------------------ options */
enum { OPT_THEME, OPT_STRUM, OPT_LEFTY, OPT_HYPER, OPT_NOFAIL, OPT_STAGE, OPT_DIM, OPT_PREVIEW, OPT_VOLM, OPT_VOLS, OPT_OFFSET, OPT_CALIB, OPT_FPS, OPT_UNLOCK, OPT_CONTROLS, OPT_RESET, OPT_N };
static const char *OPT_LABEL[OPT_N] = { "Tema visual", "Palhetada", "Canhoto", "Velocidade da pista", "Sem falhar", "Efeitos de palco", "Escurecer o fundo", "Prévia das músicas", "Volume da música", "Volume dos efeitos", "Atraso do áudio", "Calibrar atraso…", "Mostrar FPS", "Liberar todas as músicas", "Controles…", "Restaurar padrões" };
static const char *OPT_HELP[OPT_N] = {
    "Pixel art: o visual retrô desenhado pixel a pixel. HD suave: formas lisas com antialiasing. Outros temas virão.",
    "Normal: palhete com o direcional como no jogo. Toque livre: a nota toca assim que os trastes certos estão apertados.",
    "Espelha a pista para destros canhotos: o verde fica à direita.", "Notas mais espaçadas na pista (hyperspeed). Não muda a dificuldade.",
    "O medidor de rock nunca encerra a música. A pontuação continua valendo, mas não entra na carreira.", "Plateia, luzes e feixes atrás da pista. Desligue se o aparelho engasgar.",
    "Quanto mais escuro o palco atrás da pista, mais fácil enxergar as notas.", "Toca um trecho da música selecionada na lista.", "Volume das faixas da música.", "Volume dos sons do menu, erros e star power.",
    "Compensa o atraso do som do aparelho, em milissegundos. Se as notas passam pela linha antes de você ouvir, aumente. O melhor é medir em Calibrar atraso.",
    "Toca um clique e você palheta no ritmo: o jogo mede o atraso do som e ajusta as notas e o julgamento. Faça antes de jogar.", "Quadros por segundo no alto da tela.", "Abre todos os sets sem passar pela carreira. Os recordes continuam valendo.",
    "Troque os botões de cada traste, da palhetada, do star power e da alavanca.", "Volta todas as opções e controles ao padrão." };
static void opt_value(int i, char *out, size_t n) {
    switch (i) { case OPT_THEME: snprintf(out, n, "%s", render_theme_name(SET.theme)); break; case OPT_STRUM: snprintf(out, n, "%s", SET.tap ? "Toque livre" : "Normal"); break; case OPT_LEFTY: snprintf(out, n, "%s", SET.lefty ? "Sim" : "Não"); break;
    case OPT_HYPER: if (SET.hyper) snprintf(out, n, "+%d", SET.hyper); else snprintf(out, n, "Normal"); break; case OPT_NOFAIL: snprintf(out, n, "%s", SET.noFail ? "Sim" : "Não"); break;
    case OPT_STAGE: snprintf(out, n, "%s", SET.stageFx ? "Ligados" : "Desligados"); break; case OPT_DIM: snprintf(out, n, "%d%%", SET.bgDim); break; case OPT_PREVIEW: snprintf(out, n, "%s", SET.preview ? "Ligada" : "Desligada"); break;
    case OPT_VOLM: snprintf(out, n, "%d%%", SET.volMusic); break; case OPT_VOLS: snprintf(out, n, "%d%%", SET.volSfx); break; case OPT_OFFSET: snprintf(out, n, "%+d ms", SET.offsetMs); break;
    case OPT_FPS: snprintf(out, n, "%s", SET.showFps ? "Sim" : "Não"); break; case OPT_UNLOCK: snprintf(out, n, "%s", SET.unlockAll ? "Sim" : "Não"); break; default: out[0] = 0; }
}
static void opt_change(int i, int d) {
    switch (i) { case OPT_THEME: SET.theme = (SET.theme + THEME_N + d) % THEME_N; break; case OPT_STRUM: SET.tap = !SET.tap; break; case OPT_LEFTY: SET.lefty = !SET.lefty; break; case OPT_HYPER: SET.hyper = (SET.hyper + 6 + d) % 6; break; case OPT_NOFAIL: SET.noFail = !SET.noFail; break;
    case OPT_STAGE: SET.stageFx = !SET.stageFx; break; case OPT_DIM: SET.bgDim += d * 10; if (SET.bgDim < 0) SET.bgDim = 0; if (SET.bgDim > 100) SET.bgDim = 100; break; case OPT_PREVIEW: SET.preview = !SET.preview; if (!SET.preview) app_preview(-1); break;
    case OPT_VOLM: SET.volMusic += d * 10; if (SET.volMusic < 0) SET.volMusic = 0; if (SET.volMusic > 100) SET.volMusic = 100; app_apply_volumes(); break;
    case OPT_VOLS: SET.volSfx += d * 10; if (SET.volSfx < 0) SET.volSfx = 0; if (SET.volSfx > 100) SET.volSfx = 100; app_apply_volumes(); break;
    case OPT_OFFSET: SET.offsetMs += d * 5; if (SET.offsetMs < -300) SET.offsetMs = -300; if (SET.offsetMs > 300) SET.offsetMs = 300; break;
    case OPT_FPS: SET.showFps = !SET.showFps; break; case OPT_UNLOCK: SET.unlockAll = !SET.unlockAll; app_refresh_career(0); break; }
}
static void draw_options(double now) {
    if (A.optionsFrom == SC_PAUSE) { hud_info hi = { A.S.song->title, A.S.song->artist, A.S.section, 0, SET.lefty, SET.stageFx, SET.bgDim, 0, SET.noFail }; render_game(&A.S.run, now, &hi); ui_dim(A.W, A.H, 200); } else render_backdrop(now, SET.stageFx);
    int W = A.W, H = A.H; ui_header(W, "OPÇÕES", "salvas ao sair"); char v[40];
    int rowH = 24, top = 72, vis = 11, first = A.optRow - vis / 2; if (first < 0) first = 0; if (first > OPT_N - vis) first = OPT_N - vis; if (first < 0) first = 0;
    for (int i = first; i < OPT_N && i < first + vis; i++) { int y = top + (i - first) * rowH; opt_value(i, v, sizeof v); int sel = i == A.optRow;
        if (sel) { ui_panel(14, y - 5, W - 28, rowH, UI_PANEL_AMBER); gfx_sprite_at(art_get(ART_PICK), 20, y - 2, 0.5f, RGB(255, 255, 255), 1); }
        gfx_text_sh(36, y, 1, OPT_LABEL[i], sel ? C_INK : C_MUTED, C_SHADOW);
        gfx_text_sh(36, y, 1, "", C_INK, C_SHADOW);
        if (v[0]) { char b[60]; if (sel && i != OPT_CONTROLS && i != OPT_RESET && i != OPT_CALIB) snprintf(b, sizeof b, "← %s →", v); else snprintf(b, sizeof b, "%s", v); gfx_text_sh_right(W - 30, y, 1, b, sel ? C_AMP : C_INK, C_SHADOW); }
        if (i == OPT_VOLM || i == OPT_VOLS || i == OPT_DIM) ui_bar(W - 30 - 150 - 70, y + 2, 60, 8, (i == OPT_VOLM ? SET.volMusic : i == OPT_VOLS ? SET.volSfx : SET.bgDim) / 100.0f, sel ? C_AMP : C_MUTED); }
    ui_panel(14, H - 96, W - 28, 60, UI_PANEL_FLAT); gfx_text_box(26, H - 88, W - 52, 1, OPT_HELP[A.optRow], C_MUTED);
    static const char *bt[3] = { "← →", "A", "B" }, *tx[3] = { "Mudar", "Abrir", "Voltar" }; ui_hints(H - 24, W, bt, tx, 3);
}
/* ------------------------------------------------------------------ controls */
#define CTL_ROWS (ACT_N + 2)   /* profile, actions, reset */
static void draw_controls(double now) {
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
    hud_info hi = { A.S.song->title, A.S.song->artist, A.S.section, 0, SET.lefty, SET.stageFx, SET.bgDim, 0, SET.noFail }; render_game(&A.S.run, now, &hi);
    int W = A.W, H = A.H; ui_dim(W, H, 170); ui_panel(W / 2 - 140, 120, 280, 230, UI_PANEL_DARK);
    gfx_text_title(W / 2 - gfx_text_w("PAUSA", 4) / 2, 134, 4, "PAUSA", C_INK, C_SHADOW, 3);
    const char *it[4] = { "Continuar", "Reiniciar", "Opções", "Sair da música" };
    for (int i = 0; i < 4; i++) { int y = 196 + i * 34; if (i == A.pauseRow) { ui_panel(W / 2 - 120, y - 7, 240, 30, UI_PANEL_AMBER); gfx_sprite_at(art_get(ART_PICK), W / 2 - 112, y - 3, 0.5f, RGB(255, 255, 255), 1); } gfx_text_sh_center(W / 2 + 6, y, 2, it[i], i == A.pauseRow ? C_INK : C_MUTED, C_SHADOW); }
}
static void draw_results(double now) {
    render_backdrop(now, SET.stageFx); gh3_run *r = &A.S.run; char b[120], n[32]; int W = A.W, H = A.H; double T = (now - A.resultsAt) / 1000;
    ui_header(W, r->failed ? "QUE PENA…" : "VOCÊ ARRASOU!", NULL);
    song_title(A.S.song, b, sizeof b); gfx_text_sh_center(W / 2, 70, 2, b, C_INK, C_SHADOW); snprintf(b, sizeof b, "%s • %s • %s", A.S.song->artist, part_name(A.S.song, A.S.part), diff_name_pt(A.S.diff)); gfx_text_center(W / 2, 96, 1, b, C_MUTED);
    fmt_num((long)r->score, n, sizeof n); gfx_text_title(W / 2 - gfx_text_w(n, 5) / 2, 120, 5, n, r->failed ? C_MUTED : C_AMP, C_SHADOW, 4); gfx_text_center(W / 2, 180, 1, "PONTOS", C_MUTED);
    if (!r->failed) render_stars(W / 2, 200, A.resultStars, 3, T);
    else { gfx_sprite_draw(art_get(ART_SKULL), W / 2, 216, 1.5f, RGB(255, 255, 255), 1); gfx_text_center(W / 2, 236, 1, "A plateia vaiou… tente de novo!", C_BAD); }
    int total = r->nnotes, hit = r->hits; int y = 256;
    ui_panel(W / 2 - 220, y - 8, 440, 96, UI_PANEL_FLAT);
    snprintf(b, sizeof b, "Notas acertadas"); gfx_text(W / 2 - 200, y, 1, b, C_MUTED); snprintf(b, sizeof b, "%d / %d  (%d%%)", hit, total, A.resultPct); gfx_text_sh_right(W / 2 + 200, y - 3, 2, b, C_INK, C_SHADOW);
    gfx_text(W / 2 - 200, y + 26, 1, "Maior sequência", C_MUTED); snprintf(b, sizeof b, "%d", r->best); gfx_text_sh_right(W / 2 + 200, y + 23, 2, b, C_INK, C_SHADOW);
    gfx_text(W / 2 - 200, y + 52, 1, "Palhetadas erradas", C_MUTED); snprintf(b, sizeof b, "%d", r->overstrums); gfx_text_sh_right(W / 2 + 200, y + 49, 2, b, r->overstrums ? C_BAD : C_GOOD, C_SHADOW);
    ui_bar(W / 2 - 200, y + 14, 180, 6, A.resultPct / 100.0f, A.resultPct >= 95 ? C_GOOD : A.resultPct >= 80 ? C_AMP : C_BAD);
    if (A.newRecord && !r->failed && T > 1.9) { float k = (float)fmin(1, (T - 1.9) / 0.3); int w = 230; ui_panel(W / 2 - w / 2, 356 - (int)(10 * (1 - k)), w, 32, UI_PANEL_AMBER); gfx_sprite_at(art_get(ART_CROWN), W / 2 - w / 2 + 10, 362, 1, RGB(255, 255, 255), 1); gfx_text_sh(W / 2 - w / 2 + 40, 362, 2, "NOVO RECORDE!", C_INK, C_SHADOW); }
    if (SET.noFail && !r->failed) gfx_text_center(W / 2, 362, 1, "Modo sem falhar: não vale para recordes nem para a carreira.", C_MUTED);
    const char *it[2] = { "Tocar de novo", "Lista de músicas" }; int by = 404;
    for (int i = 0; i < 2; i++) { int x = W / 2 - 236 + i * 244; if (i == A.resRow) ui_panel(x, by - 7, 228, 30, UI_PANEL_AMBER); gfx_text_sh_center(x + 114, by, 2, it[i], i == A.resRow ? C_INK : C_MUTED, C_SHADOW); }
    static const char *bt[2] = { "A", "← →" }, *tx[2] = { "Confirmar", "Escolher" }; ui_hints(H - 22, W, bt, tx, 2);
}
static void draw_calib(double now) {
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
    switch (A.screen) { case SC_SETLIST: draw_setlist(now); break; case SC_SETUP: draw_setup(now); break; case SC_OPTIONS: draw_options(now); break; case SC_CONTROLS: draw_controls(now); break;
        case SC_PAUSE: draw_pause(now); break; case SC_CALIB: draw_calib(now); break; case SC_RESULTS: draw_results(now); break; default: draw_error(now); }
}

/* ------------------------------------------------------------------ input */
static void go_options(int from) { A.optionsFrom = from; A.screen = SC_OPTIONS; A.optRow = 0; }
static void to_setlist(double now) { A.screen = SC_SETLIST; app_preview(A.cursor < A.nrows ? A.rows[A.cursor].idx : -1); if (A.unlockPending) { A.unlockPending = 0; A.unlockAt = now; audio_sfx(SFX_FANFARE); } }
static void leave_options(void) { settings_save(); app_apply_volumes(); A.screen = A.optionsFrom == SC_PAUSE ? SC_PAUSE : SC_SETLIST; if (A.screen == SC_SETLIST) app_preview(A.cursor < A.nrows ? A.rows[A.cursor].idx : -1); }
void screens_event(int in, double now) {
    if (A.screen == SC_SETLIST) {
        if (A.confirmQuit) { if (in == IN_OK) app_quit(); else if (in == IN_BACK || in == IN_X || in == IN_Y) { A.confirmQuit = 0; audio_sfx(SFX_BACK); } return; }
        if (in == IN_UP || in == IN_DOWN) { int was = A.cursor; move_cursor(in == IN_UP ? -1 : 1); if (A.cursor != was) { audio_sfx(SFX_MOVE); app_preview(A.rows[A.cursor].idx); } }
        else if (in == IN_LEFT || in == IN_RIGHT || in == IN_L || in == IN_R) { A.selDiff = (A.selDiff + 4 + ((in == IN_RIGHT || in == IN_R) ? 1 : -1)) % 4; app_refresh_career(0); audio_sfx(SFX_MOVE); }
        else if (in == IN_OK && A.cursor < A.nrows) { int lock = row_lock(A.cursor); if (lock) { audio_sfx(SFX_BACK); snprintf(A.unlockMsg, sizeof A.unlockMsg, "%s", lock == LOCK_TIER ? "Termine o set anterior primeiro" : lock == LOCK_BOSS ? "Complete mais músicas do set para a batalha" : "Complete o set (e a batalha) para o encore"); A.unlockAt = now; return; }
            A.selSong = A.rows[A.cursor].idx; A.selTier = A.rows[A.cursor].tier; A.setupRow = 4; if (app_prepare_song(A.selSong) == 0) { A.screen = SC_SETUP; audio_sfx(SFX_OK); } }
        else if (in == IN_BACK) { A.confirmQuit = 1; audio_sfx(SFX_BACK); }
        else if (in == IN_X) { go_options(SC_SETLIST); audio_sfx(SFX_OK); app_preview(-1); }
        else if (in == IN_Y) { A.screen = SC_CONTROLS; A.ctlRow = 1; A.ctlSlot = 0; audio_sfx(SFX_OK); app_preview(-1); }
    } else if (A.screen == SC_SETUP) { const gh3_song *s = &A.lib->songs[A.selSong];
        if (in == IN_UP) { A.setupRow = (A.setupRow + 4) % 5; audio_sfx(SFX_MOVE); } else if (in == IN_DOWN) { A.setupRow = (A.setupRow + 1) % 5; audio_sfx(SFX_MOVE); }
        else if (in == IN_LEFT || in == IN_RIGHT) { int d = in == IN_RIGHT ? 1 : -1; audio_sfx(SFX_MOVE);
            if (A.setupRow == 0) { A.selDiff = (A.selDiff + 4 + d) % 4; app_refresh_career(0); } else if (A.setupRow == 1) A.selPart = (s->hasRhythm || A.S.sd.parts[1][A.selDiff].raw) ? !A.selPart : 0; else if (A.setupRow == 2) SET.tap = !SET.tap; else if (A.setupRow == 3) SET.hyper = (SET.hyper + 6 + d) % 6; }
        else if (in == IN_OK) { if (career_song_lock(A.lib, A.tiers, A.selTier, A.selSong)) { audio_sfx(SFX_BACK); return; } settings_save(); app_begin_song(); }
        else if (in == IN_BACK) { app_session_free(); A.screen = SC_SETLIST; audio_sfx(SFX_BACK); app_preview(A.rows[A.cursor].idx); }
    } else if (A.screen == SC_OPTIONS) {
        if (in == IN_UP) { A.optRow = (A.optRow + OPT_N - 1) % OPT_N; audio_sfx(SFX_MOVE); } else if (in == IN_DOWN) { A.optRow = (A.optRow + 1) % OPT_N; audio_sfx(SFX_MOVE); }
        else if (in == IN_LEFT || in == IN_RIGHT) { if (A.optRow < OPT_CONTROLS) { opt_change(A.optRow, in == IN_RIGHT ? 1 : -1); audio_sfx(SFX_MOVE); } }
        else if (in == IN_OK) { if (A.optRow == OPT_CALIB) { if (A.optionsFrom == SC_PAUSE) audio_sfx(SFX_BACK); else { audio_sfx(SFX_OK); app_calib_start(SC_OPTIONS); } } else if (A.optRow == OPT_CONTROLS) { A.screen = SC_CONTROLS; A.ctlRow = 1; A.ctlSlot = 0; audio_sfx(SFX_OK); } else if (A.optRow == OPT_RESET) { settings_defaults(); A.preset = 0; app_apply_volumes(); app_refresh_career(0); audio_sfx(SFX_OK); } else if (A.optRow < OPT_CONTROLS) { opt_change(A.optRow, 1); audio_sfx(SFX_MOVE); } }
        else if (in == IN_BACK) { leave_options(); audio_sfx(SFX_BACK); }
    } else if (A.screen == SC_CONTROLS) {
        if (A.capturing) return;
        if (in == IN_UP) { A.ctlRow = (A.ctlRow + CTL_ROWS - 1) % CTL_ROWS; audio_sfx(SFX_MOVE); } else if (in == IN_DOWN) { A.ctlRow = (A.ctlRow + 1) % CTL_ROWS; audio_sfx(SFX_MOVE); }
        else if (in == IN_LEFT || in == IN_RIGHT) { if (A.ctlRow == 0) { A.preset = (A.preset + PRESET_N + (in == IN_RIGHT ? 1 : -1)) % PRESET_N; input_apply_preset(A.preset); } else A.ctlSlot = (A.ctlSlot + BIND_SLOTS + (in == IN_RIGHT ? 1 : -1)) % BIND_SLOTS; audio_sfx(SFX_MOVE); }
        else if (in == IN_OK) { if (A.ctlRow == 0) { input_apply_preset(A.preset); audio_sfx(SFX_OK); } else if (A.ctlRow == CTL_ROWS - 1) { input_apply_preset(A.preset); audio_sfx(SFX_OK); } else { A.capturing = 1; A.capAt = now; input_reset_axes(); audio_sfx(SFX_OK); } }
        else if (in == IN_X) { if (A.ctlRow >= 1 && A.ctlRow < CTL_ROWS - 1) { memset(&SET.binds[A.ctlRow - 1][A.ctlSlot], 0, sizeof(bind_t)); audio_sfx(SFX_BACK); } }
        else if (in == IN_BACK) { settings_save(); A.screen = A.optionsFrom == SC_PAUSE ? SC_OPTIONS : (A.optionsFrom == SC_SETLIST ? SC_OPTIONS : SC_SETLIST); if (A.screen == SC_SETLIST) app_preview(A.rows[A.cursor].idx); audio_sfx(SFX_BACK); A.testHeld = 0; }
    } else if (A.screen == SC_PAUSE) {
        if (in == IN_UP) { A.pauseRow = (A.pauseRow + 3) % 4; audio_sfx(SFX_MOVE); } else if (in == IN_DOWN) { A.pauseRow = (A.pauseRow + 1) % 4; audio_sfx(SFX_MOVE); }
        else if (in == IN_OK) { audio_sfx(SFX_OK); if (A.pauseRow == 0) app_resume_play(); else if (A.pauseRow == 1) app_begin_song(); else if (A.pauseRow == 2) go_options(SC_PAUSE); else { app_session_free(); app_refresh_career(0); to_setlist(now); } }
        else if (in == IN_BACK) app_resume_play();
    } else if (A.screen == SC_RESULTS) {
        if (in == IN_LEFT || in == IN_RIGHT) { A.resRow = !A.resRow; audio_sfx(SFX_MOVE); }
        else if (in == IN_OK) { audio_sfx(SFX_OK); if (A.resRow == 0) { if (career_song_lock(A.lib, A.tiers, A.selTier, A.selSong)) { app_session_free(); to_setlist(now); } else app_begin_song(); } else { app_session_free(); to_setlist(now); } }
        else if (in == IN_BACK) { app_session_free(); audio_sfx(SFX_BACK); to_setlist(now); }
    } else if (A.screen == SC_CALIB) {
        if (in == IN_OK) { if (A.calN >= 6) { SET.offsetMs = app_calib_result(); settings_save(); audio_sfx(SFX_FANFARE); } else audio_sfx(SFX_BACK); app_calib_stop(); A.screen = SC_OPTIONS; }
        else if (in == IN_BACK) { app_calib_stop(); A.screen = SC_OPTIONS; audio_sfx(SFX_BACK); }
    } else if (A.screen == SC_ERROR) { if (in == IN_OK || in == IN_BACK) { app_session_free(); if (A.libOk) A.screen = SC_SETLIST; else app_quit(); } }
}
int screens_raw_event(const SDL_Event *ev, double now) {
    if (A.screen == SC_CALIB) { inev_t evs[4]; int n = input_translate(ev, evs, 4);
        for (int i = 0; i < n; i++) if ((evs[i].action == ACT_STRUM_UP || evs[i].action == ACT_STRUM_DOWN) && evs[i].pressed) {
            double te = audio_song_ms() - (SDL_GetTicks() - ev->common.timestamp); long k = lround(te / 500); if (k < 0) continue; double d = te - k * 500.0; if (fabs(d) > 240) continue;
            if (A.calN == 24) { memmove(A.calDiff, A.calDiff + 1, 23 * sizeof(double)); A.calN = 23; } A.calDiff[A.calN++] = d; A.calLastDiff = d; A.calLastTapAt = now; }
        return n > 0 && (evs[0].action == ACT_STRUM_UP || evs[0].action == ACT_STRUM_DOWN); }
    if (A.screen != SC_CONTROLS) return 0;
    if (A.capturing) { bind_t b; if (now - A.capAt < 250) return 1; /* the A press that opened the capture is not the answer */ if (input_capture(ev, A.ctlRow - 1, &b)) { int a = A.ctlRow - 1; if (a >= 0 && a < ACT_N) {
            for (int x = 0; x < ACT_N; x++) for (int s = 0; s < BIND_SLOTS; s++) if (SET.binds[x][s].type == b.type && SET.binds[x][s].id == b.id && SET.binds[x][s].dir == b.dir) memset(&SET.binds[x][s], 0, sizeof b); /* one button, one job */
            SET.binds[a][A.ctlSlot] = b; } A.capturing = 0; audio_sfx(SFX_OK); return 1; } return 1; }
    inev_t evs[4]; int n = input_translate(ev, evs, 4);
    for (int i = 0; i < n; i++) { int a = evs[i].action; if (a <= ACT_FRET4) { if (evs[i].pressed) A.testHeld |= 1 << a; else A.testHeld &= ~(1 << a); } else if (a == ACT_STRUM_UP || a == ACT_STRUM_DOWN) { if (evs[i].pressed) { A.testStrum = a == ACT_STRUM_UP ? -1 : 1; A.testStrumAt = now; } } else if (a == ACT_STAR) A.testStar = evs[i].pressed; else if (a == ACT_WHAMMY) A.testWhammy = evs[i].value; }
    return 0; /* the menu still sees the event (D-pad moves the list) */
}
