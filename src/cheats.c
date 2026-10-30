/* The cheats of the game, as qb.pak lists them (the guitar_hero_cheats array: name, text, var and unlock_pattern)
 * and as its scripts apply them. A code is a row of chords strummed on the frets in the Cheats screen (masks
 * 0x10000 green, 0x1000 red, 0x100 yellow, 0x10 blue, 0x1 orange); the game follows every code at once, a wrong chord
 * sending a code back to its start (or to its second chord when it is the first one again). A played code unlocks
 * its cheat and turns it on; here every cheat can also be switched without its code.
 * What they do, from the scripts: Easy Expert doubles check_time_early/late (not in Career, and the run is kept off
 * the leaderboard like No Fail), Precision Mode halves them, the two exclude each other; the hammer-on windows stay.
 * Hyperspeed has five levels. Air Guitar, Bret Michaels (the singer) and Performance Mode (no highway or HUD, the 3D
 * band plays alone) only change the 3D band, which this port does not draw. */
#include "cheats.h"
#include "app.h"
#include "render.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include <stdio.h>
#include <string.h>

enum { CH_AIR, CH_PERF, CH_HYPER, CH_NOFAIL, CH_EASY, CH_PRECISION, CH_BRET, CH_SONGS, CH_ALL, CH_N };
static const int CODE_AIR[] = { 272, 65792, 65792, 4112, 4112, 4352, 4352, 272, 65792, 65792, 4112, 4112, 4352, 4352, 65792, 65792, 4352, 4352 };
static const int CODE_PERF[] = { 4352, 4112, 4097, 4112, 4352, 65552, 4352, 4112 };
static const int CODE_HYPER[] = { 1, 16, 1, 256, 1, 16, 1, 256 };
static const int CODE_NOFAIL[] = { 69632, 16, 69632, 65792, 16, 65792, 4352, 1, 4352, 65792, 256, 65792, 69632 };
static const int CODE_EASY[] = { 69632, 65792, 272, 4112, 17, 257, 4352, 4112 };
static const int CODE_PREC[] = { 69632, 69632, 69632, 4352, 4352, 4112, 4112, 272, 257, 257, 69632, 69632, 69632, 4352, 4352, 4112, 4112, 272, 257, 257 };
static const int CODE_BRET[] = { 69632, 69632, 69632, 65552, 65552, 65552, 4112, 4096, 4096, 4096, 4112, 4096, 4096, 4096, 4112, 4096, 4096, 4096 };
static const int CODE_SONGS[] = { 257, 4112, 4097, 65552, 4352, 257, 4352, 4112, 65792, 65792, 272, 272, 257, 257, 272, 256, 4096, 4352, 4096, 256, 1 };
static const int CODE_ALL[] = { 69649, 69904, 69889, 65809, 69904, 4369, 69904, 65809, 69904, 69889, 69889, 69904, 69889 };
#define PAT(p) p, (int)(sizeof p / sizeof p[0])
typedef struct { const char *name; const int *code; int len; } cheat_t;
static const cheat_t CH[CH_N] = { { "Air Guitar", PAT(CODE_AIR) }, { "Performance Mode", PAT(CODE_PERF) }, { "Hyperspeed", PAT(CODE_HYPER) }, { "No Fail (sem falhar)", PAT(CODE_NOFAIL) },
    { "Easy Expert", PAT(CODE_EASY) }, { "Precision Mode", PAT(CODE_PREC) }, { "Bret Michaels", PAT(CODE_BRET) }, { "Liberar todas as músicas", PAT(CODE_SONGS) }, { "Liberar tudo", PAT(CODE_ALL) } };

static int row, retRow, entry, held, prog[CH_N], hist[16], nhist, lastUnlock = -1; static double entryAt, unlockAt = -1e9, strumAt = -1e9;
static int avail(int c) { return c != CH_AIR && c != CH_PERF && c != CH_BRET; }

