/* The light show of the 3D stage: a port of the browser port's lightShow() (index.html L6780-6827) over the game's own
 * tables, and a cheap way to show it on the venue (see r3d_lights.h and docs/research/stage.md section 3). */
#include "r3d_lights.h"
#include "app.h"
#include "settings.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define GL(name) r3d_##name
#define GL_STREAM_DRAW 0x88E0
#define NDYN 4               /* lights applied in the vertex shader */
#define DYN_GAIN 0.35f       /* the baked sum's average shading of one light (n.l for the main one, half-Lambert x .6 for the rest) */
#define MAXQ 4096            /* quads a frame: beams, glows, sparks */
#define MAXS 1200            /* sparks alive */
#define BLEND_DEFAULT 0.15f  /* the game's own figures (lightshow_settime Default, the colour cue, a rock meter change) */
#define BLEND_COLOUR 0.4f
#define BLEND_LEVEL 0.5f
#define IDLE_STEP 4.0        /* no song: the next verse preset every 8 beats at 120 bpm */
#define IDLE_BLEND 1.5f
#define BEAM_GAIN 0.2f       /* a beam side's strength at its housing (beams overlap: a rig of a dozen adds up) */
#define EXPOSURE 0.5f        /* how much of a preset's overall brightness against the base reaches the room (1 = all) */

enum { NV = 6, NB = 13 };    /* a light: r g b intensity far near; a beam: pos3 dir3 r0 r1 range density rgb */
typedef struct { uint32_t key; int kind, room, keep; float pos[3], dir[3], base[NV], cur[NV], from[NV], to[NV]; } slight;
typedef struct { uint32_t key; int keep; float cur[NB], from[NB], to[NB]; } sbeam;
typedef struct { uint32_t key; float v[NV]; } plight;
typedef struct { uint32_t key; float v[NB]; } pbeam;
typedef struct { uint32_t key; int l0, nl, b0, nb; } preset;
#define NMOOD 12
typedef struct { uint32_t key; int list[3][16], n[3]; } moodset;   /* per level (poor medium good): preset indices */
typedef struct { float pos[3]; int mask; } jet;                     /* a pyro spot of z_<v>_pyro and the levels (bits) it fires at */
typedef struct { float p[3], v[3], age, life, delay; } spark;
typedef struct { GLint uBake, uDyn, uAmb, uLP[NDYN], uLC[NDYN], uLD[NDYN]; } lightlocs;   /* where a venue program keeps the show's uniforms */

static struct {
    int loaded; char zone[32];
    slight *L; int nL; sbeam *B; int nB; plight *PL; int nPL; pbeam *PB; int nPB; preset *P; int nP; moodset M[NMOOD]; int nM; jet J[32]; int nJ;
    /* the show */
    uint32_t mood; int level, pi, cur, strobe, started, ptr, pyro, hasOver, idleStep; float blendT, dur, mix, over[3]; const gh3_songdata *cues; double lastMs, clock, lastT, beat, strobeAt;
    int sim, on;
    /* this frame */
    float bake[3], ambk[3], lp[NDYN][4], lc[NDYN][4], ld[NDYN][4], flick, pulse;
    spark S[MAXS]; int nS; uint32_t rng;
    /* GL */
    GLuint prog, vbo, ibo, tex; GLint aP, aT, aC, uVP, uTex; lightlocs u[2];   /* u: per venue program (0 plain, 1 alpha-tested) */
    float *vb; int nq; char info[200];
} S;
static uint32_t kIntro, kVerse, kChorus, kSolo, kFlare, kStrobe, kFail, kWin, kPoor, kMedium, kGood, kDefault, kTrans, kSnaps, kPos;
static void keys(void) {
    if (kIntro) return; kIntro = gh3_qbkey("intro"); kVerse = gh3_qbkey("verse"); kChorus = gh3_qbkey("chorus"); kSolo = gh3_qbkey("solo"); kFlare = gh3_qbkey("flare"); kStrobe = gh3_qbkey("strobe");
    kFail = gh3_qbkey("fail"); kWin = gh3_qbkey("win"); kPoor = gh3_qbkey("poor"); kMedium = gh3_qbkey("medium"); kGood = gh3_qbkey("good"); kDefault = gh3_qbkey("default"); kTrans = gh3_qbkey("transitions");
    kSnaps = gh3_qbkey("snapshots"); kPos = gh3_qbkey("pos");
}
static float frand(void) { S.rng = S.rng * 1664525u + 1013904223u; return (S.rng >> 8) / 16777216.0f; }
static float clampf(float x, float a, float b) { return x < a ? a : x > b ? b : x; }

