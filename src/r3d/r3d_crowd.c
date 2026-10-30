/* The crowd on the 3D stage: see r3d_crowd.h. A port of the browser port's crowd (index.html crowdOf, crowdUse,
 * crowdRead, crowdFigure, the panels of sceneVenue and their tick), with the figures redrawn at a low rate. */
#include "r3d_crowd.h"
#include "r3d_gl.h"
#include "r3d_skin.h"
#include "r3d_zone.h"
#include "app.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define GL(name) r3d_##name
#define CB 32                /* bones a crowd skeleton may have (gh3_ped: 28) */
#define KINDS 16
#define TW 128               /* the figure's picture, as the game's render target */
#define TH 256
#define FADE 0.3f
#define PED_SKE 0xbb6e508cu  /* gh3_ped */
#define GL_MAX_VERTEX_UNIFORM_VECTORS 0x8DFB
#define GL_BYTE 0x1400
static void (*glUniform4fv_)(GLint, GLsizei, const GLfloat *); static void (*discard_)(GLenum, GLsizei, const GLenum *);

/* ------------------------------------------------------------------ the game's lists (qb.pak): each venue's crowd kinds, crowd_profiles, the anim sets */
typedef struct { uint32_t id; qbv *v; } keptsec;
static struct { int tried, ok; keptsec *sec; int nsec; const qbv *prof; } L;
static uint32_t K_IDLE, K_STARPOWER, K_SLOW, K_MED, K_FAST, K_BAD, K_NORMAL, K_GOOD, K_SKA, K_SKIN, K_TEX, K_SKE, K_QB;
static qbv *qdup(const qbv *v) {
    if (!v) return NULL; qbv *o = malloc(sizeof *o); *o = *v; o->s = v->s ? strdup(v->s) : NULL; o->items = NULL; o->arr = NULL;
    if (v->nitems > 0 && v->items) { o->items = malloc(v->nitems * sizeof *o->items); for (int i = 0; i < v->nitems; i++) { o->items[i] = v->items[i]; o->items[i].v = qdup(v->items[i].v); } } else o->nitems = 0;
    if (v->narr > 0 && v->arr) { o->arr = malloc(v->narr * sizeof *o->arr); for (int i = 0; i < v->narr; i++) o->arr[i] = qdup(v->arr[i]); } else o->narr = 0;
    return o;
}
static const qbv *sec(uint32_t id) { for (int i = 0; i < L.nsec; i++) if (L.sec[i].id == id) return L.sec[i].v; return NULL; }
static void keep(uint32_t id, const qbv *v) { if (!id || !v || sec(id)) return; L.sec = realloc(L.sec, (L.nsec + 1) * sizeof *L.sec); L.sec[L.nsec].id = id; L.sec[L.nsec].v = qdup(v); L.nsec++; }
static int lib_load(void) {
    if (L.tried) return L.ok; L.tried = 1; Uint32 t0 = SDL_GetTicks();
    K_IDLE = gh3_qbkey("idle"); K_STARPOWER = gh3_qbkey("starpower"); K_SLOW = gh3_qbkey("slow"); K_MED = gh3_qbkey("med"); K_FAST = gh3_qbkey("fast");
    K_BAD = gh3_qbkey("bad"); K_NORMAL = gh3_qbkey("normal"); K_GOOD = gh3_qbkey("good"); K_SKA = gh3_qbkey(".ska"); K_SKIN = gh3_qbkey(".skin"); K_TEX = gh3_qbkey(".tex"); K_SKE = gh3_qbkey(".ske"); K_QB = gh3_qbkey(".qb");
    size_t pl, bl; uint8_t *pak = app_read_data("pak", "qb.pak.xen", &pl), *pab = app_read_data("pak", "qb.pab.xen", &bl);
    if (!pak || !pab) { free(pak); free(pab); SDL_Log("crowd: no PAK/qb.pak.xen + qb.pab.xen"); return 0; }
    gh3_pakent *ents; int ne = gh3_pak_parse(pak, pl, pab, bl, &ents); uint32_t kp = gh3_qbkey("crowd_profiles");
    qbsection **fs = calloc(ne ? ne : 1, sizeof *fs); int *fn = calloc(ne ? ne : 1, sizeof *fn);
    for (int e = 0; e < ne; e++) if (ents[e].type == K_QB && ents[e].data) { fn[e] = gh3_qb_parse(ents[e].data, ents[e].len, &fs[e]); if (fn[e] < 0) { fn[e] = 0; fs[e] = NULL; } }
    /* every venue's list (an array of structs naming a textureasset), crowd_profiles; then the anim sets the profiles name */
    for (int e = 0; e < ne; e++) for (int s = 0; s < fn[e]; s++) { const qbv *v = fs[e][s].v; if (!v) continue;
        if (fs[e][s].id == kp) keep(kp, v); else if (v->kind == QB_ARRAY && v->narr > 0 && v->arr[0] && v->arr[0]->kind == QB_STRUCT && qb_get(v->arr[0], "textureasset") && qb_get(v->arr[0], "model")) keep(fs[e][s].id, v); }
    L.prof = sec(kp); uint32_t want[32]; int nw = 0;
    if (L.prof && L.prof->kind == QB_STRUCT) for (int i = 0; i < L.prof->nitems && nw < 32; i++) { uint32_t k; if (qb_keyof(qb_get(L.prof->items[i].v, "anim_set"), &k)) want[nw++] = k; }
    for (int e = 0; e < ne; e++) for (int s = 0; s < fn[e]; s++) for (int w = 0; w < nw; w++) if (fs[e][s].id == want[w]) keep(want[w], fs[e][s].v);
    for (int e = 0; e < ne; e++) if (fs[e]) gh3_qb_free_sections(fs[e], fn[e]); free(fs); free(fn); free(ents); free(pak); free(pab);
    L.prof = sec(kp); L.ok = L.prof != NULL; SDL_Log("crowd: %d lists kept (crowd_profiles %s), %u ms", L.nsec, L.prof ? "found" : "missing", SDL_GetTicks() - t0); return L.ok;
}

