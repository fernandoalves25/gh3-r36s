/* The game's 2D art: pak index, pab slices, DDS decoding, texture banks, bitmap fonts. Ported from the browser port's
 * readers (pakIndex, decodeDDS, texBank, materialTexture, parseFont). Nothing is decoded until something asks for it. */
#include "gamearts.h"
#include "gh3fmt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct { uint32_t type, fullKey, nameKey, len; long off; } ent_t;
static ent_t *ents; static int nents; static FILE *pab; static uint32_t K_IMG, K_TEX, K_FNT, K_SCN; static int timing; static size_t memUsed;
static uint32_t be32(const uint8_t *b) { return (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3]; }
static uint16_t be16(const uint8_t *b) { return (uint16_t)(b[0] << 8 | b[1]); }
static float bef32(const uint8_t *b) { uint32_t u = be32(b); float f; memcpy(&f, &u, 4); return f; }
static double now_ms(void) { return (double)SDL_GetPerformanceCounter() * 1000.0 / (double)SDL_GetPerformanceFrequency(); }

int ga_ok(void) { return pab != NULL && nents > 0; }
int ga_open(const char *pakPath, const char *pabPath) {
    FILE *f = fopen(pakPath, "rb"); if (!f) return -1; fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(n > 0 ? (size_t)n : 1); size_t got = n > 0 ? fread(b, 1, (size_t)n, f) : 0; fclose(f); if ((long)got != n) { free(b); return -1; }
    pab = fopen(pabPath, "rb"); if (!pab) { free(b); return -1; }
    K_IMG = gh3_qbkey(".img"); K_TEX = gh3_qbkey(".tex"); K_FNT = gh3_qbkey(".fnt"); K_SCN = gh3_qbkey(".scn");
    uint32_t K_LAST = gh3_qbkey(".last"), K_LAST2 = gh3_qbkey("last"); int cap = 256; ents = malloc(cap * sizeof *ents); nents = 0;
    for (size_t p = 0; p + 32 <= got; ) { uint32_t type = be32(b + p); if (type == K_LAST || type == K_LAST2 || type == 0) break;
        ent_t e = { type, be32(b + p + 16), be32(b + p + 20), be32(b + p + 8), (long)p + (long)be32(b + p + 4) - (long)got }; uint32_t flags = be32(b + p + 28);
        if (nents == cap) { cap *= 2; ents = realloc(ents, cap * sizeof *ents); } ents[nents++] = e; p += 32 + ((flags & 0x20) ? 160 : 0); }
    free(b); timing = getenv("GH3_ARTS_TIME") != NULL; return nents ? 0 : -1;
}
static const ent_t *find_ent(uint32_t type, uint32_t key) {
    for (int i = 0; i < nents; i++) if (ents[i].type == type && ents[i].nameKey == key) return &ents[i];
    for (int i = 0; i < nents; i++) if (ents[i].type == type && ents[i].fullKey == key) return &ents[i];   /* a few are named by their whole name only */
    return NULL;
}
static uint8_t *slice(const ent_t *e, size_t *n) { /* one entry's bytes from the pab */
    if (!e || e->off < 0 || fseek(pab, e->off, SEEK_SET)) { *n = 0; return NULL; }
    uint8_t *b = malloc(e->len ? e->len : 1); *n = fread(b, 1, e->len, pab); if (*n != e->len) { free(b); *n = 0; return NULL; } return b;
}

