/* The Original look's text: every gfx_text* call (gfx.c) is drawn with the game's own fonts instead of the 5 x 11 pixel
 * font, on the same cell (y is the top of the 11-row cell, the baseline sits on row 9) and hardly wider than the pixel
 * text it stands for (see room(); a longer line is squeezed), so every layout made for the pixel font still fits. Scale 1 is the
 * game's body font (fontgrid_text_a4, the browser port's F.text), 2 and up its menu font (fontgrid_text_a6, F.head), both
 * read from sheets shrunk near the size drawn so small text stays smooth. Accents the fonts lack are put together by
 * gamearts.c (ga_char); the symbols they lack (arrows, stars, the note, bullets, ×) come from the pixel font in line. */
#include "gfx.h"
#include "gamearts.h"
#include "settings.h"
#include "original.h"
#include <math.h>
#include <string.h>

static const char *F_SMALL = "fontgrid_text_a4", *F_BIG = "fontgrid_text_a6";
typedef struct { const ga_font *f; float size, x, base, sx, tail; uint32_t c; int scale; } pen_t;   /* tail: the last glyph's blank after its ink */

static int utf8(const char **s) { const unsigned char *p = (const unsigned char *)*s; int c = *p;
    if (c < 0x80) { *s += 1; return c; } if ((c & 0xE0) == 0xC0 && p[1]) { *s += 2; return ((c & 0x1F) << 6) | (p[1] & 0x3F); }
    if ((c & 0xF0) == 0xE0 && p[1] && p[2]) { *s += 3; return ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); }
    if ((c & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) { *s += 4; return 0xFFFD; } *s += 1; return '?'; }
/* the font for a scale at the size whose capitals match the pixel letters' (7 px a scale; 9.5 at scale 1, the body font's
 * small letters need it), read from the sheet shrunk to just under that size */
static int pen_open(pen_t *p, int scale) {
    if (scale < 1) scale = 1; const char *name = scale == 1 ? F_SMALL : F_BIG; float cap = scale == 1 ? 9.5f : 7.0f * scale;
    const ga_font *f = ga_font_get(name); int sx, sy, sw, sh; float w, h, yo;
    if (!f || ga_glyph(f, 'H', &sx, &sy, &sw, &sh, &w, &h, &yo) || h <= 0) return -1;
    float size = ga_font_size(f) * cap / h; int down = (int)ceilf(ga_font_size(f) / size - 0.05f); if (down > 1 && !(f = ga_font_down(name, down))) return -1;
    memset(p, 0, sizeof *p); p->f = f; p->size = size; p->scale = scale; p->sx = 1; return 0;
}
static int on(void) { return SET.theme == 2 && orig_ready() && ga_font_get(F_SMALL) && ga_font_get(F_BIG); }

static float ch(pen_t *p, int c) { return ga_char(p->f, p->x, p->base, p->size, p->sx, c, p->c); }
static float glyph(pen_t *p, int c) {
    float a, x0 = p->x;
    if (c == 0xA0) c = ' '; if (c == 0xD7 && !ga_has(p->f, c)) c = 'x';   /* the multiplier's × as the font's x */
    if (c == '"' && ga_has(p->f, '\'') == 1) { a = ch(p, '\''); p->x += a * 0.45f; ch(p, '\''); p->x = x0; return a * 1.45f; }   /* two apostrophes */
    if (c == ' ' || ga_has(p->f, c)) return ch(p, c);
    if (c == 0x2026 && ga_has(p->f, '.') == 1) { for (int i = 0; i < 3; i++) p->x += ch(p, '.'); a = p->x - x0; p->x = x0; return a; }   /* … as ... */
    if (c == 0x2013 || c == 0x2014) return ch(p, '-');
    float s = (float)p->scale, w = FONT_W * s * p->sx;   /* a symbol from the pixel font, its cell on the same baseline */
    if (font_index((uint32_t)c) >= 0) { if (p->c >> 24) gfx_pix_glyph(p->x, p->base - 9 * s, w, FONT_H * s, (uint32_t)c, p->c); p->tail = (FONT_ADV - FONT_W) * s * p->sx; return FONT_ADV * s * p->sx; }
    int sx, sy, sw, sh; float gw, gh, yo; return ch(p, ga_glyph(p->f, c, &sx, &sy, &sw, &sh, &gw, &gh, &yo) ? '?' : c);   /* the font's own folding (case, plain letter) */
}
static float run(pen_t *p, const char *s) { float x0 = p->x; while (*s) { p->tail = 0; p->x += glyph(p, utf8(&s)); } return p->x - x0 - p->tail; }

/* ---- the face ---- */
static float measure(pen_t *p, const char *s) { p->c = 0; p->x = 0; return run(p, s); }
/* the room a text has: the pixel text's width, a little more for a short one (a fifth, 2 px a scale at most), since the
 * game's letters and digits are wider than the pixel ones; past it the line is squeezed */
static float room(const char *s, int scale) { float pw = (float)gfx_pix_w(s, scale), t = pw * 0.2f; return pw + (t < 2 * scale ? t : 2 * scale); }
static int face_w(const char *s, int scale) {
    pen_t p; if (!s || !*s || pen_open(&p, scale)) return gfx_pix_w(s, scale);
    float w = measure(&p, s), r = room(s, scale); return (int)ceilf(w < r ? w : r);
}
static void face_draw(int x, int y, int scale, const char *s, uint32_t c) {
    pen_t p; if (!s || !*s || !(c >> 24)) return; if (pen_open(&p, scale)) { gfx_pix_text(x, y, scale, s, c); return; }
    float w = measure(&p, s), r = room(s, scale);
    p.x = (float)x; p.base = y + 9.0f * p.scale; p.c = c; if (w > r && r > 0) p.sx = r / w; run(&p, s);
}
static const gfx_face FACE = { on, face_w, face_draw };
__attribute__((constructor)) static void face_install(void) { gfx_text_face(&FACE); }
