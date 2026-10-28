/* The highway and HUD after the browser port's own renderer, built for the GPU: the static parts (stage glow,
 * highway surface, lanes, rails, strike line) are baked once into two full-screen textures (normal and star
 * power); gems, fret targets, hit flashes and glyphs are baked sprites; beat lines, sustain slices, meters and
 * overlays are plain rectangles. A frame is then a few hundred RenderCopy/FillRect calls and no per-pixel work. */
#include "render.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

const uint32_t FRET_COLORS[5] = { RGB(0x31, 0xc4, 0x66), RGB(0xea, 0x4b, 0x40), RGB(0xf3, 0xd2, 0x3b), RGB(0x45, 0x94, 0xf4), RGB(0xf2, 0x8d, 0x2d) };
#define PERSP 2.5f
#define Z_NEAR (-0.1f)
typedef struct { float W, H, cx, sy, vy, hw; } view_t;
static view_t V;
static void view_set(int w, int h) {
    V.W = (float)w; V.H = (float)h; V.cx = V.W / 2; V.sy = V.H * 0.84f;
    float top = V.H * 0.1f, sFar = 1 / (1 + PERSP);
    V.vy = (top - V.sy * sFar) / (1 - sFar);
    V.hw = fminf(fminf(V.W * 0.4f, V.H * 0.4f), 340);
}
static void proj(float xo, float z, float *x, float *y, float *s) { float k = 1 / (1 + PERSP * z); *x = V.cx + xo * V.hw * k; *y = V.vy + (V.sy - V.vy) * k; *s = k; }
static float laneX(int f, int lefty) { return ((lefty ? 4 - f : f) - 2) * 0.4f; }
static float fade(float z) { float v = (1 - z) / 0.22f; return v < 0 ? 0 : v > 1 ? 1 : v; }

/* ---- baked sprites ---- */
#define GEM_R 48.0f           /* the gem sprite's body radius in pixels */
#define GEM_W 112
#define GEM_H 72
#define GEM_CY 34
#define TGT_R 40.0f           /* fret target radius in the sprite */
#define TGT_W 100
#define TGT_H 60
#define TGT_CY 28
enum { GK_PLAIN = 0, GK_HOPO, GK_STAR, GK_N };
static gfx_sprite *tgtSp[5][2];         /* fret targets, up/down */
static gfx_sprite *burstSp, *glowSp, *backSp[2];

static void star_poly(fb_t *fb, float x, float y, float r, uint32_t c) {
    float p[20]; for (int i = 0; i < 10; i++) { float a = -(float)M_PI / 2 + i * (float)M_PI / 5, rr = i % 2 ? r * 0.45f : r; p[i * 2] = x + cosf(a) * rr; p[i * 2 + 1] = y + sinf(a) * rr * 0.62f; }
    fb_poly(fb, p, 10, c);
}
static void clear0(fb_t *f) { memset(f->px, 0, (size_t)f->w * f->h * 4); }
/* A gem is two sprites: the body (white, tinted with the fret colour at draw time, plus its shadow, which is
 * near-black whatever the tint) and the markings (dark inner / white centre, star or hammer-on dot), untinted. */
