#include "fb.h"
#include "font.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

uint32_t fb_alpha(uint32_t c, float a) { if (a < 0) a = 0; if (a > 1) a = 1; uint32_t al = (uint32_t)((c >> 24) * a + 0.5f); return (c & 0xFFFFFF) | (al << 24); }
void fb_init(fb_t *f, int w, int h) { f->w = w; f->h = h; f->px = calloc((size_t)w * h, 4); }
void fb_free(fb_t *f) { free(f->px); f->px = NULL; }
void fb_clear(fb_t *f, uint32_t c) { for (int i = 0, n = f->w * f->h; i < n; i++) f->px[i] = c | 0xFF000000u; }
static inline void blend(uint32_t *d, uint32_t c) {
    uint32_t a = c >> 24; if (!a) return;
    if (a == 255) { *d = c; return; }
    uint32_t s = *d, ia = 255 - a;
    uint32_t r = (((c >> 16) & 255) * a + ((s >> 16) & 255) * ia) / 255, g = (((c >> 8) & 255) * a + ((s >> 8) & 255) * ia) / 255, b = ((c & 255) * a + (s & 255) * ia) / 255;
    *d = 0xFF000000u | (r << 16) | (g << 8) | b;
}
static void hspan(fb_t *f, int y, float xa, float xb, uint32_t c) {
    if (y < 0 || y >= f->h) return; if (xa > xb) { float t = xa; xa = xb; xb = t; }
    int x0 = (int)ceilf(xa - 0.5f), x1 = (int)floorf(xb - 0.5f); if (x0 < 0) x0 = 0; if (x1 >= f->w) x1 = f->w - 1;
    uint32_t *row = f->px + (size_t)y * f->w; for (int x = x0; x <= x1; x++) blend(row + x, c);
}
void fb_rect(fb_t *f, int x, int y, int w, int h, uint32_t c) { for (int yy = y; yy < y + h; yy++) hspan(f, yy, (float)x, (float)(x + w), c); }
void fb_rect_outline(fb_t *f, int x, int y, int w, int h, int t, uint32_t c) { fb_rect(f, x, y, w, t, c); fb_rect(f, x, y + h - t, w, t, c); fb_rect(f, x, y + t, t, h - 2 * t, c); fb_rect(f, x + w - t, y + t, t, h - 2 * t, c); }
void fb_vgrad(fb_t *f, int x, int y, int w, int h, uint32_t top, uint32_t bottom) {
    for (int i = 0; i < h; i++) { float t = h > 1 ? (float)i / (h - 1) : 0; uint32_t c = 0;
        for (int s = 0; s < 32; s += 8) { uint32_t a = (top >> s) & 255, b = (bottom >> s) & 255; c |= (uint32_t)(a + (b - (float)a) * t + 0.5f) << s; }
        hspan(f, y + i, (float)x, (float)(x + w), c); }
}
void fb_poly(fb_t *f, const float *xy, int n, uint32_t c) {
    if (n < 3) return; float ymin = 1e9f, ymax = -1e9f;
    for (int i = 0; i < n; i++) { if (xy[i * 2 + 1] < ymin) ymin = xy[i * 2 + 1]; if (xy[i * 2 + 1] > ymax) ymax = xy[i * 2 + 1]; }
    int y0 = (int)floorf(ymin), y1 = (int)ceilf(ymax); if (y0 < 0) y0 = 0; if (y1 >= f->h) y1 = f->h - 1;
    for (int y = y0; y <= y1; y++) {
        float yc = y + 0.5f, xs[32]; int k = 0;
        for (int i = 0; i < n && k < 32; i++) { int j = (i + 1) % n; float ya = xy[i * 2 + 1], yb = xy[j * 2 + 1];
            if ((yc >= ya && yc < yb) || (yc >= yb && yc < ya)) { float t = (yc - ya) / (yb - ya); xs[k++] = xy[i * 2] + (xy[j * 2] - xy[i * 2]) * t; } }
        for (int i = 1; i < k; i++) { float v = xs[i]; int j = i - 1; while (j >= 0 && xs[j] > v) { xs[j + 1] = xs[j]; j--; } xs[j + 1] = v; }
        for (int i = 0; i + 1 < k; i += 2) hspan(f, y, xs[i], xs[i + 1], c);
    }
}
void fb_ellipse(fb_t *f, float cx, float cy, float rx, float ry, uint32_t c) {
    if (rx < 0.3f) rx = 0.3f; if (ry < 0.3f) ry = 0.3f;
    int y0 = (int)floorf(cy - ry), y1 = (int)ceilf(cy + ry);
    for (int y = y0; y <= y1; y++) { float dy = (y + 0.5f - cy) / ry; if (dy * dy >= 1) continue; float hw = rx * sqrtf(1 - dy * dy); hspan(f, y, cx - hw, cx + hw, c); }
}
void fb_line(fb_t *f, float x0, float y0, float x1, float y1, float w, uint32_t c) {
    float dx = x1 - x0, dy = y1 - y0, L = sqrtf(dx * dx + dy * dy); if (L < 0.01f) return; float nx = -dy / L * w * 0.5f, ny = dx / L * w * 0.5f;
    if (w < 1.2f) { int steps = (int)L + 1; for (int i = 0; i <= steps; i++) { float t = (float)i / steps; int x = (int)(x0 + dx * t), y = (int)(y0 + dy * t); if (x >= 0 && y >= 0 && x < f->w && y < f->h) blend(f->px + (size_t)y * f->w + x, c); } return; }
    float q[8] = { x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny }; fb_poly(f, q, 4, c);
}

int fb_text_w(const char *s, int scale) { int n = font_strlen(s); return n ? (n * FONT_ADV - 1) * scale : 0; }
void fb_text(fb_t *f, int x, int y, int scale, const char *s, uint32_t c) {
    while (*s) { const uint16_t *g = font_glyph(font_next(&s));
        for (int col = 0; col < FONT_W; col++) for (int row = 0; row < FONT_H; row++) if (g[col] & (1 << row)) fb_rect(f, x + col * scale, y + row * scale, scale, scale, c);
        x += FONT_ADV * scale; }
}
void fb_text_center(fb_t *f, int cx, int y, int scale, const char *s, uint32_t c) { fb_text(f, cx - fb_text_w(s, scale) / 2, y, scale, s, c); }
void fb_text_right(fb_t *f, int rx, int y, int scale, const char *s, uint32_t c) { fb_text(f, rx - fb_text_w(s, scale), y, scale, s, c); }
