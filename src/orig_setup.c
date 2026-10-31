/* The song setup in the game's look. GH3 picks the difficulty on its poster (create_select_difficulty_menu of
 * menu_select_difficulty.qb, as the browser port rebuilt it): the venue wall, 1p_difficulty_poster stretched to 920 x 720
 * in the middle of the 1280 x 720 stage, the four words in the menu font down its pale left column 70 apart with the
 * orange bar sliding to the chosen one, the tapes in its corners. Here the stage is scaled to the screen's height (the
 * poster then fills 640 x 480 nearly edge to edge): the song is written along the poster's top, the difficulties sit where
 * the game lists them, the record goes under them as notes, and the setup's other rows (part, practice sections and
 * speed, strumming, hyperspeed, play) are on a torn notebook page taped over the poster's lower right. */
#include "orig_setup.h"
#include "original.h"
#include "gamearts.h"
#include "app.h"
#include "render.h"
#include "settings.h"
#include "ui.h"
#include "gh3fmt.h"
#include "practice.h"
#include "store.h"
#include "lang.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

enum { SR_DIFF, SR_PART, SR_SECTION, SR_STRUM, SR_HYPER, SR_PLAY, SR_END, SR_SPEED };   /* screens.c's rows, same values */
static const char *F_HEAD = "fontgrid_text_a6", *F_TEXT = "fontgrid_text_a4";
static const char *DIFF_WORD[4] = { "FÁCIL", "MÉDIO", "DIFÍCIL", "EXPERT" };
#define P_INK RGB(60, 35, 20)          /* the poster's words */
#define P_SOFT RGB(115, 78, 52)        /* notes, the year */
#define P_RED RGB(150, 32, 18)         /* headings, the values */
#define P_HILITE RGB(225, 120, 60)     /* the highlight bar */

/* ---- a picture turned by deg (clockwise) about its centre, baked once (the tapes) ---- */
typedef struct { uint32_t key; int deg, ow; gfx_sprite *s; } turned_t; static turned_t turned[8]; static int nturned;
static turned_t *turn(const char *name, int deg) {
    uint32_t k = gh3_qbkey(name); for (int i = 0; i < nturned; i++) if (turned[i].key == k && turned[i].deg == deg) return turned[i].s ? &turned[i] : NULL;
    if (nturned == 8) return NULL; turned_t *t = &turned[nturned++]; t->key = k; t->deg = deg; t->s = NULL; fb_t f, o; if (ga_img_fb(name, &f)) return NULL;
    float a = deg * (float)M_PI / 180, ca = cosf(a), sa = sinf(a); int W = (int)ceilf(fabsf(f.w * ca) + fabsf(f.h * sa)) + 2, H = (int)ceilf(fabsf(f.w * sa) + fabsf(f.h * ca)) + 2; fb_init(&o, W, H);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { float dx = x + 0.5f - W / 2.0f, dy = y + 0.5f - H / 2.0f, sx = dx * ca + dy * sa + f.w / 2.0f - 0.5f, sy = -dx * sa + dy * ca + f.h / 2.0f - 0.5f;
        int x0 = (int)floorf(sx), y0 = (int)floorf(sy); float fx = sx - x0, fy = sy - y0, acc[4] = { 0, 0, 0, 0 };
        for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) { int px = x0 + i, py = y0 + j; if (px < 0 || py < 0 || px >= f.w || py >= f.h) continue; uint32_t c = f.px[(size_t)py * f.w + px]; float al = (c >> 24) * (i ? fx : 1 - fx) * (j ? fy : 1 - fy);
            acc[0] += al; acc[1] += ((c >> 16) & 255) * al; acc[2] += ((c >> 8) & 255) * al; acc[3] += (c & 255) * al; }
        if (acc[0] > 0.5f) o.px[(size_t)y * W + x] = RGBA((int)(acc[1] / acc[0]), (int)(acc[2] / acc[0]), (int)(acc[3] / acc[0]), (int)fminf(255, acc[0])); }
    t->ow = f.w; fb_free(&f); t->s = gfx_bake(&o, 1); fb_free(&o); return t->s ? t : NULL;
}
static void tape(const char *name, int deg, float cx, float cy, float w, float shadow) { /* centred on cx, cy, w px long before turning; its shadow below and to the right */
    turned_t *t = turn(name, deg); if (!t) return; float k = w / t->ow, dw = gfx_sprite_w(t->s) * k, dh = gfx_sprite_h(t->s) * k;
    if (shadow > 0) gfx_blit(t->s, 0, 0, gfx_sprite_w(t->s), gfx_sprite_h(t->s), cx - dw / 2 + shadow, cy - dh / 2 + shadow * 0.7f, dw, dh, RGB(0, 0, 0), 0.4f);
    gfx_blit(t->s, 0, 0, gfx_sprite_w(t->s), gfx_sprite_h(t->s), cx - dw / 2, cy - dh / 2, dw, dh, RGB(255, 255, 255), 1);
}
/* ---- small pieces ---- */
static void arrow(float x, float cy, int dir, uint32_t c) { for (int i = 0; i < 6; i++) gfx_rect(dir < 0 ? x + i : x - i - 1, cy - i, 1, 2 * i + 1, c); }   /* a little triangle, its tip at x */
static void num(long n, char *out, size_t cap) { char raw[32]; snprintf(raw, sizeof raw, "%ld", n < 0 ? 0 : n); int L = (int)strlen(raw), o = 0;   /* 1.234.567 */
    for (int i = 0; i < L && o + 1 < (int)cap; i++) { out[o++] = raw[i]; int left = L - 1 - i; if (left && left % 3 == 0 && o + 1 < (int)cap) out[o++] = '.'; } out[o] = 0; }