/* ------------------------------------------------------------------ shared: the skeleton, the animations, the models (kept across venues) */
typedef struct { int first, count; GLuint tex; float aref; } cdraw;
typedef struct { char pak[32]; GLuint vbo, ibo; cdraw d[12]; int nd; float inv[CB][12]; int verts, tris, ok; GLuint tex[12]; uint32_t texKey[12]; int ntex; size_t vram; } model;
static struct { int tried, hasSke; r3d_ske ske; r3d_clip *clips; int nclips; size_t animBytes; model m[KINDS]; int nm; } S;   /* models by pak, fixed so the kinds may point at them */
static int clip_cmp(const void *a, const void *b) { uint32_t x = ((const r3d_clip *)a)->key, y = ((const r3d_clip *)b)->key; return x < y ? -1 : x > y; }
static const r3d_clip *clip_of(uint32_t key) { int lo = 0, hi = S.nclips - 1; while (lo <= hi) { int mid = (lo + hi) >> 1; if (S.clips[mid].key == key) return &S.clips[mid]; if (S.clips[mid].key < key) lo = mid + 1; else hi = mid - 1; } return NULL; }
static void rest_world(const r3d_ske *s, float (*wq)[4], float (*wp)[3]) {
    for (int i = 0; i < s->n && i < CB; i++) { int p = s->parent[i]; if (p < 0) { memcpy(wq[i], s->q[i], 16); memcpy(wp[i], s->pos[i], 12); continue; }
        r3q_mul(wq[p], s->q[i], wq[i]); float v[3]; r3q_rot(wq[p], s->pos[i], v); for (int c = 0; c < 3; c++) wp[i][c] = wp[p][c] + v[c]; }
}
static void rows_of(const float q[4], const float p[3], float m[12]) {
    float x = q[0], y = q[1], z = q[2], w = q[3];
    m[0] = 1 - 2 * (y * y + z * z); m[1] = 2 * (x * y - w * z); m[2] = 2 * (x * z + w * y); m[3] = p[0];
    m[4] = 2 * (x * y + w * z); m[5] = 1 - 2 * (x * x + z * z); m[6] = 2 * (y * z - w * x); m[7] = p[1];
    m[8] = 2 * (x * z - w * y); m[9] = 2 * (y * z + w * x); m[10] = 1 - 2 * (x * x + y * y); m[11] = p[2];
}
static void rows_inv(const float a[12], float o[12]) { for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) o[r * 4 + c] = a[c * 4 + r]; for (int r = 0; r < 3; r++) o[r * 4 + 3] = -(o[r * 4] * a[3] + o[r * 4 + 1] * a[7] + o[r * 4 + 2] * a[11]); }
static void rows_mul(const float a[12], const float b[12], float o[12]) { float t[12]; for (int r = 0; r < 3; r++) { for (int c = 0; c < 3; c++) t[r * 4 + c] = a[r * 4] * b[c] + a[r * 4 + 1] * b[4 + c] + a[r * 4 + 2] * b[8 + c]; t[r * 4 + 3] = a[r * 4] * b[3] + a[r * 4 + 1] * b[7] + a[r * 4 + 2] * b[11] + a[r * 4 + 3]; } memcpy(o, t, 48); }

/* ------------------------------------------------------------------ GPU: the figures' skinning shader (the band's, with the whole 28-bone palette in one draw) */
static struct { int ready, s3tc; GLuint prog; GLint aPos, aNrm, aUv, aIdx, aW, uVP, uB, uLdir, uLcol, uAmb, uMap, uAref; } G;
static const char *VS =
    "uniform mat4 uVP; uniform vec4 uB[96]; uniform vec3 uLdir; uniform vec3 uLcol; uniform vec3 uAmb;\n"
    "attribute vec3 aPos; attribute vec3 aNrm; attribute vec2 aUv; attribute vec3 aIdx; attribute vec3 aW;\n"
    "varying vec2 vUv; varying vec3 vL;\n"
    "void main() {\n"
    "  int i0 = int(aIdx.x), i1 = int(aIdx.y), i2 = int(aIdx.z);\n"
    "  vec4 r0 = uB[i0] * aW.x + uB[i1] * aW.y + uB[i2] * aW.z;\n"
    "  vec4 r1 = uB[i0 + 1] * aW.x + uB[i1 + 1] * aW.y + uB[i2 + 1] * aW.z;\n"
    "  vec4 r2 = uB[i0 + 2] * aW.x + uB[i1 + 2] * aW.y + uB[i2 + 2] * aW.z;\n"
    "  vec4 p = vec4(aPos, 1.0);\n"
    "  vec3 n = vec3(dot(r0.xyz, aNrm), dot(r1.xyz, aNrm), dot(r2.xyz, aNrm)); float ln = length(n); n = ln > 1e-5 ? n / ln : vec3(0.0, 0.0, 1.0);\n"
    "  float d = dot(n, uLdir); vL = uAmb + uLcol * (max(d, 0.0) * 0.8 + 0.2 * (0.5 + 0.5 * d));\n"
    "  vUv = aUv; gl_Position = uVP * vec4(dot(r0, p), dot(r1, p), dot(r2, p), 1.0);\n"
    "}\n";
static const char *FS =
    "#ifdef GL_ES\nprecision mediump float;\n#endif\n"
    "uniform sampler2D uMap; uniform float uAref; varying vec2 vUv; varying vec3 vL;\n"
    "void main() { vec4 t = texture2D(uMap, vUv); if (t.a < uAref) discard; gl_FragColor = vec4(t.rgb * vL, 1.0); }\n";
static GLuint shader(GLenum kind, const char *src) {
    GLuint s = GL(glCreateShader)(kind); GL(glShaderSource)(s, 1, &src, NULL); GL(glCompileShader)(s); GLint ok = 0; GL(glGetShaderiv)(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[1024]; GL(glGetShaderInfoLog)(s, sizeof log, NULL, log); SDL_Log("crowd: shader: %s", log); GL(glDeleteShader)(s); return 0; }
    return s;
}
int r3d_crowd_init(void) {
    memset(&G, 0, sizeof G);
    glUniform4fv_ = (void (*)(GLint, GLsizei, const GLfloat *))SDL_GL_GetProcAddress("glUniform4fv"); if (!glUniform4fv_) return -1;
    discard_ = (void (*)(GLenum, GLsizei, const GLenum *))SDL_GL_GetProcAddress("glDiscardFramebufferEXT");
    GLint mv = 0; while (GL(glGetError)() != GL_NO_ERROR) {} GL(glGetIntegerv)(GL_MAX_VERTEX_UNIFORM_VECTORS, &mv); while (GL(glGetError)() != GL_NO_ERROR) {}
    if (mv > 0 && mv < 96 + 12) { SDL_Log("crowd: %d vertex uniforms: too few", mv); return -1; }
    GLuint v = shader(GL_VERTEX_SHADER, VS), f = shader(GL_FRAGMENT_SHADER, FS); if (!v || !f) return -1;
    G.prog = GL(glCreateProgram)(); GL(glAttachShader)(G.prog, v); GL(glAttachShader)(G.prog, f);
    static const char *ATTR[5] = { "aPos", "aNrm", "aUv", "aIdx", "aW" }; for (int i = 0; i < 5; i++) GL(glBindAttribLocation)(G.prog, i, ATTR[i]);
    GL(glLinkProgram)(G.prog); GL(glDeleteShader)(v); GL(glDeleteShader)(f);
    GLint ok = 0; GL(glGetProgramiv)(G.prog, GL_LINK_STATUS, &ok); if (!ok) { char log[1024]; GL(glGetProgramInfoLog)(G.prog, sizeof log, NULL, log); SDL_Log("crowd: link: %s", log); GL(glDeleteProgram)(G.prog); G.prog = 0; return -1; }
    G.aPos = 0; G.aNrm = 1; G.aUv = 2; G.aIdx = 3; G.aW = 4;
#define U(n) G.n = GL(glGetUniformLocation)(G.prog, #n)
    U(uVP); U(uLdir); U(uLcol); U(uAmb); U(uMap); U(uAref); G.uB = GL(glGetUniformLocation)(G.prog, "uB[0]"); if (G.uB < 0) G.uB = GL(glGetUniformLocation)(G.prog, "uB");
#undef U
    const char *ext = (const char *)GL(glGetString)(GL_EXTENSIONS); G.s3tc = ext && (strstr(ext, "GL_EXT_texture_compression_s3tc") || (strstr(ext, "GL_EXT_texture_compression_dxt1") && strstr(ext, "GL_ANGLE_texture_compression_dxt"))); if (getenv("R3D_NODXT")) G.s3tc = 0;
    G.ready = 1; return 0;
}

