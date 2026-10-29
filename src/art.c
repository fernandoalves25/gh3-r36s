#include "art.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static int PX = 2;
static gfx_sprite *sprites[ART_N];
int art_px(void) { return PX; }

uint32_t art_color(char c) {
    switch (c) {
    case 'k': return RGB(0x0c, 0x0a, 0x10); case 'x': return RGB(0x1e, 0x1a, 0x26); case 'd': return RGB(0x4a, 0x4a, 0x52);
    case 'm': return RGB(0xa0, 0xa0, 0xa0); case 'l': return RGB(0xd8, 0xd8, 0xd8); case 'h': return RGB(0xff, 0xff, 0xff); case 'w': return RGB(0xf0, 0xea, 0xdf);
    case 'r': return RGB(0xea, 0x4b, 0x40); case 'R': return RGB(0xa3, 0x25, 0x1f); case 'o': return RGB(0xf2, 0x8d, 0x2d); case 'O': return RGB(0xb8, 0x5a, 0x12);
    case 'y': return RGB(0xf3, 0xd2, 0x3b); case 'Y': return RGB(0xc2, 0x9a, 0x14); case 'g': return RGB(0x31, 0xc4, 0x66); case 'G': return RGB(0x1c, 0x7a, 0x3e);
    case 'b': return RGB(0x45, 0x94, 0xf4); case 'B': return RGB(0x1f, 0x5a, 0xa8); case 'c': return RGB(0x62, 0xe4, 0xff); case 'C': return RGB(0x2a, 0x8f, 0xb0);
    case 'p': return RGB(0x8a, 0x5c, 0xc4); case 'P': return RGB(0x4a, 0x2f, 0x74); case 'a': return RGB(0xe9, 0xb0, 0x4c); case 'A': return RGB(0xa8, 0x74, 0x2a);
    case 'n': return RGB(0x6b, 0x4a, 0x2e); case 'N': return RGB(0x3e, 0x2a, 0x18); case 's': return RGB(0x6c, 0x68, 0x80); case 'S': return RGB(0x3a, 0x37, 0x48);
    case 't': return RGB(0x2a, 0x25, 0x38); case 'T': return RGB(0x16, 0x13, 0x1d); case 'e': return RGB(0xf0, 0xc8, 0xa0);
    default: return 0;
    }
}
gfx_sprite *art_bake_rows(const char *const *rows, int h, int up, int linear, const char *remap) {
    int w = (int)strlen(rows[0]); fb_t f; fb_init(&f, w, h);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) { char c = rows[y][x];
        if (remap) for (const char *m = remap; m[0] && m[1]; m += 2) if (m[0] == c) { c = m[1]; break; }
        f.px[(size_t)y * w + x] = art_color(c); }
    gfx_sprite *s = gfx_bake_up(&f, up, linear); fb_free(&f); return s;
}

/* ---- the hand-drawn sprites ---- */
static const char *GEM_BODY[12] = {
    "......kkkkkkkk......", "....kkllhhhhllkk....", "..kklhhhhhhhhhhlkk..", ".klhhhhhhhhhhhhhhlk.", "klhhhhhhhhhhhhhhhhlk", "kmllllllllllllllllmk",
    "kmmmmmmmmmmmmmmmmmmk", "kdmmmmmmmmmmmmmmmmdk", ".kddmmmmmmmmmmmmddk.", "..kkddddddddddddkk..", "....kkddddddddkk....", "......kkkkkkkk......" };
static const char *GEM_PLAIN[12] = {
    "....................", "....................", "......xxxxxxxx......", "....xxxxxxxxxxxx....", "...xxxxxhhhhxxxxx...", "...xxxxhhhhhhxxxx...",
    "...xxxxhhhhhhxxxx...", "...xxxxxhhhhxxxxx...", "....xxxxxxxxxxxx....", "......xxxxxxxx......", "....................", "...................." };