/* ---- DDS (DXT1 / DXT3 / DXT5 / 32-bit) -> ARGB, top mip only ---- */
int ga_decode_dds(const uint8_t *b, size_t n, fb_t *out) {
    if (n < 128 || b[0] != 'D' || b[1] != 'D' || b[2] != 'S') return -1;
    uint32_t h = b[12] | b[13] << 8 | b[14] << 16 | (uint32_t)b[15] << 24, w = b[16] | b[17] << 8 | b[18] << 16 | (uint32_t)b[19] << 24; if (!w || !h || w > 4096 || h > 4096) return -1;
    char cc[5] = { (char)b[84], (char)b[85], (char)b[86], (char)b[87], 0 }; if (!strcmp(cc, "DXT2")) strcpy(cc, "DXT3"); else if (!strcmp(cc, "DXT4")) strcpy(cc, "DXT5");
    int fmt = !strcmp(cc, "DXT1") ? 1 : !strcmp(cc, "DXT3") ? 3 : !strcmp(cc, "DXT5") ? 5 : 0; size_t p = 128;
    fb_init(out, (int)w, (int)h);
    if (!fmt) { uint32_t bpp = b[88] | b[89] << 8 | b[90] << 16 | (uint32_t)b[91] << 24; if (bpp != 32 || p + (size_t)w * h * 4 > n) { fb_free(out); return -1; }
        for (size_t i = 0; i < (size_t)w * h; i++) out->px[i] = RGBA(b[p + i * 4 + 2], b[p + i * 4 + 1], b[p + i * 4], b[p + i * 4 + 3]); return 0; }
    uint8_t col[16], al[8];
    for (uint32_t by = 0; by < h; by += 4) for (uint32_t bx = 0; bx < w; bx += 4) {
        size_t aoff = 0; if (fmt != 1) { aoff = p; p += 8; } if (p + 8 > n) return 0;
        uint32_t c0 = b[p] | b[p + 1] << 8, c1 = b[p + 2] | b[p + 3] << 8, bits = b[p + 4] | b[p + 5] << 8 | b[p + 6] << 16 | (uint32_t)b[p + 7] << 24; p += 8;
        col[0] = (c0 >> 11) * 255 / 31; col[1] = ((c0 >> 5) & 63) * 255 / 63; col[2] = (c0 & 31) * 255 / 31; col[3] = 255;
        col[4] = (c1 >> 11) * 255 / 31; col[5] = ((c1 >> 5) & 63) * 255 / 63; col[6] = (c1 & 31) * 255 / 31; col[7] = 255;
        if (c0 > c1 || fmt != 1) { for (int k = 0; k < 3; k++) { col[8 + k] = (2 * col[k] + col[4 + k]) / 3; col[12 + k] = (col[k] + 2 * col[4 + k]) / 3; } col[11] = 255; col[15] = 255; }
        else { for (int k = 0; k < 3; k++) { col[8 + k] = (col[k] + col[4 + k]) / 2; col[12 + k] = 0; } col[11] = 255; col[15] = 0; }
        uint32_t abits0 = 0, abits1 = 0;
        if (fmt == 5) { int a0 = b[aoff], a1 = b[aoff + 1]; al[0] = a0; al[1] = a1;
            if (a0 > a1) for (int k = 1; k < 7; k++) al[k + 1] = ((7 - k) * a0 + k * a1) / 7; else { for (int k = 1; k < 5; k++) al[k + 1] = ((5 - k) * a0 + k * a1) / 5; al[6] = 0; al[7] = 255; }
            abits0 = b[aoff + 2] | b[aoff + 3] << 8 | b[aoff + 4] << 16; abits1 = b[aoff + 5] | b[aoff + 6] << 8 | b[aoff + 7] << 16; }
        for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) { uint32_t x = bx + i, y = by + j; if (x >= w || y >= h) continue;
            int q = j * 4 + i, ci = (bits >> (q * 2)) & 3; uint8_t a = fmt == 1 ? col[ci * 4 + 3] : fmt == 3 ? ((b[aoff + (q >> 1)] >> ((q & 1) * 4)) & 15) * 17 : al[(q < 8 ? abits0 >> (q * 3) : abits1 >> ((q - 8) * 3)) & 7];
            out->px[(size_t)y * w + x] = RGBA(col[ci * 4], col[ci * 4 + 1], col[ci * 4 + 2], a); } }
    return 0;
}

/* ---- the cache: sprites by (type, key) ---- */
typedef struct { uint32_t type, key; gfx_sprite *s; int missing; } cache_t;
static cache_t *cache; static int ncache, cachecap;
static cache_t *cache_find(uint32_t type, uint32_t key) { for (int i = 0; i < ncache; i++) if (cache[i].type == type && cache[i].key == key) return &cache[i]; return NULL; }
static cache_t *cache_add(uint32_t type, uint32_t key, gfx_sprite *s) {
    if (ncache == cachecap) { cachecap = cachecap ? cachecap * 2 : 64; cache = realloc(cache, cachecap * sizeof *cache); }
    cache_t *c = &cache[ncache++]; c->type = type; c->key = key; c->s = s; c->missing = s == NULL; return c;
}
static gfx_sprite *bake_fb(fb_t *f) { memUsed += (size_t)f->w * f->h * 4; if (timing) fprintf(stderr, "[arts] baked: %zu KB held\n", memUsed / 1024); gfx_sprite *s = gfx_bake(f, 1); fb_free(f); return s; }
size_t ga_memory(void) { return memUsed; }

