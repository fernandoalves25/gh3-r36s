/* A small software rasterizer: ARGB8888 framebuffer, alpha-blended polygons, ellipses and a 5x7 font. */
#ifndef FB_H
#define FB_H
#include <stdint.h>
typedef struct { int w, h; uint32_t *px; } fb_t;
#define RGBA(r, g, b, a) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define RGB(r, g, b) RGBA(r, g, b, 255)
uint32_t fb_alpha(uint32_t c, float a);            /* scale the colour's alpha by a (0..1) */
void fb_init(fb_t *f, int w, int h);
void fb_free(fb_t *f);
void fb_clear(fb_t *f, uint32_t c);
void fb_rect(fb_t *f, int x, int y, int w, int h, uint32_t c);
void fb_rect_outline(fb_t *f, int x, int y, int w, int h, int t, uint32_t c);
void fb_poly(fb_t *f, const float *xy, int n, uint32_t c);
void fb_ellipse(fb_t *f, float cx, float cy, float rx, float ry, uint32_t c);
void fb_line(fb_t *f, float x0, float y0, float x1, float y1, float w, uint32_t c);
void fb_vgrad(fb_t *f, int x, int y, int w, int h, uint32_t top, uint32_t bottom);
int fb_text_w(const char *s, int scale);
void fb_text(fb_t *f, int x, int y, int scale, const char *s, uint32_t c);
void fb_text_center(fb_t *f, int cx, int y, int scale, const char *s, uint32_t c);
void fb_text_right(fb_t *f, int rx, int y, int scale, const char *s, uint32_t c);
#endif
