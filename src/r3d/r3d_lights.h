/* The venue's light show in the 3D stage (docs/research/stage.md section 3): a song's _lightshow_notes cues pick a mood,
 * step through that mood's snapshot presets (by the rock meter: poor / medium / good), wash the lights in one colour,
 * set the blend time, flash the strobe and fire the pyro; the venue's z_<v>_lightshow_mapping and z_<v>_snapshots say
 * how each light and each beam stands in each preset. Cheap on the Mali: each point of the venue keeps, from the load,
 * its baked light and the four room lights that can change it most and how much each one gives it (n.l, reach); the
 * shader adds what those lights have changed against the bake (uRL[32], one per room light), so each light keeps its
 * own colour, reach and side as the presets move it. The ambient gives way as the lights go up and what the show adds
 * past a point's own light rolls off (frame(), KNEE): the presets' colours, not a white room. The beams, the housings' glow and the pyro
 * sparks are additive quads in one stream, drawn after the venue; the band takes the show's band lights
 * (r3d_lights_get). All of it off (the baked look) when "Efeitos de palco" is off. */
#ifndef R3D_LIGHTS_H
#define R3D_LIGHTS_H
#include "r3d_gl.h"
#include "r3d_zone.h"
void r3d_lights_load(const char *zone, const gh3_pakent *ents, int n, const r3d_venueqb *vq);   /* after the venue is read: its presets, mapping, pyro spots */
void r3d_lights_unload(void);
/* the room lights the venue is baked with, in r3d.c's order (its slots, up to 32: the first not kept for the band or the crowd, else the default
 * daylight): writes each one's peak, the most intensity x colour the venue's presets ever give it (its own at least), so the bake keeps for each
 * point the lights that can matter there. After r3d_lights_load */
void r3d_lights_room(const r3d_light *l, int n, float *peak);
void r3d_lights_program(int slot, GLuint prog);   /* a venue shader linked (slot 0 plain, 1 alpha-tested): where its light uniforms are (uDyn uRL[32], uAmb) */
void r3d_lights_update(double t);             /* once a frame (t: wall seconds): the song's cues up to now, the rock meter, the cross-fades, the sparks */
void r3d_lights_uniforms(int slot);           /* after glUseProgram(that venue shader), once a frame for each */
void r3d_lights_batch(int slot, int lit, const float amb[3]);   /* per batch, the program it is drawn with in use: lit = the batch takes the room's lights (not unlit / ambient-only); sets uDyn and uAmb */
void r3d_lights_draw(const float *vp, const float *view);   /* after the venue: beams, glows, sparks (additive, depth tested, no depth write) */
void r3d_lights_shutdown(void);
/* --r3dtest: the show driven by a song's cues at song time ms with this rock meter (0..2) instead of the game; NULL gives the game back */
void r3d_lights_sim(const gh3_songdata *sd, double ms, double health);
const char *r3d_lights_info(void);            /* mood, level, preset, colour, strobe: for the log */
/* one of the venue's lights as the show has it now, by its NAME key: r g b intensity far near, the strobe's flash in the intensity of all but
 * the ambient ones. 0 (v untouched) when the show is off or the venue has no such light: its own values then. The band lights itself with these */
int r3d_lights_get(uint32_t key, float v[6]);
#endif