/* ---- .img ---- */
int ga_img_fb(const char *name, fb_t *out) {
    if (!ga_ok()) return -1; const ent_t *e = find_ent(K_IMG, gh3_qbkey(name)); size_t n; uint8_t *b = slice(e, &n); if (!b) return -1;
    double t0 = timing ? now_ms() : 0; size_t o = n > 0x20 ? be32(b + 0x1c) : 0; if (!o) o = 0x28; int r = o < n ? ga_decode_dds(b + o, n - o, out) : -1; free(b);
    if (timing) fprintf(stderr, "[arts] img %-36s %4dx%-4d %6.1f ms\n", name, r ? 0 : out->w, r ? 0 : out->h, now_ms() - t0);
    return r;
}
gfx_sprite *ga_img(const char *name) {
    if (!ga_ok()) return NULL; uint32_t k = gh3_qbkey(name); cache_t *c = cache_find(K_IMG, k); if (c) return c->s;
    fb_t f; if (ga_img_fb(name, &f)) { cache_add(K_IMG, k, NULL); return NULL; } return cache_add(K_IMG, k, bake_fb(&f))->s;
}

/* ---- .tex bank + .scn materials ---- */
typedef struct { uint32_t key; uint16_t w, h; uint32_t off, size; } tex_t;
static uint8_t *bank; static size_t bankLen; static tex_t *texes; static int ntexes; static uint8_t **scns; static size_t *scnLens; static int nscns; static int bankTried;
static int tex_count(const uint8_t *b, size_t n) { return n >= 8 && b[0] == 0xFA && b[1] == 0xCE ? (b[6] << 8 | b[7]) : 0; }
static void load_bank(void) { /* the richest .tex of the pak (the HUD's: gems, buttons, flames) and every .scn, which name its textures */
    if (bankTried || !ga_ok()) return; bankTried = 1; const ent_t *best = NULL; int bestN = 0; double t0 = timing ? now_ms() : 0;
    for (int i = 0; i < nents; i++) if (ents[i].type == K_TEX) { uint8_t head[16]; if (fseek(pab, ents[i].off, SEEK_SET) || fread(head, 1, 16, pab) != 16) continue; int c = tex_count(head, 16); if (c > bestN) { bestN = c; best = &ents[i]; } }
    if (!best || bestN < 20) return;
    bank = slice(best, &bankLen); if (!bank) return; int n = tex_count(bank, bankLen); uint32_t meta = be32(bank + 8); texes = malloc(n * sizeof *texes); ntexes = 0;
    for (int i = 0; i < n && (size_t)meta + i * 40 + 40 <= bankLen; i++) { const uint8_t *p = bank + meta + i * 40; tex_t t = { be32(p + 4), be16(p + 8), be16(p + 10), be32(p + 28), be32(p + 32) }; if ((size_t)t.off + t.size <= bankLen) texes[ntexes++] = t; }
    for (int i = 0; i < nents; i++) if (ents[i].type == K_SCN) { size_t L; uint8_t *b = slice(&ents[i], &L); if (!b) continue; scns = realloc(scns, (nscns + 1) * sizeof *scns); scnLens = realloc(scnLens, (nscns + 1) * sizeof *scnLens); scns[nscns] = b; scnLens[nscns] = L; nscns++; }
    if (timing) fprintf(stderr, "[arts] bank %d textures, %zu KB, %d scenes: %.1f ms\n", ntexes, bankLen / 1024, nscns, now_ms() - t0);
}
static const tex_t *tex_find(uint32_t key) { for (int i = 0; i < ntexes; i++) if (texes[i].key == key) return &texes[i]; return NULL; }
int ga_tex_fb(uint32_t key, fb_t *out) {
    load_bank(); const tex_t *t = tex_find(key); if (!t) return -1; double t0 = timing ? now_ms() : 0; int r = ga_decode_dds(bank + t->off, t->size, out);
    if (timing) fprintf(stderr, "[arts] tex %08x %4dx%-4d %6.1f ms\n", key, t->w, t->h, now_ms() - t0); return r;
}
int ga_tex_biggest(int w, int h, uint32_t *key) { load_bank(); const tex_t *big = NULL; for (int i = 0; i < ntexes; i++) if (texes[i].w == w && texes[i].h == h && (!big || texes[i].size > big->size)) big = &texes[i]; if (!big) return -1; *key = big->key; return 0; }
static int mat_key(const char *material, uint32_t *key) { /* the scene files keep each material's texture key a little after its name key */
    load_bank(); char full[200]; snprintf(full, sizeof full, "%s_%s", material, material); uint32_t k = gh3_qbkey(full);
    for (int s = 0; s < nscns; s++) { const uint8_t *b = scns[s]; size_t n = scnLens[s];
        for (size_t p = 0; p + 4 <= n; p += 4) { if (be32(b + p) != k) continue;
            for (size_t q = p + 0x100; q + 4 <= n && q <= p + 0x400; q += 4) { uint32_t t = be32(b + q); if (tex_find(t)) { *key = t; return 0; } } } }
    return -1;
}
int ga_mat_fb(const char *material, fb_t *out) { uint32_t k; if (mat_key(material, &k)) return -1; return ga_tex_fb(k, out); }
gfx_sprite *ga_mat(const char *material) {
    if (!ga_ok()) return NULL; uint32_t k = gh3_qbkey(material); cache_t *c = cache_find(K_SCN, k); if (c) return c->s;
    fb_t f; if (ga_mat_fb(material, &f)) { cache_add(K_SCN, k, NULL); return NULL; } return cache_add(K_SCN, k, bake_fb(&f))->s;
}

