/* The venue's light show in the 3D stage (docs/research/stage.md section 3): a song's _lightshow_notes cues pick a mood,
 * step through that mood's snapshot presets (by the rock meter: poor / medium / good), wash the lights in one colour,
 * set the blend time, toggle the strobe and fire the pyro; the venue's z_<v>_lightshow_mapping and z_<v>_snapshots say
 * how each light and each beam stands in each preset. Cheap on the Mali: the venue keeps its light baked per vertex at
 * load, scaled by how the room's lights stand now against their base (uBake), plus the four lights that changed most
 * applied in the vertex shader (uLP/uLC/uLD); the beams, the housings' glow and the pyro sparks are additive quads in
 * one stream, drawn after the venue. All of it off (the baked look as before) when "Efeitos de palco" is off. */
#ifndef R3D_LIGHTS_H
#define R3D_LIGHTS_H
#include "r3d_gl.h"
#include "r3d_zone.h"
void r3d_lights_load(const char *zone, const gh3_pakent *ents, int n, const r3d_venueqb *vq);   /* after the venue is read: its presets, mapping, pyro spots */
void r3d_lights_unload(void);
void r3d_lights_program(GLuint prog);         /* the venue shader linked: where its light uniforms are (uBake uDyn uLP[4] uLC[4] uLD[4], uAmb) */
void r3d_lights_update(double t);             /* once a frame (t: wall seconds): the song's cues up to now, the rock meter, the cross-fades, the sparks */
void r3d_lights_uniforms(void);               /* after glUseProgram(venue shader), once a frame */
void r3d_lights_batch(int lit, const float amb[3]);   /* per batch: lit = the batch takes the room's lights (not unlit / ambient-only); sets uDyn and uAmb */
void r3d_lights_draw(const float *vp, const float *view);   /* after the venue: beams, glows, sparks (additive, depth tested, no depth write) */
void r3d_lights_shutdown(void);
/* --r3dtest: the show driven by a song's cues at song time ms with this rock meter (0..2) instead of the game; NULL gives the game back */
void r3d_lights_sim(const gh3_songdata *sd, double ms, double health);
const char *r3d_lights_info(void);            /* mood, level, preset, colour, strobe: for the log */
#endif
