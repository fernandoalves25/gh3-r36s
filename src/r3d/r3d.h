/* The 3D stage (the gh3-3d build, -DGH3_3D): the game's own venues, read from ZONES/<zone>.pak.xen and drawn with raw
 * GLES2 calls into the SDL_Renderer's window each frame, before the 2D highway and HUD go on top. Nothing here is
 * compiled into the 2D build; render.c's stage() asks r3d_draw_stage() first and falls back to its own 2D stage when it
 * answers 0 (no GL renderer, no venue loaded, or the option is off). */
#ifndef R3D_H
#define R3D_H
#include <SDL.h>
int r3d_init(SDL_Renderer *ren);                 /* after gfx_init: 0 when the renderer is GL and the shaders are up (else the 2D stage is used) */
int r3d_ready(void);                             /* 1 when r3d_init succeeded */
void r3d_select(const char *zone);               /* the venue to show from now on ("z_dive"; "" or unknown: a default). Loads it if it is not the one loaded */
const char *r3d_zone(void);                      /* the venue loaded, "" when none */
int r3d_draw_stage(double perf, int sp, float energy);   /* draws the venue (1) or not (0): the caller then draws its 2D stage */
int r3d_test(const char *zone, const char *outdir, int W, int H);   /* --r3dtest: loads a zone in a hidden window, saves frames, prints counts and timings; the exit code */
extern int r3d_test_band; extern char r3d_test_song[64];   /* --r3dtest ZONE --band [--song NAME]: with the band (that song's: a boss song has the boss), pictures of each member and its costs */
void r3d_shutdown(void);
#endif