static gfx_sprite *bake_gem_body(void) {
    fb_t f; fb_init(&f, GEM_W, GEM_H); clear0(&f); float x = GEM_W / 2.0f, y = GEM_CY, r = GEM_R, ry = r * 0.52f;
    fb_ellipse(&f, x, y + r * 0.12f, r * 1.06f, ry * 1.06f, RGB(10, 10, 10));
    fb_ellipse(&f, x, y, r, ry, RGB(255, 255, 255));
    gfx_sprite *s = gfx_bake(&f, 1); fb_free(&f); return s;
}
static gfx_sprite *bake_gem_marks(int kind) {
    fb_t f; fb_init(&f, GEM_W, GEM_H); clear0(&f); float x = GEM_W / 2.0f, y = GEM_CY, r = GEM_R, ry = r * 0.52f;
    if (kind == GK_STAR) star_poly(&f, x, y - r * 0.06f, r * 0.86f, RGB(255, 255, 255));
    else if (kind == GK_HOPO) fb_ellipse(&f, x, y - r * 0.08f, r * 0.62f, ry * 0.62f, RGB(255, 255, 255));
    else { fb_ellipse(&f, x, y - r * 0.04f, r * 0.66f, ry * 0.66f, RGB(0x16, 0x13, 0x1d)); fb_ellipse(&f, x, y - r * 0.12f, r * 0.4f, ry * 0.4f, RGB(255, 255, 255)); }
    gfx_sprite *s = gfx_bake(&f, 1); fb_free(&f); return s;
}
static gfx_sprite *gemBody, *gemMarks[GK_N];
static gfx_sprite *bake_target(int f, int down) {
    fb_t im; fb_init(&im, TGT_W, TGT_H); clear0(&im); float x = TGT_W / 2.0f, y = TGT_CY, tr = TGT_R;
    fb_ellipse(&im, x, y + tr * 0.1f, tr * 1.08f, tr * 0.56f, RGB(0x0b, 0x0a, 0x10));
    fb_ellipse(&im, x, y, tr, tr * 0.52f, FRET_COLORS[f]);
    fb_ellipse(&im, x, y - (down ? tr * 0.1f : 0), tr * 0.68f, tr * 0.36f, down ? RGB(255, 255, 255) : RGB(0x16, 0x13, 0x1d));
    if (!down) fb_ellipse(&im, x, y, tr * 0.36f, tr * 0.19f, fb_alpha(FRET_COLORS[f], 0.35f));
    gfx_sprite *s = gfx_bake(&im, 1); fb_free(&im); return s;
}
static gfx_sprite *bake_burst(void) { /* a soft white flash: concentric ellipses fading out, tinted at draw time */
    fb_t im; fb_init(&im, 96, 64); clear0(&im);
    for (int i = 12; i >= 1; i--) { float k = i / 12.0f; fb_ellipse(&im, 48, 32, 46 * k, 30 * k, fb_alpha(RGB(255, 255, 255), 0.09f + 0.06f * (1 - k))); }
    gfx_sprite *s = gfx_bake(&im, 1); fb_free(&im); return s;
}
static gfx_sprite *bake_glow(void) { fb_t im; fb_init(&im, 8, 64); for (int y = 0; y < 64; y++) for (int x = 0; x < 8; x++) im.px[y * 8 + x] = RGBA(255, 255, 255, (int)(255 * (1 - y / 63.0f))); gfx_sprite *s = gfx_bake(&im, 1); fb_free(&im); return s; }
/* the static highway: surface, lane lines, rails and strike line, on the stage colour */
static gfx_sprite *bake_back(int sp) {
    fb_t f; fb_init(&f, (int)V.W, (int)V.H); fb_clear(&f, C_STAGE); float s, ax, ay, bx, by, cx, cy, dx, dy;
    proj(-1, 1, &ax, &ay, &s); proj(1, 1, &bx, &by, &s); proj(1, Z_NEAR, &cx, &cy, &s); proj(-1, Z_NEAR, &dx, &dy, &s);
    /* the surface, darker and fainter far away: drawn in horizontal slices */
    for (int y = (int)ay; y <= (int)cy; y++) { float k = (y - ay) / (cy - ay); float lx = ax + (dx - ax) * k, rx = bx + (cx - bx) * k; uint32_t c = k < 0.22f ? fb_alpha(RGB(0x1a, 0x17, 0x22), k / 0.22f) : RGB((int)(0x1a + (0x26 - 0x1a) * k), (int)(0x17 + (0x21 - 0x17) * k), (int)(0x22 + (0x33 - 0x22) * k)); fb_rect(&f, (int)lx, y, (int)(rx - lx), 1, c); }
    for (int i = 0; i <= 5; i++) { float xo = -1 + i * 0.4f, px, py, qx, qy; int edge = i == 0 || i == 5; proj(xo, 1, &px, &py, &s); proj(xo, Z_NEAR, &qx, &qy, &s);
        uint32_t col = edge ? (sp ? C_SP : C_AMP) : C_INK; int steps = 40;
        for (int k = 0; k < steps; k++) { float t0 = (float)k / steps, t1 = (float)(k + 1) / steps; float a = t0 < 0.3f ? t0 / 0.3f * (edge ? 0.85f : 0.13f) : (edge ? 0.85f + 0.15f * (t0 - 0.3f) / 0.7f : 0.13f + 0.07f * (t0 - 0.3f) / 0.7f);
            fb_line(&f, px + (qx - px) * t0, py + (qy - py) * t0, px + (qx - px) * t1, py + (qy - py) * t1 + 0.6f, edge ? 3 : 1, fb_alpha(col, a)); } }
    { float sx, sy, ex, ey; proj(-1, 0, &sx, &sy, &s); proj(1, 0, &ex, &ey, &s); fb_line(&f, sx, sy, ex, ey, 2, fb_alpha(C_INK, 0.55f)); }
    gfx_sprite *bs = gfx_bake(&f, 0); fb_free(&f); return bs;
}
void render_init(int W, int H) {
    view_set(W, H);
    gemBody = bake_gem_body(); for (int k = 0; k < GK_N; k++) gemMarks[k] = bake_gem_marks(k);
    for (int f = 0; f < 5; f++) { tgtSp[f][0] = bake_target(f, 0); tgtSp[f][1] = bake_target(f, 1); }
    burstSp = bake_burst(); glowSp = bake_glow(); backSp[0] = bake_back(0); backSp[1] = bake_back(1);
}
static int lower_bound_notes(const gh3_note *a, int n, double t) { int lo = 0, hi = n; while (lo < hi) { int m = (lo + hi) >> 1; if (a[m].t < t) lo = m + 1; else hi = m; } return lo; }
static int lower_bound_bars(const gh3_bar *a, int n, double t) { int lo = 0, hi = n; while (lo < hi) { int m = (lo + hi) >> 1; if (a[m].t < t) lo = m + 1; else hi = m; } return lo; }
static void gem(float x, float y, float s, uint32_t color, int kind, float alpha) {
    float scale = V.hw * 0.155f * s / GEM_R; float w = GEM_W * scale, h = GEM_H * scale, dx = x - w / 2, dy = y - GEM_CY * scale;
    gfx_blit(gemBody, 0, 0, GEM_W, GEM_H, dx, dy, w, h, color, alpha);
    gfx_blit(gemMarks[kind], 0, 0, GEM_W, GEM_H, dx, dy, w, h, RGB(255, 255, 255), alpha);
}