/* ---- .fnt: a character table (0x14), glyph records (0x200a4, 36 bytes each: u0 v0 u1 v1 w h yo ...), then the sheet ---- */
typedef struct { int sx, sy, sw, sh; float w, h, yo; } glyph_t;
struct ga_font { uint32_t key; int down; gfx_sprite *sheet; int sheetW, sheetH; int16_t table[0x250]; glyph_t *g; int n; float size, asc, space, gap, stem, acute[2][3]; };   /* acute: lower/upper case, where an acute's ink sits (gap above the letter, height, x shift) */
static ga_font **fonts; static int nfonts;
/* the sheet shrunk `d` times by averaging d x d blocks (alpha-weighted): small text from a big font stays smooth */
static void shrink(fb_t *f, int d) {
    fb_t o; fb_init(&o, (f->w + d - 1) / d, (f->h + d - 1) / d);
    for (int y = 0; y < o.h; y++) for (int x = 0; x < o.w; x++) { uint32_t a = 0, r = 0, g = 0, b = 0, n = 0;
        for (int j = 0; j < d; j++) for (int i = 0; i < d; i++) { int sx = x * d + i, sy = y * d + j; n++; if (sx >= f->w || sy >= f->h) continue; uint32_t c = f->px[(size_t)sy * f->w + sx], ca = c >> 24;
            a += ca; r += ((c >> 16) & 255) * ca; g += ((c >> 8) & 255) * ca; b += (c & 255) * ca; }
        o.px[(size_t)y * o.w + x] = a ? RGBA(r / a, g / a, b / a, a / n) : 0; }
    fb_free(f); *f = o;
}
static ga_font *font_load(const char *name, int down);
static void acute_ink(ga_font *f, const fb_t *sheet);
ga_font *ga_font_get(const char *name) { return font_load(name, 1); }
ga_font *ga_font_down(const char *name, int down) { return font_load(name, down < 1 ? 1 : down > 8 ? 8 : down); }
static ga_font *font_load(const char *name, int down) {
    if (!ga_ok()) return NULL; uint32_t k = gh3_qbkey(name); for (int i = 0; i < nfonts; i++) if (fonts[i]->key == k && fonts[i]->down == down) return fonts[i]->n ? fonts[i] : NULL;
    ga_font *f = calloc(1, sizeof *f); f->key = k; f->down = down; fonts = realloc(fonts, (nfonts + 1) * sizeof *fonts); fonts[nfonts++] = f;
    const ent_t *e = find_ent(K_FNT, k); size_t n; uint8_t *b = slice(e, &n); if (!b) return NULL; double t0 = timing ? now_ms() : 0;
    const size_t TABLE = 0x14, RECS = 0x200a4; size_t dds = 0; for (size_t i = RECS; i + 4 < n; i += 4) if (b[i] == 'D' && b[i + 1] == 'D' && b[i + 2] == 'S' && b[i + 3] == ' ') { dds = i; break; }
    fb_t sheet; if (!dds || ga_decode_dds(b + dds, n - dds, &sheet)) { free(b); return NULL; }
    int ng = (int)((dds - 0x28 - RECS) / 36); f->g = malloc((ng > 0 ? ng : 1) * sizeof *f->g); f->n = ng; f->sheetW = sheet.w; f->sheetH = sheet.h;
    for (int gi = 0; gi < ng; gi++) { const uint8_t *q = b + RECS + gi * 36; glyph_t *g = &f->g[gi]; float u0 = bef32(q), v0 = bef32(q + 4), u1 = bef32(q + 8), v1 = bef32(q + 12);
        g->sx = (int)(u0 * sheet.w + 0.5f); g->sy = (int)(v0 * sheet.h + 0.5f); g->sw = (int)((u1 - u0) * sheet.w + 0.5f); g->sh = (int)((v1 - v0) * sheet.h + 0.5f); g->w = bef32(q + 16); g->h = bef32(q + 20); g->yo = bef32(q + 24); }
    for (int c = 0; c < 0x250; c++) { f->table[c] = -1; if (TABLE + c * 2 + 2 > n) continue; int gi = be16(b + TABLE + c * 2); if (gi > 0 && gi < ng) f->table[c] = (int16_t)gi; }
    acute_ink(f, &sheet);
    if (down > 1) { shrink(&sheet, down); for (int gi = 0; gi < ng; gi++) { glyph_t *g = &f->g[gi]; int x0 = g->sx / down, y0 = g->sy / down, x1 = (g->sx + g->sw + down - 1) / down, y1 = (g->sy + g->sh + down - 1) / down; g->sx = x0; g->sy = y0; g->sw = x1 - x0; g->sh = y1 - y0; } }
    int any = 0; for (int c = 32; c < 0x250; c++) if (f->table[c] >= 0) any = 1; if (!any) f->n = -ng;   /* a font of pictures (the controller's buttons), found by number */
    float asc = 0; for (int c = 33; c < 127; c++) if (f->table[c] >= 0 && f->g[f->table[c]].h - f->g[f->table[c]].yo > asc) asc = f->g[f->table[c]].h - f->g[f->table[c]].yo;
    if (!any) for (int gi = 0; gi < ng; gi++) if (f->g[gi].h - f->g[gi].yo > asc) asc = f->g[gi].h - f->g[gi].yo;
    int32_t space = (int32_t)be32(b), gap = (int32_t)be32(b + 4); f->size = bef32(b + 12); f->asc = asc > 0 ? asc : f->size * 0.7f; f->space = space > 0 ? space : f->size * 0.3f; f->gap = (float)gap;
    f->sheet = bake_fb(&sheet); free(b); if (!f->n) f->n = ng;
    if (timing) fprintf(stderr, "[arts] fnt %-28s %d glyphs, sheet %dx%d, size %.0f: %.1f ms\n", name, ng, f->sheetW, f->sheetH, f->size, now_ms() - t0);
    return f;
}
float ga_font_size(const ga_font *f) { return f ? f->size : 0; }
gfx_sprite *ga_font_sheet(const ga_font *f) { return f ? f->sheet : NULL; }
static const glyph_t *glyph_of(const ga_font *f, int c) {
    if (c >= 0xE000 && c < 0xE000 + abs(f->n)) return &f->g[c - 0xE000];
    if (c >= 0 && c < 0x250 && f->table[c] >= 0) return &f->g[f->table[c]];
    if (c < 128) { int u = c >= 'a' && c <= 'z' ? c - 32 : c >= 'A' && c <= 'Z' ? c + 32 : c; if (u != c && f->table[u] >= 0) return &f->g[f->table[u]]; return NULL; }
    /* an accented letter the font lacks: its plain letter */
    static const char *ACC = "áàâãäåÁÀÂÃÄÅéèêëÉÈÊËíìîïÍÌÎÏóòôõöÓÒÔÕÖúùûüÚÙÛÜçÇñÑ", *PLAIN = "aaaaaaAAAAAAeeeeEEEEiiiiIIIIoooooOOOOOuuuuUUUUcCnN";
    const char *p = ACC; int i = 0; while (*p) { int cp = (unsigned char)*p; if ((cp & 0xE0) == 0xC0) { cp = ((cp & 0x1F) << 6) | (p[1] & 0x3F); p += 2; } else p++; if (cp == c) return glyph_of(f, PLAIN[i]); i++; }
    return NULL;
}
static int utf8_next(const char **s) { const unsigned char *p = (const unsigned char *)*s; int c = *p; if (!c) return 0;
    if (c < 0x80) { *s += 1; return c; } if ((c & 0xE0) == 0xC0 && p[1]) { *s += 2; return ((c & 0x1F) << 6) | (p[1] & 0x3F); }
    if ((c & 0xF0) == 0xE0 && p[1] && p[2]) { *s += 3; return ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); } *s += 1; return '?'; }
