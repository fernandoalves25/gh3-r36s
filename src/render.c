/* The stage, the highway and the HUD in pixel art, built for the GPU: the static parts (stage backdrop with
 * truss, speakers and lamps, the textured highway with lanes and rails) are baked once into full-screen
 * textures; gems, fret targets, flames, crowd and beams are pixel-art sprites drawn scaled; meters, text and
 * overlays are rectangles and glyphs. A frame is a few hundred RenderCopy/FillRect calls and no per-pixel work. */
#include "render.h"
#include "fx.h"
#include "ui.h"
#include "settings.h"
#include "original.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

const uint32_t FRET_COLORS[5] = { RGB(0x31, 0xc4, 0x66), RGB(0xea, 0x4b, 0x40), RGB(0xf3, 0xd2, 0x3b), RGB(0x45, 0x94, 0xf4), RGB(0xf2, 0x8d, 0x2d) };
const char *FRET_NAMES[5] = { "verde", "vermelho", "amarelo", "azul", "laranja" };
#define PERSP 2.5f
#define Z_NEAR (-0.1f)
typedef struct { float W, H, cx, sy, vy, hw, horizon; } view_t;
static view_t V;
static void view_set(int w, int h) {
    V.W = (float)w; V.H = (float)h; V.cx = V.W / 2; V.sy = V.H * 0.84f;
    float top = V.H * 0.12f, sFar = 1 / (1 + PERSP);
    V.vy = (top - V.sy * sFar) / (1 - sFar); V.horizon = top;
    V.hw = fminf(fminf(V.W * 0.4f, V.H * 0.4f), 340);
}
static void proj(float xo, float z, float *x, float *y, float *s) { float k = 1 / (1 + PERSP * z); *x = V.cx + xo * V.hw * k; *y = V.vy + (V.sy - V.vy) * k; *s = k; }
static float laneX(int f, int lefty) { return ((lefty ? 4 - f : f) - 2) * 0.4f; }
static float fade(float z) { float v = (1 - z) / 0.22f; return v < 0 ? 0 : v > 1 ? 1 : v; }
static gfx_sprite *backSp[THEME_N][2], *stageSp;
static int PXS = 2;
/* per-theme sprite sets: gem body + marks (plain, hopo, star), targets (up, down), flames (3 frames, normal and star power) */
typedef struct { gfx_sprite *gemBody, *gemMarks[3], *tgt[2], *flame[3], *spflame[3]; float gemAnchor, tgtAnchor; } theme_t;
static theme_t TH[THEME_N];
static const char *THEME_NAMES[THEME_N] = { "Pixel art", "HD suave", "Original" };
const char *render_theme_name(int t) { return t >= 0 && t < THEME_N ? THEME_NAMES[t] : "?"; }
static int theme(void) { return SET.theme >= 0 && SET.theme < THEME_N ? SET.theme : 0; }
static int orig(void) { return SET.theme == 2 && orig_ready(); }   /* the Original theme with its art at hand */
static void theme_tick(void) { static int last = -1; if (last == 2 && SET.theme != 2) { gfx_free_sprite(backSp[2][0]); gfx_free_sprite(backSp[2][1]); backSp[2][0] = backSp[2][1] = NULL; } last = SET.theme; orig_tick(); }