static GLuint upload_tex(const uint8_t *bank, size_t bankLen, const r3d_texent *ents, int nents, uint32_t key, size_t *bytes) {
    const r3d_texent *e = NULL; for (int i = 0; i < nents; i++) if (ents[i].key == key) { e = &ents[i]; break; }
    r3d_dds d; if (!e || e->off + e->size > bankLen || r3d_dds_info(bank + e->off, e->size, &d)) return 0;
    const uint8_t *b = bank + e->off; int l0 = 0; while (l0 + 1 < d.nlev && (d.lev[l0].w > 256 || d.lev[l0].h > 256)) l0++;
    int w = d.lev[l0].w, h = d.lev[l0].h, full = d.lev[d.nlev - 1].w == 1 && d.lev[d.nlev - 1].h == 1, gpu = G.s3tc && d.cc != DDS_RGBA32, mips = 0;
    GLuint id; GL(glGenTextures)(1, &id); GL(glBindTexture)(GL_TEXTURE_2D, id);
    if (gpu) { GLenum fmt = d.cc == DDS_DXT1 ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT : d.cc == DDS_DXT3 ? GL_COMPRESSED_RGBA_S3TC_DXT3_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; int nl = full ? d.nlev - l0 : 1;
        for (int l = 0; l < nl; l++) { GL(glCompressedTexImage2D)(GL_TEXTURE_2D, l, fmt, d.lev[l0 + l].w, d.lev[l0 + l].h, 0, (GLsizei)d.lev[l0 + l].size, b + d.lev[l0 + l].off); *bytes += d.lev[l0 + l].size; } mips = nl > 1; }
    else { uint8_t *rgba = malloc((size_t)w * h * 4); int trans = r3d_dds_decode(b, e->size, &d, l0, rgba); uint16_t *p16 = malloc((size_t)w * h * 2); GL(glPixelStorei)(GL_UNPACK_ALIGNMENT, 1);
        if (!trans) { for (int i = 0; i < w * h; i++) p16[i] = (uint16_t)(((rgba[i * 4] >> 3) << 11) | ((rgba[i * 4 + 1] >> 2) << 5) | (rgba[i * 4 + 2] >> 3)); GL(glTexImage2D)(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, p16); }
        else { for (int i = 0; i < w * h; i++) p16[i] = (uint16_t)(((rgba[i * 4] >> 4) << 12) | ((rgba[i * 4 + 1] >> 4) << 8) | ((rgba[i * 4 + 2] >> 4) << 4) | (rgba[i * 4 + 3] >> 4)); GL(glTexImage2D)(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, p16); }
        GL(glPixelStorei)(GL_UNPACK_ALIGNMENT, 4); free(rgba); free(p16); *bytes += (size_t)w * h * 2;
        if (!(w & (w - 1)) && !(h & (h - 1))) { GL(glGenerateMipmap)(GL_TEXTURE_2D); mips = 1; *bytes += (size_t)w * h * 2 / 3; } }
    GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mips ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR); GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    GL(glBindTexture)(GL_TEXTURE_2D, 0); return id;
}
typedef struct { float pos[3]; int8_t nrm[4]; float uv[2]; uint8_t idx[4], w[4]; } cvtx;   /* 32 bytes, as the band's */
/* a crowd model: PAK/<pak>.pak.xen, its largest .skin and .tex, bound to the gh3_ped rest pose */
static model *model_get(const char *pak) {
    for (int i = 0; i < S.nm; i++) if (!strcmp(S.m[i].pak, pak)) return S.m[i].ok ? &S.m[i] : NULL;
    if (S.nm >= KINDS) return NULL; model *g = &S.m[S.nm++]; memset(g, 0, sizeof *g); snprintf(g->pak, sizeof g->pak, "%s", pak);
    char fn[64]; snprintf(fn, sizeof fn, "%s.pak.xen", pak); const char *pp = app_data_path("pak", fn); r3d_pakfile pk; if (!pp || r3d_pak_open(&pk, pp, NULL)) { SDL_Log("crowd: no PAK/%s", fn); return NULL; }
    size_t sn = 0, tn = 0; uint8_t *sb = r3d_pak_read(&pk, r3d_pak_biggest(&pk, K_SKIN), &sn), *tb = r3d_pak_read(&pk, r3d_pak_biggest(&pk, K_TEX), &tn); r3d_pak_close(&pk);
    r3d_texent *te = NULL; int nt = tb ? r3d_texbank_read(tb, tn, &te) : 0; r3d_skin sk; int nb = S.ske.n;
    if (!sb || r3d_skin_read(sb, sn, &sk)) { free(sb); free(tb); free(te); SDL_Log("crowd: %s: no model", pak); return NULL; }
    { float rq[CB][4], rp[CB][3]; rest_world(&S.ske, rq, rp); for (int i = 0; i < nb; i++) { float r[12]; rows_of(rq[i], rp[i], r); rows_inv(r, g->inv[i]); } }
    cvtx *V = NULL; uint16_t *I = NULL; int nv = 0, ni = 0;
    for (int mi = 0; mi < sk.nm && g->nd < 12; mi++) { const r3d_skinmesh *me = &sk.m[mi]; const r3d_skinmat *mat = r3d_skin_mat(&sk, me->mat); if (!mat || !mat->ntex || nv + me->nvert > 65535) continue;
        int pic = 0; if (mat->ntex > 1) { const r3d_texent *e = NULL; for (int k = 0; k < nt; k++) if (te[k].key == mat->tex[0]) e = &te[k];   /* the picture first, a surface-detail map second (or the other way round) */
            if (e) { r3d_look lk = r3d_tex_look(tb + e->off, e->size); if (lk.normal || (lk.b > lk.r + 35 && lk.b > lk.g + 35 && fabsf(lk.r - lk.g) < 25)) pic = 1; } }
        GLuint tex = 0; for (int k = 0; k < g->ntex; k++) if (g->texKey[k] == mat->tex[pic]) tex = g->tex[k];
        if (!tex && g->ntex < 12) { tex = upload_tex(tb, tn, te, nt, mat->tex[pic], &g->vram); if (tex) { g->texKey[g->ntex] = mat->tex[pic]; g->tex[g->ntex++] = tex; } }
        if (!tex) continue;
        cdraw *d = &g->d[g->nd++]; d->tex = tex; d->first = ni; d->count = me->ntri * 3; d->aref = mat->alphaRef > 0 ? mat->alphaRef / 255.0f : mat->blend ? 0.35f : 0;
        V = realloc(V, (nv + me->nvert) * sizeof *V); I = realloc(I, (ni + me->ntri * 3) * 2);
        for (int v = 0; v < me->nvert; v++) { cvtx *o = &V[nv + v]; memcpy(o->pos, me->pos + v * 3, 12); o->uv[0] = me->uv[v * 2]; o->uv[1] = me->uv[v * 2 + 1];
            for (int c = 0; c < 3; c++) { float x = me->nrm[v * 3 + c] * 127; o->nrm[c] = (int8_t)(x < -127 ? -127 : x > 127 ? 127 : lrintf(x)); } o->nrm[3] = 0;
            int q0 = (int)lrintf(me->w[v * 3] * 255), q1 = (int)lrintf(me->w[v * 3 + 1] * 255), q2 = 255 - q0 - q1; if (q2 < 0) { q1 += q2; q2 = 0; } if (q1 < 0) { q0 += q1; q1 = 0; } if (me->w[v * 3 + 2] <= 0) { q0 += q2; q2 = 0; }
            o->w[0] = (uint8_t)q0; o->w[1] = (uint8_t)q1; o->w[2] = (uint8_t)q2; o->w[3] = 0;
            for (int j = 0; j < 3; j++) { int bb = me->bone[v * 3 + j]; if (bb >= nb) bb = 0; o->idx[j] = (uint8_t)(bb * 3); } o->idx[3] = 0; }
        for (int i = 0; i < me->ntri * 3; i++) I[ni + i] = (uint16_t)(me->idx[i] + nv);
        nv += me->nvert; ni += me->ntri * 3; }
    if (nv) { GL(glGenBuffers)(1, &g->vbo); GL(glBindBuffer)(GL_ARRAY_BUFFER, g->vbo); GL(glBufferData)(GL_ARRAY_BUFFER, (GLsizeiptr)nv * sizeof *V, V, GL_STATIC_DRAW);
        GL(glGenBuffers)(1, &g->ibo); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, g->ibo); GL(glBufferData)(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)ni * 2, I, GL_STATIC_DRAW);
        GL(glBindBuffer)(GL_ARRAY_BUFFER, 0); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, 0); g->vram += (size_t)nv * sizeof *V + (size_t)ni * 2; g->ok = g->nd > 0; }
    g->verts = nv; g->tris = ni / 3; free(V); free(I); r3d_skin_free(&sk); free(sb); free(tb); free(te);
    return g->ok ? g : NULL;
}
static void model_free(model *g) { if (g->vbo) GL(glDeleteBuffers)(1, &g->vbo); if (g->ibo) GL(glDeleteBuffers)(1, &g->ibo); for (int i = 0; i < g->ntex; i++) GL(glDeleteTextures)(1, &g->tex[i]); memset(g, 0, sizeof *g); }