/* ------------------------------------------------------------------ the venue's data */
static double num(const qbv *st, const char *n, double d) { return qb_num(qb_get(st, n), d); }
static int vec3(const qbv *v, float o[3]) {
    if (!v) return 0; if (v->kind == QB_VEC3) { memcpy(o, v->f, 12); return 1; }
    if (v->kind == QB_ARRAY && v->narr >= 3) { for (int i = 0; i < 3; i++) o[i] = (float)qb_num(v->arr[i], 0); return 1; } return 0;
}
typedef struct { qbsection *s; int n; } qbfile;
static const qbv *find_sec(const qbfile *f, int nf, uint32_t id) { for (int i = 0; i < nf; i++) for (int k = 0; k < f[i].n; k++) if (f[i].s[k].id == id) return f[i].s[k].v; return NULL; }
static int preset_of(uint32_t key) { for (int i = 0; i < S.nP; i++) if (S.P[i].key == key) return i; return -1; }
static void add_keys(const qbfile *f, int nf, const qbv *v, moodset *m, int lv, int depth) {   /* a list of preset keys, one key, or a key naming a list section */
    if (!v || depth > 2) return; uint32_t k;
    if (v->kind == QB_ARRAY) { for (int i = 0; i < v->narr; i++) add_keys(f, nf, v->arr[i], m, lv, depth + 1); return; }
    if (!qb_keyof(v, &k)) return; int p = preset_of(k);
    if (p >= 0) { if (m->n[lv] < 16) m->list[lv][m->n[lv]++] = p; return; }
    const qbv *l = find_sec(f, nf, k); if (l && l->kind == QB_ARRAY) add_keys(f, nf, l, m, lv, depth + 1);
}
/* z_<v>_pyro: "switch performance / case medium / case good / createparticlesystem_fast name=.. pos=(x,y,z) .. / endswitch" read from the bytecode */
static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
static int lzss(const uint8_t *in, int inN, uint8_t *out, int outN) {
    uint8_t win[4096]; memset(win, 0, sizeof win); int wp = 4078, i = 0, o = 0; unsigned flags = 0;
    while (i < inN && o < outN) { flags >>= 1; if (!(flags & 0x100)) { flags = in[i++] | 0xFF00; if (i >= inN) break; }
        if (flags & 1) { uint8_t c = in[i++]; out[o++] = c; win[wp] = c; wp = (wp + 1) & 4095; }
        else { if (i + 1 >= inN) break; int a = in[i] | ((in[i + 1] & 0xF0) << 4), len = (in[i + 1] & 0x0F) + 3; i += 2; for (int k = 0; k < len && o < outN; k++) { uint8_t c = win[(a + k) & 4095]; out[o++] = c; win[wp] = c; wp = (wp + 1) & 4095; } } }
    return o;
}
static void pyro_scan(const uint8_t *b, int n) {
    int i = 0, mask = 7, saw = 0, wantCase = 0;
    while (i < n && S.nJ < 32) { int t = b[i++];
        switch (t) {
        case 0x16: { if (i + 4 > n) return; uint32_t k = le32(b + i); i += 4;
            if (wantCase) { mask |= k == kPoor ? 1 : k == kMedium ? 2 : k == kGood ? 4 : 0; wantCase = 0; break; }
            saw = 1; if (k == kPos && i + 14 <= n && b[i] == 0x07 && b[i + 1] == 0x1E) { jet *j = &S.J[S.nJ++]; memcpy(j->pos, b + i + 2, 12); j->mask = mask; i += 14; } break; }
        case 0x3E: if (saw) mask = 0; saw = 0; wantCase = 1; break;   /* case: a run of cases adds up, a case after a statement starts again */
        case 0x3F: mask = 7; saw = 0; break; case 0x3D: mask = 7; saw = 1; break;
        case 0x17: case 0x18: case 0x1A: case 0x2E: i += 4; break; case 0x1E: i += 12; break; case 0x1F: i += 8; break; case 0x47: case 0x48: case 0x49: i += 2; break;
        case 0x1B: case 0x1C: case 0x4C: if (i + 4 > n) return; i += 4 + (int)le32(b + i); break;
        case 0x2F: case 0x37: case 0x40: case 0x41: if (i + 4 > n) return; i += 4 + (int)le32(b + i) * 6; break;
        case 0x4A: if (i + 2 > n) return; { int l = b[i] | b[i + 1] << 8; i += 2; i = (i + 3) & ~3; i += l; } break;
        case 0x2B: i += 4; while (i < n && b[i]) i++; i++; break;
        default: break; }
    }
}
static void read_pyro(const gh3_pakent *e, uint32_t id) {
    const uint8_t *b = e->data; size_t L = e->len;
    for (size_t p = 28; p + 20 <= L; p += 4) { if ((be32(b + p) & 0xFFFFFF00u) != 0x00200700 || be32(b + p + 4) != id) continue; uint32_t raw = be32(b + p + 12); if (raw < p || raw + 12 > L) continue;
        uint32_t usz = be32(b + raw + 4), csz = be32(b + raw + 8); if (usz > (1u << 20) || raw + 12 + csz > L) continue; uint8_t *out = malloc(usz + 1); int got = usz == csz ? (memcpy(out, b + raw + 12, usz), (int)usz) : lzss(b + raw + 12, (int)csz, out, (int)usz);
        pyro_scan(out, got); free(out); return; }
}
static void reset_show(void) {
    S.mood = kIntro; S.level = 1; S.pi = 0; S.cur = -1; S.strobe = 0; S.started = 0; S.ptr = 0; S.pyro = 0; S.hasOver = 0; S.idleStep = -1; S.blendT = BLEND_DEFAULT; S.dur = 0; S.mix = 1; S.cues = NULL; S.lastMs = 0; S.nS = 0; S.strobeAt = -1e9;
    for (int i = 0; i < S.nL; i++) { memcpy(S.L[i].cur, S.L[i].base, sizeof S.L[i].cur); memcpy(S.L[i].from, S.L[i].base, sizeof S.L[i].cur); memcpy(S.L[i].to, S.L[i].base, sizeof S.L[i].cur); }
    for (int i = 0; i < S.nB; i++) { S.B[i].cur[9] = 0; memcpy(S.B[i].from, S.B[i].cur, sizeof S.B[i].cur); memcpy(S.B[i].to, S.B[i].cur, sizeof S.B[i].cur); }
}
static int gl_init(void);
void r3d_lights_load(const char *zone, const gh3_pakent *ents, int n, const r3d_venueqb *vq) {
    r3d_lights_unload(); keys(); snprintf(S.zone, sizeof S.zone, "%s", zone); S.rng = 0x1234567u;
    const gh3_library *lib = A.lib; int nroom = 0;
    S.L = calloc(vq->nlights ? vq->nlights : 1, sizeof *S.L);
    for (int i = 0; i < vq->nlights; i++) { const r3d_light *l = &vq->lights[i]; slight *s = &S.L[S.nL++]; s->key = l->key; s->kind = l->kind; memcpy(s->pos, l->pos, 12); memcpy(s->dir, l->dir, 12);
        s->room = !l->band && nroom < 32; if (!l->band) nroom++;   /* the lights baked into the room (r3d.c: the first 32 not kept for the band or the crowd) */
        for (int k = 0; k < 3; k++) s->base[k] = l->colour[k]; s->base[3] = l->intensity; s->base[4] = l->far; s->base[5] = l->near;
        if (lib) for (int k = 0; k < lib->nlightKeep; k++) if (lib->lightKeep[k] == l->key) s->keep = 1; }
    char nm[64]; snprintf(nm, sizeof nm, "%s_snapshots", zone); uint32_t kSnap = gh3_qbkey(nm); snprintf(nm, sizeof nm, "%s_lightshow_mapping", zone); uint32_t kMap = gh3_qbkey(nm); snprintf(nm, sizeof nm, "%s_pyro", zone); uint32_t kPyro = gh3_qbkey(nm);
    uint32_t K_QB = gh3_qbkey(".qb"); qbfile *f = calloc(n ? n : 1, sizeof *f); int nf = 0;
    for (int e = 0; e < n; e++) { if (ents[e].type != K_QB || !ents[e].data) continue; qbsection *s; int ns = gh3_qb_parse(ents[e].data, ents[e].len, &s); if (ns < 0) continue; f[nf].s = s; f[nf].n = ns; nf++; read_pyro(&ents[e], kPyro); }
    const qbv *snap = find_sec(f, nf, kSnap), *map = find_sec(f, nf, kMap);
    if (snap && snap->kind == QB_STRUCT) { S.P = calloc(snap->nitems ? snap->nitems : 1, sizeof *S.P); int capL = 0, capB = 0;
        for (int i = 0; i < snap->nitems; i++) { const qbv *a = snap->items[i].v; if (!a || a->kind != QB_ARRAY) continue; preset *p = &S.P[S.nP++]; p->key = snap->items[i].id; p->l0 = S.nPL; p->b0 = S.nPB;
            for (int k = 0; k < a->narr; k++) { const qbv *x = a->arr[k]; uint32_t lk, hk; if (!x || x->kind != QB_STRUCT) continue;
                if (qb_keyof(qb_get(x, "lightchecksum"), &lk)) { if (S.nPL == capL) { capL = capL ? capL * 2 : 256; S.PL = realloc(S.PL, capL * sizeof *S.PL); } plight *o = &S.PL[S.nPL++]; o->key = lk;
                    o->v[0] = (float)num(x, "r", 255) / 255; o->v[1] = (float)num(x, "g", 255) / 255; o->v[2] = (float)num(x, "b", 255) / 255; o->v[3] = (float)num(x, "intensity", 0); o->v[4] = (float)num(x, "farattenend", 10);
                    o->v[5] = (float)qb_num(qb_getk(x, 0x427fbaacu), 0); if (o->v[5] >= o->v[4]) o->v[5] = 0; p->nl++; }   /* 0x427fbaac: unnamed, read as the start of the fade (attenstart) */
                else if (qb_keyof(qb_get(x, "housingchecksum"), &hk)) { float pos[3], q[3]; if (!vec3(qb_get(x, "pos"), pos) || !vec3(qb_get(x, "quat"), q)) continue;
                    if (S.nPB == capB) { capB = capB ? capB * 2 : 256; S.PB = realloc(S.PB, capB * sizeof *S.PB); } pbeam *o = &S.PB[S.nPB++]; o->key = hk; float w2 = 1 - q[0] * q[0] - q[1] * q[1] - q[2] * q[2], w = sqrtf(w2 > 0 ? w2 : 0);
                    memcpy(o->v, pos, 12); o->v[3] = -2 * (q[0] * q[2] + w * q[1]); o->v[4] = -2 * (q[1] * q[2] - w * q[0]); o->v[5] = -(1 - 2 * (q[0] * q[0] + q[1] * q[1]));   /* (0,0,-1) turned by the quaternion */
                    o->v[6] = (float)num(x, "startradius", 0); o->v[7] = (float)num(x, "endradius", 0); o->v[8] = (float)num(x, "range", 0); o->v[9] = (float)num(x, "volumedensity", 0);
                    o->v[10] = (float)num(x, "volumecolorred", 255) / 255; o->v[11] = (float)num(x, "volumecolorgreen", 255) / 255; o->v[12] = (float)num(x, "volumecolorblue", 255) / 255; p->nb++; } } } }
    /* every beam any preset names, dark to begin with */
    for (int i = 0; i < S.nPB; i++) { int have = 0; for (int k = 0; k < S.nB && !have; k++) have = S.B[k].key == S.PB[i].key; if (have) continue;
        S.B = realloc(S.B, (S.nB + 1) * sizeof *S.B); sbeam *b = &S.B[S.nB++]; memset(b, 0, sizeof *b); b->key = S.PB[i].key; memcpy(b->cur, S.PB[i].v, sizeof b->cur); b->cur[9] = 0;
        if (lib) for (int k = 0; k < lib->nlightKeep; k++) if (lib->lightKeep[k] == b->key) b->keep = 1; }
    if (map && map->kind == QB_STRUCT) for (int i = 0; i < map->nitems && S.nM < NMOOD; i++) { const qbv *mv = map->items[i].v; if (!mv || mv->kind != QB_STRUCT || map->items[i].id == kTrans) continue;
        moodset *m = &S.M[S.nM]; memset(m, 0, sizeof *m); m->key = map->items[i].id;
        for (int k = 0; k < mv->nitems; k++) { uint32_t lk = mv->items[k].id; int lv = lk == kPoor ? 0 : lk == kMedium ? 1 : lk == kGood ? 2 : lk == kDefault ? 3 : -1; if (lv < 0) continue;
            const qbv *sn = qb_getk(mv->items[k].v, kSnaps); if (lv < 3) add_keys(f, nf, sn, m, lv, 0); else for (int v = 0; v < 3; v++) add_keys(f, nf, sn, m, v, 0); }
        if (m->n[0] + m->n[1] + m->n[2]) S.nM++; }
    for (int i = 0; i < nf; i++) gh3_qb_free_sections(f[i].s, f[i].n); free(f);
    int nroomL = 0, matched = 0; for (int i = 0; i < S.nL; i++) { nroomL += S.L[i].room; for (int k = 0; k < S.nPL; k++) if (S.PL[k].key == S.L[i].key) { matched++; break; } }
    SDL_Log("r3d lights: %s: %d presets (%d light, %d beam entries), %d moods, %d lights (%d in the room, %d named by presets), %d beams, %d pyro spots, cue table %s, %d keep",
        zone, S.nP, S.nPL, S.nPB, S.nM, S.nL, nroomL, matched, S.nB, S.nJ, lib && lib->hasLightNotes ? "read" : "missing", lib ? lib->nlightKeep : 0);
    reset_show(); S.loaded = 1; if (!S.prog) gl_init();
}
void r3d_lights_unload(void) {
    free(S.L); free(S.B); free(S.PL); free(S.PB); free(S.P); S.L = NULL; S.B = NULL; S.PL = NULL; S.PB = NULL; S.P = NULL; S.nL = S.nB = S.nPL = S.nPB = S.nP = S.nM = S.nJ = S.nS = 0; S.loaded = 0; S.on = 0;
}