/* ---- the HD theme: smooth shapes drawn by the software rasterizer, linear filtered ---- */
#define HD_GEM_W 112
#define HD_GEM_H 72
#define HD_GEM_CY 34
#define HD_GEM_R 48.0f
#define HD_TGT_W 100
#define HD_TGT_H 60
#define HD_TGT_CY 28
#define HD_TGT_R 40.0f
static void star_poly(fb_t *fb, float x, float y, float r, uint32_t c) {
    float p[20]; for (int i = 0; i < 10; i++) { float a = -(float)M_PI / 2 + i * (float)M_PI / 5, rr = i % 2 ? r * 0.45f : r; p[i * 2] = x + cosf(a) * rr; p[i * 2 + 1] = y + sinf(a) * rr * 0.62f; }
    fb_poly(fb, p, 10, c);
}
static gfx_sprite *hd_gem_body(void) {
    fb_t f; fb_init(&f, HD_GEM_W, HD_GEM_H); float x = HD_GEM_W / 2.0f, y = HD_GEM_CY, r = HD_GEM_R, ry = r * 0.52f;
    fb_ellipse(&f, x, y + r * 0.14f, r * 1.08f, ry * 1.08f, RGBA(6, 4, 10, 200));
    fb_ellipse(&f, x, y, r, ry, RGB(255, 255, 255));
    fb_ellipse(&f, x, y + ry * 0.25f, r * 0.9f, ry * 0.7f, RGB(205, 205, 205));          /* a darker lower half */
    fb_ellipse(&f, x, y - ry * 0.35f, r * 0.7f, ry * 0.4f, RGB(255, 255, 255));          /* the top sheen */
    gfx_sprite *s = gfx_bake(&f, 1); fb_free(&f); return s;
}
static gfx_sprite *hd_gem_marks(int kind) {
    fb_t f; fb_init(&f, HD_GEM_W, HD_GEM_H); float x = HD_GEM_W / 2.0f, y = HD_GEM_CY, r = HD_GEM_R, ry = r * 0.52f;
    if (kind == 2) { star_poly(&f, x, y - r * 0.06f, r * 0.86f, RGBA(0, 0, 0, 90)); star_poly(&f, x, y - r * 0.1f, r * 0.8f, RGB(255, 255, 255)); }
    else if (kind == 1) { fb_ellipse(&f, x, y - r * 0.06f, r * 0.66f, ry * 0.66f, RGBA(0, 0, 0, 70)); fb_ellipse(&f, x, y - r * 0.1f, r * 0.6f, ry * 0.6f, RGB(255, 255, 255)); }
    else { fb_ellipse(&f, x, y - r * 0.04f, r * 0.66f, ry * 0.66f, RGB(0x16, 0x13, 0x1d)); fb_ellipse(&f, x, y - r * 0.12f, r * 0.4f, ry * 0.4f, RGB(255, 255, 255)); }
    gfx_sprite *s = gfx_bake(&f, 1); fb_free(&f); return s;
}
static gfx_sprite *hd_target(int down) {
    fb_t im; fb_init(&im, HD_TGT_W, HD_TGT_H); float x = HD_TGT_W / 2.0f, y = HD_TGT_CY + (down ? 2 : 0), tr = HD_TGT_R;
    fb_ellipse(&im, x, y + tr * 0.12f, tr * 1.1f, tr * 0.58f, RGBA(6, 4, 10, 190));
    fb_ellipse(&im, x, y, tr, tr * 0.52f, RGB(255, 255, 255));
    fb_ellipse(&im, x, y, tr * 0.68f, tr * 0.36f, down ? RGB(255, 255, 255) : RGB(0x16, 0x13, 0x1d));
    if (!down) fb_ellipse(&im, x, y, tr * 0.36f, tr * 0.19f, RGBA(255, 255, 255, 90));
    gfx_sprite *s = gfx_bake(&im, 1); fb_free(&im); return s;
}
static gfx_sprite *hd_burst(int frame) { /* a soft flash growing and fading over three frames */
    /* worked out per pixel (light colour, alpha from the shape): stacking translucent layers on a clear image would
     * drag the colour toward black and leave a dark ring */
    fb_t im; fb_init(&im, 96, 96); float k0 = 0.45f + frame * 0.27f, fade = 1 - frame * 0.3f;
    for (int y = 0; y < 96; y++) for (int x = 0; x < 96; x++) { float dx = (x + 0.5f - 48) / (46 * k0), dy = (y + 0.5f - 60) / (30 * k0), d = sqrtf(dx * dx + dy * dy);
        float glow = d < 1 ? (1 - d) * (1 - d) * 0.75f : 0, ang = atan2f(-(y + 0.5f - 60), x + 0.5f - 48) + frame * 0.4f, rl = d * k0 / (0.45f + frame * 0.3f);
        float ray = y < 62 && rl < 1.25f ? powf(fmaxf(0, cosf(ang * 3)), 24) * (1 - rl / 1.25f) * 0.8f : 0, a = fminf(1, (glow + ray) * fade); if (a < 0.01f) continue;
        im.px[(size_t)y * 96 + x] = RGBA(255, 238, 196, (int)(255 * a)); }
    gfx_sprite *s = gfx_bake(&im, 1); fb_free(&im); return s;
}
static void bake_theme_hd(theme_t *t) {
    t->gemBody = hd_gem_body(); for (int k = 0; k < 3; k++) t->gemMarks[k] = hd_gem_marks(k); t->tgt[0] = hd_target(0); t->tgt[1] = hd_target(1);
    for (int k = 0; k < 3; k++) { t->flame[k] = hd_burst(k); t->spflame[k] = t->flame[k]; }
    t->gemAnchor = (float)HD_GEM_CY / HD_GEM_H; t->tgtAnchor = (float)HD_TGT_CY / HD_TGT_H;
}
static void bake_theme_pixel(theme_t *t) {
    t->gemBody = art_get(ART_GEM_BODY); t->gemMarks[0] = art_get(ART_GEM_PLAIN); t->gemMarks[1] = art_get(ART_GEM_HOPO); t->gemMarks[2] = art_get(ART_GEM_STAR);
    t->tgt[0] = art_get(ART_TGT_UP); t->tgt[1] = art_get(ART_TGT_DOWN);
    for (int k = 0; k < 3; k++) { t->flame[k] = art_get(ART_FLAME0 + k); t->spflame[k] = art_get(ART_SPFLAME0 + k); }
    t->gemAnchor = 0.62f; t->tgtAnchor = 0.55f;
}
#define NLAMPS 5
static float lampX[NLAMPS];

