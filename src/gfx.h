/* Drawing backend. Everything on screen is a rectangle or a sprite (a baked texture, drawn scaled with a tint
 * and an alpha), so the GPU does the work through SDL_Renderer (GLES2 on the R36S). The same calls have a
 * software implementation into an fb_t, used when there is no accelerated renderer and for headless frames. */
#ifndef GFX_H
#define GFX_H
#include <SDL.h>
#include "fb.h"
typedef struct gfx_sprite gfx_sprite;
int gfx_init(SDL_Renderer *ren, int W, int H);          /* ren == NULL: software into gfx_soft_fb() */
int gfx_is_soft(void);
fb_t *gfx_soft_fb(void);
gfx_sprite *gfx_bake(const fb_t *src, int linear);      /* makes a sprite from a software image */
void gfx_begin(void);
void gfx_end(void);
void gfx_rect(float x, float y, float w, float h, uint32_t c);
void gfx_blit(gfx_sprite *s, int sx, int sy, int sw, int sh, float dx, float dy, float dw, float dh, uint32_t tint, float alpha);
void gfx_sprite_draw(gfx_sprite *s, float cx, float cy, float scale, uint32_t tint, float alpha); /* centred, scaled */
int gfx_sprite_w(const gfx_sprite *s);
int gfx_sprite_h(const gfx_sprite *s);
int gfx_text_w(const char *s, int scale);
void gfx_text(int x, int y, int scale, const char *s, uint32_t c);
void gfx_text_center(int cx, int y, int scale, const char *s, uint32_t c);
void gfx_text_right(int rx, int y, int scale, const char *s, uint32_t c);
#endif