/* ------------------------------------------------------------------ the show (lightShow) */
static const moodset *mood_of(uint32_t k) { for (int i = 0; i < S.nM; i++) if (S.M[i].key == k) return &S.M[i]; return NULL; }
static int preset_now(void) {
    const moodset *m = mood_of(S.mood); if (!m) m = mood_of(kVerse); if (!m && S.nM) m = &S.M[0]; if (!m) return -1;
    int lv = S.level; if (!m->n[lv]) for (lv = 0; lv < 3 && !m->n[lv]; lv++) ; if (lv > 2) return -1;
    int n = m->n[lv]; return m->list[lv][((S.pi % n) + n) % n];
}
static void settle(float e) {
    for (int i = 0; i < S.nL; i++) for (int k = 0; k < NV; k++) S.L[i].cur[k] = S.L[i].from[k] + (S.L[i].to[k] - S.L[i].from[k]) * e;
    for (int i = 0; i < S.nB; i++) for (int k = 0; k < NB; k++) S.B[i].cur[k] = S.B[i].from[k] + (S.B[i].to[k] - S.B[i].from[k]) * e;
}
static void apply(float time) {
    int pi = preset_now(); const preset *p = pi >= 0 ? &S.P[pi] : NULL; S.cur = pi;
    for (int i = 0; i < S.nL; i++) { slight *s = &S.L[i]; memcpy(s->from, s->cur, sizeof s->cur); const plight *v = NULL;
        if (p) for (int k = 0; k < p->nl && !v; k++) if (S.PL[p->l0 + k].key == s->key) v = &S.PL[p->l0 + k];
        memcpy(s->to, v ? v->v : s->base, sizeof s->to); if (v && !v->v[5]) s->to[5] = s->base[5] < s->to[4] ? s->base[5] : 0;   /* lights a preset leaves out go back to the venue's own values */
        if (S.hasOver && s->kind != L_AMBIENT && !s->keep) memcpy(s->to, S.over, 12); }
    static char seen[512]; memset(seen, 0, S.nB < 512 ? S.nB : 512);
    if (p) for (int k = 0; k < p->nb; k++) { const pbeam *v = &S.PB[p->b0 + k]; int bi = -1; for (int j = 0; j < S.nB && bi < 0; j++) if (S.B[j].key == v->key) bi = j; if (bi < 0 || bi >= 512 || seen[bi]) continue; seen[bi] = 1;
        sbeam *s = &S.B[bi]; memcpy(s->to, v->v, sizeof s->to); if (S.hasOver && !s->keep) memcpy(s->to + 10, S.over, 12);
        memcpy(s->from, s->cur, sizeof s->cur); if (s->cur[9] < 0.002f) memcpy(s->from, s->to, 9 * sizeof(float)); }   /* a beam that was dark comes on where it now points */
    for (int i = 0; i < S.nB && i < 512; i++) if (!seen[i]) { memcpy(S.B[i].from, S.B[i].cur, sizeof S.B[i].cur); memcpy(S.B[i].to, S.B[i].cur, sizeof S.B[i].cur); S.B[i].to[9] = 0; }
    S.dur = time > 0 ? time : 0; S.mix = S.dur > 0 ? 0 : 1; if (S.mix >= 1) settle(1);
}
static void fire_pyro(void) {
    for (int j = 0; j < S.nJ; j++) { if (!(S.J[j].mask & (1 << S.level))) continue;
        for (int k = 0; k < 40 && S.nS < MAXS; k++) { spark *s = &S.S[S.nS++]; float th = frand() * 6.2831853f, ph = frand() * 0.26f, sp = 6 + 2 * frand();   /* artdeco's fountain: 15 deg, 6-8 m/s, 0.3 s of emission, life 1-1.3 s */
            memcpy(s->p, S.J[j].pos, 12); s->v[0] = sinf(ph) * cosf(th) * sp; s->v[1] = cosf(ph) * sp; s->v[2] = sinf(ph) * sinf(th) * sp; s->age = 0; s->life = 1 + 0.3f * frand(); s->delay = 0.3f * frand(); } }
}
static void sparks(float dt) {
    for (int i = 0; i < S.nS; ) { spark *s = &S.S[i]; if (s->delay > 0) { s->delay -= dt; i++; continue; } s->age += dt; s->v[1] -= 7 * dt; for (int k = 0; k < 3; k++) s->p[k] += s->v[k] * dt;
        if (s->age >= s->life) S.S[i] = S.S[--S.nS]; else i++; }
}
/* one tick: sd = the song's cues (NULL: no song), endMood = FAIL / WIN at the results (0 none) */
static void step(const gh3_songdata *sd, double ms, double health, uint32_t endMood, float dt, double wall) {
    int lv = health < 0.66 ? 0 : health < 1.33 ? 1 : 2; float need = -1; const gh3_library *lib = A.lib;
    if (!S.started) { S.started = 1; need = 0; }
    if (lv != S.level) { S.level = lv; need = BLEND_LEVEL; }
    if (endMood) { if (S.mood != endMood) { S.mood = endMood; S.pi = 0; S.hasOver = 0; S.strobe = 0; need = 1; } S.strobeAt = -1e9; S.cues = NULL; S.clock = wall; }
    else if (sd && lib) {
        if (sd != S.cues || ms < S.lastMs - 1500) { S.cues = sd; S.ptr = 0; S.mood = kIntro; S.pi = 0; S.hasOver = 0; S.strobe = 0; S.blendT = BLEND_DEFAULT; need = 0; S.nS = 0; S.strobeAt = -1e9; }   /* a new song, or the clock wound back */
        S.lastMs = ms; S.clock = ms / 1000;
        while (S.ptr < sd->nlights && sd->lights[S.ptr][0] <= ms) { double late = ms - sd->lights[S.ptr][0]; int note = sd->lights[S.ptr][1]; S.ptr++; if (note < 0 || note > 127) continue; const gh3_lightnote *w = &lib->lightNote[note];
            switch (w->kind) {
            case LN_MOOD: S.strobe = w->mood == kStrobe; if (w->mood != S.mood) { S.mood = w->mood; S.pi = 0; need = S.blendT; } break;
            case LN_COLOUR: S.hasOver = w->rgb[0] >= 0; if (S.hasOver) memcpy(S.over, w->rgb, 12); need = BLEND_COLOUR; break;
            case LN_TIME: S.blendT = w->time < 0 ? BLEND_DEFAULT : w->time; break;
            case LN_STROBE: S.strobe = !S.strobe; S.strobeAt = ms; break;
            case LN_NEXT: S.pi++; need = S.blendT; break;
            case LN_PYRO: if (late < 600) S.pyro++; break; } }   /* a pyro cue long past is not set off */
        S.beat = sd->nfret > 1 ? gh3_beatpos(sd->fretbars, sd->nfret, ms) : ms / 500.0;
    } else {   /* no song: the verse presets one after another, calmly */
        if (S.cues || S.mood == kFail || S.mood == kWin || S.mood == kIntro) { S.cues = NULL; S.hasOver = 0; S.strobe = 0; S.mood = kVerse; S.pi = 0; need = IDLE_BLEND; S.strobeAt = -1e9; }
        S.clock = wall; S.beat = wall * 2; long st = (long)floor(wall / IDLE_STEP); if (st != S.idleStep) { if (S.idleStep >= 0) { S.pi++; need = IDLE_BLEND; } S.idleStep = (int)st; }
    }
    if (need >= 0) apply(need);
    if (S.mix < 1) { S.mix = S.dur > 0 ? S.mix + dt / S.dur : 1; if (S.mix > 1) S.mix = 1; settle(S.mix); }
    while (S.pyro > 0) { S.pyro--; fire_pyro(); }   /* poor: the venues' scripts list no spot for it */
    sparks(dt);
}
/* what the venue shader gets this frame: the room's lights now against their base, the ambient's, and the lights that changed most */
static void frame(void) {
    int strobing = S.strobe || S.clock * 1000 - S.strobeAt < 300; S.flick = strobing ? 0.72f + 0.28f * cosf((float)(S.clock * 6.2831853 * 2.5)) : 1;   /* the song flashes by toggling the strobe (note 60) eight times a second: here a strobe run only keeps the browser port's soft 2.5 Hz pulse going (no hard flash) */
    float fb = (float)(S.beat - floor(S.beat)); S.pulse = 1 + 0.04f * (1 - fb) * (1 - fb); float k = S.flick * S.pulse;
    double sc[3] = { 0 }, sb[3] = { 0 }, ac[3] = { 0 }, ab[3] = { 0 };
    for (int i = 0; i < S.nL; i++) { const slight *s = &S.L[i]; if (!s->room) continue;
        if (s->kind == L_AMBIENT) { for (int c = 0; c < 3; c++) { ac[c] += s->cur[c] * s->cur[3]; ab[c] += s->base[c] * s->base[3]; } continue; }
        float rc = fminf(s->cur[4], 30), rb = fminf(s->base[4], 30); for (int c = 0; c < 3; c++) { sc[c] += s->cur[c] * s->cur[3] * k * rc; sb[c] += s->base[c] * s->base[3] * rb; } }
    double mx = fmax(sb[0], fmax(sb[1], sb[2])), ma = fmax(ab[0], fmax(ab[1], ab[2]));
    float ratio[3], L = 0, x = 1;
    for (int c = 0; c < 3; c++) { ratio[c] = mx > 1e-6 ? (float)((sc[c] + 0.05 * mx) / (sb[c] + 0.05 * mx)) : 1; L = fmaxf(L, ratio[c]); S.ambk[c] = ma > 1e-6 ? clampf((float)((ac[c] + 0.05 * ma) / (ab[c] + 0.05 * ma)), 0.3f, 2) : 1; }
    if (L > 1e-4f) x = clampf(powf(L, EXPOSURE), 0.3f, 1.35f) / L;   /* the presets run up to 3x the venue's base lights: brightness squeezed, colour kept */
    for (int c = 0; c < 3; c++) S.bake[c] = clampf(ratio[c] * x, 0.12f, 2); k *= x;
    float best[NDYN]; int bi[NDYN]; for (int d = 0; d < NDYN; d++) { best[d] = 0; bi[d] = -1; }
    for (int i = 0; i < S.nL; i++) { const slight *s = &S.L[i]; if (!s->room || s->kind == L_AMBIENT || s->cur[4] > 200 || s->cur[4] < 0.05f) continue; float m = 0;
        for (int c = 0; c < 3; c++) m = fmaxf(m, fabsf(s->cur[c] * s->cur[3] * k - S.bake[c] * s->base[c] * s->base[3])); m *= fminf(s->cur[4], 30);
        for (int d = 0; d < NDYN; d++) if (m > best[d]) { for (int e = NDYN - 1; e > d; e--) { best[e] = best[e - 1]; bi[e] = bi[e - 1]; } best[d] = m; bi[d] = i; break; } }
    for (int d = 0; d < NDYN; d++) { memset(S.lp[d], 0, 16); memset(S.lc[d], 0, 16); memset(S.ld[d], 0, 16); S.lp[d][3] = 1; if (bi[d] < 0) continue; const slight *s = &S.L[bi[d]];
        memcpy(S.lp[d], s->pos, 12); S.lp[d][3] = s->cur[4]; for (int c = 0; c < 3; c++) S.lc[d][c] = clampf((s->cur[c] * s->cur[3] * k - S.bake[c] * s->base[c] * s->base[3]) * DYN_GAIN, -0.8f, 0.8f); S.lc[d][3] = s->cur[5];
        memcpy(S.ld[d], s->dir, 12); S.ld[d][3] = s->kind == L_DIR ? 1 : 0; }
    static const char *MN[] = { "intro", "verse", "chorus", "solo", "mood72", "flare", "strobe", "fail", "win" }; uint32_t MK[9] = { kIntro, kVerse, kChorus, kSolo, 0xf3b3b42au, kFlare, kStrobe, kFail, kWin }; const char *mn = "?";
    for (int i = 0; i < 9; i++) if (MK[i] == S.mood) mn = MN[i];
    int lit = 0; for (int i = 0; i < S.nB; i++) lit += S.B[i].cur[9] > 0.004f;
    snprintf(S.info, sizeof S.info, "mood %s level %d preset %d (%08x) step %d blend %.2f mix %.2f colour %s strobe %d, %d beams lit, %d sparks, bake %.2f %.2f %.2f", mn, S.level, S.cur, S.cur >= 0 ? S.P[S.cur].key : 0, S.pi, S.blendT, S.mix,
        S.hasOver ? "on" : "off", strobing, lit, S.nS, S.bake[0], S.bake[1], S.bake[2]);
}
void r3d_lights_update(double t) {
    float dt = (float)(t - S.lastT); S.lastT = t; if (dt < 0) dt = 0; if (dt > 0.1f) dt = 0.1f;
    if (S.sim) return;   /* --r3dtest drives it */
    S.on = SET.stageFx && S.loaded && S.nP > 0; if (!S.on) return;
    int song = A.S.loaded && A.S.sd.nlights && (A.screen == SC_PLAY || A.screen == SC_PAUSE || (A.screen == SC_OPTIONS && A.optionsFrom == SC_PAUSE));
    uint32_t end = A.S.loaded && A.screen == SC_RESULTS ? (A.S.run.failed ? kFail : kWin) : 0;
    step(song ? &A.S.sd : NULL, A.S.run.now, song ? A.S.run.health : 1.5, end, dt, t); frame();
}
void r3d_lights_sim(const gh3_songdata *sd, double ms, double health) {
    if (!sd) { S.sim = 0; return; } S.sim = 1; S.on = S.loaded && S.nP > 0; if (!S.on) return;
    double from = S.cues == sd && ms >= S.lastMs ? S.lastMs : (reset_show(), 0.0);
    for (double m = from; m < ms; m += 1000.0 / 60) step(sd, m, health, 0, 1 / 60.0f, m / 1000);
    step(sd, ms, health, 0, 0, ms / 1000); frame();
    /* R3D_LDEBUG=1: every light (base | now), every lit beam and the pyro spots */
    if (getenv("R3D_LDEBUG")) for (int i = 0; i < S.nL; i++) { const slight *s = &S.L[i]; printf("    light %08x %s%s%s base %.2f %.2f %.2f i%.2f far %.0f near %.0f | now %.2f %.2f %.2f i%.2f far %.0f near %.0f\n", s->key, s->kind == L_AMBIENT ? "amb " : s->kind == L_DIR ? "dir " : s->kind == L_POINT ? "pt  " : "spot",
        s->room ? " room" : " band", s->keep ? " keep" : "", s->base[0], s->base[1], s->base[2], s->base[3], s->base[4], s->base[5], s->cur[0], s->cur[1], s->cur[2], s->cur[3], s->cur[4], s->cur[5]); }
    if (getenv("R3D_LDEBUG")) for (int i = 0; i < S.nB; i++) { const float *v = S.B[i].cur; if (v[9] > 0.004f) printf("    beam %08x pos %.1f %.1f %.1f dir %.2f %.2f %.2f r %.2f-%.2f range %.1f dens %.2f rgb %.2f %.2f %.2f\n", S.B[i].key, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11], v[12]); }
    if (getenv("R3D_LDEBUG")) for (int i = 0; i < S.nJ; i++) printf("    jet %.1f %.1f %.1f mask %d\n", S.J[i].pos[0], S.J[i].pos[1], S.J[i].pos[2], S.J[i].mask);
}
const char *r3d_lights_info(void) { return S.on ? S.info : "light show off"; }

