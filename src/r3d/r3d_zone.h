/* Readers for a venue's data in ZONES/<zone>.pak.xen, ported from the browser port (index.html: readScene, texBank,
 * ddsInfo, decodeDDS, texLook, venueCameras, venueObjects, venueLights, venueHidden). Pure CPU, no GL: r3d.c turns
 * what comes out of here into textures and vertex buffers. Everything in the files is big-endian except the DDS. */
#ifndef R3D_ZONE_H
#define R3D_ZONE_H
#include <stdint.h>
#include <stddef.h>
#include "gh3fmt.h"

/* ---- .scn: materials and meshes ---- */
/* blend: 0 solid, 1 added light, else see-through by the texture's alpha. clamp: per texture, bit 0 no repeat across, bit 1 no repeat down.
 * props: the material's numbers (its colour is the first four, or the second four when the first hold the shine); vsp: its vertex-shader settings */
typedef struct { uint32_t key, template; uint32_t tex[8]; int ntex; float props[64]; int nprops; float vsp[16]; int nvsp; uint32_t flags, blend, glow; uint8_t alphaRef, clamp[4]; } r3d_mat;
/* a mesh: its own corner points (world units, as stored) and a triangle list. nrm is zero when the file carries no normals,
 * col is 0xFF when it carries no colours; uv2 is the second set and uvLast the last of nuv sets (the baked light's). */
typedef struct { int n; uint32_t sector, mat, flags; float centre[3], radius; int nvert, ntri, nuv;
    float *pos, *nrm, *uv, *uv2, *uvLast; uint8_t *col; uint16_t *idx; } r3d_mesh;
typedef struct { r3d_mat *mats; int nmats; r3d_mesh *meshes; int nmeshes; float bmin[3], bmax[3]; } r3d_scene;
int r3d_scene_read(const uint8_t *scn, size_t n, r3d_scene *out);   /* 0 when fine */
void r3d_scene_free(r3d_scene *s);
const r3d_mat *r3d_scene_mat(const r3d_scene *s, uint32_t key);

/* ---- .tex bank: DDS images by key ---- */
typedef struct { uint32_t key; int w, h; uint32_t off, size; } r3d_texent;
int r3d_texbank_read(const uint8_t *b, size_t n, r3d_texent **out);   /* the count (malloc'd list) */

/* ---- DDS ---- */
enum { DDS_NONE = 0, DDS_DXT1 = 1, DDS_DXT3 = 3, DDS_DXT5 = 5, DDS_RGBA32 = 32 };
typedef struct { int w, h, cc, nlev, alpha; struct { int w, h; uint32_t off, size; } lev[16]; } r3d_dds;
int r3d_dds_info(const uint8_t *b, size_t n, r3d_dds *out);                        /* 0 when it is a DDS this reader knows */
int r3d_dds_decode(const uint8_t *b, size_t n, const r3d_dds *d, int level, uint8_t *rgba);   /* one level to RGBA8 (lev[level].w * h * 4 bytes); returns 1 when some texel is not opaque */
typedef struct { float r, g, b; int w, h, flat, normal; } r3d_look;                 /* a rough look without decoding: average colour, flat filler, normal map */
r3d_look r3d_tex_look(const uint8_t *b, size_t n);

/* ---- the venue's script data (.qb entries of the zone pak) ---- */
typedef struct { float pos[3], quat[3], fov; uint32_t lock; } r3d_camend;          /* quat: x y z (w is worked out); lock: 0 fixed in the room, else the band member followed */
typedef struct { uint32_t list, type, name; r3d_camend a, b; int hasB; float orbit; } r3d_shot; /* list: the section it came from (zone_cameras, zone_cameras_pan…); type/name: the shot's Type and NAME keys a song's cue may ask for (0 none); orbit in degrees */
typedef struct { uint32_t key; float pos[3], angles[3]; int shown; } r3d_object;   /* a levelobject: where a movable part stands and how it is turned */
enum { L_AMBIENT = 0, L_POINT, L_DIR, L_SPOT };
typedef struct { uint32_t key; int kind, band; float pos[3], dir[3], colour[3], intensity, near, far; } r3d_light;   /* key: its NAME (the light show names lights by it); band: a light kept for the band or the crowd */
typedef struct { r3d_shot *shots; int nshots; uint32_t *hidden; int nhidden; r3d_object *objects; int nobjects; r3d_light *lights; int nlights; } r3d_venueqb;
void r3d_venueqb_read(const gh3_pakent *ents, int n, r3d_venueqb *out);
void r3d_venueqb_free(r3d_venueqb *v);
const r3d_object *r3d_venueqb_object(const r3d_venueqb *v, uint32_t key);
int r3d_venueqb_hidden(const r3d_venueqb *v, uint32_t key);

/* ---- material templates, as the browser port sorts them (T_UNLIT, T_AMB, T_BOARD, T_ANIM, T_WIB) ---- */
enum { TPL_UNLIT = 1, TPL_AMB = 2, TPL_BOARD = 4, TPL_ANIM = 8, TPL_WIB = 16 };
int r3d_template_flags(uint32_t template);
int r3d_diffuse_slot(uint32_t template);   /* which of a material's textures is the picture (VENUE_DIFFUSE), or -1 */
#endif