static const char *GEM_HOPO[12] = {
    "....................", "....................", "......hhhhhhhh......", "....hhhhhhhhhhhh....", "...hhhhhhhhhhhhhh...", "...hhhhhhhhhhhhhh...",
    "...hhhhhhhhhhhhhh...", "...hhhhhhhhhhhhhh...", "....hhhhhhhhhhhh....", "......hhhhhhhh......", "....................", "...................." };
static const char *GEM_STAR[12] = {
    "....................", ".........hh.........", "........hhhh........", "........hhhh........", "...hhhhhhhhhhhhhh...", "....hhhhhhhhhhhh....",
    ".....hhhhhhhhhh.....", ".....hhhhhhhhhh.....", "....hhhhh..hhhhh....", "....hhh......hhh....", "....................", "...................." };
static const char *TGT_UP[13] = {
    ".......kkkkkkkk.......", "....kkklllllllkkkk....", "..kkllmmmmmmmmmmlkkk..", ".klmmmkkkkkkkkkkmmmmk.", "kmmmmkddddddddddkmmmmk", "kmmmkddddddddddddkmmmk",
    "kmmmkddddddddddddkmmmk", "kmmmmkddddddddddkmmmmk", ".kmmmmkkkkkkkkkkmmmmk.", "..kkmmmmmmmmmmmmmmkk..", "....kkkddddddddkkk....", "......kkkkkkkkkk......", "......................" };
static const char *TGT_DOWN[13] = {
    "......................", ".......kkkkkkkk.......", "....kkklllllllkkkk....", "..kklllllllllllllkkk..", ".kllllkkkkkkkkkkllllk.", "kllllkhhhhhhhhhhkllllk",
    "klllkhhhhhhhhhhhhklllk", "kllllkhhhhhhhhhhkllllk", ".kllllkkkkkkkkkkllllk.", "..kkllllllllllllllkk..", "....kkkddddddddkkk....", "......kkkkkkkkkk......", "......................" };
static const char *FLAME0[16] = {
    "..............", "..............", "..............", "..............", "..............", "......oo......", ".....oyyo.....", "....oyywyo....",
    "....oywwyo....", "....oyywyo....", ".....oyyo.....", "......oo......", "..............", "..............", "..............", ".............." };
static const char *FLAME1[16] = {
    "..............", "......r.......", ".....r.r......", "....ror.r.....", "....oro.r.....", "...royoor.....", "...oyyyoor....", "..royywyyor...",
    "..oyywwwyyo...", "..oyywwwyyo...", "..royywwyyor..", "...oyyyyyyo...", "...royyyyor...", "....rooor.....", ".....rrr......", ".............." };
static const char *FLAME2[16] = {
    "....r.........", "..........r...", "..r....o......", ".......r......", "....o.........", "...r......o...", "......y.......", ".........r....",
    "..o.......o...", "......o.......", "....r.....r...", ".......o......", "..............", "..............", "..............", ".............." };
static const char *STAR[11] = {
    ".....k.....", "....kyk....", "....kyk....", "...kyhyk...", "kkkkyyykkkk", ".kyhyyyyYk.", "..kyyyyYk..", "..kyyyyYk..", ".kyyyYYYYk.", ".kyykkkYYk.", ".kk.....kk." };
static const char *PICK[11] = {
    ".kkkkkkk.", "kaahhaaak", "kaahaaaAk", "kaaaaaaAk", "kaaaaaAAk", ".kaaaaAk.", ".kaaaAAk.", "..kaaAk..", "..kaAAk..", "...kAk...", "....k...." };
static const char *BOLT[14] = {
    "....kkkk.", "...kccck.", "...kcck..", "..kccck..", "..kcck...", ".kcccckkk", ".kcccccck", ".kkkkccck", "....kcck.", "....kck..", "...kck...", "...kck...", "..kk.....", "..k......" };
static const char *CROWN[9] = {
    ".k...k...k.", ".k...k...k.", ".kk.kyk.kk.", ".kykkyykyk.", ".kyyyyyyyk.", ".kyyyyyyyk.", ".kYYYYYYYk.", ".kkkkkkkkk.", "..........." };