/* ------------------------------------------------------------------ the venue's kinds: one picture each (a cell of one atlas), every card of the kind wears it */
typedef struct {
    uint32_t panel; char pak[32]; int standIn; const qbv *set; model *m; float w, h; int carded;
    int drawn, cx, cy; double due; unsigned seen;
    const r3d_clip *cur, *prev; float t, tp, fade; uint32_t kind; int mood, n;
} kind;
/* the atlas: COLS cells across, a power of two of rows (mipmapped: far cards take the smaller levels); one framebuffer and depth for it */
#define COLS 4
static struct { char zone[32]; kind k[KINDS]; int nk; unsigned frame; double lastPerf; int hz, perFrame; r3d_crowdstats st; int forceMood, forceSp; float bpm;
    GLuint atlas, fbo, depth; int rows, aw, ah, bad; } C;
static int want_keys(const qbv *v, uint32_t *keys, int n, int cap, int depth) {
    if (!v || depth > 8) return n; uint32_t k;
    if (v->kind == QB_ARRAY) { for (int i = 0; i < v->narr; i++) n = want_keys(v->arr[i], keys, n, cap, depth + 1); return n; }
    if (v->kind == QB_STRUCT) { for (int i = 0; i < v->nitems; i++) n = want_keys(v->items[i].v, keys, n, cap, depth + 1); return n; }
    if (qb_keyof(v, &k) && n < cap) { for (int i = 0; i < n; i++) if (keys[i] == k) return n; keys[n++] = k; } return n;
}
static void basename_of(const char *path, char *out, size_t n) {   /* "Real_Crowd\Crowd_Ped_01.skin" -> "crowd_ped_01" */
    const char *b = path; for (const char *p = path; *p; p++) if (*p == '\\' || *p == '/') b = p + 1;
    snprintf(out, n, "%s", b); char *d = strrchr(out, '.'); if (d) *d = 0; for (char *c = out; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32;
}
static int pak_exists(const char *pak) { char fn[64]; snprintf(fn, sizeof fn, "%s.pak.xen", pak); return app_data_path("pak", fn) != NULL; }
static void shared_load(void) {
    if (S.tried) return; S.tried = 1;
    const char *gp = app_data_path("zones", "global.pak.xen"), *gb = app_data_path("zones", "global.pab.xen"); char gpc[700] = "", gbc[700] = ""; if (gp) snprintf(gpc, sizeof gpc, "%s", gp); if (gb) snprintf(gbc, sizeof gbc, "%s", gb);
    r3d_pakfile g; if (gp && gb && r3d_pak_open(&g, gpc, gbc) == 0) { size_t n; uint8_t *b = r3d_pak_read(&g, r3d_pak_find(&g, K_SKE, PED_SKE), &n); if (b) { S.hasSke = r3d_ske_read(b, n, &S.ske) == 0 && S.ske.n <= CB; free(b); } r3d_pak_close(&g); }
    if (!S.hasSke) SDL_Log("crowd: no gh3_ped skeleton (%08x) in ZONES/global", PED_SKE);
}
/* the animations the venue's sets name, from perm_anims (the crowd's 28-bone clips live there), added to the shared table */
static void clips_load(void) {
    uint32_t keys[512]; int nk = 0; for (int i = 0; i < C.nk; i++) nk = want_keys(C.k[i].set, keys, nk, 512, 0);
    int need = 0; for (int i = 0; i < nk; i++) if (!clip_of(keys[i])) keys[need++] = keys[i]; if (!need) return;
    const char *pp = app_data_path("pak", "perm_anims.pak.xen"); r3d_pakfile pk; if (!pp || r3d_pak_open(&pk, pp, NULL)) { SDL_Log("crowd: no PAK/perm_anims"); return; }
    S.clips = realloc(S.clips, (S.nclips + need) * sizeof *S.clips);
    for (int i = 0; i < need; i++) { int e = r3d_pak_find(&pk, K_SKA, keys[i]); if (e < 0) continue; size_t n; uint8_t *b = r3d_pak_read(&pk, e, &n); if (!b) continue;
        r3d_clip c; if (r3d_ska_read(b, n, keys[i], &c) == 0) { if (c.nb > S.ske.n + 4) r3d_clip_free(&c); else { S.clips[S.nclips++] = c; S.animBytes += r3d_clip_bytes(&c); } } free(b); }
    r3d_pak_close(&pk); qsort(S.clips, S.nclips, sizeof *S.clips, clip_cmp);
}
/* ------------------------------------------------------------------ the lighters: which parts, and when the song has them up */
static uint32_t LK[24];
static struct { char name[64]; int (*ev)[2]; int nev; } SL;   /* [time ms, up] */
int r3d_crowd_lighter(uint32_t sector) { if (sector) for (int i = 0; i < 24; i++) if (LK[i] == sector) return i + 1; return 0; }
void r3d_crowd_select(void) {
    const char *name = A.lib && A.selSong >= 0 && A.selSong < A.lib->nsongs ? A.lib->songs[A.selSong].name : ""; if (!strcmp(SL.name, name)) return;
    free(SL.ev); memset(&SL, 0, sizeof SL); snprintf(SL.name, sizeof SL.name, "%s", name); if (!*name) return;
    char fn[128]; snprintf(fn, sizeof fn, "%s_song.pak.xen", name); size_t len; uint8_t *pak = app_read_data("songs", fn, &len); if (!pak) return;
    gh3_pakent *ents; int ne = gh3_pak_parse(pak, len, NULL, 0, &ents); snprintf(fn, sizeof fn, "%s_performance", name);
    uint32_t kp = gh3_qbkey(fn), kq = gh3_qbkey(".qb"), K_UP = gh3_qbkey("crowd_startlighters"), K_DOWN = gh3_qbkey("crowd_stoplighters"), K_TIME = gh3_qbkey("time"), K_SCR = gh3_qbkey("scr");
    for (int e = 0; e < ne && !SL.ev; e++) { if (ents[e].type != kq || !ents[e].data) continue; qbsection *s; int ns = gh3_qb_parse(ents[e].data, ents[e].len, &s); if (ns < 0) continue;
        for (int i = 0; i < ns; i++) { const qbv *a = s[i].v; if (s[i].id != kp || !a || a->kind != QB_ARRAY) continue; SL.ev = calloc(a->narr + 1, sizeof *SL.ev);
            for (int j = 0; j < a->narr; j++) { uint32_t sc; if (!qb_keyof(qb_get(a->arr[j], "scr"), &sc) && !qb_keyof(qb_getk(a->arr[j], K_SCR), &sc)) continue; if (sc != K_UP && sc != K_DOWN) continue;
                SL.ev[SL.nev][0] = (int)qb_num(qb_getk(a->arr[j], K_TIME), 0); SL.ev[SL.nev][1] = sc == K_UP; SL.nev++; }
            for (int x = 1; x < SL.nev; x++) for (int y = x; y > 0 && SL.ev[y][0] < SL.ev[y - 1][0]; y--) { int t0 = SL.ev[y][0], t1 = SL.ev[y][1]; SL.ev[y][0] = SL.ev[y - 1][0]; SL.ev[y][1] = SL.ev[y - 1][1]; SL.ev[y - 1][0] = t0; SL.ev[y - 1][1] = t1; } }
        gh3_qb_free_sections(s, ns); }
    free(ents); free(pak); if (SL.nev) SDL_Log("crowd: %s: lighters up %d times", name, (SL.nev + 1) / 2);
}
int r3d_crowd_lighters(void) {
    const char *e = getenv("R3D_CROWD_LIGHTERS"); if (e && *e) return atoi(e) > 0; if (!SET.stageFx || !SL.nev) return 0;
    if (!(A.screen == SC_PLAY && A.S.loaded && A.S.song && !strcmp(A.S.song->name, SL.name))) return 0;
    double ms = A.S.run.now; int up = 0; for (int i = 0; i < SL.nev && SL.ev[i][0] <= ms; i++) up = SL.ev[i][1]; return up;
}
void r3d_crowd_zone(const char *zone) {
    r3d_crowd_unload(); snprintf(C.zone, sizeof C.zone, "%s", zone); memset(&C.st, 0, sizeof C.st);
    for (int i = 0; i < 24; i++) { char nm[64]; snprintf(nm, sizeof nm, "%s_lighter_geo%02d", zone, i + 1); LK[i] = gh3_qbkey(nm); }
    { const char *e = getenv("R3D_CROWD"); if (e && *e == '0') { memset(LK, 0, sizeof LK); return; } }   /* R3D_CROWD=0: no crowd, no lighters (to compare costs) */
    if (!G.ready || !lib_load()) return; shared_load(); if (!S.hasSke) return; Uint32 t0 = SDL_GetTicks();
    char nm[64]; snprintf(nm, sizeof nm, "%s_crowd_models", zone); const qbv *list = sec(gh3_qbkey(nm)); if (!list || list->kind != QB_ARRAY) { SDL_Log("crowd: %s lists no crowd", zone); return; }
    static const char *SPARE[] = { "crowd_ped_01", "crowd_ped_02", "crowd_ped_03", "crowd_ped_04", "crowd_pedf_1", "crowd_pedf_2" }; const char *spare[6]; int ns = 0;
    for (int i = 0; i < 6; i++) if (pak_exists(SPARE[i])) spare[ns++] = SPARE[i];
    for (int i = 0; i < list->narr && C.nk < KINDS; i++) { const qbv *it = list->arr[i]; const char *mp = qb_str(qb_get(it, "model")); const qbv *tx = qb_get(it, "textureasset"); uint32_t pk = 0, nk = 0, sk = 0;
        if (!mp || !*mp) continue; if (tx && tx->kind == QB_STR && tx->s) pk = gh3_qbkey(tx->s); else if (!qb_keyof(tx, &pk)) continue;
        int dup = 0; for (int k = 0; k < C.nk; k++) if (C.k[k].panel == pk) dup = 1; if (dup) continue;   /* the first one of a picture wins, as in the browser port */
        kind *k = &C.k[C.nk]; memset(k, 0, sizeof *k); k->panel = pk; basename_of(mp, k->pak, sizeof k->pak);
        if (!pak_exists(k->pak)) { if (!ns) continue; snprintf(k->pak, sizeof k->pak, "%s", spare[i % ns]); k->standIn = 1; }   /* z_hell's bikers are not on this install: the ordinary crowd stands in */
        if (qb_keyof(qb_get(it, "name"), &nk) && qb_keyof(qb_get(qb_getk(L.prof, nk), "anim_set"), &sk)) k->set = sec(sk);
        if (!k->set) continue; k->m = model_get(k->pak); if (!k->m) continue;
        k->w = 1.15f; k->h = 2.58f; k->mood = -1; k->n = C.nk; C.nk++; }
    clips_load();
    C.st.kinds = C.nk; C.st.models = S.nm; C.st.clips = S.nclips; C.st.animBytes = S.animBytes; for (int i = 0; i < S.nm; i++) C.st.vram += S.m[i].vram;
    for (int i = 0; i < C.nk; i++) { C.st.verts += C.k[i].m->verts; C.st.standIns += C.k[i].standIn; }
    { const char *e = getenv("R3D_CROWD_HZ"); C.hz = e && atoi(e) > 0 ? atoi(e) : 15; e = getenv("R3D_CROWD_PERFRAME"); C.perFrame = e && atoi(e) > 0 ? atoi(e) : KINDS; }
    if (C.nk) { C.rows = 1; while (C.rows * COLS < C.nk) C.rows *= 2; C.aw = COLS * TW; C.ah = C.rows * TH; uint8_t *zero = calloc((size_t)C.aw * C.ah, 4);
        for (int i = 0; i < C.nk; i++) { C.k[i].cx = i % COLS; C.k[i].cy = i / COLS; }
        GL(glGenTextures)(1, &C.atlas); GL(glBindTexture)(GL_TEXTURE_2D, C.atlas); GL(glTexImage2D)(GL_TEXTURE_2D, 0, GL_RGBA, C.aw, C.ah, 0, GL_RGBA, GL_UNSIGNED_BYTE, zero); GL(glGenerateMipmap)(GL_TEXTURE_2D);
        GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST); GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); GL(glBindTexture)(GL_TEXTURE_2D, 0); free(zero);
        C.st.vram += (size_t)C.aw * C.ah * 4 * 4 / 3 + (size_t)C.aw * C.ah * 2; }   /* the atlas with its levels, and its depth */
    C.st.loadMs = SDL_GetTicks() - t0;
    SDL_Log("crowd: %s: %d kinds (%d models, %d stand-ins), %d animations (%.1f MB), %d points a round, %.0f ms", zone, C.nk, S.nm, C.st.standIns, S.nclips, S.animBytes / 1048576.0, C.st.verts, C.st.loadMs);
}
int r3d_crowd_card(uint32_t texKey, const float *pos, const float *ctr, int nvert) {
    for (int i = 0; i < C.nk; i++) { kind *k = &C.k[i]; if (k->panel != texKey) continue;
        if (!k->carded && nvert >= 4) {   /* the card's size from its first quad (offsets from the centre): the figure is drawn at that scale, feet on its bottom edge */
            float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f; for (int v = 0; v < 4; v++) { float dx = pos[v * 3] - ctr[v * 3], dy = pos[v * 3 + 1] - ctr[v * 3 + 1], dz = pos[v * 3 + 2] - ctr[v * 3 + 2], hx = sqrtf(dx * dx + dz * dz) * (dx + dz < 0 ? -1 : 1);
                if (hx < x0) x0 = hx; if (hx > x1) x1 = hx; if (dy < y0) y0 = dy; if (dy > y1) y1 = dy; }
            if (x1 - x0 > 0.2f && y1 - y0 > 0.5f) { k->w = x1 - x0; k->h = y1 - y0; } k->carded = 1; }
        return C.atlas ? i : -1; }
    return -1;
}
unsigned r3d_crowd_tex(int i) { return i >= 0 && i < C.nk ? C.atlas : 0; }
void r3d_crowd_cell(int i, float anim[4]) { anim[0] = COLS; anim[1] = (float)C.rows; anim[2] = 0; anim[3] = (float)(i >= 0 && i < C.nk ? C.k[i].cy * COLS + C.k[i].cx : 0); }
int r3d_crowd_shown(int i) { return i >= 0 && i < C.nk && C.k[i].drawn && SET.stageFx && !C.bad; }
void r3d_crowd_seen(int i) { if (i >= 0 && i < C.nk) C.k[i].seen = C.frame; }
const r3d_crowdstats *r3d_crowd_stats(void) { return &C.st; }

