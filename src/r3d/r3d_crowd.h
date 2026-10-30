/* The crowd on the 3D stage (the gh3-3d build), as the game does it (docs/research/band.md section 7; the browser port's
 * crowdFigure / crowdUse / sceneVenue panels): the venue's .scn holds flat crowd CARDS (camera-facing quads, hundreds a
 * venue) whose material wears one of the textures listed in <zone>_crowd_models; the game paints a small skinned figure
 * (crowd_ped_*, crowd_pedf_*, crowd_prison_*: 28-bone gh3_ped skeleton, its crowd_profiles anim set from perm_anims)
 * into a 128x256 picture for each kind and every card of that kind shows it. Here the kinds are the 128x256 cells of one
 * atlas (4 across), each redrawn ~15 times a second (R3D_CROWD_HZ) outside the venue's pass, round robin: the most
 * overdue kinds on screen, at most two ~1k-point figures a frame (R3D_CROWD_PERFRAME), so no frame carries a spike; no
 * mipmaps (they looked the same at 480x360; R3D_CROWD_MIPS=1 builds them once a round, R3D_CROWD_SPREAD=0 is the first
 * scheme: every kind in one pass with a mipmap build, every 4th frame);
 * the cards are ordinary venue batches (r3d.c) that wear the atlas with their cell picked through the venue shader's
 * frames (uAnim). The figures play idle / starpower by the rock meter's mood (bad, normal, good) and the song's pace
 * (slow, med, fast), round robin with 0.3 s cross-fades; while the song has the lighters up (and the meter is not in the red) the
 * sets' lighter lists, as the game's crowd_startlighters loop does. They are lit as the game lights them: by the venue's
 * crowd-group lights alone (lightgroup $crowd: an ambient and one or two directional ones), as the light show has them now.
 * Each figure's frame holds the highest its set's animations take it (raised arms, 2.0-2.2 m): a card shorter than that
 * (z_prison's 1.87 m) shows the figure a little smaller, not cut off (the game's crowd camera is placed by the engine, hfov 20,
 * no framing in the data). Opções > Vídeo > Efeitos de palco off: no crowd. */
#ifndef R3D_CROWD_H
#define R3D_CROWD_H
#include <stdint.h>
#include <stddef.h>
#include "r3d_zone.h"
int r3d_crowd_init(void);                 /* after the GL entry points are loaded: the figures' shader; 0 when fine */
/* a venue is being loaded (before its meshes): its crowd list, the figures' models and animations (kept for the next venue), the crowd's
 * lights and the lighters' sway (their objects' TriggerScripts, in the zone pak's scripts) */
void r3d_crowd_zone(const char *zone, const gh3_pakent *ents, int n, const r3d_venueqb *vq);
/* a crowd card mesh of the venue wearing this picture (pos/ctr: its points and their card centres as the .scn stores them,
 * the centre in the normals): the kind it shows, -1 when there is none (the card is then left out) */
int r3d_crowd_card(uint32_t texKey, const float *pos, const float *ctr, int nvert);
unsigned r3d_crowd_tex(int kind);         /* the picture the cards of a kind wear (the atlas: GL texture, alpha-tested) */
void r3d_crowd_cell(int kind, float anim[4]);   /* its cell, as the venue shader's uAnim (cols, rows, 0 frames a second, the cell) */
int r3d_crowd_shown(int kind);            /* draw this kind's cards now (Efeitos de palco on and the figure drawn once) */
void r3d_crowd_seen(int kind);            /* some card of this kind is on screen this frame: keep its figure moving */
void r3d_crowd_update(double perf, int sp);   /* before the venue's pass (outside it): animate, redraw the figures that are due; restores the framebuffer and viewport */
/* the lighters: <zone>_lighter_geo01..24, parts the venue keeps hidden until the song's _performance says
 * crowd_startlighters (crowd_stoplighters takes them down); the slow songs have them */
int r3d_crowd_lighter(uint32_t sector);   /* a part of the venue that is one of its lighter groups: 1 + which, else 0 */
int r3d_crowd_lighters(void);             /* are they up now (the song's clock, the meter out of the red; R3D_CROWD_LIGHTERS=1 forces them) */
int r3d_crowd_sway(int lighter, float off[3]);   /* where that group (1 + which) has swayed to now, as its script moves it; 0 when it stays put */
void r3d_crowd_select(void);              /* the song about to be played (A.selSong): its lighter cues, read now, under the loading screen */
void r3d_crowd_unload(void);              /* the venue goes: its kinds and pictures (the models and animations stay) */
void r3d_crowd_shutdown(void);
int r3d_crowd_test(const char *zone, const char *outdir);   /* --r3dtest: each kind in each mood into <zone>_crowd_<state>.bmp, the costs printed */
typedef struct { int kinds, models, clips, renders, draws, mipBuilds, verts, standIns; double ms, msMax, loadMs; size_t vram, animBytes; } r3d_crowdstats;   /* renders/ms: the last update's */
const r3d_crowdstats *r3d_crowd_stats(void);
#endif
