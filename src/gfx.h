/* Drawing backend. Everything on screen is a rectangle or a sprite (a baked texture, drawn scaled with a tint
 * and an alpha), so the GPU does the work through SDL_Renderer (GLES2 on the R36S). The same calls have a
 * software implementation into an fb_t, used when there is no accelerated renderer and for headless frames. */
#ifndef GFX_H
#define GFX_H
#include <SDL.h>
#include "fb.h"
#include "font.h"
typedef struct gfx_sprite gfx_sprite;
int gfx_init(SDL_Renderer *ren, int W, int H);          /* ren == NULL: software into gfx_soft_fb() */
int gfx_is_soft(void);
fb_t *gfx_soft_fb(void);
gfx_sprite *gfx_bake(const fb_t *src, int linear);      /* makes a sprite from a software image */
gfx_sprite *gfx_bake_up(const fb_t *src, int up, int linear); /* the image enlarged `up` times (nearest) first: pixel art that scales smoothly */
void gfx_free_sprite(gfx_sprite *s);
void gfx_begin(void);
void gfx_end(void);
void gfx_rect(float x, float y, float w, float h, uint32_t c);
void gfx_frame(float x, float y, float w, float h, float t, uint32_t c);   /* rectangle outline, t thick */
void gfx_line(float x0, float y0, float x1, float y1, float t, uint32_t c); /* as a run of small squares */
void gfx_blit(gfx_sprite *s, int sx, int sy, int sw, int sh, float dx, float dy, float dw, float dh, uint32_t tint, float alpha);
void gfx_sprite_draw(gfx_sprite *s, float cx, float cy, float scale, uint32_t tint, float alpha); /* centred, scaled */
void gfx_sprite_at(gfx_sprite *s, float x, float y, float scale, uint32_t tint, float alpha);    /* top-left, scaled */
int gfx_sprite_w(const gfx_sprite *s);
int gfx_sprite_h(const gfx_sprite *s);
/* text (UTF-8, the pixel font of font.h): y is the top of the 11-row cell; a line is FONT_LINE * scale tall */
int gfx_text_w(const char *s, int scale);
void gfx_text(int x, int y, int scale, const char *s, uint32_t c);
void gfx_text_center(int cx, int y, int scale, const char *s, uint32_t c);
void gfx_text_right(int rx, int y, int scale, const char *s, uint32_t c);
void gfx_text_sh(int x, int y, int scale, const char *s, uint32_t c, uint32_t shadow);        /* with a drop shadow */
void gfx_text_sh_center(int cx, int y, int scale, const char *s, uint32_t c, uint32_t shadow);
void gfx_text_sh_right(int rx, int y, int scale, const char *s, uint32_t c, uint32_t shadow);
void gfx_text_title(int x, int y, int scale, const char *s, uint32_t c, uint32_t edge, int depth); /* outlined and extruded, for titles */
int gfx_text_box(int x, int y, int w, int scale, const char *s, uint32_t c);                   /* word-wrapped; returns the height used */
#endif