/* ------------------------------------------------------------------ animation (crowdFigure.tick): idle or starpower, the mood and the pace pick the list, round robin */
static int anim_keys(const qbv *kind, int mood, int speed, uint32_t *out, int max) {
    static const uint32_t *MOOD[3] = { &K_BAD, &K_NORMAL, &K_GOOD }, *SPEED[3] = { &K_SLOW, &K_MED, &K_FAST };
    const qbv *m = qb_getk(kind, *MOOD[mood]); if (!m) m = qb_getk(kind, K_NORMAL); if (!m) m = qb_getk(kind, K_GOOD);
    const qbv *l = qb_getk(m, *SPEED[speed]); if (!l) l = qb_getk(m, K_MED); int n = 0; uint32_t k;
    if (l && l->kind == QB_ARRAY) { for (int i = 0; i < l->narr && n < max; i++) if (qb_keyof(l->arr[i], &k) && clip_of(k)) out[n++] = k; } else if (qb_keyof(l, &k) && clip_of(k) && max > 0) out[n++] = k;
    return n;
}
static void next(kind *k, int speed) {
    const qbv *kd = qb_getk(k->set, k->kind); if (!kd) kd = qb_getk(k->set, K_IDLE); uint32_t keys[8]; int n = anim_keys(kd, k->mood, speed, keys, 8); if (!n) return;
    const r3d_clip *c = clip_of(keys[(k->n++) % n]); if (!c) return;
    if (k->cur) { k->prev = k->cur; k->tp = k->t; k->fade = 0; } else k->fade = 1; k->cur = c; k->t = 0;
}
static void tick(kind *k, float dt, int mood, int sp, int speed) {
    uint32_t kd = sp ? K_STARPOWER : K_IDLE; int change = kd != k->kind || mood != k->mood; k->kind = kd; k->mood = mood;
    if (!k->cur || k->t >= k->cur->dur - 0.32f || (change && k->t > 0.6f)) next(k, speed);
    if (!k->cur) return; k->t += dt; if (k->t > k->cur->dur) k->t = k->cur->dur;
    if (k->prev) { k->tp += dt; if (k->tp > k->prev->dur) k->tp = k->prev->dur; } k->fade += dt / FADE; if (k->fade >= 1) { k->fade = 1; k->prev = NULL; }
}
static void pose(const kind *k, float skin[CB][12]) {
    int nb = S.ske.n; float wq[CB][4], wp[CB][3];
    for (int b = 0; b < nb; b++) { float q[4], p[3]; r3d_clip_bone(k->cur, &S.ske, b, k->t, q, p);
        if (k->prev && k->fade < 1) { float q2[4], p2[3], o[4]; r3d_clip_bone(k->prev, &S.ske, b, k->tp, q2, p2); r3q_nlerp(q2, q, k->fade, o); memcpy(q, o, 16); for (int c = 0; c < 3; c++) p[c] = p2[c] + (p[c] - p2[c]) * k->fade; }
        int par = S.ske.parent[b]; if (par < 0 || par >= b) { memcpy(wq[b], q, 16); memcpy(wp[b], p, 12); }
        else { r3q_mul(wq[par], q, wq[b]); float v[3]; r3q_rot(wq[par], p, v); for (int c = 0; c < 3; c++) wp[b][c] = wp[par][c] + v[c]; }
        float W[12]; rows_of(wq[b], wp[b], W); rows_mul(W, k->m->inv[b], skin[b]); }
}