int ga_glyph(const ga_font *f, int code, int *sx, int *sy, int *sw, int *sh, float *w, float *h, float *yo) {
    if (!f) return -1; const glyph_t *g = glyph_of(f, code); if (!g) return -1; *sx = g->sx; *sy = g->sy; *sw = g->sw; *sh = g->sh; *w = g->w; *h = g->h; *yo = g->yo; return 0;
}
float ga_text_w(const ga_font *f, const char *s, float size) {
    if (!f || !s) return 0; float k = size / f->size, w = 0; while (*s) { int c = utf8_next(&s); const glyph_t *g = c == ' ' ? NULL : glyph_of(f, c); w += (g ? g->w + f->gap : f->space) * k; } return w;
}
/* Accented letters a font lacks are put together: the plain letter, and the accent of a letter of the font that has it in
 * the same case (Ó from Á, the rows of its glyph above its plain letter); the tilde (ã õ, which no GH3 font has) is drawn
 * over the plain letter where the acute sits. acc: 0 acute, 1 grave, 2 circumflex, 3 tilde, 4 diaeresis */
static const struct { unsigned short cp; char base, acc; } ACCS[] = {
    {0xE1,'a',0},{0xE0,'a',1},{0xE2,'a',2},{0xE3,'a',3},{0xE4,'a',4},{0xE9,'e',0},{0xE8,'e',1},{0xEA,'e',2},{0xEB,'e',4},{0xED,'i',0},{0xEC,'i',1},{0xEE,'i',2},{0xEF,'i',4},
    {0xF3,'o',0},{0xF2,'o',1},{0xF4,'o',2},{0xF5,'o',3},{0xF6,'o',4},{0xFA,'u',0},{0xF9,'u',1},{0xFB,'u',2},{0xFC,'u',4},{0xF1,'n',3},
    {0xC1,'A',0},{0xC0,'A',1},{0xC2,'A',2},{0xC3,'A',3},{0xC4,'A',4},{0xC9,'E',0},{0xC8,'E',1},{0xCA,'E',2},{0xCB,'E',4},{0xCD,'I',0},{0xCC,'I',1},{0xCE,'I',2},{0xCF,'I',4},
    {0xD3,'O',0},{0xD2,'O',1},{0xD4,'O',2},{0xD5,'O',3},{0xD6,'O',4},{0xDA,'U',0},{0xD9,'U',1},{0xDB,'U',2},{0xDC,'U',4},{0xD1,'N',3} };
