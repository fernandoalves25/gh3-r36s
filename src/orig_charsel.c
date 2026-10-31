/* The character hub and its lists in the game's look (charsel.c says what is on them). GH3 builds these menus
 * (menu_character_hub, the select character / outfit / guitar / finish menus) from one set of pieces of
 * ZONES/global, named in its scripts only by key: a list frame's body and its curled cap (each an outline and a fill
 * drawn in two colours), the dark name plate with its orange rim, the winged ornament, the swooshed highlight bar
 * with the sparkle bookends (character_hub_hilite_bookend) on the chosen row. The game shows the rocker in 3D beside
 * the list; here it is the rocker's photo (the picture each character pak carries), pinned on the wall with tape, the
 * name plate under it, the outfit and guitar written below. */
#include "charsel.h"
#include "original.h"
#include "gamearts.h"
#include "app.h"
#include "render.h"
#include "ui.h"
#include "lang.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define K_PLATE 0x68968554u       /* the dark name plate, orange rim (256 x 256) */
#define K_WING 0xd4ad250du        /* the winged ornament (128 x 64) */
#define K_BODY_LINE 0x721db38eu   /* a list frame's body: its outline (256 x 256) */
#define K_BODY_FILL 0xae017b97u   /* and its fill */
#define K_CAP_LINE 0xb532229bu    /* the curled cap: outline (256 x 64) */
#define K_CAP_FILL 0xecab1ed6u    /* and fill */
#define K_BAND 0xf1d3e6f7u        /* the ornament band (256 x 64) */
#define K_HILITE 0xec880166u      /* the highlight bar (256 x 128) */
static const char *F_TITLE = "fontgrid_title_a1", *F_HEAD = "fontgrid_text_a6", *F_TEXT = "fontgrid_text_a4";
#define C_FILL RGB(24, 14, 10)      /* the list's dark fill */
#define C_LINE_ RGB(205, 120, 52)   /* its outline */
#define C_CARD RGB(238, 230, 214)   /* the photo's card */
#define C_INKD RGB(52, 30, 18)      /* words on the card and on the highlight */
#define C_CREAM RGB(236, 222, 196)  /* words on the dark */
#define C_SOFT RGB(170, 150, 128)
#define C_HI RGB(232, 126, 58)      /* the highlight */
#define C_PRICE RGB(250, 196, 80)

static void fit(gfx_sprite *s, float x, float y, float w, float h, uint32_t t, float a) { if (s) gfx_blit(s, 0, 0, gfx_sprite_w(s), gfx_sprite_h(s), x, y, w, h, t, a); }
static gfx_sprite *flipped(uint32_t key, int fx, int fy) {   /* a piece mirrored, baked once */
    static struct { uint32_t k; int fx, fy; gfx_sprite *s; } c[8]; static int n;
    for (int i = 0; i < n; i++) if (c[i].k == key && c[i].fx == fx && c[i].fy == fy) return c[i].s;
    fb_t f, o; if (n == 8 || ga_img_key_fb(key, &f)) return NULL; fb_init(&o, f.w, f.h);
    for (int y = 0; y < f.h; y++) for (int x = 0; x < f.w; x++) o.px[(size_t)y * f.w + x] = f.px[(size_t)(fy ? f.h - 1 - y : y) * f.w + (fx ? f.w - 1 - x : x)];
    fb_free(&f); c[n].k = key; c[n].fx = fx; c[n].fy = fy; c[n].s = gfx_bake(&o, 1); fb_free(&o); return c[n++].s;
}
static float wrap(const ga_font *f, float x, float y, float size, float maxW, float lineH, float maxY, const char *s, uint32_t c) {   /* words in lines; the y under the last */
    char line[300] = "", test[300], word[120]; const char *p = s;
    while (p && *p) { while (*p == ' ' || *p == '\n') p++; int n = 0; while (p[n] && p[n] != ' ' && p[n] != '\n' && n < 119) n++; if (!n) break; memcpy(word, p, n); word[n] = 0; p += n;
        snprintf(test, sizeof test, "%s%s%s", line, *line ? " " : "", word);
        if (*line && ga_text_w(f, test, size) > maxW) { if (y + 2 * lineH > maxY) { snprintf(test, sizeof test, "%s…", line); ga_text_fit(f, x, y, size, maxW, test, c, 0); return y + lineH; } ga_text(f, x, y, size, line, c); y += lineH; snprintf(line, sizeof line, "%s", word); }
        else memcpy(line, test, sizeof line); }
    if (*line && y + lineH <= maxY + 1) { ga_text_fit(f, x, y, size, maxW, line, c, 0); y += lineH; } return y;
}
/* a list frame: the curled cap on top, the body, the cap again upside down at the bottom; each piece's fill, then its outline */
static void frame(float x, float y, float w, float h) {
    float cap = w * 40 / 256; gfx_sprite *cf = ga_img_key(K_CAP_FILL), *cl = ga_img_key(K_CAP_LINE), *bf = ga_img_key(K_BODY_FILL), *bl = ga_img_key(K_BODY_LINE), *cfd = flipped(K_CAP_FILL, 0, 1), *cld = flipped(K_CAP_LINE, 0, 1);
    float shadow = 5; uint32_t sh = RGB(0, 0, 0);
    fit(cf, x + shadow, y + shadow, w, cap, sh, 0.45f); fit(bf, x + shadow, y + cap + shadow, w, h - 2 * cap, sh, 0.45f); fit(cfd, x + shadow, y + h - cap + shadow, w, cap, sh, 0.45f);
    fit(cf, x, y, w, cap, C_FILL, 0.93f); fit(bf, x, y + cap - 1, w, h - 2 * cap + 2, C_FILL, 0.93f); fit(cfd, x, y + h - cap, w, cap, C_FILL, 0.93f);
    fit(cl, x, y, w, cap, C_LINE_, 1); fit(bl, x, y + cap - 1, w, h - 2 * cap + 2, C_LINE_, 1); fit(cld, x, y + h - cap, w, cap, C_LINE_, 1);
}
static const char *say(const ga_font *f, const char *s) {   /* the data's apostrophes as the font has them ("Black 'N Blue": the text font lacks ', not ’) */
    static char buf[4][800]; static int k; if (!s || !strchr(s, '\'') || ga_has(f, '\'')) return s; char *o = buf[k = (k + 1) & 3]; size_t n = 0; int curly = ga_has(f, 0x2019) != 0;
    for (; *s && n + 4 < sizeof buf[0]; s++) { if (*s == '\'') { if (curly) { o[n++] = (char)0xE2; o[n++] = (char)0x80; o[n++] = (char)0x99; } else o[n++] = *s; } else o[n++] = *s; }
    o[n] = 0; return o;
}
static void tape(float cx, float cy, float w) { gfx_sprite *t = ga_img("tape_h_02"); if (!t) return; float h = w * gfx_sprite_h(t) / gfx_sprite_w(t); fit(t, cx - w / 2 + 2, cy - h / 2 + 2, w, h, RGB(0, 0, 0), 0.35f); fit(t, cx - w / 2, cy - h / 2, w, h, RGB(255, 255, 255), 0.95f); }
static void star(float x, float y, float sz) { gfx_sprite *s = ga_img("star_solid_32"); fit(s, x, y, sz, sz, RGB(255, 255, 255), 1); }

