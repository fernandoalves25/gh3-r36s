#ifndef RENDER_H
#define RENDER_H
#include "gfx.h"
#include "art.h"
#include "game.h"
typedef struct { const char *title, *artist, *section; int showCountdown; int lefty; int stageFx; int bgDim; int fps; int noFail; const char *starKeys; } hud_info;
void render_calib(double t, double period, double perf, int taps, double lastDiff, double lastTapPerf); /* the calibration lane: a gem on every click */
void render_init(int W, int H);                 /* bakes the sprites and the highway backgrounds (after gfx_init and art_init) */
const char *render_theme_name(int theme);       /* themes: 0 pixel art, 1 HD (smooth); which one draws is SET.theme */
void render_game(gh3_run *r, double perf, const hud_info *hi);
void render_backdrop(double perf, int stageFx); /* the stage alone, for the menus */
void render_stars(int cx, int y, int n, int scale, double revealT);   /* n of 5 stars, revealed one by one when revealT >= 0 */
/* battles: with RL.on, render_game(r, perf, NULL) draws that run's highway alone, narrowed by q round x = cx, moved by
 * dx, dy (Amp Overload), its frets mirrored (flip: Lefty Flip) and its gems flickering (flick) */
typedef struct { int on; float cx, q, dx, dy; int flip, flick; } render_lane_t;
extern render_lane_t RL;
void render_lane_xy(int fret, float z, float *x, float *y, float *s);   /* a point of the highway RL places (z 0: the strike line) */
float render_lane_hw(void);                                             /* its half width at the strike line */
void render_stage(double perf, float pulse, float energy, int stageFx); /* the stage alone, undimmed */
int render_venue(double perf);   /* gh3-3d: the 3D venue and its band alone (1); 0, nothing drawn, when there is none (the 2D build) */
#define C_INK RGB(240, 234, 223)
#define C_MUTED RGB(163, 156, 176)
#define C_AMP RGB(233, 176, 76)
#define C_AMP_DARK RGB(120, 70, 20)
#define C_SP RGB(98, 228, 255)
#define C_BAD RGB(239, 93, 80)
#define C_GOOD RGB(0x31, 0xc4, 0x66)
#define C_STAGE RGB(18, 16, 24)
#define C_PANEL RGB(27, 24, 35)
#define C_PANEL2 RGB(36, 32, 48)
#define C_LINE RGB(55, 49, 68)
#define C_SHADOW RGB(8, 6, 12)
extern const uint32_t FRET_COLORS[5];
extern const char *FRET_NAMES[5];
#endif
