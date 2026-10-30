/* The game's own 2D art, read from DATA/ZONES/global.pak.xen + global.pab.xen: the .img pictures (DDS textures: the
 * HUD, menu backgrounds, posters), the .tex banks with the highway's textures (gems, fret buttons, flames, named by the
 * materials of the .scn files) and the .fnt bitmap fonts. Everything is read lazily, one entry at a time from the pab,
 * decoded (DXT1 / DXT3 / DXT5 / 32-bit) and baked as a sprite; a cache keeps what was asked for until ga_release(). */
#ifndef GAMEARTS_H
#define GAMEARTS_H
#include "gfx.h"
int ga_open(const char *pakPath, const char *pabPath);   /* 0 when the pak index was read; the pab stays on disk */
int ga_ok(void);                                         /* an index is open */
void ga_release(void);                                   /* frees every sprite, font and bank held (a theme change) */
size_t ga_memory(void);                                  /* bytes of decoded pixels baked so far (what the GPU holds), for the log */
/* pictures (.img) by the name the game's scripts use ("menu_main_bg"); NULL when missing. Decoded once, cached. */
gfx_sprite *ga_img(const char *name);
int ga_img_fb(const char *name, fb_t *out);              /* the decoded pixels (fb_init'ed here; caller frees); 0 when fine */
gfx_sprite *ga_img_key(uint32_t key);                    /* a picture by its name's key, for those the scripts name only by key (the character hub's frame) */
int ga_img_key_fb(uint32_t key, fb_t *out);
/* highway textures by material name ("sys_Gem2D_Green"): from the richest .tex bank through the .scn files */
gfx_sprite *ga_mat(const char *material);
int ga_mat_fb(const char *material, fb_t *out);
int ga_tex_fb(uint32_t key, fb_t *out);                  /* a texture of the bank by its key */
int ga_tex_biggest(int w, int h, uint32_t *key);         /* the largest texture of w x h (the default highway is 512 x 1024) */
/* fonts (.fnt): white glyphs on a sheet, drawn tinted. y is the top of the line; size px tall (the font's own size scaled) */
typedef struct ga_font ga_font;
ga_font *ga_font_get(const char *name);
ga_font *ga_font_down(const char *name, int down);       /* the same font on a sheet shrunk `down` times (1..8): for text drawn much smaller than its size */
int ga_has(const ga_font *f, int code);                  /* 1 the font has code's glyph, 2 it puts it together (an accent it lacks over the plain letter), 0 neither */
float ga_char(const ga_font *f, float x, float base, float size, float sx, int code, uint32_t color);   /* one code point on baseline `base`, squeezed sx; its advance (alpha 0: measured only) */
float ga_font_size(const ga_font *f);                    /* the font's native size */
float ga_text_w(const ga_font *f, const char *s, float size);
void ga_text(const ga_font *f, float x, float y, float size, const char *s, uint32_t color);
void ga_text_center(const ga_font *f, float cx, float y, float size, const char *s, uint32_t color);
void ga_text_right(const ga_font *f, float rx, float y, float size, const char *s, uint32_t color);
void ga_text_sh(const ga_font *f, float x, float y, float size, const char *s, uint32_t color, uint32_t shadow);   /* the game's 3,3 drop shadow (scaled) */
void ga_text_fit(const ga_font *f, float x, float y, float size, float maxW, const char *s, uint32_t color, int align);  /* squeezed to maxW (align 0 left, 1 centre, 2 right) */
int ga_glyph(const ga_font *f, int code, int *sx, int *sy, int *sw, int *sh, float *w, float *h, float *yo);   /* a glyph's box on the sheet (button pictures: 0xE000 + n) */
gfx_sprite *ga_font_sheet(const ga_font *f);
/* DDS -> ARGB (fb_init'ed into out); 0 when fine */
int ga_decode_dds(const uint8_t *b, size_t n, fb_t *out);
/* text stretched across by sx and laid along the direction (ca, sa) (cos, sin of a turn, clockwise on screen) from x, y,
 * each letter upright (the results' magazine sets its lines tilted) */
void ga_text_dir(const ga_font *f, float x, float y, float size, float sx, float ca, float sa, const char *s, uint32_t color);
#endif
