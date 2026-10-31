/* The set list as GH3 draws it (menu_setlist.qb: create_sl_assets, setlist_scroll, set_song_icon,
 * setlist_display_random_bg_image), placed as the browser port's drawSetlistGH places it on the game's 1280x720
 * stage, here at half size: a stack of torn pages on the wooden floor (the ruled notebook page on top, the red margin,
 * the "Setlist" title), the set names in red ink as page headers, the songs handwritten in the game's note font with
 * the artist under them and the stars and score at the right, the white highlight, the picture of the set's venue held
 * by a paper clip, the shoe prints and the gum, coin, strings and button scattered over the pages, and the light from
 * above. The pages move up as the list goes down, the chosen song resting under the highlight. What the port adds
 * (locks, prices, the career's state, the difficulty) is written on the page in the same hand; the header facts and
 * the button helpers sit in the game's grey pills.
 * Three tabs, as change_tab has them, each its own list: Setlist (the career's sets, then the co-op encores and the rest),
 * Bonus and Downloads. Each tab puts its own page on top (the notebook page, the blue page going on down the floor, the
 * white sheet), writes its name in place of the Setlist title, and has its highlight as wide as its page; the three tab
 * names are written at the top of the pages, L1 / R1 beside the ones the shoulder buttons turn to. */
#include "orig_setlist.h"
#include "gamearts.h"
#include "app.h"
#include "settings.h"
#include "career.h"
#include "store.h"
#include "ui.h"
#include "art.h"
#include "lang.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define K 0.5f                                  /* the game's 1280-wide stage on 640 px */
static const char *F_NOTE = "fontgrid_text_a5", *F_PILL = "fontgrid_title_a1";
#define SL_INK1 RGB(195, 80, 45)                 /* the first set's name */
#define SL_INK RGB(190, 75, 40)                  /* the other sets' names */
#define SL_SONG RGB(50, 30, 10)
#define SL_SONG_ON RGBA(200, 120, 0, 250)
#define SL_ARTIST RGB(60, 100, 140)
#define SL_SCORE RGB(100, 120, 160)
#define SL_STAR RGB(233, 205, 166)
#define SL_LOCK RGB(150, 45, 30)                 /* a red pencil: why a song is shut */
#define SL_PRICE RGB(35, 105, 50)                /* the price tag of a bonus song */
#define SL_DONE RGB(40, 115, 50)

/* ---- pictures baked for the screen: sized, mirrored, turned (clockwise, degrees), or made a print that darkens what
 * is under it (dark: its strength in %, three passes as the game stamps the shoe prints) ---- */