/* ------------------------------------------------------------------ the venue shader's side */
void r3d_lights_program(int slot, GLuint prog) {
    char nm[16]; lightlocs *U = &S.u[slot & 1]; U->uBake = GL(glGetUniformLocation)(prog, "uBake"); U->uDyn = GL(glGetUniformLocation)(prog, "uDyn"); U->uAmb = GL(glGetUniformLocation)(prog, "uAmb");
    for (int d = 0; d < NDYN; d++) { snprintf(nm, sizeof nm, "uLP[%d]", d); U->uLP[d] = GL(glGetUniformLocation)(prog, nm); snprintf(nm, sizeof nm, "uLC[%d]", d); U->uLC[d] = GL(glGetUniformLocation)(prog, nm); snprintf(nm, sizeof nm, "uLD[%d]", d); U->uLD[d] = GL(glGetUniformLocation)(prog, nm); }
}
void r3d_lights_uniforms(int slot) {
    const lightlocs *U = &S.u[slot & 1];
    if (!S.on) { GL(glUniform3f)(U->uBake, 1, 1, 1); for (int d = 0; d < NDYN; d++) GL(glUniform4f)(U->uLC[d], 0, 0, 0, 0); return; }
    GL(glUniform3f)(U->uBake, S.bake[0], S.bake[1], S.bake[2]);
    for (int d = 0; d < NDYN; d++) { GL(glUniform4f)(U->uLP[d], S.lp[d][0], S.lp[d][1], S.lp[d][2], S.lp[d][3]); GL(glUniform4f)(U->uLC[d], S.lc[d][0], S.lc[d][1], S.lc[d][2], S.lc[d][3]); GL(glUniform4f)(U->uLD[d], S.ld[d][0], S.ld[d][1], S.ld[d][2], S.ld[d][3]); }
}
void r3d_lights_batch(int slot, int lit, const float amb[3]) {
    const lightlocs *U = &S.u[slot & 1]; int on = S.on && lit; GL(glUniform1f)(U->uDyn, on ? 1.0f : 0.0f);
    if (on) GL(glUniform3f)(U->uAmb, amb[0] * S.ambk[0], amb[1] * S.ambk[1], amb[2] * S.ambk[2]); else GL(glUniform3f)(U->uAmb, amb[0], amb[1], amb[2]);
}