/* one pass into the atlas for every figure due: the depth cleared whole, each cell cleared see-through, then drawn (a 2-texel gutter so
 * neighbours never bleed in), then the atlas's levels made once */
static int cleared;   /* the pass cleared the whole atlas (every figure redrawn): no cell clears, nothing loaded back on a tiler */
static int pass_begin(int all) {
    if (C.bad || !C.atlas) return 0;
    if (!C.fbo) { GL(glGenRenderbuffers)(1, &C.depth); GL(glBindRenderbuffer)(GL_RENDERBUFFER, C.depth); GL(glRenderbufferStorage)(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, C.aw, C.ah); GL(glBindRenderbuffer)(GL_RENDERBUFFER, 0);
        GL(glGenFramebuffers)(1, &C.fbo); GL(glBindFramebuffer)(GL_FRAMEBUFFER, C.fbo); GL(glFramebufferTexture2D)(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, C.atlas, 0); GL(glFramebufferRenderbuffer)(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, C.depth);
        if (GL(glCheckFramebufferStatus)(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { SDL_Log("crowd: the atlas's framebuffer %dx%d is not complete: no crowd", C.aw, C.ah); C.bad = 1; return 0; } }
    else GL(glBindFramebuffer)(GL_FRAMEBUFFER, C.fbo);
    GL(glViewport)(0, 0, C.aw, C.ah); GL(glDisable)(GL_SCISSOR_TEST); GL(glDepthMask)(GL_TRUE); GL(glClearColor)(0, 0, 0, 0); GL(glClear)(all ? GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT : GL_DEPTH_BUFFER_BIT); cleared = all;
    GL(glUseProgram)(G.prog); GL(glUniform1i)(G.uMap, 0); GL(glActiveTexture)(GL_TEXTURE0);
    /* the browser port's fallback light (0.45 all round, 0.55 from the front), times the gain that undoes the cards' vertex colours
     * (0.2-0.65 in the venues: the game's 128-is-white convention; the browser port leaves them out) */
    static float gain = -1; if (gain < 0) { const char *e = getenv("R3D_CROWD_GAIN"); gain = e && atof(e) > 0 ? (float)atof(e) : 1.6f; }
    static const float LD[3] = { 0.2104f, 0.6312f, 0.7890f }; GL(glUniform3f)(G.uLdir, LD[0], LD[1], LD[2]); GL(glUniform3f)(G.uLcol, 0.55f * gain, 0.55f * gain, 0.55f * gain); GL(glUniform3f)(G.uAmb, 0.45f * gain, 0.45f * gain, 0.45f * gain);
    return 1;
}
static void figure(kind *k) {   /* crowdFigure.draw: orthographic, from the front */
    if (!k->cur) return;
    float skin[CB][12]; pose(k, skin); static float pal[CB * 12]; int nb = S.ske.n; for (int b = 0; b < nb; b++) memcpy(pal + b * 12, skin[b], 48);
    int x = k->cx * TW, y = k->cy * TH; if (!cleared) { GL(glEnable)(GL_SCISSOR_TEST); GL(glScissor)(x, y, TW, TH); GL(glClear)(GL_COLOR_BUFFER_BIT); GL(glDisable)(GL_SCISSOR_TEST); } GL(glViewport)(x + 2, y + 2, TW - 4, TH - 4);
    /* the card's width across, its height up from the feet; the cell's first row is the figure's top (the cards count rows from the top) */
    float vp[16] = { 2 / k->w, 0, 0, 0, 0, -2 / k->h, 0, 0, 0, 0, -0.25f, 0, 0, 1, 0, 1 };
    GL(glUniformMatrix4fv)(G.uVP, 1, GL_FALSE, vp); glUniform4fv_(G.uB, nb * 3, pal);
    GL(glBindBuffer)(GL_ARRAY_BUFFER, k->m->vbo); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, k->m->ibo);
    GL(glVertexAttribPointer)(0, 3, GL_FLOAT, GL_FALSE, sizeof(cvtx), (void *)0); GL(glVertexAttribPointer)(1, 3, GL_BYTE, GL_TRUE, sizeof(cvtx), (void *)12);
    GL(glVertexAttribPointer)(2, 2, GL_FLOAT, GL_FALSE, sizeof(cvtx), (void *)16); GL(glVertexAttribPointer)(3, 3, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(cvtx), (void *)24); GL(glVertexAttribPointer)(4, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(cvtx), (void *)28);
    for (int i = 0; i < k->m->nd; i++) { const cdraw *d = &k->m->d[i]; GL(glBindTexture)(GL_TEXTURE_2D, d->tex); GL(glUniform1f)(G.uAref, d->aref); GL(glDrawElements)(GL_TRIANGLES, d->count, GL_UNSIGNED_SHORT, (void *)(size_t)(d->first * 2)); }
    k->drawn = 1; C.st.renders++; C.st.draws += k->m->nd;
}
static void pass_end(void) {
    if (discard_) { GLenum att = GL_DEPTH_ATTACHMENT; discard_(GL_FRAMEBUFFER, 1, &att); }
    GL(glBindTexture)(GL_TEXTURE_2D, C.atlas); GL(glGenerateMipmap)(GL_TEXTURE_2D);   /* far cards take the smaller levels */
}
/* the GL state a figure's pass touches, put back afterwards (inside the venue's frame r3d.c sets the rest again itself) */
typedef struct { GLint fbo, vp[4], prog, tex, abuf, ebuf, act; GLboolean depth, blend, cull, scis, dmask; GLfloat clear[4]; } cstate;
static void cs_save(cstate *s) {
    GL(glGetIntegerv)(GL_FRAMEBUFFER_BINDING, &s->fbo); GL(glGetIntegerv)(GL_VIEWPORT, s->vp); GL(glGetIntegerv)(GL_CURRENT_PROGRAM, &s->prog); GL(glGetIntegerv)(GL_ACTIVE_TEXTURE, &s->act);
    GL(glActiveTexture)(GL_TEXTURE0); GL(glGetIntegerv)(GL_TEXTURE_BINDING_2D, &s->tex); GL(glGetIntegerv)(GL_ARRAY_BUFFER_BINDING, &s->abuf); GL(glGetIntegerv)(GL_ELEMENT_ARRAY_BUFFER_BINDING, &s->ebuf);
    s->depth = GL(glIsEnabled)(GL_DEPTH_TEST); s->blend = GL(glIsEnabled)(GL_BLEND); s->cull = GL(glIsEnabled)(GL_CULL_FACE); s->scis = GL(glIsEnabled)(GL_SCISSOR_TEST); GL(glGetBooleanv)(GL_DEPTH_WRITEMASK, &s->dmask); GL(glGetFloatv)(GL_COLOR_CLEAR_VALUE, s->clear);
}
static void en(GLenum c, GLboolean on) { if (on) GL(glEnable)(c); else GL(glDisable)(c); }
static void cs_restore(const cstate *s) {
    GL(glBindFramebuffer)(GL_FRAMEBUFFER, (GLuint)s->fbo); GL(glViewport)(s->vp[0], s->vp[1], s->vp[2], s->vp[3]); GL(glUseProgram)((GLuint)s->prog);
    GL(glActiveTexture)(GL_TEXTURE0); GL(glBindTexture)(GL_TEXTURE_2D, (GLuint)s->tex); GL(glActiveTexture)((GLenum)s->act); GL(glBindBuffer)(GL_ARRAY_BUFFER, (GLuint)s->abuf); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, (GLuint)s->ebuf);
    en(GL_DEPTH_TEST, s->depth); en(GL_BLEND, s->blend); en(GL_CULL_FACE, s->cull); en(GL_SCISSOR_TEST, s->scis); GL(glDepthMask)(s->dmask); GL(glClearColor)(s->clear[0], s->clear[1], s->clear[2], s->clear[3]);
}
static int pace(double ms, int live) {   /* the song's beats a second: slow under 105, fast from 150 */
    const int32_t *fb = live ? A.S.sd.fretbars : NULL; int nfb = live ? A.S.sd.nfret : 0; float bpm = 120;
    if (fb && nfb > 2) { double b0 = gh3_beatpos(fb, nfb, ms), b1 = gh3_beatpos(fb, nfb, ms + 2000); bpm = (float)((b1 - b0) * 30); if (bpm < 40) bpm = 120; }
    if (C.bpm < 1) C.bpm = bpm; else C.bpm += (bpm - C.bpm) * 0.05f; return C.bpm < 105 ? 0 : C.bpm < 150 ? 1 : 2;
}
static void step(double perf, int sp, int redraw) {
    float dt = C.lastPerf > 0 ? (float)((perf - C.lastPerf) / 1000) : 0; if (dt < 0 || dt > 0.25f) dt = dt < 0 ? 0 : 0.25f; C.lastPerf = perf; C.frame++;
    int live = A.screen == SC_PLAY && A.S.loaded && A.S.song, mood = 1, speed = pace(live ? A.S.run.now : 0, live);
    if (live) { double h = A.S.run.health; mood = h < 0.66 ? 0 : h < 1.33 ? 1 : 2; sp = sp || A.S.run.spOn; }
    { const char *e = getenv("R3D_CROWD_MOOD"); if (e && *e) mood = atoi(e) < 0 ? 0 : atoi(e) > 2 ? 2 : atoi(e); e = getenv("R3D_CROWD_SP"); if (e && *e) sp = atoi(e) > 0; }
    if (C.forceMood >= 0) mood = C.forceMood; if (C.forceSp >= 0) sp = C.forceSp;
    for (int i = 0; i < C.nk; i++) tick(&C.k[i], dt, mood, sp, speed);
    if (!redraw) return;
    /* a redraw when some kind on screen (seen in the last two frames) is due, or one was never drawn; the others on screen
     * that are due within half a period come along, so the figures share one pass and one mipmap build (~hz passes a second) */
    double now = perf / 1000, early = 0.5 / C.hz; int any = 0, n = 0; uint32_t go = 0;
    for (int i = 0; i < C.nk; i++) { kind *k = &C.k[i]; if (!k->cur) continue; if (!k->drawn || (C.frame - k->seen <= 2 && now >= k->due)) any = 1;
        if ((!k->drawn || (C.frame - k->seen <= 2 && now >= k->due - early)) && n < C.perFrame) { go |= 1u << i; n++; } }
    if (!any || !pass_begin(n == C.nk)) return;
    for (int i = 0; i < C.nk; i++) if (go >> i & 1) { kind *k = &C.k[i]; figure(k); k->due = now + 1.0 / C.hz; }
    pass_end();
}
static void pass_state(void) { GL(glDisable)(GL_SCISSOR_TEST); GL(glDisable)(GL_BLEND); GL(glDisable)(GL_CULL_FACE); GL(glEnable)(GL_DEPTH_TEST); GL(glDepthFunc)(GL_LEQUAL); GL(glColorMask)(1, 1, 1, 1); for (int i = 0; i < 5; i++) GL(glEnableVertexAttribArray)(i); }
void r3d_crowd_update(double perf, int sp) {
    C.st.renders = C.st.draws = 0; if (!C.nk || !G.ready || !SET.stageFx || C.bad) return;
    Uint64 c0 = SDL_GetPerformanceCounter(); cstate s; cs_save(&s); C.forceMood = C.forceSp = -1; pass_state();
    step(perf, sp, 1);
    for (int i = 0; i < 5; i++) GL(glDisableVertexAttribArray)(i);
    cs_restore(&s); C.st.ms = (SDL_GetPerformanceCounter() - c0) * 1000.0 / SDL_GetPerformanceFrequency(); if (C.st.ms > C.st.msMax) C.st.msMax = C.st.ms;
}
void r3d_crowd_unload(void) {
    if (C.fbo) GL(glDeleteFramebuffers)(1, &C.fbo); if (C.depth) GL(glDeleteRenderbuffers)(1, &C.depth); if (C.atlas) GL(glDeleteTextures)(1, &C.atlas); C.fbo = C.depth = C.atlas = 0; C.bad = 0;
    C.nk = 0; C.zone[0] = 0; C.lastPerf = 0; memset(&C.st, 0, sizeof C.st);
}
void r3d_crowd_shutdown(void) {
    r3d_crowd_unload(); for (int i = 0; i < S.nm; i++) model_free(&S.m[i]); for (int i = 0; i < S.nclips; i++) r3d_clip_free(&S.clips[i]); free(S.clips);
    if (S.hasSke) r3d_ske_free(&S.ske); memset(&S, 0, sizeof S); if (G.prog) GL(glDeleteProgram)(G.prog); memset(&G, 0, sizeof G);
    for (int i = 0; i < L.nsec; i++) qb_free(L.sec[i].v); free(L.sec); memset(&L, 0, sizeof L); free(SL.ev); memset(&SL, 0, sizeof SL);
}