typedef struct { const char *name; int w, h, deg, fx, fy, dark; gfx_sprite *s; } var_t; static var_t vars[96]; static int nvars;
static gfx_sprite *variant(const char *name, int w, int h, int deg, int fx, int fy, int dark) {
    for (int i = 0; i < nvars; i++) if (vars[i].w == w && vars[i].h == h && vars[i].deg == deg && vars[i].fx == fx && vars[i].fy == fy && vars[i].dark == dark && !strcmp(vars[i].name, name)) return vars[i].s;
    if (nvars == 96 || w < 1 || h < 1) return NULL; fb_t f, o; if (ga_img_fb(name, &f)) return NULL;
    float a = deg * (float)M_PI / 180, ca = cosf(a), sa = sinf(a), bw = fabsf(w / 2.0f * ca) + fabsf(h / 2.0f * sa), bh = fabsf(w / 2.0f * sa) + fabsf(h / 2.0f * ca);
    int W = (int)ceilf(bw * 2) + 2, H = (int)ceilf(bh * 2) + 2; fb_init(&o, W, H); float st = dark / 100.0f;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { float dx = x + 0.5f - W / 2.0f, dy = y + 0.5f - H / 2.0f, px = dx * ca + dy * sa, py = -dx * sa + dy * ca;
        float sx = px / w * f.w + f.w / 2.0f - 0.5f, sy = py / h * f.h + f.h / 2.0f - 0.5f; int x0 = (int)floorf(sx), y0 = (int)floorf(sy); float ux = sx - x0, uy = sy - y0;
        if (x0 < -1 || y0 < -1 || x0 >= f.w || y0 >= f.h) continue; float acc[4] = { 0, 0, 0, 0 };
        for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) { int qx = x0 + i, qy = y0 + j; if (qx < 0 || qy < 0 || qx >= f.w || qy >= f.h) continue;
            uint32_t c = f.px[(size_t)(fy ? f.h - 1 - qy : qy) * f.w + (fx ? f.w - 1 - qx : qx)]; float wt = (i ? ux : 1 - ux) * (j ? uy : 1 - uy), al = (c >> 24) * wt;
            acc[0] += al; acc[1] += ((c >> 16) & 255) * al; acc[2] += ((c >> 8) & 255) * al; acc[3] += (c & 255) * al; }
        if (acc[0] < 0.5f) continue; float r = acc[1] / acc[0], g = acc[2] / acc[0], b = acc[3] / acc[0], al = acc[0] > 255 ? 255 : acc[0];
        if (dark) { float k = st * al / 255 * (r + g + b) / 765, d = 1 - (1 - k) * (1 - k) * (1 - k); o.px[(size_t)y * W + x] = RGBA(0, 0, 0, (int)(d * 255)); }
        else o.px[(size_t)y * W + x] = RGBA((int)r, (int)g, (int)b, (int)al); }
    fb_free(&f); var_t *v = &vars[nvars++]; v->name = name; v->w = w; v->h = h; v->deg = deg; v->fx = fx; v->fy = fy; v->dark = dark; v->s = gfx_bake(&o, 1); fb_free(&o); return v->s;
}
/* a picture w x h (screen px) whose point (jx, jy) of it sits at x, y, turned deg about that point */
static void put(const char *name, float x, float y, float w, float h, int deg, float jx, float jy, int fx, int fy, int dark, uint32_t tint, float alpha) {
    if (alpha <= 0.004f) return;
    if (!deg && !fx && !fy && !dark) { gfx_sprite *s = ga_img(name); if (s) gfx_blit(s, 0, 0, gfx_sprite_w(s), gfx_sprite_h(s), x - jx * w, y - jy * h, w, h, tint, alpha); return; }
    gfx_sprite *s = variant(name, (int)lroundf(w), (int)lroundf(h), deg, fx, fy, dark);
    if (!s) { if (!dark) put(name, x, y, w, h, 0, jx, jy, 0, 0, 0, tint, alpha); return; }
    float a = deg * (float)M_PI / 180, ox = (0.5f - jx) * w, oy = (0.5f - jy) * h, cx = x + ox * cosf(a) - oy * sinf(a), cy = y + ox * sinf(a) + oy * cosf(a); int sw = gfx_sprite_w(s), sh = gfx_sprite_h(s);
    gfx_blit(s, 0, 0, sw, sh, cx - sw / 2.0f, cy - sh / 2.0f, (float)sw, (float)sh, tint, alpha);
}

/* ---- the page: where each row sits (stage px), the things scattered over it ---- */
static float *rowY; static int rowCap, nrowsAt = -1, tabAt = -9, firstSong; static double lastDraw = -1e9, changedAt = -1e9; static int lastCursor = -1, clipNeg, artRot; static float clipRot, offNow;
static uint32_t rs = 1; static float rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs / 4294967296.0f; }
typedef struct { const char *tex; float x, y, w, h; int rot, fx, fy, shoe, centre; } scat_t; static scat_t scat[64]; static int nscat;
static const struct { const char *tex; int flip, shoe, dim, lx, rx, onlyL, onlyR, centre, rmin, rmax; } SCAT[6] = {   /* setlist_random_bg_images */
    { "setlist_shoeprint", 1, 1, 512, 0, 0, 0, 0, 0, -5, 5 }, { "setlist_gum", 1, 0, 128, 20, 160, 0, 0, 0, -5, 5 }, { "setlist_coin", 1, 0, 96, 70, 115, 0, 0, 0, -5, 5 },
    { "setlist_wrigleys_gum", 0, 0, 450, 30, 0, 1, 0, 1, -10, 70 }, { "setlist_ernieball", 0, 0, 384, 0, 100, 0, 1, 0, -5, 5 }, { "setlist_button_gunsnroses", 0, 0, 256, -50, 50, 0, 0, 0, -5, 5 } };