static const char *LAMP[8] = {
    "....kkkk....", "...kssssk...", "..kSssssSk..", "..kSssssSk..", ".kkkkkkkkkk.", ".kSSSSSSSSk.", ".kSSSSSSSSk.", ".kkkkkkkkkk." };
static const char *SKULL[11] = {
    "...kkkkk...", "..klllllk..", ".kllllllllk", ".klkkllkklk", ".klkkllkklk", ".kllllllllk", ".klllkllllk", "..kllllllk.", "..kkkkkkkk.", "...klklk...", "...kkkkk..." };
static const char *NOTE[11] = {
    "......kkk..", "......kwwk.", "......kwkwk", "......kwk.k", "......kwk..", "......kwk..", "...kkkkwk..", "..kwwwwwk..", "..kwwwwwk..", "...kwwwk...", "....kkk...." };

/* ---- generated sprites (drawn at art resolution into an fb, then enlarged) ---- */
static unsigned rng = 12345u; static unsigned rnd(void) { rng = rng * 1664525u + 1013904223u; return rng >> 8; }
static void px(fb_t *f, int x, int y, uint32_t c) { if (x >= 0 && y >= 0 && x < f->w && y < f->h) f->px[(size_t)y * f->w + x] = c; }
static void box(fb_t *f, int x, int y, int w, int h, uint32_t c) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) px(f, x + i, y + j, c); }
static void disc(fb_t *f, int cx, int cy, int r, uint32_t c) { for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) if (x * x + y * y <= r * r + r / 2) px(f, cx + x, cy + y, c); }

