/* The results in the game's own look, as GH3's scripts lay them out (the browser port rebuilt them: finSummary,
 * drawSummary1, drawFailed, drawDetail) on the 1280 x 720 stage, scaled to the 480 lines of the screen (the sides of
 * the stage, bare table, are cut; the magazine fits):
 *   * Won: "VOCÊ ARRASOU!" shakes letter by letter and bursts apart (split_text_into_array_elements, jiggle_ and
 *     explode_text_array_elements) while the song's magazine lands: the one of the tier the song is in, its logo and
 *     sidebar, the song and band on the cover, the notes hit in the burst, the score box with the stars (fx_stars, as
 *     the other themes) and the difficulty, the note streak, the two choices with the magazine's highlight and icon,
 *     and the detailed breakdown (notes, overstrums, star power, the best and worst sections) on the page.
 *   * Failed: the song failed window over the stage, on the pause frame: the title banner, the song, how far it went
 *     and on which difficulty (a battle: the boss won), the choices in the dialog box.
 * The career's pay, the new record and the notes of practice / no fail / quickplay keep their place on it. */
#include "orig_results.h"
#include "original.h"
#include "gamearts.h"
#include "app.h"
#include "ui.h"
#include "render.h"
#include "fx.h"
#include "store.h"
#include "battle.h"
#include "cheats.h"
#include "practice.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static float S, OX, OY, GA;   /* the stage's scale and place on the screen, the alpha of what is drawn */
static float X(float u) { return OX + u * S; }
static float Y(float v) { return OY + v * S; }
static const char *MAGS[8] = { "backyard", "bar", "paste", "decibel", "ap", "kerrang", "gplayer", "hell" };
static const char *DIFF_UP[4] = { "FÁCIL", "MÉDIO", "DIFÍCIL", "EXPERT" };
#define WHITE RGB(255, 255, 255)
#define BLK RGB(0, 0, 0)
/* the game's g_ss_* colours, and each magazine's, one player: percent, star, star not got, score box left, score, score box right, difficulty, streak box, streak, "note streak", highlight */
enum { OFF, BK, ORG, PMAR, PBRO, PBEI, DMAG, DGRE, DYEL, ARED, ABLU, KRED, GGRE, GBLU, HLIG, HDAR, GREY, BARH };
static const uint32_t SSC[] = { RGB(230, 230, 230), RGB(0, 0, 0), RGB(200, 135, 55), RGB(80, 20, 10), RGB(140, 100, 40), RGB(195, 190, 155), RGB(180, 40, 160), RGB(180, 185, 85), RGB(230, 220, 25),
    RGB(200, 60, 55), RGB(55, 80, 135), RGB(170, 50, 55), RGB(90, 90, 90), RGB(40, 110, 130), RGB(220, 200, 190), RGB(130, 45, 35), RGB(100, 100, 100), RGB(109, 76, 44) };
static const unsigned char SS1[8][11] = { { BK, OFF, GREY, BK, OFF, OFF, BK, OFF, BK, BK, BK }, { BK, OFF, GREY, BK, OFF, GGRE, OFF, GGRE, OFF, OFF, BARH },
    { PBEI, PBRO, OFF, PBEI, PMAR, PBRO, PBEI, PBEI, PMAR, PBRO, PBRO }, { DMAG, ORG, OFF, BK, DYEL, DGRE, DMAG, DGRE, DMAG, BK, DMAG },
    { DYEL, ARED, OFF, ABLU, DYEL, ARED, DYEL, ARED, DYEL, DYEL, ABLU }, { OFF, ORG, OFF, BK, OFF, OFF, BK, BK, OFF, OFF, KRED },
    { GBLU, ORG, OFF, GBLU, OFF, GGRE, OFF, OFF, GBLU, GGRE, GBLU }, { HDAR, ORG, OFF, BK, HLIG, HDAR, BK, HDAR, BK, BK, HDAR } };