const gh3_tuning *cheats_tuning(const gh3_tuning *base, int mode) {
    static gh3_tuning t; t = *base;
    double k = SET.precision ? 0.5 : SET.easyExpert && mode != MODE_CAREER ? 2 : 1;   /* the game puts the windows back to normal in Career for Easy Expert */
    t.early *= k; t.late *= k; return &t;
}
int cheats_records_ok(int mode) { return !(SET.easyExpert && !SET.precision && mode != MODE_CAREER); }

static void value(int c, char *out, size_t n) {
    switch (c) { case CH_HYPER: if (SET.hyper) snprintf(out, n, "+%d", SET.hyper); else snprintf(out, n, "Desligada"); break;
        case CH_NOFAIL: snprintf(out, n, "%s", SET.noFail ? "Ligado" : "Desligado"); break; case CH_EASY: snprintf(out, n, "%s", SET.easyExpert ? "Ligado" : "Desligado"); break;
        case CH_PRECISION: snprintf(out, n, "%s", SET.precision ? "Ligado" : "Desligado"); break; case CH_SONGS: case CH_ALL: snprintf(out, n, "%s", SET.unlockAll ? "Ligado" : "Desligado"); break;
        default: snprintf(out, n, "precisa da banda 3D"); }
}
static void change(int c, int d) {
    switch (c) { case CH_HYPER: SET.hyper = (SET.hyper + 6 + d) % 6; break; case CH_NOFAIL: SET.noFail = !SET.noFail; break;
        case CH_EASY: SET.easyExpert = !SET.easyExpert; if (SET.easyExpert) SET.precision = 0; break;
        case CH_PRECISION: SET.precision = !SET.precision; if (SET.precision) SET.easyExpert = 0; break;
        case CH_SONGS: case CH_ALL: SET.unlockAll = !SET.unlockAll; app_refresh_career(0); break; }
}
static void turn_on(int c) {
    switch (c) { case CH_HYPER: if (!SET.hyper) SET.hyper = 1; break; case CH_NOFAIL: SET.noFail = 1; break; case CH_EASY: SET.easyExpert = 1; SET.precision = 0; break;
        case CH_PRECISION: SET.precision = 1; SET.easyExpert = 0; break; case CH_SONGS: case CH_ALL: if (!SET.unlockAll) { SET.unlockAll = 1; app_refresh_career(0); } break; }
}
static void strum_code(int mask, double now) {   /* the game's matcher (cheat menu script): every code advances or restarts on each chord */
    int g = 0; for (int f = 0; f < 5; f++) if (mask & (1 << f)) g |= 1 << (16 - 4 * f);
    if (nhist == 16) { memmove(hist, hist + 1, 15 * sizeof *hist); nhist = 15; } hist[nhist++] = g; strumAt = now; audio_sfx(SFX_MOVE);
    for (int c = 0; c < CH_N; c++) {
        if (g == CH[c].code[prog[c]]) { if (++prog[c] == CH[c].len) { prog[c] = 0; SET.cheatCodes |= 1 << c; turn_on(c); settings_save(); lastUnlock = c; unlockAt = now; row = c + 1; audio_sfx(SFX_FANFARE); } }
        else prog[c] = g == CH[c].code[0] ? 1 : 0; }
}
void cheats_open(void) { retRow = A.optRow; row = 0; entry = 0; A.screen = SC_CHEATS; }

