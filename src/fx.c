#include "fx.h"
#include <math.h>
#include <string.h>

static gfx_sprite *stars[2][2], *rays[2], *spark;   /* stars[smooth][lit] */

/* ---- the star: ten facets around the centre, each arm a light and a dark half lit from the upper left, a dark
 * outline, a glint; drawn by sampling (4x4 per pixel for the smooth one) so the edges come out clean ---- */
typedef struct { float x, y; } pt;
static int in_tri(float px, float py, pt a, pt b, pt c) {
    float d1 = (px - b.x) * (a.y - b.y) - (a.x - b.x) * (py - b.y), d2 = (px - c.x) * (b.y - c.y) - (b.x - c.x) * (py - c.y), d3 = (px - a.x) * (c.y - a.y) - (c.x - a.x) * (py - a.y);
    return !(((d1 < 0) || (d2 < 0) || (d3 < 0)) && ((d1 > 0) || (d2 > 0) || (d3 > 0)));
}
static uint32_t mix(uint32_t a, uint32_t b, float k) { if (k < 0) k = 0; if (k > 1) k = 1;
    int r = (int)(((a >> 16) & 255) + ((((b >> 16) & 255) - (float)((a >> 16) & 255)) * k)), g = (int)(((a >> 8) & 255) + ((((b >> 8) & 255) - (float)((a >> 8) & 255)) * k)), bl = (int)((a & 255) + (((b & 255) - (float)(a & 255)) * k));
    return RGB(r, g, bl); }
