/* Pixel art: hand-drawn sprites (character grids with a shared palette) and a few generated ones (crowd,
 * speakers, light beams, the rock meter). Art pixels are drawn at low resolution and enlarged, so the look
 * is chunky whatever the screen size. */
#ifndef ART_H
#define ART_H
#include "gfx.h"
enum {
    ART_GEM_BODY, ART_GEM_PLAIN, ART_GEM_HOPO, ART_GEM_STAR, ART_TGT_UP, ART_TGT_DOWN,
    ART_FLAME0, ART_FLAME1, ART_FLAME2, ART_SPFLAME0, ART_SPFLAME1, ART_SPFLAME2,
    ART_STAR, ART_STAR_EMPTY, ART_PICK, ART_BOLT, ART_CROWN, ART_LAMP, ART_SKULL, ART_NOTE,
    ART_SPEAKER, ART_TRUSS, ART_BEAM, ART_GLOW, ART_METER, ART_CROWD0, ART_CROWD1, ART_CROWD2, ART_CROWD3,
    ART_N
};
void art_init(int px);                     /* px = screen pixels per art pixel (2 at 640x480) */
int art_px(void);
gfx_sprite *art_get(int id);
uint32_t art_color(char c);                /* the shared palette */
gfx_sprite *art_bake_rows(const char *const *rows, int h, int up, int linear, const char *remap); /* remap: pairs "yS" = draw y as S */
void art_highway_tile(fb_t *out);          /* the highway surface texture, 32x32 art pixels (out must be initialised) */
#endif