/* ---- the baked stage: sky, truss, lamps, speaker stacks, floor ---- */
static void bake_stage(void) {
    fb_t f; fb_init(&f, (int)V.W, (int)V.H); int W = f.w, H = f.h;
    fb_vgrad(&f, 0, 0, W, (int)(H * 0.55f), RGB(0x1c, 0x12, 0x30), RGB(0x12, 0x0e, 0x1c));
    fb_rect(&f, 0, (int)(H * 0.55f), W, H, RGB(0x12, 0x0e, 0x1c));
    /* a hazy glow behind the band */
    for (int i = 10; i >= 1; i--) { float k = i / 10.0f; fb_ellipse(&f, W / 2.0f, V.horizon + 6, W * 0.46f * k, H * 0.11f * k, fb_alpha(RGB(0x7a, 0x4c, 0xb0), 0.045f)); }
    /* floor: a dark band at the horizon with a lit edge */
    fb_rect(&f, 0, (int)V.horizon - 2, W, 3, RGB(0x2a, 0x22, 0x3a)); fb_rect(&f, 0, (int)V.horizon + 1, W, 1, RGB(0x4a, 0x3a, 0x60));
    /* truss across the top with lamps under it */
    gfx_sprite *tr = art_get(ART_TRUSS); int tw = gfx_sprite_w(tr), th = gfx_sprite_h(tr); int ty = (int)(H * 0.045f);
    (void)tw; (void)th;
    fb_t tile; fb_init(&tile, 16, 10); { /* redraw the truss tile in software (sprites are not readable once baked) */
        uint32_t S = art_color('S'), s2 = art_color('s'), K = art_color('k');
        fb_rect(&tile, 0, 0, 16, 2, S); fb_rect(&tile, 0, 8, 16, 2, S); fb_rect(&tile, 0, 0, 16, 1, s2); fb_rect(&tile, 0, 9, 16, 1, K);
        for (int i = 0; i < 8; i++) { int yy = 2 + i * 6 / 8; tile.px[yy * 16 + i] = S; tile.px[yy * 16 + 15 - i] = S; tile.px[yy * 16 + 7 + i] = s2; tile.px[yy * 16 + 8 - i] = s2; } }
    for (int x = 0; x < W; x += 16 * PXS) for (int yy = 0; yy < 10 * PXS; yy++) for (int xx = 0; xx < 16 * PXS; xx++) { uint32_t c = tile.px[(yy / PXS) * 16 + xx / PXS]; if (c) f.px[(size_t)(ty + yy) * W + x + xx] = c; }
    fb_free(&tile);
    for (int i = 0; i < NLAMPS; i++) { lampX[i] = W * (0.12f + 0.76f * i / (NLAMPS - 1)); int lx = (int)lampX[i] - 6 * PXS, ly = ty + 10 * PXS;
        fb_rect(&f, (int)lampX[i] - PXS, ty + 9 * PXS, 2 * PXS, 2 * PXS, art_color('S'));
        static const char *LAMP[8] = { "....kkkk....", "...kssssk...", "..kSssssSk..", "..kSssssSk..", ".kkkkkkkkkk.", ".kSSSSSSSSk.", ".kSSSSSSSSk.", ".kkkkkkkkkk." };
        for (int r = 0; r < 8; r++) for (int c = 0; c < 12; c++) { uint32_t col = art_color(LAMP[r][c]); if (col) fb_rect(&f, lx + c * PXS, ly + r * PXS, PXS, PXS, col); } }
    /* speaker stacks at both sides, two cabinets high, standing on the floor line */
    /* (they stand on the stage floor beside the near end of the highway, where the sides of the screen are free) */
    { int cw = 24, ch = 40; for (int side = 0; side < 2; side++) for (int n = 0; n < 2; n++) { int x = side ? W - (cw + 5) * PXS : 5 * PXS, y = (int)(H * 0.80f) - (n + 1) * ch * PXS;
        /* cabinet drawn in software, same look as the sprite */
        fb_rect(&f, x, y, cw * PXS, ch * PXS, art_color('k')); fb_rect(&f, x + PXS, y + PXS, (cw - 2) * PXS, (ch - 2) * PXS, art_color('S')); fb_rect(&f, x + 2 * PXS, y + 2 * PXS, (cw - 4) * PXS, (ch - 4) * PXS, art_color('x'));
        fb_rect(&f, x + PXS, y + PXS, (cw - 2) * PXS, PXS, art_color('s')); fb_rect(&f, x + PXS, y + PXS, PXS, (ch - 2) * PXS, art_color('s'));
        for (int yy = 4; yy < ch - 4; yy++) for (int xx = 4; xx < cw - 4; xx++) if ((xx + yy) & 1) fb_rect(&f, x + xx * PXS, y + yy * PXS, PXS, PXS, art_color('T'));
        for (int c = 0; c < 2; c++) { float cy = y + (11 + c * 18) * PXS + PXS / 2.0f, cx = x + 12 * PXS; fb_ellipse(&f, cx, cy, 7 * PXS, 7 * PXS, art_color('d')); fb_ellipse(&f, cx, cy, 6 * PXS, 6 * PXS, art_color('S')); fb_ellipse(&f, cx, cy, 4 * PXS, 4 * PXS, art_color('x')); fb_ellipse(&f, cx, cy, 2 * PXS, 2 * PXS, art_color('s')); fb_rect(&f, (int)cx - 3 * PXS, (int)cy - 4 * PXS, PXS, PXS, art_color('m')); }
        fb_rect(&f, x + 9 * PXS, y + (ch - 4) * PXS, 6 * PXS, 2 * PXS, art_color('a')); } }
    stageSp = gfx_bake(&f, 0); fb_free(&f);
}
/* the static highway: surface (a pixel texture, or a smooth gradient in HD), lane lines, rails and the strike line */
static gfx_sprite *bake_back(int th, int sp) {
    fb_t f; fb_init(&f, (int)V.W, (int)V.H); float s, ax, ay, bx, by, cx, cy, dx, dy;
    proj(-1, 1, &ax, &ay, &s); proj(1, 1, &bx, &by, &s); proj(1, Z_NEAR, &cx, &cy, &s); proj(-1, Z_NEAR, &dx, &dy, &s);
    fb_t tile; fb_init(&tile, 32, 32); art_highway_tile(&tile);
    float nearW = cx - dx; float tilesAcross = nearW / (32.0f * PXS);
    for (int y = (int)ay; y <= (int)cy && y < f.h; y++) {
        float kk = (y + 0.5f - V.vy) / (V.sy - V.vy); if (kk <= 0.001f) continue; float z = (1 / kk - 1) / PERSP;
        float k = (y - ay) / (cy - ay); float lx = ax + (dx - ax) * k, rx = bx + (cx - bx) * k;
        float v = z * tilesAcross * 1.1f; int ty = ((int)floorf(v * 32) % 32 + 32) % 32;
        float dark = z > 0.75f ? 1 - (z - 0.75f) / 0.25f : 1; if (dark < 0) dark = 0; float shade = 0.55f + 0.45f * (1 - z);
        if (th == 1) { uint32_t c = RGB((int)(0x1a + (0x28 - 0x1a) * (1 - z)), (int)(0x17 + (0x22 - 0x17) * (1 - z)), (int)(0x22 + (0x36 - 0x22) * (1 - z))); for (int x = (int)lx; x < (int)rx; x++) f.px[(size_t)y * f.w + x] = (c & 0xFFFFFF) | ((uint32_t)(255 * dark) << 24); continue; }
        for (int x = (int)lx; x < (int)rx; x++) { float u = (x - lx) / (rx - lx) * tilesAcross; int tx = ((int)floorf(u * 32) % 32 + 32) % 32; uint32_t c = tile.px[ty * 32 + tx];
            int r = (int)(((c >> 16) & 255) * shade), g = (int)(((c >> 8) & 255) * shade), b = (int)((c & 255) * shade);
            f.px[(size_t)y * f.w + x] = RGBA(r, g, b, (int)(255 * dark)); } }
    fb_free(&tile);
    for (int i = 0; i <= 5; i++) { float xo = -1 + i * 0.4f, px, py, qx, qy; int edge = i == 0 || i == 5; proj(xo, 1, &px, &py, &s); proj(xo, Z_NEAR, &qx, &qy, &s);
        uint32_t col = edge ? (sp ? C_SP : C_AMP) : (th == 1 ? C_INK : RGB(0x5a, 0x50, 0x70)); int steps = 40;
        for (int k = 0; k < steps; k++) { float t0 = (float)k / steps, t1 = (float)(k + 1) / steps; float lane = th == 1 ? 0.12f : 0.3f; float a = t0 < 0.3f ? t0 / 0.3f * (edge ? 0.9f : lane) : (edge ? 0.9f + 0.1f * (t0 - 0.3f) / 0.7f : lane + 0.15f * (t0 - 0.3f) / 0.7f);
            float wdt = edge ? 1.5f + 2.5f * t0 : 1.0f; fb_line(&f, px + (qx - px) * t0, py + (qy - py) * t0, px + (qx - px) * t1, py + (qy - py) * t1 + 0.6f, wdt, fb_alpha(col, a)); }
        if (edge) { fb_line(&f, px, py, qx, qy, 1, fb_alpha(RGB(0xff, 0xf0, 0xd0), 0.5f)); } }
    { float sx, sy2, ex, ey; proj(-1, 0, &sx, &sy2, &s); proj(1, 0, &ex, &ey, &s); if (th == 1) fb_line(&f, sx, sy2, ex, ey, 2.5f, fb_alpha(C_INK, 0.6f)); else { fb_rect(&f, (int)sx, (int)sy2 - 2, (int)(ex - sx), 4, fb_alpha(C_INK, 0.35f)); fb_rect(&f, (int)sx, (int)sy2 - 1, (int)(ex - sx), 2, fb_alpha(C_INK, 0.6f)); } }
    gfx_sprite *bs = gfx_bake(&f, th == 1); fb_free(&f); return bs;
}
static gfx_sprite *back(int th, int sp) {
    if (!backSp[th][sp]) { if (th == 2 && orig()) { fb_t f; fb_init(&f, (int)V.W, (int)V.H); if (orig_bake_back(&f, proj, V.vy, V.sy, PERSP, Z_NEAR, sp) == 0) backSp[th][sp] = gfx_bake(&f, 1); fb_free(&f); }
        if (!backSp[th][sp]) backSp[th][sp] = bake_back(th == 2 ? 1 : th, sp); }
    return backSp[th][sp];
}
void render_init(int W, int H) {
    view_set(W, H); PXS = art_px();
    bake_stage(); for (int t = 0; t < 2; t++) { backSp[t][0] = bake_back(t, 0); backSp[t][1] = bake_back(t, 1); }
    bake_theme_pixel(&TH[0]); bake_theme_hd(&TH[1]); TH[2] = TH[1];   /* the Original theme falls back on the HD sprites piece by piece */
}
static int lower_bound_notes(const gh3_note *a, int n, double t) { int lo = 0, hi = n; while (lo < hi) { int m = (lo + hi) >> 1; if (a[m].t < t) lo = m + 1; else hi = m; } return lo; }
static int lower_bound_bars(const gh3_bar *a, int n, double t) { int lo = 0, hi = n; while (lo < hi) { int m = (lo + hi) >> 1; if (a[m].t < t) lo = m + 1; else hi = m; } return lo; }