void render_backdrop(double perf) {
    (void)perf; gfx_rect(0, 0, V.W, V.H, C_STAGE);
    gfx_blit(glowSp, 0, 0, 8, 64, 0, 0, V.W, V.H * 0.5f, C_AMP, 0.16f);
}

void render_game(gh3_run *r, double perf, const hud_info *hi) {
    double t = r ? r->now : 0, L = r ? r->lookahead : 1500; int sp = r && r->spOn, lefty = hi && hi->lefty;
    float pulse = r && r->hasLastBeat ? 1 - (float)fmod(r->lastBeat, 1.0) : 0;
    gfx_blit(backSp[sp ? 1 : 0], 0, 0, (int)V.W, (int)V.H, 0, 0, V.W, V.H, RGB(255, 255, 255), 1);
    gfx_blit(glowSp, 0, 0, 8, 64, 0, 0, V.W, V.H * 0.55f, sp ? C_SP : C_AMP, 0.10f + 0.07f * pulse);
    float s;
    /* beat lines: horizontal, so plain rectangles */
    if (r) { const gh3_bar *bars = r->chart->bars; int nb = r->chart->nbars;
        for (int i = lower_bound_bars(bars, nb, t + Z_NEAR * L); i < nb && bars[i].t <= t + L; i++) {
            float z = (float)((bars[i].t - t) / L), px, py, qx, qy, ps; if (z < Z_NEAR) continue;
            proj(-1, z, &px, &py, &ps); proj(1, z, &qx, &qy, &s);
            float lw = (bars[i].strong ? 2.6f : 1.4f) * ps; if (lw < 1) lw = 1;
            gfx_rect(px, py - lw / 2, qx - px, lw, fb_alpha(C_INK, (bars[i].strong ? 0.6f : 0.24f) * fade(z))); } }
    /* fret targets */
    float tr = V.hw * 0.17f, tscale = tr / TGT_R;
    for (int f = 0; f < 5; f++) { float px, py; proj(laneX(f, lefty), 0, &px, &py, &s); int down = r && (r->held & (1 << f));
        gfx_blit(tgtSp[f][down ? 1 : 0], 0, 0, TGT_W, TGT_H, px - TGT_W * tscale / 2, py - TGT_CY * tscale, TGT_W * tscale, TGT_H * tscale, RGB(255, 255, 255), 1); }
    if (r) {
        const gh3_note *notes = r->notes; int n = r->nnotes;
        int i0 = lower_bound_notes(notes, n, t + Z_NEAR * L - 40000), i1 = i0; while (i1 < n && notes[i1].t <= t + L) i1++;
        /* sustain tails, as stacked slices (each a rectangle), a few pixels tall near and thinner far away */
        for (int i = i0; i < i1; i++) { const gh3_note *nt = &notes[i]; if (!nt->sus) continue;
            double end = nt->t + nt->sus; if (end < t + Z_NEAR * L) continue;
            int active = r->sus == nt; if (nt->hit && !active && !nt->dropped) continue;
            float z0 = (float)fmax(Z_NEAR, ((active ? fmax(t, nt->t) : nt->t) - t) / L), z1 = (float)fmin(1, (end - t) / L); if (z1 <= z0) continue;
            int dead = nt->missed || nt->dropped; float w = active ? 0.06f * (1 + (float)r->whammy * 0.9f) : 0.04f;
            for (int f = 0; f < 5; f++) if (nt->mask & (1 << f)) { float x = laneX(f, lefty);
                uint32_t c = dead ? RGBA(120, 114, 134, 128) : fb_alpha(sp ? C_SP : FRET_COLORS[f], (active ? 1 : 0.8f) * fade(z0 * 0.6f));
                float y0, y1, xx, ss; proj(x, z0, &xx, &y0, &ss); proj(x, z1, &xx, &y1, &ss); int slices = (int)((y0 - y1) / 4) + 1; if (slices > 48) slices = 48;
                for (int k = 0; k < slices; k++) { float za = z0 + (z1 - z0) * k / slices, zb = z0 + (z1 - z0) * (k + 1) / slices, cxp, ya, yb, sa;
                    proj(x, za, &cxp, &ya, &sa); proj(x, zb, &cxp, &yb, &ss); float hw = w * V.hw * sa;
                    gfx_rect(cxp - hw, yb, hw * 2, ya - yb + 1, c);
                    if (active) gfx_rect(cxp - hw * 0.35f, yb, hw * 0.7f, ya - yb + 1, RGBA(255, 255, 255, 140)); } } }
        /* gems, far to near */
        for (int i = i1 - 1; i >= i0; i--) { const gh3_note *nt = &notes[i]; if (nt->hit) continue;
            float z = (float)((nt->t - t) / L); if (z < Z_NEAR) continue;
            const gh3_phrase *ph = nt->star >= 0 ? &r->chart->phrases[nt->star] : NULL; int star = ph && !ph->failed && !ph->done;
            float alpha = nt->missed ? 0.3f : fade(z); int kind = star ? GK_STAR : nt->hopo ? GK_HOPO : GK_PLAIN;
            for (int f = 0; f < 5; f++) if (nt->mask & (1 << f)) { float px, py, ps; proj(laneX(f, lefty), z, &px, &py, &ps); gem(px, py, ps, nt->missed ? RGB(0x6d, 0x66, 0x7c) : (sp ? C_SP : FRET_COLORS[f]), kind, alpha); } }
        /* hit bursts */
        for (int f = 0; f < 5; f++) { float age = (float)((perf - r->flash[f]) / 190); if (age < 0 || age >= 1) continue;
            float px, py; proj(laneX(f, lefty), 0, &px, &py, &s); float rr = tr * (0.9f + age * 1.5f);
            gfx_sprite_draw(burstSp, px, py - tr * (0.4f + age * 0.9f), rr * 2.4f / 96, sp ? C_SP : RGB(255, 214, 140), 1 - age); }
        float sa = (float)((perf - r->spFlash) / 500); if (sa >= 0 && sa < 1) gfx_rect(0, 0, V.W, V.H, fb_alpha(C_SP, 0.22f * (1 - sa)));
        float so = (float)((perf - r->spAt) / 320); if (so >= 0 && so < 1) gfx_rect(0, 0, V.W, V.H, fb_alpha(RGB(150, 238, 255), 0.4f * (1 - so) * (1 - so)));
        float ma = (float)((perf - r->missFlash) / 220); if (ma >= 0 && ma < 1 && !r->autoPlay) gfx_rect(0, 0, V.W, V.H, fb_alpha(C_BAD, 0.1f * (1 - ma)));
        if (t < 0 && hi && hi->showCountdown) { char b[16]; snprintf(b, sizeof b, "%d", (int)ceil(-t / 1000)); gfx_text_center((int)V.W / 2, (int)V.H / 2 - 40, 8, b, fb_alpha(C_INK, 0.9f)); }
        /* HUD */
        if (hi) { int W = (int)V.W, H = (int)V.H;
            if (hi->title) gfx_text(14, 10, 2, hi->title, C_INK);
            if (hi->artist) gfx_text(14, 28, 1, hi->artist, C_MUTED);
            if (hi->section && *hi->section) gfx_text(14, 40, 1, hi->section, C_AMP);
            char b[40]; snprintf(b, sizeof b, "%ld", (long)r->score); gfx_text_right(W - 14, 10, 3, b, C_INK); gfx_text_right(W - 14, 34, 1, "SCORE", C_MUTED);
            snprintf(b, sizeof b, "%dX", game_mult(r)); gfx_text(14, H - 60, 5, b, sp ? C_SP : C_INK);
            snprintf(b, sizeof b, "%d NOTE STREAK", r->streak); gfx_text(14, H - 22, 1, b, C_MUTED);
            int mw = 150, mx = W - 14 - mw;
            gfx_text(mx, H - 62, 1, "ROCK", C_MUTED);
            gfx_rect(mx, H - 52, mw, 9, C_PANEL2);
            double h = r->health > 1 ? 1 : r->health; uint32_t hc = h < 0.33 ? C_BAD : h < 0.66 ? RGB(0xf3, 0xd2, 0x3b) : RGB(0x31, 0xc4, 0x66);
            gfx_rect(mx + 1, H - 51, (float)((mw - 2) * h), 7, hc);
            gfx_text(mx, H - 36, 1, "STAR POWER", C_MUTED);
            gfx_rect(mx, H - 26, mw, 9, C_PANEL2);
            gfx_rect(mx + 1, H - 25, (float)((mw - 2) * (r->sp < 0 ? 0 : r->sp > 1 ? 1 : r->sp)), 7, C_SP);
            if (!sp && r->sp >= 0.5) gfx_text_right(W - 14, H - 14, 1, "READY: PRESS STAR POWER", C_SP);
            if (r->streakAt > -1e8 && perf - r->streakAt < 1500) { snprintf(b, sizeof b, "%d NOTE STREAK!", r->streakN); gfx_text_center(W / 2, (int)(H * 0.22f), 3, b, fb_alpha(C_AMP, (float)(1 - (perf - r->streakAt) / 1500))); }
        }
    }
}