static void chord(int x, int y, int g, int cell) {   /* a chord as a column of five cells, green on top */
    for (int f = 0; f < 5; f++) { int on = g & (1 << (16 - 4 * f)); gfx_rect(x, y + f * (cell + 1), cell, cell, on ? FRET_COLORS[f] : RGBA(255, 255, 255, 28)); }
}
static void help(int c, char *b, size_t n) {
    const gh3_tuning *t = &A.lib->tuning; int e = (int)(t->early + 0.5), l = (int)(t->late + 0.5);
    switch (c) {
    case CH_AIR: snprintf(b, n, "O guitarrista da banda toca sem a guitarra. Só muda o personagem 3D, que este port não desenha."); break;
    case CH_PERF: snprintf(b, n, "No jogo, tira a pista e o painel da tela para a banda 3D tocar sozinha. Sem a banda, não há o que assistir aqui."); break;
    case CH_HYPER: snprintf(b, n, "As notas descem mais rápido e mais espaçadas, em 5 níveis como no jogo. Não muda a dificuldade."); break;
    case CH_NOFAIL: snprintf(b, n, "O medidor de rock nunca encerra a música. A pontuação aparece, mas não vale recorde nem carreira."); break;
    case CH_EASY: snprintf(b, n, "Janelas de acerto dobradas: %d ms antes e %d depois da nota (normal %d e %d). Como no jogo, não vale na carreira; fora dela não vale recorde. Desliga o Precision Mode.", e * 2, l * 2, e, l); break;
    case CH_PRECISION: snprintf(b, n, "Janelas de acerto pela metade: %d ms antes e %d depois da nota (normal %d e %d); os hammer-ons mantêm a janela deles. Desliga o Easy Expert.", e / 2, l / 2, e, l); break;
    case CH_BRET: snprintf(b, n, "Troca o vocalista da banda por Bret Michaels. Só muda o personagem 3D, que este port não desenha."); break;
    case CH_SONGS: snprintf(b, n, "Abre todos os sets sem passar pela carreira. Os recordes continuam valendo."); break;
    default: snprintf(b, n, "No jogo também libera a loja (guitarras, roupas e personagens). Aqui só há músicas para liberar: é o mesmo que a de cima."); }
}
void cheats_draw(double now) {
    if (A.optionsFrom == SC_PAUSE) { hud_info hi = { A.S.song->title, A.S.song->artist, A.S.section, 0, SET.lefty, SET.stageFx, SET.bgDim, 0, SET.noFail, NULL }; render_game(&A.S.run, now, &hi); ui_dim(A.W, A.H, 200); } else render_backdrop(now, SET.stageFx);
    int W = A.W, H = A.H, rowH = 27, top = 70; char v[48], b[300];
    ui_header(W, "TRAPAÇAS", "salvas ao sair");
    for (int i = 0; i <= CH_N; i++) { int y = top + i * rowH, sel = i == row, c = i - 1, ok = i == 0 || avail(c);
        if (sel) { ui_panel(14, y - 6, W - 28, rowH - 2, ok ? UI_PANEL_AMBER : UI_PANEL_DARK); gfx_sprite_at(art_get(ART_PICK), 20, y - 2, 0.5f, RGB(255, 255, 255), 1); }
        const char *name = i == 0 ? "Tocar um código" : CH[c].name; uint32_t tc = !ok ? RGB(110, 104, 122) : sel ? C_INK : C_MUTED;
        gfx_text_sh(36, y, 2, name, tc, C_SHADOW);
        if (i > 0 && (SET.cheatCodes >> c & 1)) gfx_text_sh(36 + gfx_text_w(name, 2) + 8, y + 3, 1, "✓ código", C_GOOD, C_SHADOW);
        if (i == 0) snprintf(v, sizeof v, "trastes + palhetada"); else value(c, v, sizeof v);
        if (sel && ok && i > 0) { char a[60]; snprintf(a, sizeof a, "← %s →", v); gfx_text_sh_right(W - 30, y + 3, 1, a, C_AMP, C_SHADOW); } else gfx_text_sh_right(W - 30, y + 3, 1, v, !ok ? RGB(110, 104, 122) : C_INK, C_SHADOW); }
    int py = H - 112; ui_panel(14, py, W - 28, 80, UI_PANEL_FLAT);
    if (row == 0) gfx_text_box(26, py + 8, W - 52, 1, "Como no jogo: segure os trastes de cada acorde e palhete, um acorde por vez. Um código certo libera a trapaça e já a liga. Start sai.", C_MUTED);
    else { int c = row - 1; help(c, b, sizeof b); gfx_text_box(26, py + 8, W - 52, 1, b, C_MUTED);
        gfx_text(26, py + 56, 1, "Código:", C_LINE); for (int k = 0; k < CH[c].len; k++) chord(80 + k * 9, py + 50, CH[c].code[k], 3); }
    static const char *bt[3] = { "← →", "A", "B" }, *tx[3] = { "Mudar", "Escolher", "Voltar" }; ui_hints(H - 24, W, bt, tx, 3);
    if (entry) { int pw = 460, ph = 190, px = W / 2 - pw / 2, y0 = H / 2 - ph / 2;
        ui_dim(W, H, 150); ui_panel(px, y0, pw, ph, UI_PANEL_SP);
        ui_title(W / 2 - gfx_text_w("TOQUE O CÓDIGO", 3) / 2, y0 + 12, 3, "TOQUE O CÓDIGO", C_INK, C_SHADOW, 2);
        gfx_text_center(W / 2, y0 + 50, 1, "Segure os trastes do acorde e palhete. Start sai.", C_SP);
        for (int f = 0; f < 5; f++) gfx_rect(W / 2 - 70 + f * 28, y0 + 70, 22, 14, held & (1 << f) ? FRET_COLORS[f] : RGBA(255, 255, 255, 30));
        float k = now - strumAt < 180 ? 1 - (float)((now - strumAt) / 180) : 0; if (k > 0) gfx_frame(W / 2 - 76, y0 + 66, 152, 22, 2, fb_alpha(C_INK, k));
        for (int i = 0; i < nhist; i++) chord(px + 30 + i * 26, y0 + 104, hist[i], 9);
        if (!nhist) gfx_text_center(W / 2, y0 + 124, 1, "(os acordes tocados aparecem aqui)", C_LINE); }
    if (now - unlockAt < 3000 && lastUnlock >= 0) { float a = now - unlockAt > 2400 ? (float)(1 - (now - unlockAt - 2400) / 600) : 1;
        snprintf(b, sizeof b, "Trapaça liberada: %s", CH[lastUnlock].name); int w = gfx_text_w(b, 2) + 40; ui_panel(W / 2 - w / 2, 40, w, 40, UI_PANEL_AMBER); gfx_text_sh_center(W / 2, 52, 2, b, fb_alpha(C_INK, a), C_SHADOW);
        if (!avail(lastUnlock)) gfx_text_sh_center(W / 2, 84, 1, "(só muda a banda 3D, que este port não desenha)", fb_alpha(C_MUTED, a), C_SHADOW); }
}
void cheats_event(int in, double now) {
    if (entry) return;
    int n = CH_N + 1, c = row - 1;
    if (in == IN_UP) { row = (row + n - 1) % n; audio_sfx(SFX_MOVE); }
    else if (in == IN_DOWN) { row = (row + 1) % n; audio_sfx(SFX_MOVE); }
    else if (in == IN_LEFT || in == IN_RIGHT) { if (row > 0 && avail(c)) { change(c, in == IN_RIGHT ? 1 : -1); audio_sfx(SFX_MOVE); } }
    else if (in == IN_OK) { if (row == 0) { entry = 1; held = 0; nhist = 0; memset(prog, 0, sizeof prog); entryAt = now; input_reset_axes(); audio_sfx(SFX_OK); } else if (avail(c)) { change(c, 1); audio_sfx(SFX_MOVE); } else audio_sfx(SFX_BACK); }
    else if (in == IN_BACK) { settings_save(); A.screen = SC_OPTIONS; A.optCat = -1; A.optRow = retRow; audio_sfx(SFX_BACK); }
}
int cheats_raw_event(const SDL_Event *ev, double now) {
    if (!entry) return 0;
    inev_t evs[4]; int n = input_translate(ev, evs, 4);
    if (now - entryAt < 250) return 1;   /* the press that opened it is not a chord */
    for (int i = 0; i < n; i++) { int a = evs[i].action, pr = evs[i].pressed;
        if (a <= ACT_FRET4) { if (pr) held |= 1 << a; else held &= ~(1 << a); }
        else if ((a == ACT_STRUM_UP || a == ACT_STRUM_DOWN) && pr) strum_code(held, now);
        else if (a == ACT_PAUSE && pr) { entry = 0; audio_sfx(SFX_BACK); } }
    return 1;
}