static void build(void) { /* create_sl_assets: the first set's name under the title, the songs 80 apart, 110 + 50 around each later set's name */
    if (rowCap < A.nrows + 1) { rowCap = A.nrows + 16; rowY = realloc(rowY, rowCap * sizeof *rowY); }
    float y = 437; int first = 1, nsongs = 0; firstSong = -1;
    for (int i = 0; i < A.nrows; i++) { if (A.rows[i].kind == 0) { if (first) rowY[i] = 383; else { y += 110; rowY[i] = y; y += 50; } first = 0; } else { if (firstSong < 0) firstSong = i; rowY[i] = y; y += 80; nsongs++; } }
    nscat = 0;   /* setlist_display_random_bg_image: two pictures every few songs, one each side, a little below where the list has reached */
    for (int n = 0; n * 4 <= nsongs + 4 && nscat + 2 <= 64; n++) { int old = -1;
        for (int side = 0; side < 2; side++) { int i; do i = (int)(rnd() * 6) % 6; while (i == old); old = i; scat_t *s = &scat[nscat++];
            float down = 300 + (float)(int)(rnd() * 301); int left = side == 0 ? !SCAT[i].onlyR : SCAT[i].onlyL, f = SCAT[i].flip ? 1 + (int)(rnd() * 3) % 3 : 0; float r = SCAT[i].rmin + (SCAT[i].rmax - SCAT[i].rmin) * rnd();
            s->tex = SCAT[i].tex; s->x = left ? (float)SCAT[i].lx : (float)SCAT[i].rx + 1000; s->y = n * 4 * 300 + down; s->w = s->h = (float)SCAT[i].dim; s->shoe = SCAT[i].shoe; s->centre = SCAT[i].centre;
            s->fy = f == 1 || f == 2; s->fx = f == 1 || f == 3; s->rot = SCAT[i].rmax > 20 ? (int)lroundf(r / 10) * 10 : (int)lroundf(r / 2) * 2; } }   /* angles in steps: each one is a texture baked once */
    nrowsAt = A.nrows; tabAt = A.slTab;
}
static const char *ICONS[8] = { "setlist_icon_backyard", "setlist_icon_bar", "setlist_icon_videoshoot", "setlist_icon_odeon", "setlist_icon_prison", "setlist_icon_desert", "setlist_icon_megadome", "setlist_icon_hell" };
static const char *tier_icon(int t) { /* set_song_icon: the venue of the set (the career's sets in their order), the downloads' own, else the general one */
    const gh3_tier *tr = &A.lib->tiers[t]; if (tr->tab == 2) return "setlist_icon_download"; if (tr->tab != 0) return "setlist_icon_generic";
    int ord = 0; for (int i = 0; i < t; i++) ord += A.lib->tiers[i].tab == 0; return ord < 8 && ga_img(ICONS[ord]) ? ICONS[ord] : "setlist_icon_generic";
}

/* ---- words ---- */
static void fmt_num(long n, char *out, size_t cap) { char raw[32]; snprintf(raw, sizeof raw, "%ld", n < 0 ? -n : n); int L = (int)strlen(raw), o = 0; if (n < 0) out[o++] = '-';
    for (int i = 0; i < L && o + 1 < (int)cap; i++) { out[o++] = raw[i]; int left = L - 1 - i; if (left && left % 3 == 0 && o + 1 < (int)cap) out[o++] = '.'; } out[o] = 0; }
static void upper(const char *s, char *out, size_t cap) { /* capitals, the accented Latin-1 letters too */
    size_t o = 0; for (const unsigned char *p = (const unsigned char *)s; *p && o + 2 < cap; p++) { unsigned c = *p;
        if (c >= 'a' && c <= 'z') c -= 32; else if (c == 0xC3 && p[1] >= 0xA0 && p[1] <= 0xBE && p[1] != 0xB7) { out[o++] = (char)c; out[o++] = (char)(p[1] - 0x20); p++; continue; } out[o++] = (char)c; } out[o] = 0; }