/* the colour of the star at a point, 0 outside: `grow` > 1 is the outline's larger star */
static uint32_t star_at(float x, float y, float cx, float cy, float R, int lit, int *inside) {
    pt C = { cx, cy }, O[5], I[5]; float ri = R * 0.5f;
    for (int i = 0; i < 5; i++) { float a = (float)(-M_PI / 2 + i * 2 * M_PI / 5), b = a + (float)(M_PI / 5); O[i].x = cx + cosf(a) * R; O[i].y = cy + sinf(a) * R; I[i].x = cx + cosf(b) * ri; I[i].y = cy + sinf(b) * ri; }
    for (int i = 0; i < 5; i++) for (int side = 0; side < 2; side++) { pt in = side ? I[i] : I[(i + 4) % 5];
        if (!in_tri(x, y, C, O[i], in)) continue; *inside = 1;
        /* the facet faces the direction between its tip and its inner corner; light comes from the upper left */
        float fx = (O[i].x + in.x) / 2 - cx, fy = (O[i].y + in.y) / 2 - cy, L = sqrtf(fx * fx + fy * fy); fx /= L; fy /= L;
        float b = 0.5f + 0.5f * (fx * -0.62f + fy * -0.78f); b = b * b;
        float v = (y - (cy - R)) / (2 * R);   /* a little darker toward the bottom */
        if (lit) return mix(mix(RGB(0xb8, 0x6a, 0x10), RGB(0xff, 0xf2, 0x9a), b), RGB(0xa8, 0x5a, 0x08), v * 0.35f);
        return mix(RGB(0x24, 0x21, 0x31), RGB(0x55, 0x50, 0x6c), b * 0.9f); }
    return 0;
}
static gfx_sprite *gen_star(int N, int ss, int lit, int up, int smooth) {
    fb_t f; fb_init(&f, N, N); float cx = N / 2.0f, cy = N / 2.0f + N * 0.04f, R = N * 0.44f, R2 = R + (smooth ? N * 0.045f : 1.4f);
    uint32_t edge = lit ? RGB(0x4a, 0x24, 0x04) : RGB(0x0c, 0x0a, 0x12);
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) { float r = 0, g = 0, b = 0, a = 0;
        for (int sy = 0; sy < ss; sy++) for (int sx = 0; sx < ss; sx++) { float px = x + (sx + 0.5f) / ss, py = y + (sy + 0.5f) / ss; int in = 0;
            uint32_t c = star_at(px, py, cx, cy, R, lit, &in); if (!in) { int o = 0; star_at(px, py, cx, cy + (R2 - R) * 0.15f, R2, lit, &o); if (!o) continue; c = edge; }
            r += (c >> 16) & 255; g += (c >> 8) & 255; b += c & 255; a += 1; }
        if (a > 0) f.px[(size_t)y * N + x] = RGBA((int)(r / a), (int)(g / a), (int)(b / a), (int)(255 * a / (ss * ss))); }
    if (lit) {   /* the glint on the upper-left arm */
        float gx = cx - R * 0.2f, gy = cy - R * 0.42f, gr = smooth ? N * 0.09f : 2.2f;
        for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) { float dx = fabsf(x + 0.5f - gx), dy = fabsf(y + 0.5f - gy); float k = 1 - (dx + dy) / gr; if (dx > 0.9f && dy > 0.9f && !smooth) continue;
            if (k <= 0) continue; uint32_t *p = &f.px[(size_t)y * N + x]; if (!(*p >> 24)) continue; *p = (*p & 0xff000000u) | (mix(*p, RGB(255, 255, 255), smooth ? k * k * 1.6f : 1) & 0xffffff); } }
    gfx_sprite *s = gfx_bake_up(&f, up, smooth); fb_free(&f); return s;
}
static gfx_sprite *gen_rays(int N, float phase) {   /* soft light rays fading outward, white (tinted when drawn) */
    fb_t f; fb_init(&f, N, N); float c = N / 2.0f;
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) { float dx = x + 0.5f - c, dy = y + 0.5f - c, d = sqrtf(dx * dx + dy * dy) / c; if (d >= 1) continue;
        float a = atan2f(dy, dx) + phase, k = powf(fmaxf(0, cosf(a * 6)), 6) * (1 - d) * (1 - d) + (1 - d) * (1 - d) * (1 - d) * 0.35f; if (k > 1) k = 1;
        f.px[(size_t)y * N + x] = RGBA(255, 255, 255, (int)(255 * k)); }
    gfx_sprite *s = gfx_bake(&f, 1); fb_free(&f); return s;
}
static gfx_sprite *gen_spark(int N) {   /* a bright core with a four-point twinkle */
    fb_t f; fb_init(&f, N, N); float c = N / 2.0f;
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) { float dx = fabsf(x + 0.5f - c) / c, dy = fabsf(y + 0.5f - c) / c, d = sqrtf(dx * dx + dy * dy);
        float k = fmaxf(0, 1 - d * 1.6f); k = k * k + fmaxf(0, 1 - dx * 8) * fmaxf(0, 1 - dy) * 0.7f + fmaxf(0, 1 - dy * 8) * fmaxf(0, 1 - dx) * 0.7f; if (k > 1) k = 1; if (k <= 0.02f) continue;
        f.px[(size_t)y * N + x] = RGBA(255, 255, 255, (int)(255 * k)); }
    gfx_sprite *s = gfx_bake(&f, 1); fb_free(&f); return s;
}
void fx_init(int px) {
    stars[0][1] = gen_star(24, 1, 1, px, 0); stars[0][0] = gen_star(24, 1, 0, px, 0);   /* pixel style: drawn at 24 art px, enlarged without blur */
    stars[1][1] = gen_star(96, 4, 1, 1, 1); stars[1][0] = gen_star(96, 4, 0, 1, 1);
    rays[0] = gen_rays(128, 0); rays[1] = gen_rays(128, (float)(M_PI / 12)); spark = gen_spark(16);
}
gfx_sprite *fx_star(int lit, int smooth) { return stars[smooth ? 1 : 0][lit ? 1 : 0]; }