/* ------------------------------------------------------------------ --r3dtest: every kind in each mood, side by side, and what a redraw costs */
int r3d_crowd_test(const char *zone, const char *outdir) {
    r3d_crowd_select(); printf("crowd: %s: %s: lighters up %d times (R3D_CROWD_LIGHTERS=1 shows them)\n", zone, SL.name[0] ? SL.name : "no song", (SL.nev + 1) / 2);
    if (!C.nk) { printf("crowd: %s: no crowd (%s)\n", zone, G.ready ? "no kinds" : "no shader"); return 0; }
    static const char *STATE[4] = { "poor", "normal", "good", "starpower" }; uint8_t *sheet = malloc((size_t)C.nk * TW * TH * 4), *px = malloc(TW * TH * 4);
    cstate s; cs_save(&s); pass_state(); double perf = 1000; C.lastPerf = 0;
    for (int st = 0; st < 4; st++) { C.forceMood = st < 3 ? st : 2; C.forceSp = st == 3; for (int i = 0; i < C.nk; i++) { C.k[i].cur = C.k[i].prev = NULL; C.k[i].kind = 0; }
        for (int f = 0; f < 23; f++) { perf += 1000.0 / 15; step(perf, 0, 0); }   /* 1.5 s into the mood */
        if (!pass_begin(1)) break; for (int i = 0; i < C.nk; i++) figure(&C.k[i]); pass_end(); GL(glBindFramebuffer)(GL_FRAMEBUFFER, C.fbo); GL(glPixelStorei)(GL_PACK_ALIGNMENT, 1);
        for (int i = 0; i < C.nk; i++) { GL(glReadPixels)(C.k[i].cx * TW, C.k[i].cy * TH, TW, TH, GL_RGBA, GL_UNSIGNED_BYTE, px);
            for (int y = 0; y < TH; y++) for (int x = 0; x < TW; x++) { const uint8_t *p = px + (y * TW + x) * 4; uint8_t *o = sheet + ((size_t)y * C.nk * TW + i * TW + x) * 4; int a = p[3];   /* over a grey check, so the cut-out shows */
                int bg = ((x >> 3) + (y >> 3)) & 1 ? 70 : 100; for (int c = 0; c < 3; c++) o[c] = (uint8_t)(a >= 100 ? p[c] : bg); o[3] = 255; } }
        SDL_Surface *sf = SDL_CreateRGBSurfaceFrom(sheet, C.nk * TW, TH, 32, C.nk * TW * 4, 0x000000FF, 0x0000FF00, 0x00FF0000, 0); char p[700]; snprintf(p, sizeof p, "%s/%s_crowd_%s.bmp", outdir && *outdir ? outdir : ".", zone, STATE[st]); SDL_SaveBMP(sf, p); SDL_FreeSurface(sf);
        printf("crowd: %s %-9s ->", zone, STATE[st]); for (int i = 0; i < C.nk; i++) printf(" %s%s:%08x(%.1fs)", C.k[i].pak, C.k[i].standIn ? "*" : "", C.k[i].cur ? C.k[i].cur->key : 0, C.k[i].cur ? C.k[i].cur->dur : 0); printf(" -> %s\n", p); }
    /* the cost of a redraw of every kind: the CPU's part (pose, palette, draws, mipmaps submitted), then with the GPU's (glFinish; llvmpipe here) */
    C.forceMood = 1; C.forceSp = 0; double cpu = 0; int N = 40; Uint64 f0 = SDL_GetPerformanceCounter(), fr = SDL_GetPerformanceFrequency();
    for (int r = 0; r < N; r++) { perf += 1000.0 / 15; Uint64 c0 = SDL_GetPerformanceCounter(); step(perf, 0, 0); if (pass_begin(1)) { for (int i = 0; i < C.nk; i++) figure(&C.k[i]); pass_end(); } cpu += (SDL_GetPerformanceCounter() - c0) * 1000.0 / fr; GL(glFinish)(); }
    double all = (SDL_GetPerformanceCounter() - f0) * 1000.0 / fr / N; cpu /= N;
    for (int i = 0; i < 5; i++) GL(glDisableVertexAttribArray)(i); cs_restore(&s); C.forceMood = C.forceSp = -1; for (int i = 0; i < C.nk; i++) C.k[i].due = 0;
    printf("crowd: %s: %d kinds in a %dx%d atlas, %d models (%d points a round), %d animations (%.1f MB), %.1f MB on the GPU, loaded in %.0f ms; a redraw of all: %.3f ms CPU submit, %.3f ms with the GPU (host); %d a second (R3D_CROWD_HZ) -> ~%.2f ms CPU a second of 60 frames\n",
        zone, C.nk, C.aw, C.ah, S.nm, C.st.verts, S.nclips, S.animBytes / 1048576.0, C.st.vram / 1048576.0, C.st.loadMs, cpu, all, C.hz, cpu * C.hz / 60);
    free(sheet); free(px); return 0;
}