/* a gem: tinted body + untinted marks; x,y is where the gem sits on the lane, s the perspective scale */
static void gem(float x, float y, float s, uint32_t color, int kind, float alpha, int fret, int missed, int sp, double perf) {
    if (orig()) { orig_gem(x, y, s, V.hw, fret, kind, sp, missed, alpha, perf); return; }
    const theme_t *t = &TH[theme()]; gfx_sprite *body = t->gemBody; int sw = gfx_sprite_w(body), sh = gfx_sprite_h(body);
    float w = V.hw * (theme() >= 1 ? 0.36f : 0.33f) * s, scale = w / sw, h = sh * scale, dx = x - w / 2, dy = y - h * t->gemAnchor;
    gfx_blit(body, 0, 0, sw, sh, dx, dy, w, h, color, alpha);
    gfx_blit(t->gemMarks[kind], 0, 0, sw, sh, dx, dy, w, h, RGB(255, 255, 255), alpha);
}
/* a fret target at the strike line: lit while held, raised for an instant on a hit and while a sustain rings */
static void target(const theme_t *th, int f, float px, float py, float tgtW, int down, int popped) {
    if (orig()) { orig_target(px, py, V.hw, f, down, popped); return; }
    gfx_sprite *tu = th->tgt[0], *td = th->tgt[1]; int tw = gfx_sprite_w(tu), tgh = gfx_sprite_h(tu); float tscale = tgtW / tw;
    gfx_blit(down ? td : tu, 0, 0, tw, tgh, px - tgtW / 2, py - tgh * tscale * th->tgtAnchor, tgtW, tgh * tscale, FRET_COLORS[f], 1);
}
/* the stage behind the highway: baked backdrop plus crowd, lamps and beams that move with the music */
static void stage(double perf, float pulse, int sp, int level, int fx, float energy) {
    gfx_sprite_at(stageSp, 0, 0, 1, RGB(255, 255, 255), 1);
    if (!fx) return;
    static const uint32_t LIGHTS[5] = { RGB(0xe9, 0xb0, 0x4c), RGB(0x31, 0xc4, 0x66), RGB(0xf3, 0xd2, 0x3b), RGB(0xf2, 0x8d, 0x2d), RGB(0xea, 0x4b, 0x40) };
    gfx_sprite *beam = art_get(ART_BEAM), *glow = art_get(ART_GLOW), *crowd[2] = { art_get(ART_CROWD0), art_get(ART_CROWD1) };
    int beat = (int)floor(perf / 1000.0 * 2) & 1; (void)beat;
    /* beams: each lamp a cone, swaying slowly, brighter on the beat */
    int ty = (int)(V.H * 0.045f) + 18 * PXS;
    for (int i = 0; i < NLAMPS; i++) {
        uint32_t col = sp ? C_SP : LIGHTS[(i + level) % 5]; float sway = sinf((float)perf / 1400.0f + i * 1.3f) * V.W * 0.08f;
        float bw = V.W * 0.16f, bh = V.H * 0.62f; float a = (0.10f + 0.16f * pulse) * (0.6f + 0.4f * energy) * (sp ? 1.5f : 1);
        gfx_blit(beam, 0, 0, 64, 160, lampX[i] - bw / 2 + sway * 0.5f, (float)ty, bw, bh, col, a);
        gfx_sprite_draw(glow, lampX[i], (float)ty, (V.W * 0.07f) / 64 * (1 + 0.3f * pulse), col, 0.55f + 0.3f * pulse);
        gfx_rect(lampX[i] - 4 * PXS, (float)ty - 2 * PXS, 8 * PXS, 2 * PXS, col);
    }
    /* the crowd: a strip across the horizon, swapping frames on the beat and jumping higher with the energy */
    int frame = pulse > 0.5f ? 0 : 1; gfx_sprite *c = crowd[frame]; int cw = gfx_sprite_w(c), chh = gfx_sprite_h(c);
    float cy = V.horizon - chh + 6 * PXS - energy * 2 * PXS;
    for (float x = -cw * 0.3f; x < V.W; x += cw) gfx_sprite_at(c, x, cy, 1, RGB(255, 255, 255), 1);
    /* a floor wash from the lights */
    gfx_blit(glow, 0, 0, 64, 64, -V.W * 0.2f, V.horizon - V.H * 0.12f, V.W * 1.4f, V.H * 0.28f, sp ? C_SP : LIGHTS[level % 5], 0.12f + 0.1f * pulse);
}