static void stars(float x, float y, float sz, int n) { gfx_sprite *on = ga_img("star_solid_32"), *off = ga_img("star_empty_32");
    for (int i = 0; i < 5; i++) { gfx_sprite *s = i < n ? on : off; if (s) gfx_blit(s, 0, 0, gfx_sprite_w(s), gfx_sprite_h(s), x + i * (sz + 1), y, sz, sz, i < n ? RGB(255, 255, 255) : RGB(120, 90, 70), 1); } }
static float wrap(const ga_font *f, float x, float y, float size, float maxW, float lineH, float maxY, const char *s, uint32_t c) { /* words broken into lines; returns the y under the last */
    char line[260] = "", test[260], word[96]; const char *p = s;
    while (*p) { while (*p == ' ') p++; int n = 0; while (p[n] && p[n] != ' ' && n < 95) n++; if (!n) break; memcpy(word, p, n); word[n] = 0; p += n;
        snprintf(test, sizeof test, "%s%s%s", line, *line ? " " : "", word);
        if (*line && ga_text_w(f, test, size) > maxW) { if (y + lineH > maxY) return y; ga_text(f, x, y, size, line, c); y += lineH; snprintf(line, sizeof line, "%s", word); } else memcpy(line, test, sizeof line); }
    if (*line && y + lineH <= maxY + 1) { ga_text_fit(f, x, y, size, maxW, line, c, 0); y += lineH; } return y;
}
static const char *part_word(const gh3_song *s, int part) { return T(part == 1 ? (s->hasRhythm ? "Baixo" : "Guitarra base") : "Guitarra solo"); }   /* screens.c's part_name */
static void section_word(int k, char *out, size_t n) { if (k <= 0 || k > A.S.sd.nmarkers) snprintf(out, n, "%s", T("Do começo")); else { const char *t = app_section_at(A.S.sd.markers[k - 1].t); snprintf(out, n, "%d. %s", k, *t ? t : T("Seção")); } }