/* ---- sparks ---- */
#define NP 192
typedef struct { float x, y, vx, vy, age, life, size; uint32_t c; int on; } part_t;
static part_t P[NP]; static int pNext; static double lastPerf = -1;
static unsigned seed = 12345u; static float frand(void) { seed = seed * 1664525u + 1013904223u; return ((seed >> 8) & 0xffff) / 65535.0f; }
void fx_burst(float x, float y, int count, uint32_t color, float speed, float lifeMs, float up) {
    for (int i = 0; i < count; i++) { part_t *p = &P[pNext]; pNext = (pNext + 1) % NP; float a = (float)(-M_PI / 2) + (frand() - 0.5f) * (float)(2 * M_PI) * (1 - up), v = speed * (0.4f + 0.6f * frand());
        p->x = x; p->y = y; p->vx = cosf(a) * v; p->vy = sinf(a) * v; p->age = 0; p->life = lifeMs * (0.6f + 0.4f * frand()); p->size = 0.6f + 0.6f * frand(); p->c = color; p->on = 1; }
}
void fx_clear(void) { memset(P, 0, sizeof P); lastPerf = -1; }
void fx_draw(double perf) {
    float dt = lastPerf < 0 || perf < lastPerf ? 0 : (float)(perf - lastPerf); if (dt > 50) dt = 50; lastPerf = perf; int sw = gfx_sprite_w(spark);
    for (int i = 0; i < NP; i++) { part_t *p = &P[i]; if (!p->on) continue; p->age += dt; if (p->age >= p->life) { p->on = 0; continue; }
        float s = dt / 1000; p->x += p->vx * s; p->y += p->vy * s; p->vy += 900 * s; p->vx *= 1 - 1.5f * s;   /* gravity and drag */
        float k = 1 - p->age / p->life, w = sw * p->size * (0.5f + 0.5f * k);
        gfx_blit(spark, 0, 0, sw, sw, p->x - w / 2, p->y - w / 2, w, w, p->c, k); gfx_blit(spark, 0, 0, sw, sw, p->x - w / 4, p->y - w / 4, w / 2, w / 2, RGB(255, 255, 255), k); }
}

/* ---- the five stars of the results ---- */
void fx_stars(int cx, int y, int n, int size, double T, int smooth) {
    gfx_sprite *lit = fx_star(1, smooth), *dim = fx_star(0, smooth); int sw = gfx_sprite_w(lit), rw = gfx_sprite_w(rays[0]);
    float step = size * 1.3f, x0 = cx - step * 2; static unsigned burst; static double lastT = 1e9;
    if (T < lastT) burst = 0; lastT = T;
    double all = T < 0 ? 99 : T - (n - 1) * 0.35 - 0.3;   /* time since the last star landed */
    for (int i = 0; i < 5; i++) { float sx = x0 + i * step, sy = y + size / 2.0f;
        gfx_blit(dim, 0, 0, sw, sw, sx - size / 2.0f, sy - size / 2.0f, (float)size, (float)size, RGB(255, 255, 255), 0.9f);
        if (i >= n) continue;
        double at = T < 0 ? 99 : T - i * 0.35; if (at < 0) continue;
        float k = at < 0.28 ? (float)(at / 0.28) : 1, pop = at < 0.28 ? 2.2f - 1.2f * (1 - (1 - k) * (1 - k)) : at < 0.45 ? 1 + 0.12f * (float)sin((at - 0.28) / 0.17 * M_PI) : 1;
        if (at >= 0.28 && !(burst & (1u << i))) { burst |= 1u << i; fx_burst(sx, sy, 18, RGB(255, 214, 90), size * 6.0f, 650, 0); }
        /* rays: a flash when the star lands, then a slow breathing glow behind every lit star */
        float flash = at < 0.28 ? 0 : (float)fmax(0, 1 - (at - 0.28) / 0.7), calm = 0.22f + 0.08f * (float)sin(T * 2.2 + i), ra = fmaxf(flash * 0.85f, calm), rs = size * (1.9f + flash * 1.2f);
        int rf = (int)(T * 1.6 + i) & 1; gfx_blit(rays[rf], 0, 0, rw, rw, sx - rs / 2, sy - rs / 2, rs, rs, RGB(255, 200, 80), ra);
        float w = size * pop; gfx_blit(lit, 0, 0, sw, sw, sx - w / 2, sy - w / 2, w, w, RGB(255, 255, 255), k);
        /* once all have landed, a shine runs across them now and then */
        if (all > 0) { double ph = fmod(all, 2.6) * 2.2 - i * 0.18; if (ph > 0 && ph < 0.5) gfx_blit(rays[0], 0, 0, rw, rw, sx - size * 0.6f, sy - size * 0.6f, size * 1.2f, size * 1.2f, RGB(255, 250, 220), (float)(0.7 * sin(ph / 0.5 * M_PI))); } }
    if (T >= 0) fx_draw(T * 1000);
}
