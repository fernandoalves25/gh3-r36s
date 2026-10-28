#include "gfx.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct gfx_sprite { SDL_Texture *tex; fb_t img; int w, h; };
static SDL_Renderer *R; static fb_t soft; static int W, H;
static gfx_sprite *font;

int gfx_is_soft(void) { return R == NULL; }
fb_t *gfx_soft_fb(void) { return &soft; }
int gfx_sprite_w(const gfx_sprite *s) { return s->w; }
int gfx_sprite_h(const gfx_sprite *s) { return s->h; }

gfx_sprite *gfx_bake(const fb_t *src, int linear) {
    gfx_sprite *s = calloc(1, sizeof *s); s->w = src->w; s->h = src->h;
    if (R) {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, linear ? "1" : "0");
        s->tex = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, src->w, src->h);
        /* straight alpha in, blended by the renderer */
        SDL_UpdateTexture(s->tex, NULL, src->px, src->w * 4);
        SDL_SetTextureBlendMode(s->tex, SDL_BLENDMODE_BLEND);
    } else { fb_init(&s->img, src->w, src->h); memcpy(s->img.px, src->px, (size_t)src->w * src->h * 4); }
    return s;
}
/* the 5x7 font as one atlas: 64 glyphs, 6 px apart, white on transparent */
extern void fb_text(fb_t *f, int x, int y, int scale, const char *s, uint32_t c);
static void bake_font(void) {
    fb_t a; fb_init(&a, 64 * 6, 7); for (int i = 0; i < a.w * a.h; i++) a.px[i] = 0;
    for (int g = 0; g < 64; g++) { char s[2] = { (char)(32 + g), 0 }; fb_text(&a, g * 6, 0, 1, s, RGB(255, 255, 255)); }
    font = gfx_bake(&a, 0); fb_free(&a);
}
int gfx_init(SDL_Renderer *ren, int w, int h) {
    R = ren; W = w; H = h;
    if (!R) fb_init(&soft, w, h); else { SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND); }
    bake_font(); return 0;
}
void gfx_begin(void) { if (R) { SDL_SetRenderDrawColor(R, 18, 16, 24, 255); SDL_RenderClear(R); } else fb_clear(&soft, RGB(18, 16, 24)); }
void gfx_end(void) { if (R) SDL_RenderPresent(R); }

void gfx_rect(float x, float y, float w, float h, uint32_t c) {
    if (w <= 0 || h <= 0 || !(c >> 24)) return;
    if (R) { SDL_Rect r = { (int)floorf(x + 0.5f), (int)floorf(y + 0.5f), (int)(w + 0.5f), (int)(h + 0.5f) }; if (r.w < 1) r.w = 1; if (r.h < 1) r.h = 1;
        SDL_SetRenderDrawColor(R, (c >> 16) & 255, (c >> 8) & 255, c & 255, c >> 24); SDL_RenderFillRect(R, &r); }
    else fb_rect(&soft, (int)floorf(x + 0.5f), (int)floorf(y + 0.5f), (int)(w + 0.5f) > 0 ? (int)(w + 0.5f) : 1, (int)(h + 0.5f) > 0 ? (int)(h + 0.5f) : 1, c);
}
static inline uint32_t mulc(uint32_t a, uint32_t b) { return (a * b + 127) / 255; }
void gfx_blit(gfx_sprite *s, int sx, int sy, int sw, int sh, float dx, float dy, float dw, float dh, uint32_t tint, float alpha) {
    if (alpha <= 0.002f || dw < 0.5f || dh < 0.5f) return; if (alpha > 1) alpha = 1;
    if (R) {
        SDL_Rect src = { sx, sy, sw, sh }, dst = { (int)floorf(dx + 0.5f), (int)floorf(dy + 0.5f), (int)(dw + 0.5f), (int)(dh + 0.5f) }; if (dst.w < 1) dst.w = 1; if (dst.h < 1) dst.h = 1;
        SDL_SetTextureColorMod(s->tex, (tint >> 16) & 255, (tint >> 8) & 255, tint & 255); SDL_SetTextureAlphaMod(s->tex, (Uint8)(alpha * 255 + 0.5f));
        SDL_RenderCopy(R, s->tex, &src, &dst); return;
    }
    /* software: nearest-neighbour scaled blit with tint and alpha */
    int x0 = (int)floorf(dx + 0.5f), y0 = (int)floorf(dy + 0.5f), w = (int)(dw + 0.5f), h = (int)(dh + 0.5f); if (w < 1) w = 1; if (h < 1) h = 1;
    uint32_t tr = (tint >> 16) & 255, tg = (tint >> 8) & 255, tb = tint & 255, ta = (uint32_t)(alpha * 255 + 0.5f);
    for (int y = 0; y < h; y++) { int py = y0 + y; if (py < 0 || py >= soft.h) continue; int syy = sy + (int)((y + 0.5f) * sh / h); if (syy >= s->img.h) syy = s->img.h - 1;
        const uint32_t *srow = s->img.px + (size_t)syy * s->img.w; uint32_t *drow = soft.px + (size_t)py * soft.w;
        for (int x = 0; x < w; x++) { int px = x0 + x; if (px < 0 || px >= soft.w) continue; int sxx = sx + (int)((x + 0.5f) * sw / w); if (sxx >= s->img.w) sxx = s->img.w - 1;
            uint32_t c = srow[sxx], a = mulc(c >> 24, ta); if (!a) continue;
            uint32_t r = mulc((c >> 16) & 255, tr), g = mulc((c >> 8) & 255, tg), b = mulc(c & 255, tb);
            uint32_t d = drow[px], ia = 255 - a;
            drow[px] = 0xFF000000u | ((r * a + ((d >> 16) & 255) * ia) / 255) << 16 | ((g * a + ((d >> 8) & 255) * ia) / 255) << 8 | ((b * a + (d & 255) * ia) / 255); } }
}
void gfx_sprite_draw(gfx_sprite *s, float cx, float cy, float scale, uint32_t tint, float alpha) {
    float w = s->w * scale, h = s->h * scale; gfx_blit(s, 0, 0, s->w, s->h, cx - w / 2, cy - h / 2, w, h, tint, alpha);
}
static int glyph_of(unsigned char ch) { if (ch >= 'a' && ch <= 'z') ch = (unsigned char)(ch - 32); if (ch >= 128) return '?' - 32; if (ch < 32 || ch > 95) return 0; return ch - 32; }
int gfx_text_w(const char *s, int scale) { int n = (int)strlen(s); return n ? (n * 6 - 1) * scale : 0; }
void gfx_text(int x, int y, int scale, const char *s, uint32_t c) {
    float a = (c >> 24) / 255.0f; uint32_t tint = c & 0xFFFFFF;
    for (; *s; s++, x += 6 * scale) { int g = glyph_of((unsigned char)*s); if (!g) continue; gfx_blit(font, g * 6, 0, 5, 7, (float)x, (float)y, 5.0f * scale, 7.0f * scale, tint, a); }
}
void gfx_text_center(int cx, int y, int scale, const char *s, uint32_t c) { gfx_text(cx - gfx_text_w(s, scale) / 2, y, scale, s, c); }
void gfx_text_right(int rx, int y, int scale, const char *s, uint32_t c) { gfx_text(rx - gfx_text_w(s, scale), y, scale, s, c); }