#define NACCS ((int)(sizeof ACCS / sizeof *ACCS))
static int acc_of(int c) { for (int i = 0; i < NACCS; i++) if (ACCS[i].cp == c) return i; return -1; }
static int own(const ga_font *f, int c) { return c >= 0 && c < 0x250 && f->table[c] >= 0; }
static const glyph_t *donor(const ga_font *f, int acc, int upper, const glyph_t **plain) {   /* a letter with this accent (a e o u i, in that order) */
    for (const char *o = "aeoui"; *o; o++) { int b = upper ? *o - 32 : *o; if (!own(f, b)) continue;
        for (int i = 0; i < NACCS; i++) if (ACCS[i].acc == acc && ACCS[i].base == b && own(f, ACCS[i].cp)) { *plain = &f->g[f->table[b]]; return &f->g[f->table[ACCS[i].cp]]; } }
    return NULL;
}
/* tildes drawn at the size they are shown (w x h px, a stroke t px thick), so a small one stays crisp: one turn and a bit of a sine */
static struct { int w, h, t; gfx_sprite *s; } tildes[24]; static int ntildes;
static gfx_sprite *tilde_at(int w, int h, float t) {
    int tq = (int)(t * 4 + 0.5f); for (int i = 0; i < ntildes; i++) if (tildes[i].w == w && tildes[i].h == h && tildes[i].t == tq) return tildes[i].s;
    fb_t f; fb_init(&f, w, h); float r = tq / 8.0f, amp = (h - 2 * r) / 2, x0 = r, x1 = w - r; if (amp < 0.3f) amp = 0.3f;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) { int in = 0;
        for (int sy = 0; sy < 4; sy++) for (int sx = 0; sx < 4; sx++) { float px = x + (sx + 0.5f) / 4, py = y + (sy + 0.5f) / 4, d = 1e9f;
            for (int i = 0; i <= 48; i++) { float u = i / 48.0f, tt = -0.3f * (float)M_PI + 2.6f * (float)M_PI * u, cx = x0 + (x1 - x0) * u, cy = h / 2.0f - amp * sinf(tt), dx = px - cx, dy = py - cy; if (dx * dx + dy * dy < d) d = dx * dx + dy * dy; }
            if (d <= r * r) in++; }
        if (in) f.px[(size_t)y * w + x] = RGBA(255, 255, 255, in * 255 / 16); }
    if (ntildes == 24) { gfx_free_sprite(tildes[23].s); ntildes = 23; }
    tildes[ntildes].w = w; tildes[ntildes].h = h; tildes[ntildes].t = tq; tildes[ntildes].s = gfx_bake(&f, 1); fb_free(&f); return tildes[ntildes++].s;
}
static void acute_ink(ga_font *f, const fb_t *sh) {   /* where the ink of an acute (á, Á...) sits over its letter: the tilde goes there */
    for (int up = 0; up < 2; up++) { const glyph_t *pg = NULL, *dg = donor(f, 0, up, &pg); float *o = f->acute[up]; o[0] = o[1] = o[2] = 0; if (!dg || dg->sh <= 0 || dg->h <= 0) continue;
        int rows = (int)(dg->sh * ((dg->h - dg->yo) - (pg->h - pg->yo)) / dg->h + 0.5f), top = -1, bot = -1; double sx = 0, sw = 0; float u = dg->h / dg->sh;
        for (int r = 0; r < rows && r < dg->sh; r++) for (int x = 0; x < dg->sw; x++) { int px = dg->sx + x, py = dg->sy + r; if (px < 0 || py < 0 || px >= sh->w || py >= sh->h) continue;
            uint32_t a = sh->px[(size_t)py * sh->w + px] >> 24; if (a < 96) continue; if (top < 0) top = r; bot = r + 1; sx += (x + 0.5) * a; sw += a; }
        if (top < 0) continue; o[0] = (rows - bot) * u; o[1] = (bot - top) * u; o[2] = (float)(sx / sw) * dg->w / dg->sw - dg->w / 2; }
    const glyph_t *l = own(f, 'l') ? &f->g[f->table['l']] : own(f, 'I') ? &f->g[f->table['I']] : NULL; f->stem = 0;   /* a stem's width: how thick the strokes are */
    if (l && l->sw > 0) { int py = l->sy + l->sh / 2, n = 0; for (int x = 0; x < l->sw; x++) if (py < sh->h && l->sx + x < sh->w && (sh->px[(size_t)py * sh->w + l->sx + x] >> 24) >= 128) n++; f->stem = n * l->w / l->sw; }
}
int ga_has(const ga_font *f, int code) { if (!f) return 0; if (own(f, code)) return 1; int i = acc_of(code); return i >= 0 && own(f, ACCS[i].base) ? 2 : 0; }
/* one code point, its baseline at `base`; the advance */
static float draw_char(const ga_font *f, float x, float base, float k, float sx, int c, uint32_t color) {
    const glyph_t *g = c == ' ' ? NULL : glyph_of(f, c); if (!g) return f->space * k * sx;
    float adv = (g->w + f->gap) * k * sx, a = (color >> 24) / 255.0f; uint32_t tint = color & 0xFFFFFF; if (a <= 0.002f) return adv;
    float top = base + (g->yo - g->h) * k; if (g->sw > 0 && g->sh > 0) gfx_blit(f->sheet, g->sx, g->sy, g->sw, g->sh, x, top, g->w * k * sx, g->h * k, tint, a);
    int i; if (own(f, c) || (i = acc_of(c)) < 0) return adv;
    int up = ACCS[i].base < 'a', acc = ACCS[i].acc; const glyph_t *pg = NULL, *dg = donor(f, acc == 3 ? 0 : acc, up, &pg); if (!dg) dg = donor(f, acc == 3 ? 0 : acc, !up, &pg);   /* Ê from ê when no capital has it */
    float strip = dg ? (dg->h - dg->yo) - (pg->h - pg->yo) : g->h * 0.3f, cx = x + g->w * k * sx / 2; if (strip <= 0) return adv; strip *= k;
    if (acc == 3) {   /* about as wide as the letter's bowl, as thick as the font's strokes (a bit less), in the acute's place */
        const float *ink = f->acute[f->acute[up][1] > 0 ? up : !up]; float t = f->stem > 0 ? f->stem * k * 0.75f : g->h * k * 0.08f; if (t < 1.2f) t = 1.2f; if (t > 6) t = 6;
        int tw = (int)(g->w * k * sx * (up ? 0.66f : 0.74f) + 0.5f), th = (int)(tw * 0.36f + t * 0.6f + 0.5f); if (tw < 4) tw = 4; if (tw > 64) tw = 64; if (ink[1] > 0 && th > ink[1] * k * 1.15f + 1) th = (int)(ink[1] * k * 1.15f + 1); if (th < 3) th = 3; if (th > 24) th = 24;
        float lift = ink[1] > 0 ? ink[0] * k : strip * 0.2f; if (lift < 1) lift = 1; cx += ink[2] * k * sx;
        gfx_blit(tilde_at(tw, th, t), 0, 0, tw, th, floorf(cx - tw / 2.0f + 0.5f), floorf(top - lift - th + 0.5f), (float)tw, (float)th, tint, a); }
    else if (dg) { int rows = (int)(dg->sh * strip / (dg->h * k) + 0.5f); float h = rows * dg->h * k / dg->sh; if (rows > 0) gfx_blit(f->sheet, dg->sx, dg->sy, dg->sw, rows, cx - dg->w * k * sx / 2, top - h, dg->w * k * sx, h, tint, a); }
    return adv;
}
float ga_char(const ga_font *f, float x, float base, float size, float sx, int code, uint32_t color) { return f ? draw_char(f, x, base, size / f->size, sx, code, color) : 0; }
static void draw_text(const ga_font *f, float x, float y, float size, float sx, const char *s, uint32_t color) {
    float k = size / f->size, base = y + f->asc * k; while (*s) x += draw_char(f, x, base, k, sx, utf8_next(&s), color);
}
void ga_text(const ga_font *f, float x, float y, float size, const char *s, uint32_t color) { if (f && s) draw_text(f, x, y, size, 1, s, color); }
void ga_text_center(const ga_font *f, float cx, float y, float size, const char *s, uint32_t color) { if (f && s) draw_text(f, cx - ga_text_w(f, s, size) / 2, y, size, 1, s, color); }
void ga_text_right(const ga_font *f, float rx, float y, float size, const char *s, uint32_t color) { if (f && s) draw_text(f, rx - ga_text_w(f, s, size), y, size, 1, s, color); }
void ga_text_sh(const ga_font *f, float x, float y, float size, const char *s, uint32_t color, uint32_t shadow) { if (!f || !s) return; float d = 3 * size / 48; if (d < 1) d = 1; draw_text(f, x + d, y + d, size, 1, s, shadow); draw_text(f, x, y, size, 1, s, color); }
void ga_text_fit(const ga_font *f, float x, float y, float size, float maxW, const char *s, uint32_t color, int align) {
    if (!f || !s) return; float w = ga_text_w(f, s, size), sx = maxW > 0 && w > maxW ? maxW / w : 1; w *= sx;
    draw_text(f, align == 1 ? x - w / 2 : align == 2 ? x - w : x, y, size, sx, s, color);
}