static void song_title(const gh3_song *s, char *out, size_t cap) { if (s->isBoss) { const char *p = strstr(s->title, "vs."); snprintf(out, cap, T("Batalha %s"), p ? p : s->title); } else snprintf(out, cap, "%s", s->title); }
static float fit(const ga_font *f, float x, float y, float size, float maxW, const char *s, uint32_t c, int align, int shadow) { /* returns the width drawn */
    float w = ga_text_w(f, s, size); if (maxW > 0 && w > maxW) w = maxW;
    if (shadow) ga_text_fit(f, x + 1, y + 1, size, maxW, s, (c & 0xFF000000) | 0, align); ga_text_fit(f, x, y, size, maxW, s, c, align); return w;
}
/* the game's grey helper pill (helper_pill_body between two ends), as wide as w */
static void pill(float x, float y, float w, float h) {
    float e = h / 2; uint32_t c = RGB(180, 180, 180); gfx_sprite *b = ga_img("helper_pill_body"); if (b) gfx_blit(b, 0, 0, gfx_sprite_w(b), gfx_sprite_h(b), x, y, w, h, c, 1);
    put("helper_pill_end", x - e, y, e, h, 0, 0, 0, 1, 0, 0, c, 1); put("helper_pill_end", x + w, y, e, h, 0, 0, 0, 0, 0, 0, c, 1);
}

