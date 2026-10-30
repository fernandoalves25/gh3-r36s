/* Effects drawn over the game and the menus: the result stars (faceted gold, baked at load in a pixel style and a
 * smooth style), their reveal (pop, rays, sparkles) and the sparks that fly off a fret when a note is hit. */
#ifndef FX_H
#define FX_H
#include "gfx.h"
void fx_init(int px);                                  /* px: the art pixel size (art_px) */
gfx_sprite *fx_star(int lit, int smooth);              /* the star sprite: lit gold or an empty slot */
/* five stars centred on cx, `size` px each; revealT >= 0 animates them landing one by one (seconds since the reveal began) */
void fx_stars(int cx, int y, int n, int size, double revealT, int smooth);
void fx_burst(float x, float y, int count, uint32_t color, float speed, float lifeMs, float up);   /* sparks from a point */
void fx_draw(double perf);                             /* moves and draws the live sparks */
void fx_clear(void);
#endif
