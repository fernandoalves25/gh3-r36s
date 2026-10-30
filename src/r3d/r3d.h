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
extern int r3d_test_band; extern char r3d_test_song[64];   /* --r3dtest ZONE --band [--song NAME]: with the band (that song's: a boss song has the boss), pictures of each member and its costs */   /* the song also drives the light show frames (else the first song of the venue's set) */
void r3d_shutdown(void);

/* ---- for the parts drawn inside the venue's pass (the band, r3d_band.c; the light show, r3d_lights.c) ----
 * The pass: framebuffer bound (the venue's own, smaller one at Média/Baixa, else the window's), viewport set, depth test on
 * (LEQUAL), cleared; then the solid batches, then the parts' solid draws, then the see-through batches far to near.
 * r3d_view_now() is the camera of the frame being drawn (column-major, GL conventions; vp = proj * view), valid from the
 * start of the pass to the next frame. r3d_program() is the venue's shader: same attributes, uniforms and material
 * meaning in both; 0 is the one without the alpha test (no discard: Mali keeps its early depth and hidden-surface
 * removal), 1 the one with it (uAlphaRef > 0). Vertex layout of the venue: r3d_vtx, 48 bytes, arrays 0-5 (its own batches also
 * carry the light show's room lights in arrays 6 and 7, turned off again with aLW read as 0 before anything else is drawn).
 * Uniform meaning: uProj/uView the camera; uBoard 1 = a card that faces the camera about aCtr; uAnim (cols, rows, fps, first)
 * a picture of frames; uWib/uWibU/uWibV/uWibR a scrolling, swaying picture; uMap the picture (unit 0) and uSmap the baked
 * light map on aUv2 (unit 1, r3d_program()->white when none); colour = map * uColour * (aLit*4 * smap + uAmb) * aCol.rgb * uTint
 * (under the light show, r3d_lights.c: plus its lights' changes from arrays 6-7, what passes the point's own light rolled off in aCol);
 * alpha = map.a * uColour.a * mix(1, aCol.a, uColA), discarded under uAlphaRef. */
typedef struct { float pos[3], ctr[3]; unsigned char col[4], lit[4]; float uv[2], uv2[2]; } r3d_vtx;
typedef struct { float view[16], proj[16], vp[16], cam[3], tint[3]; double t; int w, h; } r3d_view;   /* w, h: the size the pass draws at */
typedef struct { unsigned prog, white; int aPos, aCtr, aCol, aLit, aUv, aUv2;
    int uProj, uView, uBoard, uAnim, uWibU, uWibV, uWibR, uWib, uTime, uMap, uSmap, uColour, uAmb, uTint, uAlphaRef, uColA, uTile; } r3d_prog;
const r3d_view *r3d_view_now(void);
const r3d_prog *r3d_program(int alphaTest);
#endif