static gfx_sprite *gen_crowd(int frame, unsigned seed) { /* a strip of silhouettes with raised arms, 128 x 28 art px, tileable */
    int Wd = 128, Hd = 28; fb_t f; fb_init(&f, Wd, Hd); rng = seed;
    uint32_t K = art_color('k'), X = art_color('x'), rim = fb_alpha(art_color('p'), 0.9f);
    for (int layer = 0; layer < 2; layer++) {
        for (int x = (int)(rnd() % 6); x < Wd; x += 6 + (int)(rnd() % 5)) {
            int r = 2 + (int)(rnd() % 2), bob = ((rnd() + frame) % 2 == 0) ? 0 : 1; int base = layer ? Hd : Hd - 6 - (int)(rnd() % 3);
            int hy = base - 12 - r + bob - (layer ? 0 : 2); uint32_t body = layer ? K : X;
            box(&f, x - r - 1, hy + r, 2 * r + 3, base - hy - r, body);        /* body */
            disc(&f, x, hy, r, body);                                           /* head */
            if (!layer) px(&f, x - 1, hy - r, rim), px(&f, x, hy - r, rim);      /* a rim light on the front row */
            int arms = (int)(rnd() % 3);                                        /* 0 none, 1 one arm, 2 both */
            if (arms) { int ay = hy - 2 - (frame ? 2 : 0); for (int k = 0; k < 6; k++) px(&f, x + r + 1 + k / 2, hy + 2 - k - (frame ? 1 : 0), body); px(&f, x + r + 4, ay, body); px(&f, x + r + 5, ay - 1, body); }
            if (arms == 2) { for (int k = 0; k < 6; k++) px(&f, x - r - 1 - k / 2, hy + 2 - k + (frame ? 1 : 0), body); }
        }
    }
    gfx_sprite *s = gfx_bake_up(&f, PX, 0); fb_free(&f); return s;
}
static gfx_sprite *gen_speaker(void) { /* a cabinet 24 x 40 art px */
    int Wd = 24, Hd = 40; fb_t f; fb_init(&f, Wd, Hd);
    box(&f, 0, 0, Wd, Hd, art_color('k')); box(&f, 1, 1, Wd - 2, Hd - 2, art_color('S')); box(&f, 2, 2, Wd - 4, Hd - 4, art_color('x'));
    box(&f, 1, 1, Wd - 2, 1, art_color('s')); box(&f, 1, 1, 1, Hd - 2, art_color('s'));
    for (int y = 4; y < Hd - 4; y++) for (int x = 4; x < Wd - 4; x++) if ((x + y) & 1) px(&f, x, y, art_color('T'));
    for (int c = 0; c < 2; c++) { int cy = 11 + c * 18; disc(&f, 12, cy, 7, art_color('d')); disc(&f, 12, cy, 6, art_color('S')); disc(&f, 12, cy, 4, art_color('x')); disc(&f, 12, cy, 2, art_color('s')); px(&f, 10, cy - 3, art_color('m')); px(&f, 9, cy - 2, art_color('m')); }
    box(&f, 9, Hd - 4, 6, 2, art_color('a')); px(&f, 9, Hd - 4, art_color('A'));
    gfx_sprite *s = gfx_bake_up(&f, PX, 0); fb_free(&f); return s;
}
static gfx_sprite *gen_truss(void) { /* 16 x 10 tile */
    fb_t f; fb_init(&f, 16, 10); uint32_t S = art_color('S'), s2 = art_color('s'), K = art_color('k');
    box(&f, 0, 0, 16, 2, S); box(&f, 0, 8, 16, 2, S); box(&f, 0, 0, 16, 1, s2); box(&f, 0, 9, 16, 1, K);
    for (int i = 0; i < 8; i++) { px(&f, i, 2 + i * 6 / 8, S); px(&f, 15 - i, 2 + i * 6 / 8, S); px(&f, 7 + i, 2 + i * 6 / 8, s2); px(&f, 8 - i, 2 + i * 6 / 8, s2); }
    gfx_sprite *sp = gfx_bake_up(&f, PX, 0); fb_free(&f); return sp;
}
static gfx_sprite *gen_beam(void) { /* a soft cone, narrow at the top: 64 x 160, white, alpha falling to the sides and downward */
    int Wd = 64, Hd = 160; fb_t f; fb_init(&f, Wd, Hd);
    for (int y = 0; y < Hd; y++) { float t = (float)y / Hd, half = 3 + t * 28; for (int x = 0; x < Wd; x++) { float d = fabsf(x + 0.5f - Wd / 2.0f) / half; if (d >= 1) continue; float a = (1 - d * d) * (1 - t * 0.85f) * 0.9f; f.px[(size_t)y * Wd + x] = RGBA(255, 255, 255, (int)(a * 255)); } }
    gfx_sprite *s = gfx_bake(&f, 1); fb_free(&f); return s;
}
static gfx_sprite *gen_glow(void) { /* radial, 64 x 64 */
    fb_t f; fb_init(&f, 64, 64); for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) { float dx = (x + 0.5f - 32) / 32, dy = (y + 0.5f - 32) / 32, d = sqrtf(dx * dx + dy * dy); if (d >= 1) continue; float a = (1 - d) * (1 - d); f.px[(size_t)y * 64 + x] = RGBA(255, 255, 255, (int)(a * 255)); }
    gfx_sprite *s = gfx_bake(&f, 1); fb_free(&f); return s;
}
static gfx_sprite *gen_meter(void) { /* the rock meter: a half-dial 56 x 30 art px with red / yellow / green zones and tick marks; the needle is drawn live */
    int Wd = 56, Hd = 30; fb_t f; fb_init(&f, Wd, Hd); float cx = 28, cy = 28;
    for (int y = 0; y < Hd; y++) for (int x = 0; x < Wd; x++) { float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = sqrtf(dx * dx + dy * dy); if (dy > 0.5f) continue;
        float ang = atan2f(-dy, dx); /* pi at left .. 0 at right */ float k = 1 - ang / (float)M_PI; /* 0 red .. 1 green */
        uint32_t c = 0;
        if (d > 27) c = 0; else if (d > 26) c = art_color('k'); else if (d > 19) { c = k < 0.34f ? art_color('r') : k < 0.6f ? art_color('y') : art_color('g'); if (d > 25) c = k < 0.34f ? art_color('R') : k < 0.6f ? art_color('Y') : art_color('G'); }
        else if (d > 18) c = art_color('k'); else if (d > 7) c = fb_alpha(art_color('T'), 0.85f);
        if (c) f.px[(size_t)y * Wd + x] = c; }
    for (int i = 0; i <= 6; i++) { float a = (float)M_PI * (1 - i / 6.0f); for (int r = 20; r < 26; r++) px(&f, (int)(cx + cosf(a) * r), (int)(cy - sinf(a) * r), art_color('k')); }
    disc(&f, 28, 28, 3, art_color('k')); disc(&f, 28, 28, 2, art_color('s'));
    gfx_sprite *s = gfx_bake_up(&f, PX, 0); fb_free(&f); return s;
}
void art_highway_tile(fb_t *f) { /* 32 x 32 art px: dark boards with seams and a little grain */
    rng = 777u; uint32_t base = RGB(0x24, 0x20, 0x30), seam = RGB(0x17, 0x14, 0x1f), light = RGB(0x2c, 0x27, 0x3a);
    for (int y = 0; y < f->h; y++) for (int x = 0; x < f->w; x++) { uint32_t c = base; if ((y % 16) == 0) c = seam; else if ((y % 16) == 1) c = light; else if (((x + (y / 16) * 11) % 32) == 0) c = seam; else if (rnd() % 23 == 0) c = light; f->px[(size_t)y * f->w + x] = c; }
}

