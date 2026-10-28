#ifndef RENDER_H
#define RENDER_H
#include "gfx.h"
#include "game.h"
typedef struct { const char *title, *artist, *section; int showCountdown; int lefty; } hud_info;
void render_init(int W, int H);                 /* bakes the sprites and the highway backgrounds (after gfx_init) */
void render_game(gh3_run *r, double perf, const hud_info *hi);
void render_backdrop(double perf);
#define C_INK RGB(240, 234, 223)
#define C_MUTED RGB(163, 156, 176)
#define C_AMP RGB(233, 176, 76)
#define C_SP RGB(98, 228, 255)
#define C_BAD RGB(239, 93, 80)
#define C_STAGE RGB(18, 16, 24)
#define C_PANEL RGB(27, 24, 35)
#define C_PANEL2 RGB(36, 32, 48)
#define C_LINE RGB(55, 49, 68)
extern const uint32_t FRET_COLORS[5];
#endif