int orig_setup_draw(double now, const int *rows, int n) {
    gfx_sprite *poster = ga_img("1p_difficulty_poster"); const ga_font *fh = ga_font_get(F_HEAD), *ft = ga_font_get(F_TEXT); if (!poster || !fh || !ft) return 0;
    int W = A.W, H = A.H, focus = A.setupRow >= 0 && A.setupRow < n ? rows[A.setupRow] : -1; float K = H / 720.0f, OX = (W - 1280 * K) / 2; const gh3_song *s = &A.lib->songs[A.selSong]; char b[240], v[120];
#define SX(x) (OX + (x) * K)
#define SY(y) ((y) * K)
    render_backdrop(now, SET.stageFx);
    gfx_blit(poster, 0, 0, gfx_sprite_w(poster), gfx_sprite_h(poster), SX(180) + 4, 4, 920 * K, 720 * K, RGB(0, 0, 0), 0.45f);   /* its shadow on the wall */
    gfx_blit(poster, 0, 0, gfx_sprite_w(poster), gfx_sprite_h(poster), SX(180), 0, 920 * K, 720 * K, RGB(255, 255, 255), 1);
    tape("tape_01", 90, SX(1086), SY(106), 212 * K, 4);   /* the game's tape over the poster's top right corner */
    /* the song along the poster's top, the artist and year in the pale column */
    if (s->isBoss) { const char *p = strstr(s->title, "vs."); snprintf(b, sizeof b, T("Batalha %s"), p ? p : s->title); } else snprintf(b, sizeof b, "%s", s->title);
    float left = SX(216), colW = SX(500) - left; ga_text_fit(fh, left, 9, 30, SX(1080) - left, b, P_INK, 0);
    ga_text_fit(ft, left, 54, 17, colW, s->artist, P_SOFT, 0); if (*s->year) ga_text(ft, left, 73, 15, s->year, P_SOFT);
    /* the difficulties (menu_sd_move_highlight: the bar slides to its row in 0.05 s) */
    static int lastDiff = -1; static double slideAt; static float slideFrom, barNow; float want = SY(235 + 70 * A.selDiff);
    if (lastDiff < 0) barNow = slideFrom = want; else if (lastDiff != A.selDiff) { slideFrom = barNow; slideAt = now; } lastDiff = A.selDiff;
    { float k = now - slideAt >= 50 || now < slideAt ? 1 : (float)(now - slideAt) / 50; barNow = slideFrom + (want - slideFrom) * k; }
    ga_text(fh, left, SY(235) - 30, 19, T("DIFICULDADE"), P_RED);
    float bh = 37 * K, bw = 300 * K; gfx_rect(left, barNow, bw, bh, focus == SR_DIFF ? P_HILITE : fb_alpha(P_HILITE, 0.42f));
    if (focus == SR_DIFF) { arrow(left + 5, barNow + bh / 2, -1, P_INK); arrow(left + bw - 5, barNow + bh / 2, 1, P_INK); }
    for (int i = 0; i < 4; i++) { float y = SY(235 + 70 * i); ga_text_fit(fh, SX(280), y + bh / 2 - 16, 32, 210 * K, T(DIFF_WORD[i]), i == A.selDiff ? P_INK : fb_alpha(P_INK, 0.8f), 0); }
    /* the record, as notes under them */
    const record_t *rc = records_get(s->name, A.selDiff, A.selPart); float ny = SY(235 + 70 * 4) + 6, nmax = H - 34;
    if (A.mode == MODE_PRACTICE) { ga_text(fh, left, ny, 17, T("TREINO"), P_RED); ny = wrap(ft, left, ny + 22, 14, colW, 16, nmax, T("Não falha, sem Star Power, nada é salvo."), P_SOFT);
        if (rc && ny + 34 <= nmax) { num(rc->score, v, sizeof v); snprintf(b, sizeof b, T("Recorde: %s"), v); ga_text_fit(ft, left, ny + 2, 14, colW, b, P_INK, 0); stars(left, ny + 19, 13, rc->stars); } }
    else { ga_text(fh, left, ny, 17, T("SEU RECORDE"), P_RED); ny += 22;
        if (rc) { num(rc->score, v, sizeof v); snprintf(b, sizeof b, T("%s pontos"), v); ga_text_fit(ft, left, ny, 17, colW - 72, b, P_INK, 0); stars(left + colW - 70, ny + 2, 13, rc->stars); ny += 21;
            snprintf(b, sizeof b, T("%d%% das notas"), rc->pct); ga_text_fit(ft, left, ny, 14, colW, b, P_SOFT, 0); ny += 16;
            snprintf(b, sizeof b, T("maior sequência: %d"), rc->streak); ga_text_fit(ft, left, ny, 14, colW, b, P_SOFT, 0); ny += 16;
            snprintf(b, sizeof b, T("tocada %d %s"), rc->plays, rc->plays == 1 ? T("vez") : T("vezes")); if (ny + 16 <= nmax) ga_text_fit(ft, left, ny, 14, colW, b, P_SOFT, 0); }
        else wrap(ft, left, ny, 14, colW, 16, nmax, T("Ainda sem recorde nesta parte e dificuldade."), P_SOFT); }
    /* the other rows on a taped notebook page */
    int m = n - 1, lock = app_song_lock(A.selTier, A.selSong); float rh = m > 4 ? 25 : 30, cw = 336, ch = 16 + 30 + m * rh + 26, cx = W - 18 - cw, cy = H - 36 - ch;
    gfx_sprite *paper = ga_img("options_calibrate_paper");
    if (paper) { gfx_blit(paper, 8, 4, 198, 244, cx - 8 + 5, cy - 6 + 5, cw + 16, ch + 14, RGB(0, 0, 0), 0.45f); gfx_blit(paper, 8, 4, 198, 244, cx - 8, cy - 6, cw + 16, ch + 14, RGB(246, 238, 222), 1); }
    else ui_panel(cx, cy, cw, ch, UI_PANEL_FLAT);
    tape("tape_02", -24, cx + 14, cy + 4, 62, 3); tape("tape_01", 20, cx + cw - 10, cy + 4, 62, 3);
    ga_text(fh, cx + 22, cy + 12, 21, A.mode == MODE_PRACTICE ? T("PREPARAR O TREINO") : T("PREPARAR"), P_RED);
    int spk = A.practiceSpeed >= 0 && A.practiceSpeed < PRACTICE_SPEEDS ? A.practiceSpeed : 0;
    for (int i = 0, k = 0; i < n; i++) { int r = rows[i], sel = A.setupRow == i; if (r == SR_DIFF) continue; float ry = cy + 16 + 30 + k++ * rh, ty = ry + (rh - 4) / 2 - 9, x0 = cx + 14, x1 = cx + cw - 14;
        if (sel) gfx_rect(x0, ry, x1 - x0, rh - 4, r == SR_PLAY && lock ? RGB(170, 150, 135) : P_HILITE);
        if (r == SR_PLAY) { const char *w = lock ? T("Bloqueada nesta dificuldade") : A.mode == MODE_PRACTICE ? T("TREINAR!") : T("TOCAR!");
            if (lock) ga_text_fit(ft, (x0 + x1) / 2, ty + 1, 16, x1 - x0 - 20, w, RGB(110, 96, 86), 1); else ga_text_center(fh, (x0 + x1) / 2, ty - 3, 23, w, sel ? P_INK : P_RED); continue; }
        const char *label = T(r == SR_PART ? "Parte" : r == SR_SECTION ? "Começar em" : r == SR_END ? "Terminar em" : r == SR_SPEED ? "Velocidade" : r == SR_STRUM ? "Palhetada" : "Velocidade da pista");
        switch (r) { case SR_PART: snprintf(v, sizeof v, "%s", part_word(s, A.selPart)); break; case SR_SECTION: section_word(A.practiceSec, v, sizeof v); break;
            case SR_END: if (A.practiceEnd > 0) section_word(A.practiceEnd, v, sizeof v); else snprintf(v, sizeof v, "%s", T("Até o fim")); break;
            case SR_SPEED: snprintf(v, sizeof v, "%s (%d%%)", T(PRACTICE_SPEED_NAME[spk]), (int)lround(PRACTICE_SPEED[spk] * 100)); break;
            case SR_STRUM: snprintf(v, sizeof v, "%s", T(SET.tap ? "Toque livre" : "Normal")); break; default: if (SET.hyper) snprintf(v, sizeof v, "+%d", SET.hyper); else snprintf(v, sizeof v, "%s", T("Normal")); }
        float lw = ga_text_w(ft, label, 16), vx = x1 - 8 - (sel ? 12 : 0), vmax = vx - (x0 + 8 + lw + 22), vw = fminf(ga_text_w(ft, v, 16), vmax);
        ga_text(ft, x0 + 8, ty, 16, label, P_INK); ga_text_fit(ft, vx, ty, 16, vmax, v, sel ? P_INK : P_RED, 2);
        if (sel) { arrow(vx - vw - 12, ry + (rh - 4) / 2, -1, P_INK); arrow(x1 - 6, ry + (rh - 4) / 2, 1, P_INK); } }
    /* the chart under the rows: its notes, or the stretch being practised */
    const gh3_partdiff *own = &A.S.sd.parts[A.selPart][A.selDiff], *other = &A.S.sd.parts[A.selPart ? 0 : 1][A.selDiff], *pd = own->raw ? own : other;
    if (A.mode == MODE_PRACTICE) { double end = A.S.sd.nfret ? A.S.sd.fretbars[A.S.sd.nfret - 1] : 0, from, to; practice_range(&A.S.sd, A.practiceSec, A.practiceEnd, end, &from, &to); int cnt = 0;
        for (int i = 0; i + 2 < pd->nraw; i += 3) cnt += pd->raw[i] >= from - 1 && pd->raw[i] < to;
        int sec = (int)((to - from) / PRACTICE_SPEED[spk] / 1000 + 0.5); snprintf(b, sizeof b, T("Trecho: %d notas, %d:%02d a %d%% da velocidade"), cnt, sec / 60, sec % 60, (int)lround(PRACTICE_SPEED[spk] * 100)); }
    else snprintf(b, sizeof b, T("%s em %s: %d notas%s"), part_word(s, A.selPart), T(diff_name_pt(A.selDiff)), pd->raw ? pd->nraw / 3 : 0, own->raw ? "" : T(" (partitura da outra parte)"));
    ga_text_fit(ft, cx + 22, cy + ch - 22, 14, cw - 44, b, P_SOFT, 0);
    static const char *bt[3] = { "A", "B", "← →" }, *tx[3] = { "Tocar", "Voltar", "Mudar" }; const char *txl[3] = { T(tx[0]), T(tx[1]), T(tx[2]) }; ui_hints(H - 24, W, bt, txl, 3);
    encore_draw(now);
#undef SX
#undef SY
    return 1;
}
