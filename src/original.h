/* Theme 2 "Original": the game's own 2D art (gamearts.c) laid out like GH3's HUD and menus, at 640x480. render.c and
 * ui.c call in here when SET.theme == 2 and the art is there; when a piece has no texture, they draw as the HD theme. */
#ifndef ORIGINAL_H
#define ORIGINAL_H
#include "gfx.h"
#include "game.h"
#include "render.h"
int orig_ready(void);                  /* the art can be used: DATA/ZONES/global.pak.xen + .pab.xen were opened (tried once) */
void orig_tick(void);                  /* once a frame: lets go of everything baked when the theme is no longer this one */
typedef void (*orig_proj)(float xo, float z, float *x, float *y, float *s);
/* ---- gameplay (positions from render.c's perspective; hw is the highway's half width at the strike line) ---- */
int orig_bake_back(fb_t *f, orig_proj proj, float vy, float sy, float persp, float zNear, int sp);   /* the highway surface and rails; -1 without the texture */
void orig_gem(float x, float y, float s, float hw, int fret, int kind, int sp, int missed, float alpha, double perf);   /* kind 0 plain, 1 hopo, 2 star */
void orig_target(float x, float y, float hw, int fret, int held, int popped);
void orig_flame(float x, float y, float hw, float age, int sp);          /* age 0..1 since the hit */
void orig_hold(float x, float y, float hw, double perf);                 /* the fire on a fret while its sustain is held */
void orig_fretbar(float x0, float x1, float y, float s, int strong, float alpha);
void orig_tail_slice(int fret, int sp, int dead, float cx, float yTop, float yBot, float w);   /* one slice of a sustain tail */
void orig_tail_cap(int fret, int sp, int dead, float cx, float y, float w);                    /* its far end */
void orig_hud(const gh3_run *r, const hud_info *hi, double perf, int W, int H, int mult, int sp, float pulse);
/* ---- menus ---- */
void orig_backdrop(double perf, int W, int H);          /* the game's background for the screen being shown (A.screen) */
int orig_header(int W, const char *title, const char *subtitle);
int orig_panel(float x, float y, float w, float h, int style);
int orig_hints(int y, int W, const char *const *buttons, const char *const *texts, int n);
int orig_title(int x, int y, int scale, const char *s, uint32_t c, uint32_t edge);   /* a title in the game's font, centred on the pixel title's span */
#endif
