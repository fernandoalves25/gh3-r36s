/* Readers for the band's files, ported from the browser port (index.html: readSkeleton, readScene/skinnedMesh, readSka,
 * charClip) and checked against tools/research/{skin,ska}.py: the .ske skeletons of ZONES/global.pab, the .skin
 * character meshes (bone indices and weights) and the .ska animations of PAK/<char>.pak, perm_anims and the songs.
 * Pure CPU, no GL (r3d_band.c draws). Everything in the files is big-endian. docs/research/band.md is the recipe. */
#ifndef R3D_SKIN_H
#define R3D_SKIN_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* ---- a pak read piecemeal from disk: its list of contents, then the entries wanted (a character pak is 1-12 MB, global.pab 83 MB) ---- */
typedef struct { uint32_t type, len, fullKey, nameKey; long off; } r3d_pkent;
typedef struct { FILE *f; r3d_pkent *e; int n; } r3d_pakfile;
int r3d_pak_open(r3d_pakfile *p, const char *pakPath, const char *pabPath);   /* pabPath NULL: one file (<char>.pak.xen); 0 when fine */
int r3d_pak_find(const r3d_pakfile *p, uint32_t type, uint32_t fullKey);      /* the entry's index, -1 */
int r3d_pak_biggest(const r3d_pakfile *p, uint32_t type);                     /* the largest entry of a type (the body's .skin and .tex), -1 */
uint8_t *r3d_pak_read(r3d_pakfile *p, int i, size_t *n);                      /* malloc'd bytes, NULL */
void r3d_pak_close(r3d_pakfile *p);

/* ---- .ske: the rest pose, local to each bone's parent (quats x y z w, already turned the right way) ---- */
typedef struct { int n; uint32_t *key; int *parent; float (*pos)[3], (*q)[4]; } r3d_ske;
int r3d_ske_read(const uint8_t *b, size_t n, r3d_ske *o);
int r3d_ske_bone(const r3d_ske *s, const char *name);   /* by name ("bone_bicep_l"), -1 */
void r3d_ske_free(r3d_ske *s);

/* ---- .skin: materials and skinned meshes (positions and normals in the skeleton's rest space, Y up) ---- */
typedef struct { uint32_t key, template, tex[8], blend, flags; int ntex; uint8_t alphaRef, clamp[4]; } r3d_skinmat;
typedef struct { uint32_t mat; int nvert, ntri; float *pos, *nrm, *uv; uint8_t *bone /* 3 a point */; float *w /* 3 a point */; uint16_t *idx; float centre[3], radius; } r3d_skinmesh;
typedef struct { r3d_skinmat *mats; int nmats; r3d_skinmesh *m; int nm; } r3d_skin;
int r3d_skin_read(const uint8_t *b, size_t n, r3d_skin *o);   /* 0 when fine (only the skinned meshes are kept) */
const r3d_skinmat *r3d_skin_mat(const r3d_skin *s, uint32_t key);
void r3d_skin_free(r3d_skin *s);

/* ---- .ska: an animation. Rotation keys as 60 Hz frames and x y z * 16384 (w >= 0 worked out); positions as offsets ---- */
typedef struct { int nr, nt; uint16_t *rf; int16_t *rq /* 3 a key */; uint16_t *tf; float *tv /* 3 a key */; } r3d_track;
typedef struct { uint32_t key; float dur, land, travel; int nb, delta, plain; r3d_track *b; uint8_t *driven /* nb: 1 rotation keys move it (more than one key) */; } r3d_clip;
int r3d_ska_read(const uint8_t *b, size_t n, uint32_t key, r3d_clip *o);   /* 0 when fine */
void r3d_clip_free(r3d_clip *c);
size_t r3d_clip_bytes(const r3d_clip *c);
/* one bone at time t (s, clamped to the clip): its local turn and place, the rest pose filled in where the clip has no keys,
 * a difference clip laid over the rest turn. Returns bit 1 when the clip has rotation keys for it, bit 2 position keys. */
int r3d_clip_bone(const r3d_clip *c, const r3d_ske *rest, int bone, float t, float q[4], float p[3]);
/* a drummer's hit: the moment the stick tip (or the foot) is lowest, as the browser port measures it (charClip `land`) */
void r3d_clip_land(r3d_clip *c, const r3d_ske *rest);

/* ---- small maths shared by the band (quats x y z w) ---- */
void r3q_mul(const float a[4], const float b[4], float o[4]);
void r3q_rot(const float q[4], const float v[3], float o[3]);
void r3q_nlerp(const float a[4], const float b[4], float t, float o[4]);
void r3q_from_basis(const float x[3], const float y[3], const float z[3], float q[4]);   /* columns of a rotation matrix */
void r3q_inv(const float q[4], float o[4]);
#endif