void art_init(int p) {
    PX = p < 1 ? 1 : p; memset(sprites, 0, sizeof sprites);
    sprites[ART_GEM_BODY] = art_bake_rows(GEM_BODY, 12, PX, 1, NULL); sprites[ART_GEM_PLAIN] = art_bake_rows(GEM_PLAIN, 12, PX, 1, NULL);
    sprites[ART_GEM_HOPO] = art_bake_rows(GEM_HOPO, 12, PX, 1, NULL); sprites[ART_GEM_STAR] = art_bake_rows(GEM_STAR, 12, PX, 1, NULL);
    sprites[ART_TGT_UP] = art_bake_rows(TGT_UP, 13, PX, 1, NULL); sprites[ART_TGT_DOWN] = art_bake_rows(TGT_DOWN, 13, PX, 1, NULL);
    sprites[ART_FLAME0] = art_bake_rows(FLAME0, 16, PX, 1, NULL); sprites[ART_FLAME1] = art_bake_rows(FLAME1, 16, PX, 1, NULL); sprites[ART_FLAME2] = art_bake_rows(FLAME2, 16, PX, 1, NULL);
    const char *spmap = "wh" "yc" "oC" "rB";
    sprites[ART_SPFLAME0] = art_bake_rows(FLAME0, 16, PX, 1, spmap); sprites[ART_SPFLAME1] = art_bake_rows(FLAME1, 16, PX, 1, spmap); sprites[ART_SPFLAME2] = art_bake_rows(FLAME2, 16, PX, 1, spmap);
    sprites[ART_STAR] = art_bake_rows(STAR, 11, PX, 0, NULL); sprites[ART_STAR_EMPTY] = art_bake_rows(STAR, 11, PX, 0, "yS" "hs" "YT");
    sprites[ART_PICK] = art_bake_rows(PICK, 11, PX, 0, NULL); sprites[ART_BOLT] = art_bake_rows(BOLT, 14, PX, 0, NULL); sprites[ART_CROWN] = art_bake_rows(CROWN, 9, PX, 0, NULL);
    sprites[ART_LAMP] = art_bake_rows(LAMP, 8, PX, 0, NULL); sprites[ART_SKULL] = art_bake_rows(SKULL, 11, PX, 0, NULL); sprites[ART_NOTE] = art_bake_rows(NOTE, 11, PX, 0, NULL);
    sprites[ART_SPEAKER] = gen_speaker(); sprites[ART_TRUSS] = gen_truss(); sprites[ART_BEAM] = gen_beam(); sprites[ART_GLOW] = gen_glow(); sprites[ART_METER] = gen_meter();
    for (int i = 0; i < 4; i++) sprites[ART_CROWD0 + i] = gen_crowd(i & 1, 1000u + (i >> 1) * 77u);
}
gfx_sprite *art_get(int id) { return id >= 0 && id < ART_N ? sprites[id] : NULL; }