/* ------------------------------------------------------------------ beams, glows, sparks */
static const char *FXVS =
    "uniform mat4 uVP; attribute vec3 aP; attribute vec2 aT; attribute vec3 aC; varying vec2 vT; varying vec3 vC;\n"
    "void main() { gl_Position = uVP * vec4(aP, 1.0); vT = aT; vC = aC; }\n";
static const char *FXFS =
    "#ifdef GL_ES\nprecision mediump float;\n#endif\n"
    "uniform sampler2D uTex; varying vec2 vT; varying vec3 vC;\n"
    "void main() { gl_FragColor = vec4(vC * texture2D(uTex, vT).r, 1.0); }\n";
static GLuint sh(GLenum kind, const char *src) {
    GLuint s = GL(glCreateShader)(kind); GL(glShaderSource)(s, 1, &src, NULL); GL(glCompileShader)(s); GLint ok = 0; GL(glGetShaderiv)(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[512]; GL(glGetShaderInfoLog)(s, sizeof log, NULL, log); SDL_Log("r3d lights: shader: %s", log); GL(glDeleteShader)(s); return 0; } return s;
}
static int gl_init(void) {
    GLuint vs = sh(GL_VERTEX_SHADER, FXVS), fs = sh(GL_FRAGMENT_SHADER, FXFS); if (!vs || !fs) return -1;
    S.prog = GL(glCreateProgram)(); GL(glAttachShader)(S.prog, vs); GL(glAttachShader)(S.prog, fs); GL(glLinkProgram)(S.prog); GL(glDeleteShader)(vs); GL(glDeleteShader)(fs);
    GLint ok = 0; GL(glGetProgramiv)(S.prog, GL_LINK_STATUS, &ok); if (!ok) { SDL_Log("r3d lights: link failed"); GL(glDeleteProgram)(S.prog); S.prog = 0; return -1; }
    S.aP = GL(glGetAttribLocation)(S.prog, "aP"); S.aT = GL(glGetAttribLocation)(S.prog, "aT"); S.aC = GL(glGetAttribLocation)(S.prog, "aC"); S.uVP = GL(glGetUniformLocation)(S.prog, "uVP"); S.uTex = GL(glGetUniformLocation)(S.prog, "uTex");
    uint16_t *ix = malloc(MAXQ * 6 * 2); for (int q = 0; q < MAXQ; q++) { uint16_t b = (uint16_t)(q * 4); uint16_t *o = ix + q * 6; o[0] = b; o[1] = b + 1; o[2] = b + 2; o[3] = b; o[4] = b + 2; o[5] = b + 3; }
    GL(glGenBuffers)(1, &S.ibo); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, S.ibo); GL(glBufferData)(GL_ELEMENT_ARRAY_BUFFER, MAXQ * 6 * 2, ix, GL_STATIC_DRAW); free(ix);
    GL(glGenBuffers)(1, &S.vbo); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, 0);
    uint8_t px[32 * 32 * 4]; for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) { float dx = (x - 15.5f) / 15.5f, dy = (y - 15.5f) / 15.5f, r = sqrtf(dx * dx + dy * dy), v = r < 0.12f ? 1 : clampf(1 - (r - 0.12f) / 0.88f, 0, 1); v = v * v;
        uint8_t c = (uint8_t)(v * 255 + 0.5f); px[(y * 32 + x) * 4] = px[(y * 32 + x) * 4 + 1] = px[(y * 32 + x) * 4 + 2] = c; px[(y * 32 + x) * 4 + 3] = 255; }   /* a soft dot, flat in the middle (the beams read the middle) */
    GL(glGenTextures)(1, &S.tex); GL(glBindTexture)(GL_TEXTURE_2D, S.tex); GL(glTexImage2D)(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL(glBindTexture)(GL_TEXTURE_2D, 0); S.vb = malloc((size_t)MAXQ * 4 * 8 * sizeof(float)); return 0;
}
static void quad(const float p[4][3], const float uv[4][2], const float c[4][3]) {
    if (S.nq >= MAXQ) return; float *o = S.vb + (size_t)S.nq * 32; S.nq++;
    for (int i = 0; i < 4; i++) { o[i * 8] = p[i][0]; o[i * 8 + 1] = p[i][1]; o[i * 8 + 2] = p[i][2]; o[i * 8 + 3] = uv[i][0]; o[i * 8 + 4] = uv[i][1]; o[i * 8 + 5] = c[i][0]; o[i * 8 + 6] = c[i][1]; o[i * 8 + 7] = c[i][2]; }
}
static void board(const float at[3], float size, const float rt[3], const float up[3], const float col[3]) {
    static const float UV[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } }; float p[4][3], c[4][3];
    for (int i = 0; i < 4; i++) { float sx = (i == 1 || i == 2) ? 1.0f : -1.0f, sy = i >= 2 ? 1.0f : -1.0f; for (int k = 0; k < 3; k++) p[i][k] = at[k] + (rt[k] * sx + up[k] * sy) * size; memcpy(c[i], col, 12); }
    quad(p, UV, c);
}
#define SIDES 10
#define RINGS 3
/* a beam: an open cone from its housing, r0 -> r1 over its range; bright along its axis, fading toward its end and toward the edges seen side-on (browser port venueBeam) */
static void beam(const sbeam *b, const float cam[3]) {
    const float *v = b->cur; float d[3] = { v[3], v[4], v[5] }, L = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]); if (L < 1e-4f || v[8] < 0.01f) return; for (int k = 0; k < 3; k++) d[k] /= L;
    float a[3] = { fabsf(d[1]) < 0.9f ? 0.0f : 1.0f, fabsf(d[1]) < 0.9f ? 1.0f : 0.0f, 0 }, u[3], w[3];
    u[0] = d[1] * a[2] - d[2] * a[1]; u[1] = d[2] * a[0] - d[0] * a[2]; u[2] = d[0] * a[1] - d[1] * a[0]; float ul = sqrtf(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]); for (int k = 0; k < 3; k++) u[k] /= ul;
    w[0] = d[1] * u[2] - d[2] * u[1]; w[1] = d[2] * u[0] - d[0] * u[2]; w[2] = d[0] * u[1] - d[1] * u[0];
    float power = fminf(1.2f, v[9]) * 0.9f * S.flick, P[RINGS][SIDES][3], C[RINGS][SIDES][3];
    for (int r = 0; r < RINGS; r++) { float t = (float)r / (RINGS - 1), rad = v[6] + (v[7] - v[6]) * t, fade = (1 - t) * (1 - t) * power;
        for (int s = 0; s < SIDES; s++) { float an = s * 6.2831853f / SIDES, cs = cosf(an), sn = sinf(an), n[3], vv[3];
            for (int k = 0; k < 3; k++) { n[k] = u[k] * cs + w[k] * sn; P[r][s][k] = v[k] + d[k] * v[8] * t + n[k] * rad; vv[k] = cam[k] - P[r][s][k]; }
            float vl = sqrtf(vv[0] * vv[0] + vv[1] * vv[1] + vv[2] * vv[2]), nv = vl > 1e-4f ? fabsf(n[0] * vv[0] + n[1] * vv[1] + n[2] * vv[2]) / vl : 0, dv = vl > 1e-4f ? fabsf(d[0] * vv[0] + d[1] * vv[1] + d[2] * vv[2]) / vl : 0;
            float al = fade * fmaxf(powf(nv, 1.7f), dv * dv * 0.35f) * BEAM_GAIN;   /* the port's soft edge; looking down the beam (where every side is edge-on) it stays lit */
            for (int k = 0; k < 3; k++) C[r][s][k] = v[10 + k] * al; } }
    static const float MID[4][2] = { { 0.5f, 0.5f }, { 0.5f, 0.5f }, { 0.5f, 0.5f }, { 0.5f, 0.5f } };
    for (int r = 0; r + 1 < RINGS; r++) for (int s = 0; s < SIDES; s++) { int s2 = (s + 1) % SIDES; float p[4][3], c[4][3];
        memcpy(p[0], P[r][s], 12); memcpy(p[1], P[r][s2], 12); memcpy(p[2], P[r + 1][s2], 12); memcpy(p[3], P[r + 1][s], 12); memcpy(c[0], C[r][s], 12); memcpy(c[1], C[r][s2], 12); memcpy(c[2], C[r + 1][s2], 12); memcpy(c[3], C[r + 1][s], 12);
        quad(p, MID, c); }
}
void r3d_lights_draw(const float *vp, const float *view) {
    if (!S.on || !S.prog) return; S.nq = 0;
    float cam[3] = { -(view[0] * view[12] + view[1] * view[13] + view[2] * view[14]), -(view[4] * view[12] + view[5] * view[13] + view[6] * view[14]), -(view[8] * view[12] + view[9] * view[13] + view[10] * view[14]) };
    float rt[3] = { view[0], view[4], view[8] }, up[3] = { view[1], view[5], view[9] };
    for (int i = 0; i < S.nB; i++) if (S.B[i].cur[9] > 0.004f) beam(&S.B[i], cam);
    for (int i = 0; i < S.nB; i++) { const float *v = S.B[i].cur; if (v[9] <= 0.004f) continue; float k = fminf(1.5f, v[9]) * S.flick * 0.55f, c[3] = { v[10] * k, v[11] * k, v[12] * k }; board(v, 0.25f + v[6] * 1.5f, rt, up, c); }   /* the housing's glow (its Flare) */
    for (int i = 0; i < S.nS; i++) { const spark *s = &S.S[i]; if (s->delay > 0) continue; float e = s->age / s->life, k = (1 - e) * 1.3f, c[3] = { k, 0.45f * k, 0.1f * k }; board(s->p, 0.12f - 0.06f * e, rt, up, c); }   /* orange 0xff731a, .1 -> .05 */
    if (!S.nq) return;
    GL(glUseProgram)(S.prog); GL(glUniformMatrix4fv)(S.uVP, 1, GL_FALSE, vp); GL(glUniform1i)(S.uTex, 0);
    GL(glActiveTexture)(GL_TEXTURE0); GL(glBindTexture)(GL_TEXTURE_2D, S.tex);
    GL(glBindBuffer)(GL_ARRAY_BUFFER, S.vbo); GL(glBufferData)(GL_ARRAY_BUFFER, (GLsizeiptr)S.nq * 4 * 8 * sizeof(float), S.vb, GL_STREAM_DRAW); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, S.ibo);
    GL(glEnableVertexAttribArray)(S.aP); GL(glEnableVertexAttribArray)(S.aT); GL(glEnableVertexAttribArray)(S.aC);
    GL(glVertexAttribPointer)(S.aP, 3, GL_FLOAT, GL_FALSE, 32, (void *)0); GL(glVertexAttribPointer)(S.aT, 2, GL_FLOAT, GL_FALSE, 32, (void *)12); GL(glVertexAttribPointer)(S.aC, 3, GL_FLOAT, GL_FALSE, 32, (void *)20);
    GL(glEnable)(GL_BLEND); GL(glBlendFunc)(GL_ONE, GL_ONE); GL(glDepthMask)(GL_FALSE); GL(glEnable)(GL_DEPTH_TEST);
    GL(glDrawElements)(GL_TRIANGLES, S.nq * 6, GL_UNSIGNED_SHORT, (void *)0);
    GL(glDisableVertexAttribArray)(S.aP); GL(glDisableVertexAttribArray)(S.aT); GL(glDisableVertexAttribArray)(S.aC);
    GL(glBindBuffer)(GL_ARRAY_BUFFER, 0); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, 0); GL(glDepthMask)(GL_TRUE);
}
void r3d_lights_shutdown(void) {
    r3d_lights_unload(); if (S.prog) GL(glDeleteProgram)(S.prog); if (S.vbo) GL(glDeleteBuffers)(1, &S.vbo); if (S.ibo) GL(glDeleteBuffers)(1, &S.ibo); if (S.tex) GL(glDeleteTextures)(1, &S.tex);
    free(S.vb); S.vb = NULL; S.prog = S.vbo = S.ibo = S.tex = 0;
}