void ga_release(void) {
    for (int i = 0; i < ncache; i++) gfx_free_sprite(cache[i].s); ncache = 0;
    for (int i = 0; i < nfonts; i++) { gfx_free_sprite(fonts[i]->sheet); free(fonts[i]->g); free(fonts[i]); } free(fonts); fonts = NULL; nfonts = 0; for (int i = 0; i < ntildes; i++) gfx_free_sprite(tildes[i].s); ntildes = 0;
    free(bank); bank = NULL; bankLen = 0; free(texes); texes = NULL; ntexes = 0; for (int i = 0; i < nscns; i++) free(scns[i]); free(scns); free(scnLens); scns = NULL; scnLens = NULL; nscns = 0; bankTried = 0;
    memUsed = 0;
}
void ga_text_dir(const ga_font *f, float x, float y, float size, float sx, float ca, float sa, const char *s, uint32_t color) {
    if (!f || !s) return; float k = size / f->size, d = 0, al = (color >> 24) / 255.0f; uint32_t tint = color & 0xFFFFFF;
    while (*s) { int c = utf8_next(&s); const glyph_t *g = c == ' ' ? NULL : glyph_of(f, c);
        if (g && g->sw > 0 && g->sh > 0) gfx_blit(f->sheet, g->sx, g->sy, g->sw, g->sh, x + d * ca, y + d * sa + (f->asc + g->yo - g->h) * k, g->w * k * sx, g->h * k, tint, al);
        d += (g ? g->w + f->gap : f->space) * k * sx; }
}