int orig_charsel_draw(double now, const cs_view *v) {
    const ga_font *ft = ga_font_get(F_TEXT), *fh = ga_font_get(F_HEAD), *fT = ga_font_get(F_TITLE); if (!ft || !fh || !fT || !ga_img_key(K_BODY_FILL) || !ga_img_key(K_PLATE)) return 0;
    int W = A.W, H = A.H; char b[160]; render_backdrop(now, SET.stageFx);
    snprintf(b, sizeof b, T("Seu dinheiro: %s"), v->cash); ui_header(W, v->title, b);
    /* the photo on its card, taped to the wall; a locked rocker's photo in the dark with its price */
    float cx = 18, cy = 66, cw = 252, ch = 252, px = cx + 10, py = cy + 10, pw = cw - 20; int locked = !v->hub && v->sel >= 0 && v->sel < v->n && v->rows[v->sel].locked && !strcmp(v->title, T("ROQUEIROS"));
    gfx_rect(cx + 5, cy + 6, cw, ch, RGBA(0, 0, 0, 110)); gfx_rect(cx, cy, cw, ch, C_CARD);
    if (v->photo) fit(v->photo, px, py, pw, pw, locked ? RGB(80, 70, 66) : RGB(255, 255, 255), 1); else { gfx_rect(px, py, pw, pw, RGB(40, 30, 26)); ga_text_fit(ft, px + pw / 2, py + pw / 2 - 9, 16, pw - 20, T("sem foto"), C_SOFT, 1); }
    if (locked) { const char *tag = v->rows[v->sel].tag; const char *lk = T("BLOQUEADO"); float lsz = 34; while (lsz > 18 && ga_text_w(fT, lk, lsz) > pw - 16) lsz -= 2; ga_text_center(fT, px + pw / 2 + 2, py + pw / 2 - 30, lsz, lk, RGB(0, 0, 0)); ga_text_center(fT, px + pw / 2, py + pw / 2 - 32, lsz, lk, RGB(225, 70, 40));
        if (tag[0] == '$') { snprintf(b, sizeof b, T("%s na Loja"), tag); ga_text_fit(fh, px + pw / 2, py + pw / 2 + 12, 20, pw - 16, b, C_PRICE, 1); } }
    tape(cx + 40, cy + 2, 74); tape(cx + cw - 40, cy + 2, 74);
    /* the name plate under it, the winged ornament on its sides */
    float ny = cy + ch + 4, nw = 270, nh = 66, nx = cx + cw / 2 - nw / 2; fit(ga_img_key(K_PLATE), nx - 12, ny - 30, nw + 24, nh + 56, RGB(255, 255, 255), 1);
    { gfx_sprite *wl = ga_img_key(K_WING), *wr = flipped(K_WING, 1, 0); fit(wr, nx - 18, ny + 6, 44, 22, RGB(255, 255, 255), 1); fit(wl, nx + nw - 26, ny + 6, 44, 22, RGB(255, 255, 255), 1); }
    ga_text_fit(fT, nx + nw / 2 + 1, ny + 5, 30, nw - 70, v->name, RGB(0, 0, 0), 1); ga_text_fit(fT, nx + nw / 2, ny + 4, 30, nw - 70, v->name, C_CREAM, 1);
    float ly = ny + nh - 6; if (v->outfit[0]) { snprintf(b, sizeof b, T("Visual: %s"), v->outfit); ga_text_fit(ft, cx + 4, ly, 15, cw + 10, say(ft, b), C_CREAM, 0); ly += 18; }
    if (v->guitar[0]) { snprintf(b, sizeof b, T("Guitarra: %s"), v->guitar); ga_text_fit(ft, cx + 4, ly, 15, cw + 10, say(ft, b), C_CREAM, 0); }
    /* the list in its frame */
    float fx = 290, fy = 64, fw = W - fx - 12, fh_ = H - fy - 70, cap = fw * 40 / 256; frame(fx, fy, fw, fh_);
    fit(ga_img_key(K_BAND), fx + fw / 2 - 70, fy + 2, 140, 35, RGB(255, 255, 255), 1);   /* the ornament band on the cap */
    float rowH = v->hub ? 34 : 25, y0 = fy + cap * 0.75f + 4, inner = fh_ - cap * 1.5f - 8;   /* the caps' rims are their top third: rows go over the rest */ int vis = (int)(inner / rowH), n = v->n; if (vis < 1) vis = 1;
    static int top; if (v->sel < top) top = v->sel; if (v->sel >= top + vis) top = v->sel - vis + 1; if (top > n - vis) top = n - vis; if (top < 0) top = 0;
    float rx0 = fx + 16, rx1 = fx + fw - 16;
    for (int i = top; i < n && i < top + vis; i++) { const cs_row *r = &v->rows[i]; float y = y0 + (i - top) * rowH; int on = i == v->sel;
        if (on) { fit(ga_img_key(K_HILITE), rx0 - 8, y - rowH * 0.18f, rx1 - rx0 + 16, rowH * 1.36f, C_HI, 1); gfx_sprite *be = ga_img("character_hub_hilite_bookend"); fit(be, rx0 - 16, y + rowH / 2 - 11, 22, 22, RGB(255, 255, 255), 1); fit(be, rx1 - 6, y + rowH / 2 - 11, 22, 22, RGB(255, 255, 255), 1); }
        uint32_t c = r->off ? fb_alpha(C_SOFT, 0.7f) : on ? C_INKD : r->locked ? C_SOFT : C_CREAM; float size = v->hub ? 21 : 16, tagW = 0;
        if (r->chosen && !r->tag[0]) tagW = 0; else if (r->chosen) { star(rx1 - 20, y + rowH / 2 - 9, 18); tagW = 24; }
        else if (r->tag[0]) { float tsz = r->tag[0] == '$' ? 15 : 12; tagW = ga_text_w(ft, r->tag, tsz) + 6; ga_text_right(ft, rx1 - 2, y + (rowH - tsz) / 2 - 1, tsz, r->tag, on ? C_INKD : r->tag[0] == '$' ? C_PRICE : C_SOFT); }
        float lx = rx0 + 6, room = rx1 - lx - tagW - 6;
        if (v->hub) ga_text_fit(fh, lx, y + (rowH - size) / 2 - 2, size, room, r->label, c, 0);
        else if (r->sub[0] && room > 120) { float lw = fminf(ga_text_w(ft, r->label, size), room * 0.62f); ga_text_fit(ft, lx, y + 3, size, lw, say(ft, r->label), c, 0); ga_text_fit(ft, lx + lw + 8, y + 6, 12, room - lw - 8, say(ft, r->sub), on ? fb_alpha(C_INKD, 0.75f) : C_SOFT, 0); }
        else ga_text_fit(ft, lx, y + 3, size, room, say(ft, r->label), c, 0); }
    if (top > 0) ga_text_center(ft, fx + fw / 2, y0 - 14, 14, "▲", C_CREAM); if (top + vis < n) ga_text_center(ft, fx + fw / 2, y0 + vis * rowH - 4, 14, "▼", C_CREAM);
    if (v->hub && v->blurb) wrap(ft, rx0 + 4, y0 + n * rowH + 10, 13, rx1 - rx0 - 18, 15, fy + fh_ - cap * 0.75f, say(ft, v->blurb), C_SOFT);   /* the rocker's story under the hub's four rows */
    /* what A does here, under the frame */
    wrap(ft, fx + 6, fy + fh_ + 2, 14, fw - 12, 15, H - 30, v->info, C_CREAM);
    ui_hints(H - 24, W, v->bt, v->tx, v->nh);
    return 1;
}