int orig_setlist_draw(double now) {
    const ga_font *f = ga_font_get(F_NOTE), *pf = ga_font_get(F_PILL); gh3_library *lib = A.lib;
    if (!f || !pf || !ga_img("setlist_bg_head_676") || !ga_img("setlist_page1_head_90x75") || !A.nrows) return 0;
    int W = A.W, H = A.H, career = A.mode == MODE_CAREER; char b[200], b2[64];
    int back = now - lastDraw > 250 || now < lastDraw || tabAt != A.slTab;   /* coming back to the list (or to another tab): the pictures are thrown again, the page is where the song is */
    if (back) rs = (uint32_t)((uint32_t)now * 2654435761u) ^ 0x5EED1E57u, rs = rs ? rs : 1;
    if (back || nrowsAt != A.nrows) build();
    int tab = A.slTab > 0 && A.slTab <= 2 ? A.slTab : 0; const float hlW = tab == 0 ? 737 : tab == 2 ? 722 : 690;   /* the tab's own page on top, the highlight as wide as it */
    int cur = A.cursor >= 0 && A.cursor < A.nrows ? A.cursor : firstSong; if (cur < 0) return 0;
    if (cur != lastCursor) { if (!back && lastCursor >= 0) { changedAt = now; artRot = (int)(rnd() * 5) * 2 - 4;   /* setlist_scroll: the picture and clip drop away and come back half a second later */
            if (clipNeg) { clipRot = -clipRot; clipNeg = 0; } else { clipRot = -30 + rnd() * 40; clipNeg = 1; } } else changedAt = -1e9; lastCursor = cur; }
    float target = cur == firstSong ? 0 : 357 - rowY[cur]; if (back) offNow = target; else offNow += (target - offNow) * fminf(1, (float)(now - lastDraw) / 70);
    if (fabsf(target - offNow) < 0.5f) offNow = target; lastDraw = now;
    float off = offNow, top = -off - 40, bottom = H / K - off + 40; int fixedUp = cur == firstSong ? 0 : 80;
#define X(sx) ((sx) * K)
#define Y(sy) (((sy) + off) * K)
#define SEE(y0, y1) ((y1) >= top && (y0) <= bottom)
    /* the pages, stacked in the tab's order (change_tab: the notebook page on top for the set list, the blue page for the
     * bonus songs, the white sheet for the downloads), each layer of drawSetlistGH in its turn */
    enum { L_FLOOR, L_SHOE, L_P3, L_P2, L_P1, L_LINES, L_TITLE, L_HL, L_WORDS };
    const float zp1 = tab == 2 ? 3.3f : tab == 1 ? 3.4f : 3.5f, zp2 = tab == 2 ? 3.4f : tab == 1 ? 3.8f : 3.4f, zp3 = tab == 2 ? 3.5f : 3.3f, p3y = tab == 2 ? 126 : 86;
    struct { float z; int id; } ly[9] = { { 3.1f, L_FLOOR }, { 3.2f, L_SHOE }, { zp3, L_P3 }, { zp2, L_P2 }, { zp1, L_P1 }, { zp1 + 0.1f, L_LINES }, { tab == 0 ? zp1 + 0.2f : -1, L_TITLE }, { 4.0f, L_HL }, { 4.1f, L_WORDS } };
    for (int i = 1; i < 9; i++) for (int j = i; j > 0 && ly[j - 1].z > ly[j].z; j--) { float z = ly[j].z; int d = ly[j].id; ly[j] = ly[j - 1]; ly[j - 1].z = z; ly[j - 1].id = d; }   /* stable: equal layers keep their order */
    for (int li = 0; li < 9; li++) switch (ly[li].z < 0 ? -1 : ly[li].id) {
    case L_FLOOR:
        put("setlist_bg_head_676", 0, Y(0), 640, 676 * K, 0, 0, 0, 0, 0, 0, RGB(255, 255, 255), 1);
        for (int k = 0; 676 + k * 1308 < bottom; k++) { float y = 676 + k * 1308.0f; if (SEE(y, y + 1352)) put("setlist_bg_loop_1308", 0, Y(y), 640, (k ? 1308 : 1352) * K, 0, 0, 0, 0, 0, 0, RGB(255, 255, 255), 1); } break;
    case L_SHOE:
        put("setlist_shoeprint", X(850), Y(-70), 640 * K, 768 * K, 10, 0, 0, 1, 0, 15, RGB(255, 255, 255), 1);
        for (int i = 0; i < nscat; i++) if (scat[i].shoe && SEE(scat[i].y - 400, scat[i].y + 600)) put(scat[i].tex, X(scat[i].x), Y(scat[i].y), 512 * K, 512 * K, scat[i].rot, 0, 0, scat[i].fx, scat[i].fy, 100, RGB(255, 255, 255), 1); break;
    case L_P3:   /* the white sheet */
        put("setlist_page3_head_9x6", X(210), Y(p3y), 922 * K, 614 * K, 0, 0, 0, 0, 0, 0, RGB(255, 255, 255), 1);
        for (float y = p3y + 614; y < bottom; y += 532) if (SEE(y, y + 533)) put("setlist_page3_loop_13x11", X(30), Y(y), 1254 * K, 533 * K, 0, 0, 0, 0, 0, 0, RGB(255, 255, 255), 1); break;
    case L_P2:   /* the blue page: it goes on down the floor only when it is the bonus songs' */
        put("setlist_page2_head_80x54", X(240), Y(50), 819 * K, 553 * K, 0, 0, 0, 0, 0, 0, RGB(255, 255, 255), 1);
        if (tab == 1) for (float y = 603; y < bottom; y += 665.5f) if (SEE(y, y + 666)) put("setlist_page2_loop_80x65", X(240), Y(y), 819 * K, 666 * K, 0, 0, 0, 0, 0, 0, RGB(255, 255, 255), 1); break;
    case L_P1:   /* the notebook page */
        put("setlist_page1_head_90x75", X(160), Y(0), 922 * K, 768 * K, 0, 0, 0, 0, 0, 0, RGB(255, 255, 255), 1);
        for (float y = 768; y < bottom; y += 512) if (SEE(y, y + 512)) put("setlist_page1_loop_89x33", X(157), Y(y), 922 * K, 513 * K, 0, 0, 0, 0, 0, 0, RGB(255, 255, 255), 1); break;
    case L_LINES: {
        float y0 = fmaxf(12, top); put("setlist_page1_line_red", X(320), Y(y0), 8 * K, (bottom - y0) * K, 0, 0, 0, 0, 0, 0, RGB(255, 255, 255), 1);   /* the margin */
        put("setlist_page1_head_lines", X(176), Y(64), 896 * K, 320 * K, 0, 0, 0, 0, 0, 0, RGB(255, 255, 255), 1);
        static const char *SOLID[3] = { "setlist_page1_line_solid_1", "setlist_page1_line_solid_2", "setlist_page1_line_solid_3" }, *DOTTED[3] = { "setlist_page1_line_dotted_1", "setlist_page1_line_dotted_2", "setlist_page1_line_dotted_3" };
        for (int k = top > 420 ? (int)((top - 420) / 80) : 0; 420 + 80.0f * k < bottom; k++) { float y = 420 + 80.0f * k, w = k < 8 ? 883 : 896; uint32_t h = (uint32_t)k * 2654435761u;   /* which of the three ruled lines each one is */
            put(SOLID[(h >> 7) % 3], X(176), Y(y), w * K, 16 * K, 0, 0, 0, 0, 0, 0, RGB(255, 255, 255), 1); put(DOTTED[(h >> 13) % 3], X(176), Y(y + 40), w * K, 16 * K, 0, 0, 0, 0, 0, 0, RGB(255, 255, 255), 1); } break; }
    case L_TITLE:
        put("setlist_page1_title", X(330), Y(220), 512 * K, 128 * K, 0, 0, 0, 0, 0, 0, RGB(255, 255, 255), 0.7f); put("setlist_page1_title", X(325), Y(230), 512 * K, 128 * K, -2, 0, 0, 0, 0, 0, RGB(255, 255, 255), 0.2f); break;
    case L_HL:   /* the highlight: it stays put while the pages move */
        gfx_rect(X(326), (428 - fixedUp) * K, hlW * K, 80 * K, RGBA(255, 255, 255, 128)); break;
    case L_WORDS:   /* the words on the page: the bonus and downloaded lists have their name written large in place of the title */
    if (tab) { const ga_font *a10 = ga_font_get("fontgrid_text_a10"); if (!a10) a10 = f; fit(a10, X(330), Y(220), ga_font_size(a10) * 2 * K, X(1010 - 330), tab == 1 ? T("MÚSICAS BÔNUS") : T("MÚSICAS BAIXADAS"), RGB(50, 30, 20), 0, 0); }
    for (int i = 0; i < A.nrows; i++) { float y = rowY[i]; if (!SEE(y - 10, y + 90)) continue;
        if (A.rows[i].kind == 0) { int t = A.rows[i].idx; const gh3_tier *tr = &lib->tiers[t]; const tierstate_t *ts = &A.tiers[t]; const char *st = NULL; uint32_t sc = SL_ARTIST;
            if (tr->tab == 1) snprintf(b, sizeof b, "%s", T("MÚSICAS BÔNUS")); else if (tr->tab == 2) snprintf(b, sizeof b, "%s", T("MÚSICAS BAIXADAS")); else if (tr->tab == 3) snprintf(b, sizeof b, "%s", T("MAIS MÚSICAS")); else upper(tr->title, b, sizeof b);
            if (tr->career && career) { st = b2; if (!ts->open) st = T("bloqueado: termine o set anterior"); else if (ts->done) { st = T("set concluído"); sc = SL_DONE; }
                else if (ts->regDone < ts->required) snprintf(b2, sizeof b2, T("%d de %d músicas para liberar %s"), ts->regDone, ts->required, tr->bossIdx >= 0 ? T("a batalha") : T("o encore"));
                else if (tr->bossIdx >= 0 && !ts->bossDone) st = T("vença a batalha para liberar o encore"); else st = T("toque o encore para fechar o set"); }
            else if (tr->tab == 1 && !SET.unlockAll) st = career ? T("à venda na Loja: aperte Y") : T("à venda na Loja da carreira");
            float sw = st ? ga_text_w(f, st, 55 * 0.5f * K) : 0; if (st) ga_text_right(f, X(1010), Y(y + 18), 55 * 0.5f * K, st, sc);
            if (tab == 0 || i != 0) fit(f, X(300), Y(y), 55 * K, X(1010 - 300) - sw - (st ? 12 : 0), b, fb_alpha(i == 0 ? SL_INK1 : SL_INK, tr->career && career && !ts->open ? 0.45f : 1), 0, 0); continue; }
        const gh3_song *s = &lib->songs[A.rows[i].idx]; int on = i == cur, lock = app_song_lock(A.rows[i].tier, A.rows[i].idx);
        const record_t *rc = records_get(s->name, A.selDiff, 0), *rc2 = records_get(s->name, A.selDiff, 1); if (!rc || (rc2 && rc2->score > rc->score)) rc = rc2; float x = 340, right = 0;
        if (lock) { const char *why = lock >= LOCK_BUY ? store_lock_text(A.rows[i].idx, career) : !career ? T("libere na carreira") : lock == LOCK_TIER ? T("bloqueado") : lock == LOCK_BOSS ? T("batalha bloqueada") : T("encore bloqueado");
            right = ga_text_w(f, why, 55 * 0.6f * K); ga_text_right(f, X(x + 660), Y(y + 40), 55 * 0.6f * K, why, lock == LOCK_BUY ? SL_PRICE : SL_LOCK); }
        else if (rc) { if (rc->stars > 2) for (int k = 1; k <= rc->stars && k <= 5; k++) { uint32_t h = (uint32_t)(A.rows[i].idx * 7 + k) * 2654435761u; static const char *ST[3] = { "setlist_page1_loop_star1", "setlist_page1_loop_star2", "setlist_page1_loop_star3" };
                put(rc->pct >= 100 ? "setlist_star_gold" : ST[(h >> 9) % 3], X(x + 660 - 20 * k), Y(y), 32 * K, 32 * K, 0, 0, 0, 0, 0, 0, SL_STAR, 1); }
            if (rc->score > 0) { fmt_num(rc->score, b2, sizeof b2); right = ga_text_w(f, b2, 55 * 0.75f * K); ga_text_right(f, X(x + 660), Y(y + 40), 55 * 0.75f * K, b2, SL_SCORE); } }
        song_title(s, b, sizeof b); float tw = fit(f, X(x), Y(y), 55 * 0.85f * K, X(rc && rc->stars > 2 && !lock ? 540 : 640) - (s->isBoss ? 16 : 0), b, lock ? fb_alpha(on ? SL_SONG_ON : SL_SONG, 0.5f) : on ? SL_SONG_ON : SL_SONG, 0, on);
        if (s->isBoss) { gfx_sprite *sk = art_get(ART_SKULL); if (sk) gfx_blit(sk, 0, 0, gfx_sprite_w(sk), gfx_sprite_h(sk), X(x) + tw + 5, Y(y) + 3, 16, 16.0f * gfx_sprite_h(sk) / gfx_sprite_w(sk), RGB(60, 35, 20), lock ? 0.5f : 1); }
        upper(s->artist, b, sizeof b); fit(f, X(x + 60), Y(y + 40), 55 * 0.6f * K, X(600) - right - 10, b, lock ? fb_alpha(SL_ARTIST, 0.6f) : SL_ARTIST, 0, 0); }
    break; }
    /* the venue's picture and the clip holding it, fading in half a second after the choice settles */
    float k = (float)fmin(1, fmax(0, (now - changedAt - 500) / 250)), ez = k * k * (3 - 2 * k); const char *icon = tier_icon(A.rows[cur].tier);
    if (k > 0) { put(icon, X(163), (393 - fixedUp) * K, 160 * K, 160 * K, artRot, 0, 0, 0, 0, 0, RGB(0, 0, 0), 0.5f * k); put(icon, X(160), (390 - fixedUp) * K, 160 * K, 160 * K, artRot, 0, 0, 0, 0, 0, RGB(200, 200, 200), k);
        put("setlist_clip", X(175), (440 - fixedUp) * K, 141 * K, 102 * K, (int)lroundf((clipRot + 10 * (1 - ez)) / 4) * 4, 0.25f, 0.05f, 0, 0, 0, RGB(255, 255, 255), k); }
    put("setlist_overshadow", 0, 0, (float)W, (float)H, 0, 0, 0, 0, 0, 0, RGB(105, 65, 7), 160 / 255.0f);   /* the light from above */
    for (int i = 0; i < nscat; i++) if (!scat[i].shoe && SEE(scat[i].y - 300, scat[i].y + 300)) for (int n = 0; n < 2; n++)
        put(scat[i].tex, X(scat[i].x), Y(scat[i].y), scat[i].w * K, scat[i].h * K, scat[i].rot, scat[i].centre ? 0.5f : 0, scat[i].centre ? 0.5f : 0, scat[i].fx, scat[i].fy, 0, RGB(255, 255, 255), 1);
    /* the tabs, written at the top of each page, with the shoulder button that turns to the one beside (the game puts the
     * fret button that opens each there; on the R36S L1 and R1 go one tab left or right). A tab with no songs is faded. */
    { static const char *TW[3] = { "setlist", "bônus", "downloads" }; const float TX[3] = { 300, 624, 870 }, TY[3] = { 70, 102, tab == 2 ? 160.0f : 120 }, GX[3] = { 260, 580, 830 }, GY[3] = { 65, 90, tab == 2 ? 150.0f : 110 };
      for (int t = 0; t < 3; t++) { int has = 0; for (int i = 0; i < lib->ntiers && !has; i++) if (screens_tab_of(i) == t) has = lib->tiers[i].nsongs > 0;
          fit(f, X(TX[t]), Y(TY[t]), 55 * K, 0, T(TW[t]), has ? RGB(0, 0, 0) : RGBA(0, 0, 0, 90), 0, 0);
          if (t == tab - 1 || t == tab + 1) fit(f, X(GX[t]) - 6, Y(GY[t]) + 4, 44 * K, 0, t < tab ? "L1" : "R1", RGB(110, 110, 110), 0, 0); } }
    /* the mode and difficulty, and the career at a glance, in pills along the top */
    { const float TS = 15, PH = 22, py = 5; uint32_t ink = RGB(0, 0, 0), red = RGB(120, 20, 10); const char *mode = career ? T("CARREIRA") : A.mode == MODE_QUICK ? T("JOGO RÁPIDO") : T("TREINO"); char d[32]; upper(T(diff_name_pt(A.selDiff)), d, sizeof d);
        float w = ga_text_w(pf, mode, TS) + 10 + ga_text_w(pf, d, TS); pill(14, py, w + 8, PH); ga_text(pf, 18, py + 3, TS, mode, ink); ga_text(pf, 18 + ga_text_w(pf, mode, TS) + 10, py + 3, TS, d, red);
        int done = career_tiers_done(lib, A.tiers), ntier = 0; for (int t = 0; t < lib->ntiers; t++) ntier += lib->tiers[t].career;
        char st[16]; snprintf(st, sizeof st, "%d", records_count_stars()); if (career) snprintf(b, sizeof b, "%d/%d SETS", done, ntier); const char *cash = career ? store_cash_str() : NULL;
        float ww = 20 + ga_text_w(pf, st, TS) + (career ? 14 + ga_text_w(pf, b, TS) + 14 + ga_text_w(pf, cash, TS) : 0), px = W - 14 - ww - 8;
        pill(px, py, ww + 8, PH); px += 4; put("setlist_star_gold", px, py + 2, 18, 18, 0, 0, 0, 0, 0, 0, RGB(255, 255, 255), 1); px += 20; ga_text(pf, px, py + 3, TS, st, ink); px += ga_text_w(pf, st, TS) + 14;
        if (career) { ga_text(pf, px, py + 3, TS, b, done == ntier ? RGB(20, 90, 30) : ink); px += ga_text_w(pf, b, TS) + 14; ga_text(pf, px, py + 3, TS, cash, RGB(20, 90, 30)); } }
    static const char *bt[5] = { "A", "B", "← →", "L1 R1", "Y" }, *tx[5] = { "Escolher", "Voltar", "Dificuldade", "Abas", "Loja" }; const char *txl[5]; for (int i = 0; i < 5; i++) txl[i] = T(tx[i]); ui_hints(H - 24, W, bt, txl, career ? 5 : 4);
    /* a set or song just opened: a note taped over the list */
    if (A.unlockAt > 0 && now - A.unlockAt < 4500) { float a = now - A.unlockAt > 3500 ? (float)(1 - (now - A.unlockAt - 3500) / 1000) : 1, sz = 26, tw = fminf(W - 120.0f, ga_text_w(f, A.unlockMsg, sz)), cw = tw + 70, ch = 74, cx = W / 2.0f - cw / 2, cy = H / 2.0f - ch / 2;
        gfx_rect(cx + 5, cy + 6, cw, ch, RGBA(0, 0, 0, (int)(90 * a))); put("setlist_page3_head_9x6", cx, cy, cw, ch, 0, 0, 0, 0, 0, 0, RGB(255, 250, 235), a);
        put("tape_01", W / 2.0f, cy + 2, 90, 30, -4, 0.5f, 0.5f, 0, 0, 0, RGB(255, 255, 255), a);
        ga_text_fit(f, W / 2.0f, cy + ch / 2 - sz * 0.55f, sz, W - 120.0f, A.unlockMsg, fb_alpha(SL_INK, a), 1); }
#undef X
#undef Y
#undef SEE
    return 1;
}