void render_backdrop(double perf, int stageFx) {
    theme_tick(); if (orig()) { orig_backdrop(perf, (int)V.W, (int)V.H); return; }
    float pulse = 0.5f + 0.5f * sinf((float)perf / 600.0f); int level = (int)(perf / 2400) % 5;
    stage(perf, pulse * 0.6f, 0, level, stageFx, 0.5f);
    gfx_rect(0, 0, V.W, V.H, RGBA(10, 8, 16, 190));
}
void render_stars(int cx, int y, int n, int scale, double revealT) { fx_stars(cx, y, n, 13 * scale + 1, revealT, theme() >= 1); }

/* a battle's highway: RL.on draws the lane alone (no stage, no HUD), narrowed by q round x = cx, shaken by dx, dy */
render_lane_t RL;
static view_t lane_view(void) { view_t v = V; if (RL.on) { v.cx = RL.cx + RL.dx; v.hw *= RL.q; v.vy += RL.dy; v.sy += RL.dy; v.horizon += RL.dy; } return v; }
void render_lane_xy(int f, float z, float *x, float *y, float *s) { view_t v0 = V; V = lane_view(); proj(laneX(f, SET.lefty ^ RL.flip), z, x, y, s); V = v0; }
float render_lane_hw(void) { return lane_view().hw; }
void render_stage(double perf, float pulse, float energy, int fx) { stage(perf, pulse, 0, (int)(perf / 2400) % 5, fx, energy); }
static unsigned flick_hash(unsigned x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
static int lastMult = 1; static double multAt = -1e9;
void render_game(gh3_run *r, double perf, const hud_info *hi) {
    theme_tick(); double t = r ? r->now : 0, L = r ? r->lookahead : 1500; int sp = r && r->spOn, lefty = hi ? hi->lefty : RL.on && SET.lefty;
    view_t V0 = V; V = lane_view();
    float pulse = r && r->hasLastBeat ? 1 - (float)fmod(r->lastBeat, 1.0) : 0;
    int mult = r ? game_mult(r) : 1; if (mult != lastMult) { if (mult > lastMult) multAt = perf; lastMult = mult; }
    float energy = r ? (float)fmin(1, r->streak / 40.0) : 0;
    if (!RL.on) stage(perf, pulse, sp, mult - 1, !hi || hi->stageFx, energy);
    if (!RL.on && hi && hi->bgDim > 0) gfx_rect(0, 0, V.W, V.H, RGBA(8, 6, 12, (int)(hi->bgDim * 1.8)));
    const theme_t *th = &TH[theme()];
    if (RL.on) gfx_blit(back(theme(), sp ? 1 : 0), 0, 0, (int)V.W, (int)V.H, V.cx - V0.cx * RL.q, RL.dy, V.W * RL.q, V.H, RGB(255, 255, 255), 1);
    else gfx_blit(back(theme(), sp ? 1 : 0), 0, 0, (int)V.W, (int)V.H, 0, 0, V.W, V.H, RGB(255, 255, 255), 1);
    float s;
    /* beat lines */
    if (r) { const gh3_bar *bars = r->chart->bars; int nb = r->chart->nbars;
        for (int i = lower_bound_bars(bars, nb, t + Z_NEAR * L); i < nb && bars[i].t <= t + L; i++) {
            float z = (float)((bars[i].t - t) / L), px, py, qx, qy, ps; if (z < Z_NEAR) continue;
            proj(-1, z, &px, &py, &ps); proj(1, z, &qx, &qy, &s);
            float lw = (bars[i].strong ? 3.0f : 1.6f) * ps; if (lw < 1) lw = 1;
            if (orig()) { orig_fretbar(px, qx, py, ps, bars[i].strong, fade(z)); continue; }
            gfx_rect(px, py - lw / 2, qx - px, lw, fb_alpha(C_INK, (bars[i].strong ? 0.5f : 0.2f) * fade(z))); } }
    /* fret targets */
    float tgtW = V.hw * (theme() >= 1 ? 0.4f : 0.36f);
    for (int f = 0; f < 5; f++) { float px, py; proj(laneX(f, lefty ^ RL.flip), 0, &px, &py, &s); int down = r && (r->held & (1 << f));
        int popped = r && ((perf - r->flash[f] >= 0 && perf - r->flash[f] < 85) || (r->sus && (r->sus->mask & (1 << f)))); target(th, f, px, py, tgtW, down, popped); }
    if (r) {
        const gh3_note *notes = r->notes; int n = r->nnotes;
        int i0 = lower_bound_notes(notes, n, t + Z_NEAR * L - 40000), i1 = i0; while (i1 < n && notes[i1].t <= t + L) i1++;
        /* sustain tails: stacked slices, a bright core when held */
        for (int i = i0; i < i1; i++) { const gh3_note *nt = &notes[i]; if (!nt->sus) continue;
            double end = nt->t + nt->sus; if (end < t + Z_NEAR * L) continue;
            int active = r->sus == nt; if (nt->hit && !active && !nt->dropped) continue;
            float z0 = (float)fmax(Z_NEAR, ((active ? fmax(t, nt->t) : nt->t) - t) / L), z1 = (float)fmin(1, (end - t) / L); if (z1 <= z0) continue;
            int dead = nt->missed || nt->dropped; float w = active ? 0.065f : 0.045f;
            float wob = active ? (float)r->whammy : 0;   /* the tail waves while the whammy bends it, as in the PS2 game */
            for (int f = 0; f < 5; f++) if (nt->mask & (1 << f)) { float x = laneX(f, lefty ^ !!(nt->bflags & BN_FLIP));
                uint32_t c = dead ? RGBA(120, 114, 134, 128) : fb_alpha(sp ? C_SP : FRET_COLORS[f], (active ? 1 : 0.85f) * fade(z0 * 0.6f));
                float y0, y1, xx, ss; proj(x, z0, &xx, &y0, &ss); proj(x, z1, &xx, &y1, &ss); int slices = (int)((y0 - y1) / 6) + 1; if (slices > 40) slices = 40;
                for (int k = 0; k < slices; k++) { float za = z0 + (z1 - z0) * k / slices, zb = z0 + (z1 - z0) * (k + 1) / slices, cxp, ya, yb, sa;
                    proj(x, za, &cxp, &ya, &sa); proj(x, zb, &cxp, &yb, &ss); float hw = w * V.hw * sa;
                    if (wob > 0.02f) cxp += sinf(za * 38.0f - (float)perf / 45.0f) * wob * V.hw * 0.05f * sa;
                    if (orig()) { orig_tail_slice(f, sp, dead, cxp, yb, ya, hw * 2); if (k == slices - 1) orig_tail_cap(f, sp, dead, cxp, yb, hw * 2); continue; }
                    gfx_rect(cxp - hw, yb, hw * 2, ya - yb + 1, c);
                    if (active) gfx_rect(cxp - hw * 0.35f, yb, hw * 0.7f, ya - yb + 1, RGBA(255, 255, 255, 150 + (int)(60 * pulse))); } } }
        /* gems, far to near */
        for (int i = i1 - 1; i >= i0; i--) { const gh3_note *nt = &notes[i]; if (nt->hit) continue;
            float z = (float)((nt->t - t) / L); if (z < Z_NEAR) continue;
            const gh3_phrase *ph = nt->star >= 0 ? &r->chart->phrases[nt->star] : NULL; int star = ph && !ph->failed && !ph->done;
            float alpha = nt->missed ? 0.3f : fade(z); int kind = star ? 2 : nt->hopo ? 1 : 0, nl = lefty ^ !!(nt->bflags & BN_FLIP);
            if (RL.flick && !nt->missed && flick_hash((unsigned)(perf / 80) * 7919u + (unsigned)i) % 10 < (unsigned)RL.flick * 2) continue;   /* Amp Overload: the gems flicker */
            if (nt->n > 1) { float ax, bx, yy, ps; int lo = -1, hi2 = -1; for (int f = 0; f < 5; f++) if (nt->mask & (1 << f)) { if (lo < 0) lo = f; hi2 = f; }
                proj(laneX(nl ? hi2 : lo, nl), z, &ax, &yy, &ps); proj(laneX(nl ? lo : hi2, nl), z, &bx, &yy, &ps); gfx_rect(ax, yy - 2 * ps, bx - ax, 4 * ps + 1, fb_alpha(RGB(0xd0, 0xc8, 0xe0), alpha * 0.8f)); }
            for (int f = 0; f < 5; f++) if (nt->mask & (1 << f)) { float px, py, ps; proj(laneX(f, nl), z, &px, &py, &ps); gem(px, py, ps, nt->missed ? RGB(0x6d, 0x66, 0x7c) : (sp ? C_SP : FRET_COLORS[f]), kind, alpha, f, nt->missed, sp, perf); } }
        /* sparks fly off a fret the moment its note is hit */
        { static double seen[5], susAt[5]; for (int f = 0; f < 5; f++) { float px, py; proj(laneX(f, lefty ^ RL.flip), 0, &px, &py, &s); uint32_t col = sp ? C_SP : FRET_COLORS[f];
            if (r->flash[f] != seen[f]) { seen[f] = r->flash[f]; if (perf - r->flash[f] < 80 && perf >= r->flash[f]) fx_burst(px, py - tgtW * 0.15f, theme() >= 1 ? 12 : 8, col, tgtW * 7, 420, 0.55f); }
            /* a held sustain: a glow on the fret and a thin stream of sparks, as long as it is held */
            if (r->sus && (r->sus->mask & (1 << f))) { gfx_sprite *gl = art_get(ART_GLOW); float gw = tgtW * 1.7f, gh = gw * gfx_sprite_h(gl) / gfx_sprite_w(gl); gfx_blit(gl, 0, 0, gfx_sprite_w(gl), gfx_sprite_h(gl), px - gw / 2, py - gh / 2, gw, gh, col, 0.55f + 0.2f * (float)sin(perf / 70.0));
                if (perf - susAt[f] > 45 || perf < susAt[f]) { susAt[f] = perf; fx_burst(px, py - tgtW * 0.1f, 2, col, tgtW * 4.5f, 320, 0.8f); } } } }
        /* flames on hit; a held sustain keeps its frets burning */
        for (int f = 0; f < 5; f++) { float age = (float)((perf - r->flash[f]) / 260);
            if (orig()) { float px, py; proj(laneX(f, lefty ^ RL.flip), 0, &px, &py, &s); if (r->sus && (r->sus->mask & (1 << f))) orig_hold(px, py, V.hw, perf); if (age >= 0 && age < 1) orig_flame(px, py, V.hw, age, sp); continue; }
            if (r->sus && (r->sus->mask & (1 << f)) && (age < 0 || age >= 1)) age = 0.15f + 0.25f * (float)fmod(perf / 260.0, 1.0);
            if (age < 0 || age >= 1) continue;
            float px, py; proj(laneX(f, lefty ^ RL.flip), 0, &px, &py, &s); int fr = age < 0.3f ? 0 : age < 0.65f ? 1 : 2;
            gfx_sprite *fl = sp ? th->spflame[fr] : th->flame[fr]; float fw = tgtW * (theme() >= 1 ? 1.3f : 0.75f), fh = fw * gfx_sprite_h(fl) / gfx_sprite_w(fl);
            uint32_t tint = theme() >= 1 ? (sp ? C_SP : RGB(255, 214, 140)) : RGB(255, 255, 255);
            gfx_blit(fl, 0, 0, gfx_sprite_w(fl), gfx_sprite_h(fl), px - fw / 2, py - fh * (theme() >= 1 ? 0.62f : 0.85f) - age * tgtW * 0.3f, fw, fh, tint, 1 - age * 0.6f); }
        fx_draw(perf);
        float sa = (float)((perf - r->spFlash) / 500); if (sa >= 0 && sa < 1) gfx_rect(0, 0, V.W, V.H, fb_alpha(C_SP, 0.22f * (1 - sa)));
        float so = (float)((perf - r->spAt) / 320); if (so >= 0 && so < 1) gfx_rect(0, 0, V.W, V.H, fb_alpha(RGB(150, 238, 255), 0.4f * (1 - so) * (1 - so)));
        float ma = (float)((perf - r->missFlash) / 220); if (ma >= 0 && ma < 1 && !r->autoPlay) gfx_rect(0, 0, V.W, V.H, fb_alpha(C_BAD, 0.1f * (1 - ma)));
        if (t < 0 && hi && hi->showCountdown) { char b[16]; int n3 = (int)ceil(-t / 1000); snprintf(b, sizeof b, "%d", n3); float k = (float)fmod(-t, 1000) / 1000; int sc = 8 + (int)(3 * k); gfx_text_title((int)V.W / 2 - gfx_text_w(b, sc) / 2, (int)V.H / 2 - 50, sc, b, C_INK, C_SHADOW, 3); gfx_text_sh_center((int)V.W / 2, (int)V.H / 2 + 50, 2, "Prepare-se!", C_AMP, C_SHADOW); }
        /* HUD */
        if (hi) { int W = (int)V.W, H = (int)V.H; char b[64];
            uint32_t mc = sp ? C_SP : mult >= 4 ? RGB(0xf2, 0x8d, 0x2d) : mult == 3 ? RGB(0xf3, 0xd2, 0x3b) : mult == 2 ? RGB(0x31, 0xc4, 0x66) : C_INK;
            float bump = (float)((perf - multAt) / 350); float bs = bump >= 0 && bump < 1 ? 1 + 0.35f * (1 - bump) : 1;
            /* top left: the song */
            if (hi->title) gfx_text_sh(14, 10, 2, hi->title, C_INK, C_SHADOW);
            if (hi->artist) gfx_text_sh(14, 36, 1, hi->artist, C_MUTED, C_SHADOW);
            if (hi->section && *hi->section) { gfx_sprite_at(art_get(ART_NOTE), 14, 52, 1.0f / PXS * 1.0f, C_AMP, 1); gfx_text_sh(14 + 14, 50, 1, hi->section, C_AMP, C_SHADOW); }
            if (orig()) orig_hud(r, hi, perf, W, H, mult, sp, pulse); else {
            /* top right: score panel */
            snprintf(b, sizeof b, "%ld", (long)r->score); int sw = gfx_text_w(b, 3); int pw = (sw > 120 ? sw : 120) + 24;
            ui_panel(W - 14 - pw, 8, pw, 52, 0); gfx_text_sh_right(W - 26, 16, 3, b, C_INK, C_SHADOW); gfx_text_right(W - 26, 46, 1, "PONTOS", C_MUTED);
            /* bottom left: multiplier badge and streak */
            snprintf(b, sizeof b, "%d×", mult); int ms = (int)(5 * bs); ui_panel(14, H - 78, 92, 64, sp ? 2 : 0); gfx_text_title(22, H - 70 - (ms - 5) * 4, ms, b, mc, C_SHADOW, 3);
            snprintf(b, sizeof b, "%d seguidas", r->streak); gfx_text_sh(116, H - 36, 1, b, r->streak >= 10 ? C_AMP : C_MUTED, C_SHADOW);
            if (r->streak > 0) { int dots = r->streak % 10; if (mult >= 4 && !sp) dots = 10; for (int i = 0; i < 10; i++) gfx_rect(116 + i * 9, H - 48, 7, 7, i < dots ? mc : C_PANEL2); }
            /* bottom right: rock meter and star power */
            gfx_sprite *meter = art_get(ART_METER); int mw = gfx_sprite_w(meter), mh = gfx_sprite_h(meter); float mx = W - 14 - mw, my = H - 14 - mh;
            ui_panel(mx - 8, my - 34, mw + 16, mh + 48, 0);
            gfx_sprite_at(meter, mx, my, 1, RGB(255, 255, 255), 1);
            double h = r->health > 1 ? 1 : r->health < 0 ? 0 : r->health; float ang = (float)M_PI * (float)(1 - h); float ncx = mx + mw / 2.0f, ncy = my + mh - 2 * PXS;
            gfx_line(ncx, ncy, ncx + cosf(ang) * (mw * 0.42f), ncy - sinf(ang) * (mw * 0.42f), 2.0f * PXS, C_INK); gfx_rect(ncx - 2 * PXS, ncy - 2 * PXS, 4 * PXS, 4 * PXS, C_AMP);
            if (hi->noFail) gfx_text_right(W - 22, my - 30, 1, "SEM FALHAR", C_MUTED); else gfx_text_right(W - 22, my - 30, 1, "ROCK", h < 0.33 ? C_BAD : C_MUTED);
            if (h < 0.33 && !hi->noFail) { float k = 0.5f + 0.5f * sinf((float)perf / 90); gfx_rect(mx - 8, my - 34, mw + 16, mh + 48, fb_alpha(C_BAD, 0.12f + 0.18f * k));
                gfx_rect(0, 0, 6, V.H, fb_alpha(C_BAD, 0.35f * k)); gfx_rect(V.W - 6, 0, 6, V.H, fb_alpha(C_BAD, 0.35f * k)); }
            /* star power cells */
            gfx_sprite *bolt = art_get(ART_BOLT); gfx_sprite_at(bolt, mx - 2, my - 20, 0.75f, sp ? C_SP : RGB(255, 255, 255), 1);
            int cells = 8; float cw = (mw - 14) / (float)cells; float spv = (float)(r->sp < 0 ? 0 : r->sp > 1 ? 1 : r->sp);
            for (int i = 0; i < cells; i++) { float fill = spv * cells - i; int on = fill >= 1; float part = fill > 0 && fill < 1 ? fill : 0;
                gfx_rect(mx + 12 + i * cw, my - 18, cw - 2, 10, C_PANEL2);
                if (on || sp) gfx_rect(mx + 12 + i * cw, my - 18, cw - 2, 10, sp ? fb_alpha(C_SP, 0.6f + 0.4f * pulse) : (r->sp >= 0.5 ? fb_alpha(C_SP, 0.8f + 0.2f * sinf((float)perf / 120)) : C_SP));
                else if (part > 0) gfx_rect(mx + 12 + i * cw, my - 18, (cw - 2) * part, 10, fb_alpha(C_SP, 0.7f)); }
            gfx_rect(mx + 12 + 4 * cw - 2, my - 21, 2, 16, r->sp >= 0.5 ? C_INK : C_MUTED);   /* half: from here star power can be used */
            if (!sp && r->sp >= 0.5) { char sk[80]; snprintf(sk, sizeof sk, "STAR POWER: %s", hi->starKeys && *hi->starKeys ? hi->starKeys : "Select"); float k = 0.6f + 0.4f * sinf((float)perf / 110);
                int tw2 = gfx_text_w(sk, 1) + 16; ui_panel(W - 14 - tw2, my - 58, tw2, 20, UI_PANEL_SP); gfx_text_sh_right(W - 22, my - 53, 1, sk, fb_alpha(C_INK, k), C_SHADOW); }
            }
            /* messages in the middle */
            if (r->streakAt > -1e8 && perf - r->streakAt < 1500) { snprintf(b, sizeof b, "%d NOTAS SEGUIDAS!", r->streakN); gfx_text_title(W / 2 - gfx_text_w(b, 3) / 2, (int)(H * 0.24f), 3, b, C_AMP, C_SHADOW, 2); }
            else if (bump >= 0 && bump < 1 && mult > 1) { snprintf(b, sizeof b, "%d×!", mult); gfx_text_title(W / 2 - gfx_text_w(b, 4) / 2, (int)(H * 0.24f) - (int)(10 * bump), 4, b, mc, C_SHADOW, 2); }
            if (hi->fps > 0) { snprintf(b, sizeof b, "%d fps", hi->fps); gfx_text(W / 2 - 20, 6, 1, b, C_MUTED); }
        }
    }
    V = V0;
}

/* calibration: the highway with a yellow gem on every metronome click; a tap flashes the strike line and shows how far off it was */
void render_calib(double t, double period, double perf, int taps, double lastDiff, double lastTapPerf) {
    const theme_t *th = &TH[theme()]; double L = 1500; float s;
    stage(perf, 0.3f, 0, 2, SET.stageFx, 0.3f);
    gfx_rect(0, 0, V.W, V.H, RGBA(8, 6, 12, 120));
    gfx_blit(back(theme(), 0), 0, 0, (int)V.W, (int)V.H, 0, 0, V.W, V.H, RGB(255, 255, 255), 1);
    float tgtW = V.hw * (theme() >= 1 ? 0.4f : 0.36f);
    { float px, py; proj(laneX(2, 0), 0, &px, &py, &s); target(th, 2, px, py, tgtW, 0, 0); }
    for (long k = (long)floor((t + Z_NEAR * L) / period); k * period <= t + L; k++) { if (k < 0) continue; float z = (float)((k * period - t) / L); if (z < Z_NEAR) continue;
        float px, py, ps; proj(laneX(2, 0), z, &px, &py, &ps); gem(px, py, ps, k % 4 == 0 ? FRET_COLORS[4] : FRET_COLORS[2], 0, fade(z), k % 4 == 0 ? 4 : 2, 0, 0, perf); }
    float age = (float)((perf - lastTapPerf) / 300);
    if (taps > 0 && age >= 0 && age < 1) { float sx, sy2, ex, ey; proj(-1, 0, &sx, &sy2, &s); proj(1, 0, &ex, &ey, &s); uint32_t c = fabs(lastDiff) < 35 ? C_GOOD : fabs(lastDiff) < 80 ? C_AMP : C_BAD;
        gfx_rect(sx, sy2 - 3, ex - sx, 6, fb_alpha(c, 0.8f * (1 - age))); }
}