/* ---- helpers: pictures and text placed on the stage */
static gfx_sprite *spr_s(gfx_sprite *s, float u, float v, float w, float h, uint32_t tint) {
    if (!s) return NULL; if (w <= 0) { w = (float)gfx_sprite_w(s); h = (float)gfx_sprite_h(s); } gfx_blit(s, 0, 0, gfx_sprite_w(s), gfx_sprite_h(s), X(u), Y(v), w * S, h * S, tint, GA); return s;
}
static gfx_sprite *spr(const char *name, float u, float v, float w, float h, uint32_t tint) { return spr_s(ga_img(name), u, v, w, h, tint); }
static gfx_sprite *mirror(const char *name, int fx, int fy) {   /* a picture flipped, baked once */
    static struct { uint32_t k; int fx, fy; gfx_sprite *s; } c[8]; static int n; uint32_t k = gh3_qbkey(name);
    for (int i = 0; i < n; i++) if (c[i].k == k && c[i].fx == fx && c[i].fy == fy) return c[i].s;
    fb_t f, o; if (n == 8 || ga_img_fb(name, &f)) return NULL; fb_init(&o, f.w, f.h);
    for (int y = 0; y < f.h; y++) for (int x = 0; x < f.w; x++) o.px[(size_t)y * f.w + x] = f.px[(size_t)(fy ? f.h - 1 - y : y) * f.w + (fx ? f.w - 1 - x : x)];
    fb_free(&f); c[n].k = k; c[n].fx = fx; c[n].fy = fy; c[n].s = gfx_bake(&o, 1); fb_free(&o); return c[n++].s;
}
static float tw(const ga_font *f, const char *s) { return ga_text_w(f, s, ga_font_size(f)); }   /* width at the font's own size, stage units */
/* gmText: sx / sy of the font's size, turned rot degrees about its anchor, (jx, jy) the anchor's place in the box */
static void txt(const ga_font *f, const char *s, float u, float v, float sx, float sy, float rot, float jx, float jy, uint32_t c) {
    if (!f || !s || !*s) return; float size = ga_font_size(f) * sy * S, hs = sx / sy, w = ga_text_w(f, s, size) * hs, a = rot * (float)M_PI / 180, ca = cosf(a), sa = sinf(a);
    ga_text_dir(f, X(u) - jx * w * ca + jy * size * sa, Y(v) - jx * w * sa - jy * size * ca, size, hs, ca, sa, s, fb_alpha(c, GA));
}
static void txt_sh(const ga_font *f, const char *s, float u, float v, float sx, float sy, float rot, float jx, float jy, uint32_t c, float d) {
    txt(f, s, u + d, v + d, sx, sy, rot, jx, jy, BLK); txt(f, s, u, v, sx, sy, rot, jx, jy, c);
}
/* finBlock: the text broken into lines of at most w, turned about (u, v); bottom: the block ends at v */
static int block(const ga_font *f, const char *s, float u, float v, float sx, float sy, float w, float rot, int bottom, uint32_t c) {
    if (!f || !s) return 0; char ln[4][160]; int n = 0; ln[0][0] = 0; char word[160]; const char *p = s;
    while (*p && n < 4) { while (*p == ' ') p++; int k = 0; while (*p && *p != ' ' && k < 159) word[k++] = *p++; word[k] = 0; if (!k) break;
        char t[330]; snprintf(t, sizeof t, "%s%s%s", ln[n], ln[n][0] ? " " : "", word);
        if (ln[n][0] && tw(f, t) * sx > w) { if (n == 3) break; n++; snprintf(ln[n], sizeof ln[n], "%s", word); } else snprintf(ln[n], sizeof ln[n], "%.159s", t); }
    if (ln[n][0]) n++; float lh = ga_font_size(f) * sy * 0.85f, y0 = bottom ? -n * lh : 0, a = rot * (float)M_PI / 180;
    for (int i = 0; i < n; i++) { float o = y0 + i * lh; txt(f, ln[i], u - o * sinf(a), v + o * cosf(a), sx, sy, rot, 0, 0, c); }
    return n;
}
static void dots(long n, char *out, size_t cap) {   /* 1.234.567 */
    char raw[32]; snprintf(raw, sizeof raw, "%ld", n < 0 ? -n : n); int L = (int)strlen(raw), o = 0; if (n < 0) out[o++] = '-';
    for (int i = 0; i < L && o + 1 < (int)cap; i++) { out[o++] = raw[i]; int left = L - 1 - i; if (left && left % 3 == 0 && o + 1 < (int)cap) out[o++] = '.'; } out[o] = 0;
}
static void upper(const char *s, char *out, size_t cap) {   /* ASCII and Latin-1 letters to capitals */
    size_t o = 0; for (const unsigned char *p = (const unsigned char *)s; *p && o + 2 < cap; p++) {
        if (*p == 0xC3 && p[1] >= 0xA0 && p[1] <= 0xBE && p[1] != 0xB7) { out[o++] = (char)0xC3; out[o++] = (char)(p[1] - 0x20); p++; }
        else out[o++] = (char)(*p >= 'a' && *p <= 'z' ? *p - 32 : *p); }
    out[o] = 0;
}
static unsigned hsh(unsigned x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
static int mag_of(void) {   /* finMag: the magazine of the career tier the song is in (the first one elsewhere) */
    const gh3_library *L = A.lib; int t = A.selTier; if (!L || t < 0 || t >= L->ntiers || L->tiers[t].tab != 0) return 0;
    int k = 0; for (int i = 0; i < t; i++) if (L->tiers[i].tab == 0) k++; return k > 7 ? 7 : k;
}
static const char *second_item(void) { return A.mode == MODE_PRACTICE ? "MUDAR O TREINO" : store_encore_offer() >= 0 ? "ENCORE!" : "LISTA DE MÚSICAS"; }
/* the notes of each section of the song (the detailed breakdown) */
typedef struct { const char *name; int hit, total; } sec_t;
static int sections(sec_t *out, int cap) {
    const gh3_songdata *sd = &A.S.sd; const gh3_run *r = &A.S.run; int n = 0;
    for (int i = 0; i < sd->nmarkers && n < cap; i++) { int t0 = sd->markers[i].t, t1 = i + 1 < sd->nmarkers ? sd->markers[i + 1].t : 0x7fffffff, hit = 0, tot = 0; if (t1 <= t0) continue;
        for (int k = 0; k < r->nnotes; k++) if (r->notes[k].t >= t0 && r->notes[k].t < t1) { tot++; hit += r->notes[k].hit != 0; }
        if (tot) { out[n].name = app_section_at(t0); out[n].hit = hit; out[n].total = tot; n++; } }
    return n;
}
/* finStrip: a dark band across the bottom with a line or two in it */
static void strip(char lines[][160], int n) {
    const ga_font *f = ga_font_get("fontgrid_text_a4"); if (!f || n <= 0) return; float h = 14 + n * 24, v0 = 640 - h;
    float g = GA; GA = 1; gfx_rect(0, Y(v0), (float)A.W, h * S, RGBA(0, 0, 0, (int)(184 * g))); GA = g;
    for (int i = 0; i < n; i++) { float w = tw(f, lines[i]) * 0.55f, k = w > 1100 ? 1100 / w : 1; txt(f, lines[i], 640, v0 + 7 + i * 24, 0.55f * k, 0.55f, 0, 0.5f, 0, SSC[OFF]); }
}
static int notes_line(char *b, size_t cap) {   /* what the run counts for (as the other themes say it) */
    const gh3_run *r = &A.S.run;
    if (A.mode == MODE_PRACTICE) { int k = A.practiceSpeed > 0 && A.practiceSpeed < PRACTICE_SPEEDS ? A.practiceSpeed : 0; snprintf(b, cap, "Treino a %d%% da velocidade: nada é salvo.", (int)lround(PRACTICE_SPEED[k] * 100)); return 1; }
    if (SET.noFail && !r->failed) { snprintf(b, cap, "Modo sem falhar: não vale para recordes nem para a carreira."); return 1; }
    if (!cheats_records_ok(A.mode) && !r->failed) { snprintf(b, cap, "Easy Expert: não vale para recordes."); return 1; }
    if (A.mode == MODE_QUICK && !r->failed && !A.newRecord) { snprintf(b, cap, "Jogo rápido: o recorde conta, a carreira não muda."); return 1; }
    return 0;
}
static void hints(void) { static const char *bt[2] = { "A", "← →" }, *tx[2] = { "Confirmar", "Escolher" }; ui_hints(A.H - 22, A.W, bt, tx, 2); }

/* ---- won: "VOCÊ ARRASOU!" letter by letter, shaking, then flying off to the four sides */
static void you_rock(double T) {
    const ga_font *f = ga_font_get("fontgrid_text_a10_large"); if (!f) f = ga_font_get("fontgrid_text_a10"); if (!f || T > 2.1) return;
    static const char *L[13] = { "V", "O", "C", "Ê", " ", "A", "R", "R", "A", "S", "O", "U", "!" }; const int n = 13; const float space = 46;
    double age = T * 1000; int beat = (int)(age / 92), shake = age < 1300 && fmod(age, 92) < 75; float boom = age >= 1300 ? (float)fmin(1, (age - 1300) / 700) : 0, e = 1 - (1 - boom) * (1 - boom);
    float g = GA; GA = 1; gfx_rect(0, 0, (float)A.W, (float)A.H, RGBA(0, 0, 0, (int)(130 * (1 - e))));   /* the page in the dark while they shake */
    for (int i = 0, li = 0; i < n; i++) { if (L[i][0] == ' ') continue; float x = 640 - (n - 1) * space / 2 + i * space; unsigned r = hsh(i * 977 + 13), h = hsh(beat * 31 + i * 7); int dir = li++ % 4;
        float tx = dir == 0 || dir == 3 ? -2000 + (r & 1023) / 1023.0f * 2000 : 1280 + (r & 1023) / 1023.0f * 2000, ty = dir < 2 ? -2000 + ((r >> 10) & 1023) / 1023.0f * 1800 : 920 + ((r >> 10) & 1023) / 1023.0f * 1800;
        float dx = shake ? (h & 255) / 255.0f * 20 - 10 : 0, dy = shake ? -10.0f : 0, ks = shake ? 0.7f + ((h >> 10) & 255) / 255.0f * 0.6f : 1, u = x + dx + (tx - x) * e, v = 360 + dy + (ty - 360) * e;
        txt(f, L[i], u, v, ks * 1.3f, ks * 1.2f, 0, 0.5f, 0.5f, BLK); txt(f, L[i], u, v, ks, ks, 0, 0.5f, 0.5f, RGB(223, 223, 223)); }
    GA = g;
}
/* ---- the detailed breakdown (create_detailed_stats_menu's rows), on slips of the detail sheet pasted on the page:
 * label left, number right, every other row shaded; the best section green, the worst red */
#define ROWS 4
typedef struct { char lab[96], val[48]; uint32_t col; } drow_t;
static void slip(const drow_t *rw, int n, float u, float v, float w, float rowH) {
    const ga_font *f = ga_font_get("fontgrid_text_a4"); if (!f || n <= 0) return; float h = n * rowH + 8;
    gfx_rect(X(u + 3), Y(v + 3), w * S, h * S, RGBA(0, 0, 0, (int)(60 * GA))); gfx_rect(X(u), Y(v), w * S, h * S, fb_alpha(RGB(240, 240, 236), GA));
    spr("song_summary_detail_sheet_bottom", u - 2, v + h - w / 8 * 0.3f, w + 4, w / 8, RGB(240, 240, 236));   /* its torn edge */
    float sy = rowH * 0.78f / ga_font_size(f);
    for (int i = 0; i < n; i++) { float y = v + 4 + i * rowH; if (i & 1) gfx_rect(X(u + 4), Y(y), (w - 8) * S, rowH * S, RGBA(0, 0, 0, (int)(26 * GA)));
        float vw = tw(f, rw[i].val) * sy, lw = tw(f, rw[i].lab) * sy, room = w - 22 - vw - 8, k = lw > room ? room / lw : 1;
        txt(f, rw[i].lab, u + 10, y + rowH * 0.1f, sy * k, sy, 0, 0, 0, RGB(75, 75, 75)); txt(f, rw[i].val, u + w - 10, y + rowH * 0.1f, sy, sy, 0, 1, 0, rw[i].col); }
}
static void detail(void) {
    const gh3_run *r = &A.S.run; drow_t a[ROWS], s[2]; int n = 0, m = 0; const uint32_t ink = RGB(75, 75, 75);
#define ROW(arr, cnt, c, l, ...) do { snprintf(arr[cnt].lab, sizeof arr[cnt].lab, "%s", l); snprintf(arr[cnt].val, sizeof arr[cnt].val, __VA_ARGS__); arr[cnt++].col = c; } while (0)
    ROW(a, n, ink, "NOTAS ACERTADAS", "%d/%d", r->hits, r->nnotes);
    ROW(a, n, r->overstrums ? RGB(165, 0, 0) : ink, "PALHETADAS ERRADAS", "%d", r->overstrums);
    if (BTL.on) ROW(a, n, ink, "ATAQUES", "%d SEUS, %d DELE", BTL.me.fired, BTL.cpu.fired);
    else { const gh3_chart *ch = &A.S.chart; int done = 0; for (int i = 0; i < ch->nphrases; i++) done += ch->phrases[i].done != 0; ROW(a, n, ink, "FRASES DE STAR POWER", "%d/%d", done, ch->nphrases); }
    sec_t sc[64]; int ns = sections(sc, 64);
    if (ns > 1) { int hi = 0, lo = 0; for (int i = 1; i < ns; i++) { if (sc[i].hit * sc[hi].total > sc[hi].hit * sc[i].total) hi = i; if (sc[i].hit * sc[lo].total < sc[lo].hit * sc[i].total) lo = i; }
        if (sc[hi].hit * sc[lo].total != sc[lo].hit * sc[hi].total) { char nm[96]; upper(sc[hi].name, nm, sizeof nm); ROW(s, m, RGB(20, 150, 0), nm, "%d %%", sc[hi].hit * 100 / sc[hi].total);
            upper(sc[lo].name, nm, sizeof nm); ROW(s, m, RGB(165, 0, 0), nm, "%d %%", sc[lo].hit * 100 / sc[lo].total); } }
#undef ROW
    slip(a, n, 650, 244, 312, 22);   /* under the choices */
    slip(s, m, 652, 500, 212, 22);   /* under the note streak */
}
/* ---- won: the magazine */
static int magazine(double T) {
    const ga_font *small = ga_font_get("fontgrid_text_a11"), *text = ga_font_get("fontgrid_text_a4"), *a3 = ga_font_get("fontgrid_text_a3"); int m = mag_of();
    gfx_sprite *bg = ga_img(m == 7 ? "song_summary_bg_hell" : "song_summary_bg"); if (!bg) bg = ga_img("song_summary_bg"); if (!bg || !small || !text) return 0;
    const gh3_run *r = &A.S.run; const unsigned char *P = SS1[m]; char b[200], nm[64]; int prac = A.mode == MODE_PRACTICE;
    spr_s(bg, 0, 0, 1280, 720, WHITE);
    snprintf(nm, sizeof nm, "song_summary_logo_%s", MAGS[m]); spr(nm, 158, 25, 0, 0, WHITE);
    snprintf(nm, sizeof nm, "song_summary_sidebar_%s", MAGS[m]); spr(nm, 858, 130, 0, 0, WHITE);
    /* the cover: the song and the band, the notes hit in the burst */
    if (A.S.song->isBoss) { const char *p = strstr(A.S.song->title, "vs."); snprintf(b, sizeof b, "Batalha %s", p ? p : A.S.song->title); } else snprintf(b, sizeof b, "%s", A.S.song->title);
    { float k = strlen(b) >= 19 ? 0.75f : 1; block(small, b, 262, 345, 0.75f * k, 0.75f * k, 300, -7.5f, 1, SSC[OFF]); }
    upper(A.S.song->artist, b, sizeof b); block(text, b, 262, 350, 0.5f, 0.55f, 400, -7.5f, 0, m <= 1 ? SSC[OFF] : SSC[ORG]);
    snprintf(nm, sizeof nm, "song_summary_percent_fill_%s", MAGS[m]); spr(nm, 267, 460, 336, 168, WHITE);
    int pc = A.resultPct; snprintf(b, sizeof b, "%d", pc); float pp[4] = { 302, 501, 1.6f, 1.6f }; if (pc == 100) { pp[0] = 290; pp[1] = 503; pp[2] = 1.3f; } else if (pc < 10) { pp[0] = 332; pp[1] = 496; }
    txt(text, b, pp[0], pp[1], pp[2], pp[3], -10, 0, 0, SSC[P[0]]); txt(text, "%", 364, 500, 1, 1, -10, 0, 0, SSC[P[0]]);
    if (a3) { float w = tw(a3, "ACERTOS"); txt(a3, "ACERTOS", 403, 522, fminf(0.44f, 66 / w), 0.7f, -10, 0, 0, m == 7 ? SSC[HLIG] : BLK); }
    /* the score box: the stars and the score (practice: the notes), the difficulty */
    spr("song_summary_score_fill_l", 674, 90, 268, 67, SSC[P[3]]);
    if (!prac) { dots((long)r->score, b, sizeof b); float w = tw(text, b) * 0.9f, k = w > 200 ? 200 / w : 1; txt(text, b, 926, 116, 0.9f * k, 0.65f, 0, 1, 0.5f, SSC[P[4]]); }
    else { snprintf(b, sizeof b, "%d DE %d", r->hits, r->nnotes); float w = tw(text, b) * 0.9f, k = w > 200 ? 0.9f * 200 / w : 0.9f; txt(text, b, 911, 117, k, 0.65f, -2, 1, 0.5f, SSC[P[4]]); }
    spr("song_summary_score_fill_r", 934, 83, 134, 67, SSC[P[5]]);
    { const char *s = prac ? "NOTAS" : DIFF_UP[A.S.diff < 0 ? 0 : A.S.diff > 3 ? 3 : A.S.diff]; float w = tw(small, s) * 0.7f, k = w > 100 ? 100 / w : 1; txt(small, s, 946, prac ? 98 : 133, 0.7f * k, 0.6f * k, 0, 0, prac ? 0 : 1, SSC[P[6]]); }
    /* the note streak */
    spr("song_summary_notestreak_fill", 719, 359, 0, 0, SSC[P[7]]);
    { int st = r->best; float sp[4] = { 752, 358, 1.5f, 1.5f }; if (st > 999) { sp[0] = 740; sp[2] = 1.12f; } else if (st > 99) sp[0] = 739; else if (st < 10) sp[0] = 766;
      snprintf(b, sizeof b, "%d", st); txt(text, b, sp[0], sp[1], sp[2], sp[3], 0, 0, 0, SSC[P[8]]);
      float w = tw(small, "sequência"), h = ga_font_size(small), k = fminf(80 / w, 35 / h); txt(small, "sequência", 732, 425, k, k, 0, 0, 0, SSC[P[9]]); }
    /* the choices (np_create_options_menu): the highlight and the magazine's own icon on the chosen one */
    static const float NP[6][3] = { { 680, 168, 1 }, { 680, 201, 0.5f }, { 680, 234, 0.5f }, { 680, 266, 0 }, { 680, 162, 0 }, { 680, 195, 0 } };
    const char *items[2] = { "TOCAR DE NOVO", second_item() }; int first = prac ? 4 : 0; float off = prac ? -6 : 0;
    for (int i = 0; i < 2; i++) { float x = NP[first + i][0], y = NP[first + i][1] + off, rot = NP[first + i][2]; int on = i == A.resRow;
        if (on) { spr("song_summary_menu_hilite", x - 30, y + 5, 320, 40, SSC[P[10]]); snprintf(nm, sizeof nm, "song_summary_menu_icon_%s", MAGS[m]); spr(nm, x + 195, y - 17, 80, 80, WHITE); }
        if (on) txt_sh(small, items[i], x, y, i ? 0.8f : 0.85f, 0.7f, rot, 0, 0, SSC[OFF], 2); else txt(small, items[i], x, y, i ? 0.8f : 0.85f, 0.7f, rot, 0, 0, BLK); }
    detail();
    return 1;
}
/* ---- failed: the song failed window */
static void stack(const ga_font *f, const char *const *parts, const float *sc, const uint32_t *col, int n, float cx, float cy, float fitW) {
    float total = 0; for (int i = 0; i < n; i++) total += tw(f, parts[i]) * sc[i]; float k = 0.5f; if (total * k > fitW) k = fitW / total; float x = cx - total * k / 2;
    for (int i = 0; i < n; i++) { float w = tw(f, parts[i]) * sc[i] * k; if (strcmp(parts[i], " ")) txt_sh(f, parts[i], x, cy, sc[i] * k, sc[i] * k, 0, 0, 0.5f, col[i], 1.5f); x += w; }
}
static int failed_window(double T) {
    const ga_font *title = ga_font_get("fontgrid_title_a1"); if (!title || !ga_img("dialog_menu_bg")) return 0;
    const gh3_run *r = &A.S.run; int W = A.W, H = A.H; char b[200], song[160]; float k = T < 0.3 ? (float)(T / 0.3) : 1, e = 1 - (1 - k) * (1 - k);
    if (A.mode == MODE_PRACTICE || !render_venue(A.resultsAt + T * 1000)) render_backdrop(A.resultsAt + T * 1000, SET.stageFx); gfx_rect(0, 0, (float)W, (float)H, RGBA(0, 0, 0, (int)(140 * e)));   /* (gh3-3d: the band's lose stance behind it, as in the game) */
    OY -= 40 * S * (1 - e); GA = e;
    ui_panel(X(388), Y(190), 508 * S, 340 * S, UI_PANEL_DARK);   /* the pause menu's frame */
    spr_s(mirror("dialog_title_bg", 1, 0), 416, 100, 224, 224, WHITE); spr("dialog_title_bg", 640, 100, 224, 224, WHITE);
    const char *head = BTL.on ? "BATALHA PERDIDA" : "VOCÊ FALHOU"; { float w = tw(title, head) * 1.2f, s = w > 285 ? 285 / w * 1.2f : 1.2f; txt_sh(title, head, 640, 217, s, s, 0, 0.5f, 0.5f, RGB(223, 223, 223), 3); }
    const char *dn = DIFF_UP[A.S.diff < 0 ? 0 : A.S.diff > 3 ? 3 : A.S.diff]; uint32_t OR = RGB(210, 130, 0), LT = RGB(223, 223, 223);
    if (BTL.on && BTL.boss) { upper(BTL.boss->name, b, sizeof b); strcat(b, " "); const char *pt[6] = { b, "VENCEU", " ", "NO", " ", dn }; const float sc[6] = { 2, 2, 1, 1, 1, 1 }; const uint32_t cl[6] = { LT, LT, OR, OR, OR, OR }; stack(title, pt, sc, cl, 6, 640, 383, 425); }
    else { upper(A.S.song->title, song, sizeof song); float w = tw(title, song) * 1.65f, s = w > 430 ? 430 / w * 1.65f : 1.65f; txt_sh(title, song, 640, 367, s, s, 0, 0.5f, 0.5f, LT, 3);
        int pct = r->endMs > 0 ? (int)(fmax(0, r->now) / r->endMs * 100) : 0; if (pct > 100) pct = 100; char ps[16]; snprintf(ps, sizeof ps, "%d", pct);
        const char *pt[7] = { "COMPLETOU", " ", ps, "% ", "NO", " ", dn }; const float sc[7] = { 1, 1, 2, 1, 1, 1, 2 }; const uint32_t cl[7] = { OR, OR, OR, OR, OR, OR, OR }; stack(title, pt, sc, cl, 7, 640, 422, 405); }
    /* the choices in the dialog box */
    spr("dialog_menu_bg", 480, 450, 320, 62, WHITE); spr_s(mirror("dialog_menu_bg", 0, 1), 480, 512, 320, 62, WHITE); spr("white", 492, 500, 300, 24, RGB(223, 223, 223));
    const char *items[2] = { "TOCAR DE NOVO", second_item() }; const float iy[2] = { 484, 538 };
    for (int i = 0; i < 2; i++) { float w = tw(title, items[i]) * 0.9f, sx = w > 220 ? 220 / w * 0.9f : 0.9f; int on = i == A.resRow; if (w > 220) w = 220;
        txt(title, items[i], 640, iy[i], sx, 0.95f, 0, 0.5f, 0.5f, on ? RGB(180, 50, 50) : BLK);
        if (on) { spr_s(mirror("dialog_menu_hilight", 1, 0), 635 - w / 2 - 64, iy[i] - 16, 64, 32, WHITE); spr("dialog_menu_hilight", 645 + w / 2, iy[i] - 16, 64, 32, WHITE); } }
    spr("dialog_frame_joiner", 480, 510, 50, 48, WHITE); spr_s(mirror("dialog_frame_joiner", 1, 0), 750, 514, 50, 48, WHITE);
    /* what the run did, under it */
    char lines[3][160]; int n = 0; char sc[32]; dots((long)r->score, sc, sizeof sc);
    snprintf(lines[n++], 160, "PONTOS %s     NOTAS %d DE %d (%d%%)     MAIOR SEQUÊNCIA %d", sc, r->hits, r->nnotes, A.resultPct, r->best);
    if (BTL.on && BTL.boss) snprintf(lines[n++], 160, "Ataques: %d seus, %d de %s", BTL.me.fired, BTL.cpu.fired, BTL.boss->name);
    if (notes_line(b, sizeof b)) snprintf(lines[n++], 160, "%s", b);
    GA = 1; strip(lines, n); return 1;
}

int orig_results_draw(double now) {
    if (!ga_ok() || !A.S.song) return 0; int W = A.W, H = A.H; double T = (now - A.resultsAt) / 1000; const gh3_run *r = &A.S.run; char b[200];
    S = fmaxf(W / 1280.0f, H / 720.0f); OX = (W - 1280 * S) / 2; OY = (H - 720 * S) / 2; GA = 1;
    if (r->failed) { if (!failed_window(T)) return 0; hints(); return 1; }
    /* the magazine lands: a little large and see-through at first, over the stage */
    float k = T < 0.45 ? (float)(T / 0.45) : 1, e = 1 - (1 - k) * (1 - k), z = 1 + 0.12f * (1 - e);
    if (k < 1) { if (!ga_img("song_summary_bg")) return 0; if (A.mode == MODE_PRACTICE || !render_venue(now)) render_backdrop(now, SET.stageFx); }
    float S0 = S; S = S0 * z; OX = W / 2.0f - 640 * S; OY = H / 2.0f - 360 * S; GA = fminf(1, (float)T / 0.3f);
    if (!magazine(T)) return 0;
    const ga_font *small = ga_font_get("fontgrid_text_a11"), *title = ga_font_get("fontgrid_title_a1");
    /* the stars in the score box (or a battle won, in their place) */
    if (BTL.on) { if (BTL.boss && small) { upper(BTL.boss->name, b, sizeof b); char t[160]; snprintf(t, sizeof t, "VENCEU %.140s!", b); float w = tw(small, t) * 0.5f, s = w > 150 ? 0.5f * 150 / w : 0.5f; txt(small, t, 686, 122, s, 0.5f, 0, 0, 0.5f, SSC[SS1[mag_of()][1]]); } }
    int starX = (int)X(744), starY = (int)Y(116), starS = (int)(23 * S + 0.5f);
    S = S0; OX = (W - 1280 * S) / 2; OY = (H - 720 * S) / 2; GA = 1;
    /* a new record: a stamp on the cover */
    if (A.newRecord && T > 1.9 && small) { float q = (float)fmin(1, (T - 1.9) / 0.3), pop = 1 + 0.6f * (1 - q) * (1 - q); GA = q;
        float cx = 440, cy = 414, w = 330 * pop, h = 44 * pop; spr("song_summary_menu_hilite", cx - w / 2, cy - h / 2, w, h, RGB(170, 30, 25));
        txt_sh(small, "NOVO RECORDE!", cx, cy - 2, 0.62f * pop, 0.62f * pop, -2, 0.5f, 0.5f, SSC[OFF], 2); GA = 1; }
    /* the career's pay, on the page's corner */
    if (A.mode == MODE_CAREER && !r->autoPlay && !SET.noFail && T >= 1.2 && title) { float q = (float)fmin(1, (T - 1.2) / 0.3); GA = q; int g = store_last_gain(); char c[32];
        if (g > 0) { dots(g, c, sizeof c); snprintf(b, sizeof b, "+$%s", c); txt_sh(title, b, 660, 578 - 12 * (1 - q), 0.75f, 0.75f, -3, 0, 0, RGB(49, 196, 102), 3); }
        else txt(small, "sem dinheiro novo", 660, 588, 0.45f, 0.45f, -3, 0, 0, RGB(60, 60, 60));
        snprintf(b, sizeof b, "total %s", store_cash_str()); txt(small, b, 864, 584, 0.45f, 0.45f, -3, 1, 0, RGB(60, 60, 60)); GA = 1; }
    you_rock(T);
    if (!BTL.on && A.mode != MODE_PRACTICE) fx_stars(starX, starY, A.resultStars, starS, T, 1);   /* landing one by one with the dings, over the letters' dark */
    { char lines[1][160]; if (notes_line(lines[0], sizeof lines[0])) strip(lines, 1); }
    hints(); return 1;
}
