/* The venue drawn in 3D behind the highway. A zone's meshes are read once (r3d_zone.c), lit on the CPU from the venue's
 * own lights (baked into the vertex colours), merged per material into one vertex buffer each (world space, objects
 * already placed; each part keeps its index range and bounding sphere), and the textures uploaded at most 256 px: DXT
 * where the GPU takes it, else decoded and then ETC1 (opaque, encoded here at load) or 16-bit (565 / 4444), mipmapped.
 * Every frame: SDL_RenderFlush, the GL state SDL relies on saved, the venue drawn through one shader (lit / unlit /
 * ambient-only are the same formula with different numbers; a copy with the alpha test for the cut-out materials) into
 * a framebuffer of 3/4 or 1/2 the screen (Opções > Vídeo > Resolução do palco 3D) stretched under the 2D, parts out of
 * view or under a pixel left out, the state put back. Inside that pass, at its size and against its depth: the band
 * (r3d_band.c) after the solid batches, the light show's beams, glows and sparks (r3d_lights.c) after the see-through
 * ones; the show's lighting itself is in the venue shader (both programs). The camera walks the venue's own shots (cameras, _pan, _dolly…)
 * one after another, as the game does between songs. While a song plays a line of timings goes to the log every 10 s. */
#include "r3d.h"
#include "r3d_gl.h"
#include "r3d_zone.h"
#include "r3d_lights.h"
#include "r3d_band.h"
#include "r3d_crowd.h"
#include "app.h"
#include "settings.h"
#include "battle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dlfcn.h>
#include <sys/stat.h>

#define R3D_GL_DEF(ret, name, args) ret (*r3d_##name) args;
R3D_GL_FUNCS(R3D_GL_DEF)
R3D_GL_FUNCS2(R3D_GL_DEF)
#undef R3D_GL_DEF
int r3d_gl_load(void) {
    int missing = 0;
#define R3D_GL_GET(ret, name, args) r3d_##name = (ret (*) args)SDL_GL_GetProcAddress(#name); if (!r3d_##name) { missing++; SDL_Log("r3d: no %s", #name); }
    R3D_GL_FUNCS(R3D_GL_GET)
    R3D_GL_FUNCS2(R3D_GL_GET)
#undef R3D_GL_GET
    return missing ? -1 : 0;
}
#define GL(name) r3d_##name

#define TEX_MAX 256          /* 16-bit textures above this take a smaller mip level, ETC1 / DXT / signs above twice this (R3D_TEXMAX=128..512 to compare): texel bandwidth, what bounds a Mali-G31 */
#define VRAM_MAX (64u << 20)
#define SHOT_MS 8000.0       /* with no cue list, the next shot every eight seconds */
#define Z_NEAR 0.25f
#define Z_FAR 1500.0f
#define LOD_PX 1.0f          /* a part whose bounding sphere is under this radius on the screen (in 640x480 pixels) is left out */
#define RUN_GAP 96           /* parts of a batch drawn in one call across a gap of up to this many hidden indices */
#define PERF_MS 10000        /* the timings line in the log, while a song plays */

/* ---- one vertex: 48 bytes (r3d_vtx), and beside it in a buffer of its own the room lights that fall on it (8 bytes) ---- */
typedef r3d_vtx vtx;
typedef struct { uint8_t li[4], lw[4]; } lightvtx;   /* the four room lights (r3d_lights.c's slots) that give the point most, and each one's share of its colour x intensity (n.l and reach, /255) */
#define NATTR 8              /* the venue's vertex arrays: r3d_vtx's six, then lightvtx's two */
typedef struct { uint32_t key; int clamp, max; GLuint id; } texref;
typedef struct { int first, count; float c[3], r; } part;   /* a mesh inside a batch: its indices and bounding sphere */
typedef struct {
    uint32_t mat; int smapped; GLuint vbo, lvbo, ibo, tex, smap; int nidx, nvert, blend /* 0 solid, 1 added, 2 see-through */, board, colA;
    float colour[4], alphaRef, tile, anim[4], wibU[4], wibV[4], wibR[3]; int wib, hasAnim, crowd /* 1 + the crowd kind its cards show (r3d_crowd.c), 0 */, lighter /* 1 + the crowd's lighter group: shown while they are up, swayed by its script */; float amb[3], centre[3], radius; part *p; int np; float order;
    vtx *v; lightvtx *lv; uint16_t *i; int capV, capI; double ambSum[3]; float bmin[3], bmax[3];   /* build-time only */
} batch;
typedef struct { float pos[3], q[4], fov; } camend;
typedef struct { uint32_t list, type, name; camend a, b; int hasB; float orbit; int who; } shot;   /* who: the band member the shot follows (BAND_*), -1 fixed in the room */
typedef struct { int frames; double sumFrame, maxFrame, sumCpu, maxCpu, sumGpu; int nGpu; long calls, tris; Uint64 last, t0; } perfwin;
typedef struct {
    int ready, loaded; SDL_Renderer *ren; char zone[32]; int W, H;
    r3d_prog P[2]; GLuint white; r3d_view V;   /* P[0] no alpha test, P[1] with it */
    int s3tc, npot, etc1, texMax; GLenum etcFmt; int (*flush)(SDL_Renderer *);
    batch *b; int nb; texref *tex; int ntex; shot *shots; int nshots; int *order, ngen, shotSeq, sortedSeq; uint32_t kCameras; float ctr[3], span;   /* order: the general shots, shuffled */
    /* the framebuffer the venue is drawn into below Alta, and the program that stretches it */
    GLuint fbo, fboTex, fboDepth, blitProg, blitVbo; GLint bPos, bTex; int fw, fh, fboBad, res;
    /* optional GL: timer queries (GL_EXT_disjoint_timer_query, or desktop GL 3.3), framebuffer discard */
    void (*genQ)(GLsizei, GLuint *); void (*beginQ)(GLenum, GLuint); void (*endQ)(GLenum); void (*getQ)(GLuint, GLenum, GLuint *); void (*discard)(GLenum, GLsizei, const GLenum *); void (*attrib4f)(GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
    GLuint q[4]; int qBusy[4], qi, timer, disjoint;
    perfwin pw;
    /* counts, for --r3dtest and the log */
    int meshes, skipped, tris, textures, nEtc, n16, nDxt; size_t vram, texBytes; int drawCalls, drawnTris, culled; double loadMs, texMs;
} r3d_t;
static r3d_t R;

/* ------------------------------------------------------------------ small maths */
static void mat_perspective(float *m, float vfov, float aspect, float zn, float zf) {
    float f = 1 / tanf(vfov / 2); memset(m, 0, 64); m[0] = f / aspect; m[5] = f; m[10] = (zf + zn) / (zn - zf); m[11] = -1; m[14] = 2 * zf * zn / (zn - zf);
}
static void quat_rot(const float q[4], const float v[3], float o[3]) {   /* v turned by q (x y z w) */
    float x = q[0], y = q[1], z = q[2], w = q[3];
    float tx = 2 * (y * v[2] - z * v[1]), ty = 2 * (z * v[0] - x * v[2]), tz = 2 * (x * v[1] - y * v[0]);
    o[0] = v[0] + w * tx + (y * tz - z * ty); o[1] = v[1] + w * ty + (z * tx - x * tz); o[2] = v[2] + w * tz + (x * ty - y * tx);
}
static void quat_mul(const float a[4], const float b[4], float o[4]) {
    o[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1]; o[1] = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
    o[2] = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3]; o[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
}
static void quat_nlerp(const float a[4], const float b[4], float t, float o[4]) {
    float d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3], s = d < 0 ? -1 : 1, n = 0;
    for (int i = 0; i < 4; i++) { o[i] = a[i] * (1 - t) + b[i] * s * t; n += o[i] * o[i]; } n = sqrtf(n); if (n > 1e-6f) for (int i = 0; i < 4; i++) o[i] /= n;
}
/* the view matrix of the game's camera: it looks along its own +Z (the browser port turns it 180° about Y for three.js) */
static void mat_view(float *m, const float pos[3], const float q[4], float yaw) {
    float ax[3] = { -1, 0, 0 }, ay[3] = { 0, 1, 0 }, az[3] = { 0, 0, -1 }, r[3], u[3], b[3]; quat_rot(q, ax, r); quat_rot(q, ay, u); quat_rot(q, az, b);
    if (yaw) { float c = cosf(yaw), s = sinf(yaw), r2[3], b2[3]; for (int i = 0; i < 3; i++) { r2[i] = r[i] * c - b[i] * s; b2[i] = r[i] * s + b[i] * c; } memcpy(r, r2, 12); memcpy(b, b2, 12); }   /* a slow sway about the camera's own up */
    m[0] = r[0]; m[4] = r[1]; m[8] = r[2]; m[12] = -(r[0] * pos[0] + r[1] * pos[1] + r[2] * pos[2]);
    m[1] = u[0]; m[5] = u[1]; m[9] = u[2]; m[13] = -(u[0] * pos[0] + u[1] * pos[1] + u[2] * pos[2]);
    m[2] = b[0]; m[6] = b[1]; m[10] = b[2]; m[14] = -(b[0] * pos[0] + b[1] * pos[1] + b[2] * pos[2]);
    m[3] = m[7] = m[11] = 0; m[15] = 1;
}
static void mat_mul(const float *a, const float *b, float *o) { for (int c = 0; c < 4; c++) for (int r = 0; r < 4; r++) { float s = 0; for (int k = 0; k < 4; k++) s += a[k * 4 + r] * b[c * 4 + k]; o[c * 4 + r] = s; } }
/* the six planes of a projection*view matrix, normalised (a, b, c, d: inside when a x + b y + c z + d >= 0) */
static void frustum(const float *vp, float pl[6][4]) {
    for (int i = 0; i < 6; i++) { int row = i >> 1; float s = (i & 1) ? -1.0f : 1.0f; for (int k = 0; k < 4; k++) pl[i][k] = vp[k * 4 + 3] + s * vp[k * 4 + row];
        float L = sqrtf(pl[i][0] * pl[i][0] + pl[i][1] * pl[i][1] + pl[i][2] * pl[i][2]); if (L < 1e-6f) { pl[i][0] = pl[i][1] = pl[i][2] = 0; pl[i][3] = 1; continue; } for (int k = 0; k < 4; k++) pl[i][k] /= L; }
}
static int sphere_in(const float pl[6][4], const float c[3], float rad) {
    for (int i = 0; i < 6; i++) if (pl[i][0] * c[0] + pl[i][1] * c[1] + pl[i][2] * c[2] + pl[i][3] < -rad) return 0;
    return 1;
}

/* ------------------------------------------------------------------ shaders */
static const char *VS =
    "uniform mat4 uProj; uniform mat4 uView; uniform float uBoard; uniform vec4 uAnim; uniform vec4 uWibU; uniform vec4 uWibV; uniform vec3 uWibR; uniform float uWib; uniform float uTime;\n"
    "uniform float uDyn; uniform vec3 uRL[32]; uniform vec3 uKnee;\n"   /* the light show (r3d_lights.c): what each room light gives now against the bake; the knee (from, span, the batch's own ambient) */
    "#ifdef GL_ES\nuniform mediump vec3 uAmb;\n#else\nuniform vec3 uAmb;\n#endif\n"   /* (the fragment shader's, at its precision) */
    "attribute vec3 aPos; attribute vec3 aCtr; attribute vec4 aCol; attribute vec4 aLit; attribute vec2 aUv; attribute vec2 aUv2; attribute vec4 aLI; attribute vec4 aLW;\n"   /* aLI/aLW: the point's four room lights and their shares (lightvtx) */
    "varying vec2 vUv; varying vec2 vUv2; varying vec4 vCol; varying vec3 vLit;\n"
    "void main() {\n"
    "  vec4 vp;\n"
    "  if (uBoard > 0.5) { vp = uView * vec4(aCtr, 1.0); vec3 off = aPos - aCtr; vp.xyz += vec3(off.x, off.y, -off.z); }\n"   /* a card that always faces the camera, turning about its centre */
    "  else vp = uView * vec4(aPos, 1.0);\n"
    "  gl_Position = uProj * vp;\n"
    "  vec2 uv = aUv;\n"
    "  if (uAnim.x > 0.5) { float fr = floor(mod(uTime * uAnim.z + uAnim.w, uAnim.x * uAnim.y)); uv = (vec2(mod(fr, uAnim.x), floor(fr / uAnim.x)) + aUv) / uAnim.xy; }\n"   /* a picture made of frames */
    "  if (uWib > 0.5) { float su = uTime * uWibU.x + uWibU.z * sin(uWibU.y * uTime + uWibU.w), sv = uTime * uWibV.x + uWibV.z * sin(uWibV.y * uTime + uWibV.w), ra = uTime * uWibR.x;\n"   /* one that scrolls, sways and turns */
    "    vec2 c0 = aUv - uWibR.yz; uv = vec2(c0.x * cos(ra) + c0.y * sin(ra), c0.y * cos(ra) - c0.x * sin(ra)) + uWibR.yz + vec2(mod(su, 8.0), mod(sv, 8.0)); }\n"
    "  vUv = uv; vUv2 = aUv2; vCol = aCol; vec3 lit = aLit.rgb * 4.0;\n"
    "  if (uDyn > 0.5) lit = max(lit + uRL[int(aLI.x)] * aLW.x + uRL[int(aLI.y)] * aLW.y + uRL[int(aLI.z)] * aLW.z + uRL[int(aLI.w)] * aLW.w, 0.0);\n"
    "  vec3 tot = lit + uAmb; float mx = max(max(tot.r, tot.g), tot.b), k = max(uKnee.x, max(max(aLit.r, aLit.g), aLit.b) * 4.0 + uKnee.z), o = mx - k;\n"
    "  if (o > 0.0) vCol.rgb *= (k + o / (1.0 + o / uKnee.y)) / mx;\n"   /* what the show adds past the point's own light (or the knee) rolls off, at most uKnee.y more, its colour kept: through vCol, nothing more for the fragment shader */
    "  vLit = lit;\n"
    "}\n";
/* mediump for the colour sums (half the work on Mali), highp kept for the texture coordinates (tiled and scrolled ones go past 8) */
static const char *FS =
    "#ifdef GL_ES\nprecision mediump float;\n#define HI highp\n#else\n#define HI\n#endif\n"
    "uniform sampler2D uMap; uniform sampler2D uSmap; uniform vec4 uColour; uniform vec3 uAmb; uniform vec3 uTint; uniform float uAlphaRef; uniform float uColA; uniform HI float uTile;\n"
    "varying HI vec2 vUv; varying HI vec2 vUv2; varying vec4 vCol; varying vec3 vLit;\n"
    "void main() {\n"
    "  vec4 t = texture2D(uMap, vUv * uTile) * uColour;\n"
    "  vec3 L = vLit * texture2D(uSmap, vUv2).rgb + uAmb;\n"   /* the game's sum: the lights dimmed by the baked map, plus ambient (unlit parts: lit 1, amb 0, map white) */
    "  float a = t.a * mix(1.0, vCol.a, uColA);\n"
    "#ifdef ATEST\n  if (a < uAlphaRef) discard;\n#endif\n"   /* only in P[1]: a discard anywhere in a program turns off Mali's early depth for all its draws */
    "  gl_FragColor = vec4(t.rgb * L * vCol.rgb * uTint, a);\n"
    "}\n";
static const char *BLIT_VS = "attribute vec2 aP; varying vec2 vT; void main() { vT = aP * 0.5 + 0.5; gl_Position = vec4(aP, 0.0, 1.0); }\n";
static const char *BLIT_FS = "#ifdef GL_ES\nprecision mediump float;\n#endif\nuniform sampler2D uT; varying vec2 vT; void main() { gl_FragColor = texture2D(uT, vT); }\n";
static GLuint shader(GLenum kind, const char *pre, const char *src) {
    GLuint s = GL(glCreateShader)(kind); const char *srcs[2] = { pre, src }; GL(glShaderSource)(s, 2, srcs, NULL); GL(glCompileShader)(s); GLint ok = 0; GL(glGetShaderiv)(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[1024]; GL(glGetShaderInfoLog)(s, sizeof log, NULL, log); SDL_Log("r3d: shader: %s", log); GL(glDeleteShader)(s); return 0; }
    return s;
}
static GLuint link(const char *vsrc, const char *pre, const char *fsrc) {
    GLuint vs = shader(GL_VERTEX_SHADER, "", vsrc), fs = shader(GL_FRAGMENT_SHADER, pre, fsrc); if (!vs || !fs) return 0;
    GLuint p = GL(glCreateProgram)(); GL(glAttachShader)(p, vs); GL(glAttachShader)(p, fs);
    static const char *ATTR[NATTR] = { "aPos", "aCtr", "aCol", "aLit", "aUv", "aUv2", "aLI", "aLW" }; for (int i = 0; i < NATTR; i++) GL(glBindAttribLocation)(p, i, ATTR[i]); GL(glBindAttribLocation)(p, 0, "aP");   /* the same slots in both venue programs */
    GL(glLinkProgram)(p); GL(glDeleteShader)(vs); GL(glDeleteShader)(fs);
    GLint ok = 0; GL(glGetProgramiv)(p, GL_LINK_STATUS, &ok); if (!ok) { char log[1024]; GL(glGetProgramInfoLog)(p, sizeof log, NULL, log); SDL_Log("r3d: link: %s", log); GL(glDeleteProgram)(p); return 0; }
    return p;
}
static int build_program(void) {
    for (int k = 0; k < 2; k++) { r3d_prog *P = &R.P[k]; GLuint p = link(VS, k ? "#define ATEST 1\n" : "", FS); if (!p) return -1; P->prog = p;
        P->aPos = GL(glGetAttribLocation)(p, "aPos"); P->aCtr = GL(glGetAttribLocation)(p, "aCtr"); P->aCol = GL(glGetAttribLocation)(p, "aCol"); P->aLit = GL(glGetAttribLocation)(p, "aLit"); P->aUv = GL(glGetAttribLocation)(p, "aUv"); P->aUv2 = GL(glGetAttribLocation)(p, "aUv2");
#define U(n) P->n = GL(glGetUniformLocation)(p, #n)
        U(uProj); U(uView); U(uBoard); U(uAnim); U(uWibU); U(uWibV); U(uWibR); U(uWib); U(uTime); U(uMap); U(uSmap); U(uColour); U(uAmb); U(uTint); U(uAlphaRef); U(uColA); U(uTile);
#undef U
        r3d_lights_program(k, p);   /* the light show's uniforms: each program has its own locations */
    }
    if ((R.blitProg = link(BLIT_VS, "", BLIT_FS))) { R.bPos = GL(glGetAttribLocation)(R.blitProg, "aP"); R.bTex = GL(glGetUniformLocation)(R.blitProg, "uT");
        static const float tri[6] = { -1, -1, 3, -1, -1, 3 }; GL(glGenBuffers)(1, &R.blitVbo); GL(glBindBuffer)(GL_ARRAY_BUFFER, R.blitVbo); GL(glBufferData)(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW); GL(glBindBuffer)(GL_ARRAY_BUFFER, 0); }
    else R.fboBad = 1;   /* no stretching: drawn at the screen's size */
    return 0;
}
const r3d_prog *r3d_program(int alphaTest) { return R.ready ? &R.P[alphaTest ? 1 : 0] : NULL; }
const r3d_view *r3d_view_now(void) { return &R.V; }

/* ------------------------------------------------------------------ init */
int r3d_ready(void) { return R.ready; }
static void *proc(const char *a, const char *b) { void *p = SDL_GL_GetProcAddress(a); if (!p && b) p = SDL_GL_GetProcAddress(b); return p; }
int r3d_init(SDL_Renderer *ren) {
    memset(&R, 0, sizeof R); R.ren = ren; if (!ren) return -1;
    SDL_RendererInfo ri; SDL_GetRendererInfo(ren, &ri); if (!strstr(ri.name, "opengl")) { SDL_Log("r3d: renderer %s is not GL: 2D stage", ri.name); return -1; }
    if (!SDL_GL_GetCurrentContext()) { SDL_Log("r3d: no current GL context: 2D stage"); return -1; }
    R.flush = (int (*)(SDL_Renderer *))dlsym(RTLD_DEFAULT, "SDL_RenderFlush"); if (!R.flush) { SDL_Log("r3d: this SDL has no SDL_RenderFlush: 2D stage"); return -1; }   /* 2.0.10+; the sysroot headers (2.0.8) do not declare it */
    if (r3d_gl_load()) return -1;
    if (build_program()) return -1;
    if (r3d_band_init()) SDL_Log("r3d: no band (skinning shader)");   /* the venue still shows without it */
    if (r3d_crowd_init()) SDL_Log("r3d: no crowd (figure shader)");
    const char *ext = (const char *)GL(glGetString)(GL_EXTENSIONS); if (!ext) ext = "";
    const char *ver = (const char *)GL(glGetString)(GL_VERSION); int es = ver && strstr(ver, "ES") != NULL;
    R.s3tc = strstr(ext, "GL_EXT_texture_compression_s3tc") != NULL || (strstr(ext, "GL_EXT_texture_compression_dxt1") && strstr(ext, "GL_ANGLE_texture_compression_dxt"));
    if (getenv("R3D_NODXT")) R.s3tc = 0;   /* R3D_NODXT=1: the textures decoded on the CPU as on the Mali-G31, to check that path on a desktop GPU */
    R.npot = strstr(ext, "GL_OES_texture_npot") != NULL || !es;   /* desktop GL: always */
    if (strstr(ext, "GL_OES_compressed_ETC1_RGB8_texture")) { R.etc1 = 1; R.etcFmt = GL_ETC1_RGB8_OES; } else if (!es && strstr(ext, "GL_ARB_ES3_compatibility")) { R.etc1 = 1; R.etcFmt = GL_COMPRESSED_RGB8_ETC2; }   /* ETC2 RGB8 reads ETC1 blocks the same */
    if (getenv("R3D_NOETC")) R.etc1 = 0;
    { const char *e = getenv("R3D_TEXMAX"); R.texMax = e && atoi(e) >= 16 ? atoi(e) : TEX_MAX; }
    if (es ? strstr(ext, "GL_EXT_disjoint_timer_query") != NULL : strstr(ext, "GL_ARB_timer_query") != NULL) {
        R.genQ = (void (*)(GLsizei, GLuint *))proc(es ? "glGenQueriesEXT" : "glGenQueries", NULL); R.beginQ = (void (*)(GLenum, GLuint))proc(es ? "glBeginQueryEXT" : "glBeginQuery", NULL);
        R.endQ = (void (*)(GLenum))proc(es ? "glEndQueryEXT" : "glEndQuery", NULL); R.getQ = (void (*)(GLuint, GLenum, GLuint *))proc(es ? "glGetQueryObjectuivEXT" : "glGetQueryObjectuiv", NULL);
        if (R.genQ && R.beginQ && R.endQ && R.getQ) { R.genQ(4, R.q); R.timer = 1; R.disjoint = es; } }
    if (es ? strstr(ext, "GL_EXT_discard_framebuffer") != NULL : strstr(ext, "GL_ARB_invalidate_subdata") != NULL) R.discard = (void (*)(GLenum, GLsizei, const GLenum *))proc(es ? "glDiscardFramebufferEXT" : "glInvalidateFramebuffer", NULL);
    R.attrib4f = (void (*)(GLuint, GLfloat, GLfloat, GLfloat, GLfloat))proc("glVertexAttrib4f", NULL);
    GL(glGenTextures)(1, &R.white); GL(glBindTexture)(GL_TEXTURE_2D, R.white); uint8_t px[4] = { 255, 255, 255, 255 }; GL(glTexImage2D)(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); GL(glBindTexture)(GL_TEXTURE_2D, 0);
    R.P[0].white = R.P[1].white = R.white;
    SDL_GetRendererOutputSize(ren, &R.W, &R.H);
    SDL_Log("r3d: %s / %s, %s, s3tc %d etc1 %d npot %d, gpu timer %d, discard %d, textures up to %d px", GL(glGetString)(GL_RENDERER), ver ? ver : "?", ri.name, R.s3tc, R.etc1, R.npot, R.timer, R.discard != NULL, R.texMax);
    R.ready = 1; return 0;
}

/* ------------------------------------------------------------------ textures */
static int pot(int x) { return x > 0 && !(x & (x - 1)); }
/* ---- ETC1, encoded at load: 4 bits a texel (a quarter of 565), what the Mali samples fastest. A quick encoder: per 4x4 block
 * the split (two 2x4 or two 4x2 halves) that keeps each half's colours closest to its mean, the means as the base colours
 * (555 + a 333 difference, else 444 each), and per half the modifier table with the least error, each texel taking the
 * modifier nearest to its own offset from the base (exact for the error sum while nothing clips) */
static const int ETC_T[8][2] = { { 2, 8 }, { 5, 17 }, { 9, 29 }, { 13, 42 }, { 18, 60 }, { 24, 80 }, { 33, 106 }, { 47, 183 } };
static int etc_half(const int (*p)[3], const int *who, const int b[3], uint32_t *lo) {   /* the table; the texels' indices into *lo */
    int s[8], bestT = 0, bestE = 0x7fffffff, mx = 0;
    for (int i = 0; i < 8; i++) { s[i] = p[who[i]][0] - b[0] + p[who[i]][1] - b[1] + p[who[i]][2] - b[2]; int as = s[i] < 0 ? -s[i] : s[i]; if (as > mx) mx = as; }
    int t0 = 0; while (t0 < 7 && 3 * ETC_T[t0][1] < mx) t0++;   /* only the tables round the one whose large step reaches the farthest texel */
    for (int t = t0 > 0 ? t0 - 1 : 0; t <= t0 + 1 && t < 8; t++) { int a = ETC_T[t][0], B = ETC_T[t][1], lim = 3 * (a + B), e = 0;   /* per texel 3m^2 - 2ms: the large step once |s| > 1.5 (a + B) */
        for (int i = 0; i < 8; i++) { int as = s[i] < 0 ? -s[i] : s[i], m = 2 * as > lim ? B : a; e += 3 * m * m - 2 * m * as; }
        if (e < bestE) { bestE = e; bestT = t; } }
    int a = ETC_T[bestT][0], B = ETC_T[bestT][1], lim = 3 * (a + B);
    for (int i = 0; i < 8; i++) { int as = s[i] < 0 ? -s[i] : s[i], big = 2 * as > lim, neg = s[i] < 0, idx = (neg << 1) | big; *lo |= (uint32_t)(idx >> 1) << (16 + who[i]) | (uint32_t)(idx & 1) << who[i]; }
    return bestT;
}
static void etc1_block(const uint8_t *rgba, int w, int h, int bx, int by, uint8_t *out) {
    int p[16][3];   /* texel x, y at x * 4 + y, as the block numbers them; edges repeated past a small level's border */
    for (int x = 0; x < 4; x++) for (int y = 0; y < 4; y++) { int sx = bx + x < w ? bx + x : w - 1, sy = by + y < h ? by + y : h - 1; const uint8_t *q = rgba + ((size_t)sy * w + sx) * 4; p[x * 4 + y][0] = q[0]; p[x * 4 + y][1] = q[1]; p[x * 4 + y][2] = q[2]; }
    static const int HALF[2][2][8] = { { { 0, 1, 2, 3, 4, 5, 6, 7 }, { 8, 9, 10, 11, 12, 13, 14, 15 } }, { { 0, 1, 4, 5, 8, 9, 12, 13 }, { 2, 3, 6, 7, 10, 11, 14, 15 } } };   /* flip 0: left | right; 1: top / bottom */
    int sum[2][2][3] = { { { 0 } } }, flip = 0; long best = -1;
    for (int f = 0; f < 2; f++) { long v = 0; for (int hf = 0; hf < 2; hf++) for (int c = 0; c < 3; c++) { int S = 0; for (int i = 0; i < 8; i++) S += p[HALF[f][hf][i]][c]; sum[f][hf][c] = S; v += (long)S * S; } if (v > best) { best = v; flip = f; } }   /* the bigger sum of squared half-sums: the smaller spread inside the halves */
    int q[2][3], base[2][3], diff = 1; uint32_t hi = 0, lo = 0;
    for (int hf = 0; hf < 2; hf++) for (int c = 0; c < 3; c++) q[hf][c] = (sum[flip][hf][c] * 31 + 8 * 255 / 2) / (8 * 255);
    for (int c = 0; c < 3; c++) { int d = q[1][c] - q[0][c]; if (d < -4 || d > 3) diff = 0; }
    if (diff) { for (int hf = 0; hf < 2; hf++) for (int c = 0; c < 3; c++) base[hf][c] = (q[hf][c] << 3) | (q[hf][c] >> 2);
        hi = (uint32_t)q[0][0] << 27 | (uint32_t)((q[1][0] - q[0][0]) & 7) << 24 | (uint32_t)q[0][1] << 19 | (uint32_t)((q[1][1] - q[0][1]) & 7) << 16 | (uint32_t)q[0][2] << 11 | (uint32_t)((q[1][2] - q[0][2]) & 7) << 8 | 2u; }
    else { for (int hf = 0; hf < 2; hf++) for (int c = 0; c < 3; c++) { q[hf][c] = (sum[flip][hf][c] * 15 + 8 * 255 / 2) / (8 * 255); base[hf][c] = q[hf][c] * 17; }
        hi = (uint32_t)q[0][0] << 28 | (uint32_t)q[1][0] << 24 | (uint32_t)q[0][1] << 20 | (uint32_t)q[1][1] << 16 | (uint32_t)q[0][2] << 12 | (uint32_t)q[1][2] << 8; }
    int t0 = etc_half(p, HALF[flip][0], base[0], &lo), t1 = etc_half(p, HALF[flip][1], base[1], &lo);
    hi |= (uint32_t)t0 << 5 | (uint32_t)t1 << 2 | (uint32_t)flip;
    for (int k = 0; k < 4; k++) { out[k] = (uint8_t)(hi >> (24 - 8 * k)); out[4 + k] = (uint8_t)(lo >> (24 - 8 * k)); }
}
static size_t etc1_size(int w, int h) { return (size_t)((w + 3) / 4) * ((h + 3) / 4) * 8; }
static void etc1_encode(const uint8_t *rgba, int w, int h, uint8_t *out) { for (int by = 0; by < h; by += 4) for (int bx = 0; bx < w; bx += 4, out += 8) etc1_block(rgba, w, h, bx, by, out); }
static void half_size(const uint8_t *s, int w, int h, uint8_t *d) {   /* the next mip level: a 2x2 box (a side already 1 stays 1) */
    int w2 = w > 1 ? w / 2 : 1, h2 = h > 1 ? h / 2 : 1, dx = w > 1, dy = h > 1;
    for (int y = 0; y < h2; y++) for (int x = 0; x < w2; x++) for (int c = 0; c < 4; c++) { const uint8_t *a = s + ((size_t)(y * (dy + 1)) * w + x * (dx + 1)) * 4 + c;
        d[((size_t)y * w2 + x) * 4 + c] = (uint8_t)((a[0] + a[dx * 4] + a[(size_t)dy * w * 4] + a[(size_t)dy * w * 4 + dx * 4] + 2) / 4); }
}
/* ---- the ETC1 cache: <GH3_HOME>/cache-3d/<zone>.etc, written when a load had to encode, read on the next ones (encoding is
 * the slow part of a load on the A35). Header "R3DC", version, texture cap, a stamp of the zone's texture bank; then per
 * picture 16 bytes (key, w, h, levels, size; levels 0: not opaque, kept 16-bit) and its ETC1 levels. R3D_NOCACHE=1: none. */
#define CACHE_VER 2
static struct { uint8_t *in; size_t inLen; uint8_t *out; size_t outLen, outCap; int hits, miss; uint32_t stamp; char path[600]; } C;
static void put32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static uint32_t get32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void cache_head(uint8_t *h, uint32_t key, int w, int hh, int nl, size_t size) { put32(h, key); put32(h + 4, (uint32_t)w | (uint32_t)hh << 16); put32(h + 8, (uint32_t)nl); put32(h + 12, (uint32_t)size); }
static const uint8_t *cache_find(uint32_t key, int w, int h) {   /* in the file read, or among the ones this load already made (a picture used with two wrap modes) */
    for (int k = 0; k < 2; k++) { const uint8_t *B = k ? C.out : C.in; size_t n = k ? C.outLen : C.inLen;
        for (size_t p = k ? 0 : 16; B && p + 16 <= n; p += 16 + get32(B + p + 12)) if (get32(B + p) == key && get32(B + p + 4) == ((uint32_t)w | (uint32_t)h << 16) && p + 16 + get32(B + p + 12) <= n) return B + p; }
    return NULL;
}
static void cache_keep(const uint8_t *rec, size_t n) {
    if (C.out && rec >= C.out && rec < C.out + C.outLen) return;   /* already kept */
    if (C.outLen + n > C.outCap) { C.outCap = (C.outLen + n) * 2; C.out = realloc(C.out, C.outCap); } memcpy(C.out + C.outLen, rec, n); C.outLen += n;
}
static void cache_open(const char *zone, const uint8_t *bank, size_t bankLen, const r3d_texent *ents, int n) {
    free(C.in); free(C.out); memset(&C, 0, sizeof C); if (!R.etc1 || getenv("R3D_NOCACHE")) return;
    uint32_t h = 2166136261u; for (int i = 0; i < n; i++) { uint32_t v[4] = { ents[i].key, (uint32_t)ents[i].w, (uint32_t)ents[i].h, ents[i].size }; for (int k = 0; k < 16; k++) h = (h ^ ((uint8_t *)v)[k]) * 16777619u; }
    for (size_t k = 0; k < bankLen; k += 4093) h = (h ^ bank[k]) * 16777619u;   /* a sparse look at the bytes too */
    C.stamp = h ^ (uint32_t)bankLen;
    snprintf(C.path, sizeof C.path, "%s/cache-3d/%s.etc", settings_home(), zone);
    FILE *f = fopen(C.path, "rb"); if (!f) return; fseek(f, 0, SEEK_END); long L = ftell(f); fseek(f, 0, SEEK_SET);
    if (L >= 16) { C.in = malloc((size_t)L); if (fread(C.in, 1, (size_t)L, f) != (size_t)L || memcmp(C.in, "R3DC", 4) || get32(C.in + 4) != CACHE_VER || get32(C.in + 8) != (uint32_t)R.texMax || get32(C.in + 12) != C.stamp) { free(C.in); C.in = NULL; } else C.inLen = (size_t)L; }
    fclose(f);
}
static void cache_close(void) {   /* written again only when something was missing from it */
    if (C.miss && C.path[0]) { char dir[620], tmp[620]; snprintf(dir, sizeof dir, "%s/cache-3d", settings_home()); mkdir(dir, 0755); snprintf(tmp, sizeof tmp, "%s.tmp", C.path);
        FILE *f = fopen(tmp, "wb"); uint8_t h[16]; memcpy(h, "R3DC", 4); put32(h + 4, CACHE_VER); put32(h + 8, (uint32_t)R.texMax); put32(h + 12, C.stamp);
        if (f) { int ok = fwrite(h, 1, 16, f) == 16 && fwrite(C.out, 1, C.outLen, f) == C.outLen; ok = fclose(f) == 0 && ok; if (ok) rename(tmp, C.path); else { remove(tmp); SDL_Log("r3d: could not write %s", C.path); } } }
    free(C.in); free(C.out); C.in = C.out = NULL; C.inLen = C.outLen = C.outCap = 0;
}
/* a picture of the bank as a GL texture, once per wrap mode. Sizes: 16-bit pictures at most texMax (256), ETC1 and DXT ones
 * (4 bits a texel) at most twice that, the same bytes as a 16-bit one of half the side; with the mips the GPU reads the level
 * that fits the screen anyway, so the larger side costs memory, not bandwidth, and keeps the close-ups sharp. sign: a sign,
 * neon or glow (unlit or added light): twice the size whatever the format, and never ETC1 (thin coloured lines on black are
 * where it loses most) */
static GLuint tex_get(const uint8_t *bank, size_t bankLen, const r3d_texent *ents, int nents, uint32_t key, int clamp, int sign) {
    for (int i = 0; i < R.ntex; i++) if (R.tex[i].key == key && R.tex[i].clamp == clamp && R.tex[i].max == sign) return R.tex[i].id;
    const r3d_texent *e = NULL; for (int i = 0; i < nents; i++) if (ents[i].key == key) { e = &ents[i]; break; }
    GLuint id = 0; r3d_dds d;
    if (e && r3d_dds_info(bank + e->off, e->size, &d) == 0) {
        const uint8_t *b = bank + e->off; size_t n = e->size; int gpu = R.s3tc && d.cc != DDS_RGBA32, etc = !gpu && R.etc1 && !sign, big = R.texMax * 2, small = gpu || etc || sign ? big : R.texMax;
        int l0 = 0; while (l0 + 1 < d.nlev && (d.lev[l0].w > small || d.lev[l0].h > small)) l0++;
        while (l0 + 1 < d.nlev && R.vram + (gpu ? (size_t)d.lev[l0].size : (size_t)d.lev[l0].w * d.lev[l0].h * 2 * 4 / 3) > VRAM_MAX) l0++;   /* over the budget, a smaller level still */
        int full = d.lev[d.nlev - 1].w == 1 && d.lev[d.nlev - 1].h == 1, mips = 0; size_t bytes = 0;
        GL(glGenTextures)(1, &id); GL(glBindTexture)(GL_TEXTURE_2D, id);
        const uint8_t *cr = etc ? cache_find(key, d.lev[l0].w, d.lev[l0].h) : NULL;   /* the ETC1 cache: this picture's levels as encoded the first time, or a mark that it is not opaque */
        int w = d.lev[l0].w, h = d.lev[l0].h, isPot = pot(w) && pot(h);
        if (gpu) {
            GLenum fmt = d.cc == DDS_DXT1 ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT : d.cc == DDS_DXT3 ? GL_COMPRESSED_RGBA_S3TC_DXT3_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            int nl = full ? d.nlev - l0 : 1; for (int l = 0; l < nl; l++) { GL(glCompressedTexImage2D)(GL_TEXTURE_2D, l, fmt, d.lev[l0 + l].w, d.lev[l0 + l].h, 0, (GLsizei)d.lev[l0 + l].size, b + d.lev[l0 + l].off); bytes += d.lev[l0 + l].size; }
            mips = nl > 1; R.nDxt++;
        } else if (cr && cr[8]) {   /* opaque, levels at hand: no decoding, no encoding */
            const uint8_t *q = cr + 16; int lw = w, lh = h, nl = cr[8];
            for (int l = 0; l < nl; l++) { size_t sz = etc1_size(lw, lh); GL(glCompressedTexImage2D)(GL_TEXTURE_2D, l, R.etcFmt, lw, lh, 0, (GLsizei)sz, q); q += sz; bytes += sz; lw = lw > 1 ? lw / 2 : 1; lh = lh > 1 ? lh / 2 : 1; }
            mips = nl > 1; R.nEtc++; cache_keep(cr, (size_t)(q - cr)); C.hits++;
        } else {
            int known = cr != NULL, trans = known;   /* a cache mark: not opaque, so straight to the 16-bit size */
            if (known) { cache_keep(cr, 16); C.hits++; }
            uint8_t *rgba = NULL;
            if (!known) { rgba = malloc((size_t)w * h * 4); trans = r3d_dds_decode(b, n, &d, l0, rgba); }
            GL(glPixelStorei)(GL_UNPACK_ALIGNMENT, 1);
            if (!trans && etc) {   /* opaque: ETC1, every level encoded here (a compressed texture cannot glGenerateMipmap), kept for the cache */
                int nl = 1; if (isPot) { int lw = w, lh = h; while (lw > 1 || lh > 1) { lw = lw > 1 ? lw / 2 : 1; lh = lh > 1 ? lh / 2 : 1; nl++; } }
                size_t total = 0; { int lw = w, lh = h; for (int l = 0; l < nl; l++) { total += etc1_size(lw, lh); lw = lw > 1 ? lw / 2 : 1; lh = lh > 1 ? lh / 2 : 1; } }
                uint8_t *rec = malloc(16 + total), *q = rec + 16, *nx = malloc((size_t)(w / 2 + 1) * (h / 2 + 1) * 4); int lw = w, lh = h; Uint64 c0 = SDL_GetPerformanceCounter();
                cache_head(rec, key, w, h, nl, total);
                for (int l = 0; l < nl; l++) { size_t sz = etc1_size(lw, lh); etc1_encode(rgba, lw, lh, q); q += sz;
                    if (l + 1 < nl) { half_size(rgba, lw, lh, nx); uint8_t *t = rgba; rgba = nx; nx = t; lw = lw > 1 ? lw / 2 : 1; lh = lh > 1 ? lh / 2 : 1; } }
                R.texMs += (SDL_GetPerformanceCounter() - c0) * 1000.0 / SDL_GetPerformanceFrequency();   /* the encoder alone (not the upload) */
                q = rec + 16; lw = w; lh = h; for (int l = 0; l < nl; l++) { size_t sz = etc1_size(lw, lh); GL(glCompressedTexImage2D)(GL_TEXTURE_2D, l, R.etcFmt, lw, lh, 0, (GLsizei)sz, q); q += sz; bytes += sz; lw = lw > 1 ? lw / 2 : 1; lh = lh > 1 ? lh / 2 : 1; }
                mips = nl > 1; cache_keep(rec, 16 + total); free(rec); free(nx); R.nEtc++; C.miss++;
            } else {   /* 16 bits a texel: 565 when the picture is opaque, 4444 when it has holes or soft edges; at most texMax unless a sign */
                if (etc && !known) { uint8_t h16[16]; cache_head(h16, key, w, h, 0, 0); cache_keep(h16, 16); C.miss++; }   /* remembered as not for ETC1 */
                if (!sign) { int l1 = l0; while (l1 + 1 < d.nlev && (d.lev[l1].w > R.texMax || d.lev[l1].h > R.texMax)) l1++;
                    if (l1 != l0 || !rgba) { l0 = l1; w = d.lev[l0].w; h = d.lev[l0].h; isPot = pot(w) && pot(h); free(rgba); rgba = malloc((size_t)w * h * 4); trans = r3d_dds_decode(b, n, &d, l0, rgba); } }
                uint16_t *p16 = malloc((size_t)w * h * 2);
                if (!trans) { for (int i = 0; i < w * h; i++) p16[i] = (uint16_t)(((rgba[i * 4] >> 3) << 11) | ((rgba[i * 4 + 1] >> 2) << 5) | (rgba[i * 4 + 2] >> 3)); GL(glTexImage2D)(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, p16); }
                else { for (int i = 0; i < w * h; i++) p16[i] = (uint16_t)(((rgba[i * 4] >> 4) << 12) | ((rgba[i * 4 + 1] >> 4) << 8) | ((rgba[i * 4 + 2] >> 4) << 4) | (rgba[i * 4 + 3] >> 4)); GL(glTexImage2D)(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, p16); }
                bytes = (size_t)w * h * 2; free(p16); R.n16++;
                if (isPot) { GL(glGenerateMipmap)(GL_TEXTURE_2D); mips = 1; bytes += bytes / 3; }
            }
            GL(glPixelStorei)(GL_UNPACK_ALIGNMENT, 4); free(rgba);
        }
        /* bilinear within the nearest mip level: one level read per texel instead of two, the mips keeping the far walls from shimmering */
        GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mips ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR); GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        int rep = isPot || R.npot; GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (clamp & 1) || !rep ? GL_CLAMP_TO_EDGE : GL_REPEAT); GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (clamp & 2) || !rep ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        GL(glBindTexture)(GL_TEXTURE_2D, 0); R.vram += bytes; R.texBytes += bytes; R.textures++;
    }
    R.tex = realloc(R.tex, (R.ntex + 1) * sizeof *R.tex); R.tex[R.ntex].key = key; R.tex[R.ntex].clamp = clamp; R.tex[R.ntex].max = sign; R.tex[R.ntex].id = id; R.ntex++;
    return id;
}

/* ------------------------------------------------------------------ lights, baked per vertex at load */
static uint8_t q8(float v, float scale) { float x = v * scale; return (uint8_t)(x < 0 ? 0 : x > 255 ? 255 : x + 0.5f); }
typedef struct { r3d_light *l; int n; float peak[64]; } lightset;   /* peak: the most each light ever gives (r3d_lights_room) */
/* the game's sum, as the browser port does it per part: the strongest light falling on the point lights it by n.l, the
 * others softly from their side (half-Lambert), each faded between its attenstart and attenend; ambient apart. And for the
 * light show, the four lights that can change the point most (by the strongest any preset makes them) and each one's share */
static void light_at(const lightset *L, const float p[3], const float n[3], float lit[3], float amb[3], lightvtx *o) {
    lit[0] = lit[1] = lit[2] = amb[0] = amb[1] = amb[2] = 0; memset(o, 0, sizeof *o); int hasN = n[0] * n[0] + n[1] * n[1] + n[2] * n[2] > 1e-8f; float best = -1; int bi = -1;
    float k[64], at[64], d[64][3], pot[4] = { 0, 0, 0, 0 }; int top[4] = { -1, -1, -1, -1 };
    for (int i = 0; i < L->n && i < 64; i++) { const r3d_light *l = &L->l[i]; float dx = l->pos[0] - p[0], dy = l->pos[1] - p[1], dz = l->pos[2] - p[2], dd = sqrtf(dx * dx + dy * dy + dz * dz);
        float far = l->far > 0.01f ? l->far : 0.01f, near = l->near < far * 0.999f ? (l->near > 0 ? l->near : 0) : far * 0.999f, att = (far - dd) / (far - near); if (att > 1) att = 1; if (att <= 0) { k[i] = 0; continue; }
        k[i] = l->intensity * att; at[i] = att;
        if (l->kind == L_AMBIENT) { for (int c = 0; c < 3; c++) amb[c] += l->colour[c] * k[i]; k[i] = 0; continue; }
        if (l->kind == L_DIR) { d[i][0] = -l->dir[0]; d[i][1] = -l->dir[1]; d[i][2] = -l->dir[2]; } else { float q = dd > 1e-4f ? dd : 1; d[i][0] = dx / q; d[i][1] = dy / q; d[i][2] = dz / q; }
        float w = fmaxf(l->colour[0], fmaxf(l->colour[1], l->colour[2])) * k[i]; if (w > best) { best = w; bi = i; }
        float pw = att * L->peak[i]; if (pw <= 1e-4f || i >= 32) continue;   /* the shader knows 32 (uRL) */
        for (int t = 0; t < 4; t++) if (pw > pot[t]) { for (int j = 3; j > t; j--) { pot[j] = pot[j - 1]; top[j] = top[j - 1]; } pot[t] = pw; top[t] = i; break; } }
    for (int i = 0; i < L->n && i < 64; i++) { if (k[i] <= 0) continue; float ndl = hasN ? n[0] * d[i][0] + n[1] * d[i][1] + n[2] * d[i][2] : 0;
        float f = i == bi ? (hasN ? fmaxf(ndl, 0) : 0.5f) : (hasN ? 0.5f * (1 + ndl) : 0.5f) * 0.6f;
        for (int c = 0; c < 3; c++) lit[c] += L->l[i].colour[c] * k[i] * f;
        for (int t = 0; t < 4; t++) if (top[t] == i) { o->li[t] = (uint8_t)i; o->lw[t] = q8(at[i] * f, 255); } }
}
static void default_lights(lightset *L) {
    static r3d_light def[2]; def[0].kind = L_AMBIENT; def[0].colour[0] = 135 / 255.0f; def[0].colour[1] = 130 / 255.0f; def[0].colour[2] = 125 / 255.0f; def[0].intensity = 1; def[0].near = 1e5f; def[0].far = 1e6f;
    def[1].kind = L_DIR; def[1].colour[0] = 1; def[1].colour[1] = 173 / 255.0f; def[1].colour[2] = 97 / 255.0f; def[1].intensity = 0.6f; def[1].dir[0] = 0.5f; def[1].dir[1] = -0.7f; def[1].dir[2] = 0.5f; def[1].near = 1e5f; def[1].far = 1e6f;
    L->l = realloc(L->l, (L->n + 2) * sizeof *L->l); L->l[L->n++] = def[0]; L->l[L->n++] = def[1];
}

/* ------------------------------------------------------------------ building the batches */
static void unload(void) {
    for (int i = 0; i < R.nb; i++) { GL(glDeleteBuffers)(1, &R.b[i].vbo); GL(glDeleteBuffers)(1, &R.b[i].lvbo); GL(glDeleteBuffers)(1, &R.b[i].ibo); free(R.b[i].v); free(R.b[i].lv); free(R.b[i].i); free(R.b[i].p); }
    for (int i = 0; i < R.ntex; i++) if (R.tex[i].id) GL(glDeleteTextures)(1, &R.tex[i].id);
    free(R.b); free(R.tex); free(R.shots); free(R.order); R.b = NULL; R.tex = NULL; R.shots = NULL; R.order = NULL; R.nb = R.ntex = R.nshots = R.ngen = 0; R.loaded = 0; R.zone[0] = 0;
    R.meshes = R.skipped = R.tris = R.textures = R.nEtc = R.n16 = R.nDxt = 0; R.vram = R.texBytes = 0; R.texMs = 0; R.sortedSeq = -0x7fffffff; r3d_lights_unload(); r3d_crowd_unload();   /* sortedSeq: no shot id (-1 is the first general shot's) */
}
static void euler_xyz(const float a[3], float m[9]) {   /* three.js rotation.set(x, y, z), order XYZ: Rx * Ry * Rz */
    float cx = cosf(a[0]), sx = sinf(a[0]), cy = cosf(a[1]), sy = sinf(a[1]), cz = cosf(a[2]), sz = sinf(a[2]);
    m[0] = cy * cz; m[1] = -cy * sz; m[2] = sy; m[3] = cx * sz + sx * sy * cz; m[4] = cx * cz - sx * sy * sz; m[5] = -sx * cy; m[6] = sx * sz - cx * sy * cz; m[7] = sx * cz + cx * sy * sz; m[8] = cx * cy;
}
static const char *GENERAL[] = { "cameras", "cameras_orbit", "cameras_pan", "cameras_dolly", "cameras_zoom_slow", "cameras_zoom_out", "cameras_singer", "cameras_drummer", "cameras_bassist", "cameras_guitar_closeup" };
static void cam_of(const r3d_camend *e, camend *o) { memcpy(o->pos, e->pos, 12); float w2 = 1 - e->quat[0] * e->quat[0] - e->quat[1] * e->quat[1] - e->quat[2] * e->quat[2]; memcpy(o->q, e->quat, 12); o->q[3] = sqrtf(w2 > 0 ? w2 : 0); o->fov = e->fov; }
static int load_zone(const char *zone) {
    unload(); Uint32 t0 = SDL_GetTicks(); char name[64]; snprintf(name, sizeof name, "%s.pak.xen", zone); size_t len; uint8_t *pak = app_read_data("zones", name, &len);
    if (!pak) { SDL_Log("r3d: no ZONES/%s", name); return -1; }
    gh3_pakent *ents; int ne = gh3_pak_parse(pak, len, NULL, 0, &ents); uint32_t K_SCN = gh3_qbkey(".scn"), K_TEX = gh3_qbkey(".tex"), kz = gh3_qbkey(zone);
    const gh3_pakent *se = NULL, *te = NULL;   /* the zone's own scene and bank: named after it, else the biggest */
    for (int i = 0; i < ne; i++) { const gh3_pakent *e = &ents[i]; if (!e->data) continue;
        if (e->type == K_SCN && (!se || e->nameKey == kz || (se->nameKey != kz && e->len > se->len))) se = e;
        if (e->type == K_TEX && (!te || e->nameKey == kz || (te->nameKey != kz && e->len > te->len))) te = e; }
    r3d_scene sc; if (!se || !te || r3d_scene_read(se->data, se->len, &sc)) { SDL_Log("r3d: %s holds no stage", name); free(ents); free(pak); return -1; }
    r3d_texent *tents; int nt = r3d_texbank_read(te->data, te->len, &tents); cache_open(zone, te->data, te->len, tents, nt);
    r3d_venueqb vq; r3d_venueqb_read(ents, ne, &vq);
    r3d_band_zone(zone, ents, ne, vq.lights, vq.nlights);   /* where the band stands, its lights */
    r3d_crowd_zone(zone, ents, ne, &vq);   /* the crowd's kinds: the figures painted on its cards; its lights, the lighters' sway */
    r3d_lights_load(zone, ents, ne, &vq);   /* the venue's light show (presets, mapping, pyro): first, the bake keeps for each point the lights the show moves most there */
    lightset L = { NULL, 0, { 0 } }; int roomLit = 0;   /* the lights that fall on the room: not the ones kept for the band or the crowd */
    for (int i = 0; i < vq.nlights && L.n < 32; i++) if (!vq.lights[i].band) { L.l = realloc(L.l, (L.n + 1) * sizeof *L.l); L.l[L.n++] = vq.lights[i]; if (vq.lights[i].kind != L_AMBIENT) roomLit = 1; }
    if (!roomLit) default_lights(&L);   /* a venue with no lights of its own: the game's default daylight */
    r3d_lights_room(L.l, L.n, L.peak);
    const gh3_zonecrowd *crowd = A.lib ? gh3_library_crowd(A.lib, zone) : NULL; int panels = 0;
    for (int mi = 0; mi < sc.nmeshes; mi++) {
        const r3d_mesh *me = &sc.meshes[mi]; if (me->nvert > 65535) { R.skipped++; continue; }
        { const char *sk = getenv("R3D_SKIP"); if (sk) { int hit = 0; for (const char *q = sk; *q; ) { if (atoi(q) == me->n) hit = 1; while (*q && *q != ',') q++; if (*q) q++; } if (hit) continue; } }   /* R3D_SKIP=3,16: parts left out, to tell what is what */
        int lg = r3d_crowd_lighter(me->sector);   /* the crowd's lighters (<zone>_lighter_geoNN): put up by the song's crowd_startlighters */
        if (me->sector && r3d_venueqb_hidden(&vq, me->sector) && !lg) continue;   /* a part the venue's scripts put up later (or never) */
        const r3d_mat *m = r3d_scene_mat(&sc, me->mat); if (!m || !m->ntex) { R.skipped++; continue; }
        int ck = -1; { int isPanel = 0; uint32_t pk = 0; if (crowd) for (int i = 0; i < m->ntex && !isPanel; i++) for (int k = 0; k < crowd->n; k++) if (crowd->tex[k] == m->tex[i]) { isPanel = 1; pk = m->tex[i]; }
          if (isPanel) { panels++; ck = r3d_crowd_card(pk, me->pos, me->nrm, me->nvert); if (ck < 0) continue; } }   /* a crowd card: it wears the picture the crowd's figure of its kind is drawn into (r3d_crowd.c); none: left out */
        /* which texture is the picture, and which the baked light (venueSlots) */
        int n = m->ntex, light = n >= 2 ? n - 1 : -1, pic = ck >= 0 ? 0 : r3d_diffuse_slot(m->template);
        if (ck < 0 && (pic < 0 || pic >= n)) { pic = -1; for (int i = 0; i < n && pic < 0; i++) { if (i == light) continue; const r3d_texent *e = NULL; for (int k = 0; k < nt; k++) if (tents[k].key == m->tex[i]) e = &tents[k];
                if (!e) continue; r3d_look lk = r3d_tex_look(te->data + e->off, e->size); if (!lk.normal && !(lk.flat && lk.w <= 32)) pic = i; }
            if (pic < 0) { pic = 0; const r3d_texent *e = NULL; for (int k = 0; k < nt; k++) if (tents[k].key == m->tex[0]) e = &tents[k]; r3d_look lk = e ? r3d_tex_look(te->data + e->off, e->size) : (r3d_look){ 0 };
                if (e && lk.flat && lk.w <= 32) { R.skipped++; continue; } } }   /* a part with only a flat filler for a picture: a stand-in (the grey figure on the dive bar's stage), not scenery */
        int tf = r3d_template_flags(m->template), unlit = tf & TPL_UNLIT, amb = tf & TPL_AMB, lit = !unlit && !amb, board = tf & TPL_BOARD;
        int smapOk = 0; if (ck < 0 && lit && light >= 0 && (me->uvLast || me->uv2) && m->tex[light] != m->tex[pic]) { const r3d_texent *e = NULL; for (int k = 0; k < nt; k++) if (tents[k].key == m->tex[light]) e = &tents[k]; if (e && e->w > 32) smapOk = 1; }
        int sign = unlit || m->blend == 1;   /* signs, neon and glows: fine lines that are the room's focus */
        GLuint tex = ck >= 0 ? r3d_crowd_tex(ck) : tex_get(te->data, te->len, tents, nt, m->tex[pic], m->clamp[pic < 3 ? pic : 3], sign); if (!tex) { R.skipped++; continue; }   /* a screen or sign whose picture the game makes while it runs */
        GLuint smap = smapOk ? tex_get(te->data, te->len, tents, nt, m->tex[light], 3, 0) : 0;
        if (getenv("R3D_DEBUG")) { const r3d_texent *e = NULL; for (int k = 0; k < nt; k++) if (tents[k].key == m->tex[pic]) e = &tents[k]; r3d_look lk = e ? r3d_tex_look(te->data + e->off, e->size) : (r3d_look){ 0 };
            printf("mesh %3d sector %08x mat %08x tpl %08x tf %02x blend %u aref %u ntex %d pic %08x(%dx%d avg %.0f,%.0f,%.0f%s) tris %5d at %.1f %.1f %.1f r %.1f%s\n", me->n, me->sector, m->key, m->template, tf, m->blend, m->alphaRef, m->ntex, m->tex[pic], lk.w, lk.h, lk.r, lk.g, lk.b, lk.flat ? " flat" : "", me->ntri, me->centre[0], me->centre[1], me->centre[2], me->radius, me->sector && r3d_venueqb_object(&vq, me->sector) ? " (object)" : ""); }
        /* the batch of this material (and whether its parts carry a light map) */
        batch *b = NULL; for (int i = 0; i < R.nb; i++) if (R.b[i].mat == m->key && R.b[i].crowd == ck + 1 && R.b[i].lighter == lg && R.b[i].smapped == (smap != 0) && R.b[i].nvert + me->nvert <= 65535) { b = &R.b[i]; break; }
        if (!b) { R.b = realloc(R.b, (R.nb + 1) * sizeof *R.b); b = &R.b[R.nb++]; memset(b, 0, sizeof *b); b->mat = m->key; b->smapped = smap != 0; b->tex = tex; b->smap = smap ? smap : R.white;
            b->blend = m->blend == 1 ? 1 : m->blend ? 2 : 0; b->board = board; b->crowd = ck + 1; b->lighter = lg; b->colA = lit ? 0 : 1; b->tile = 1;
            int at = m->nprops >= 8 && m->props[1] == 0 && m->props[2] == 0 && m->props[3] == 0 ? 4 : 0; for (int i = 0; i < 4; i++) b->colour[i] = m->nprops >= at + 4 ? fminf(8, fmaxf(0, m->props[at + i])) : 1;
            b->alphaRef = m->alphaRef > 0 ? m->alphaRef / 255.0f : b->blend == 2 ? 0.02f : 0;
            if ((tf & TPL_ANIM) && m->nvsp >= 3 && m->vsp[0] >= 1 && m->vsp[1] >= 1) { b->hasAnim = 1; for (int i = 0; i < 4; i++) b->anim[i] = i < m->nvsp ? m->vsp[i] : 0; }
            if (ck >= 0) { b->hasAnim = 1; r3d_crowd_cell(ck, b->anim); }   /* its kind's cell of the crowd's atlas */
            if (tf & TPL_WIB) { int any = 0; for (int i = 0; i < 11 && i < m->nvsp; i++) if (m->vsp[i]) any = 1; if (m->nvsp >= 11 && any) { b->wib = 1; memcpy(b->wibU, m->vsp, 16); memcpy(b->wibV, m->vsp + 4, 16); memcpy(b->wibR, m->vsp + 8, 12); }
                if (m->nprops >= 6 && m->props[4]) b->tile = m->props[4]; }
            for (int i = 0; i < 3; i++) { b->bmin[i] = 1e30f; b->bmax[i] = -1e30f; } }
        /* where the part stands: a movable object's parts are stored about its own centre */
        float rot[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 }, tr[3] = { 0, 0, 0 }; const r3d_object *ob = me->sector ? r3d_venueqb_object(&vq, me->sector) : NULL; if (ob) { euler_xyz(ob->angles, rot); memcpy(tr, ob->pos, 12); }
        if (b->nvert + me->nvert > b->capV) { b->capV = (b->nvert + me->nvert) * 2; if (b->capV > 65535) b->capV = 65535; b->v = realloc(b->v, (size_t)b->capV * sizeof *b->v); b->lv = realloc(b->lv, (size_t)b->capV * sizeof *b->lv); }
        if (b->nidx + me->ntri * 3 > b->capI) { b->capI = (b->nidx + me->ntri * 3) * 2; b->i = realloc(b->i, (size_t)b->capI * sizeof *b->i); }
        float pmin[3] = { 1e30f, 1e30f, 1e30f }, pmax[3] = { -1e30f, -1e30f, -1e30f };   /* the part's bounds (a card's: its centre and how far its corners reach) */
        for (int v = 0; v < me->nvert; v++) { vtx *o = &b->v[b->nvert + v]; const float *p = me->pos + v * 3, *nn = me->nrm + v * 3; float wp[3], wn[3], wc[3];
            for (int r = 0; r < 3; r++) { wp[r] = rot[r * 3] * p[0] + rot[r * 3 + 1] * p[1] + rot[r * 3 + 2] * p[2] + tr[r]; wn[r] = rot[r * 3] * nn[0] + rot[r * 3 + 1] * nn[1] + rot[r * 3 + 2] * nn[2]; }
            if (board) { for (int r = 0; r < 3; r++) wc[r] = rot[r * 3] * nn[0] + rot[r * 3 + 1] * nn[1] + rot[r * 3 + 2] * nn[2] + tr[r]; for (int r = 0; r < 3; r++) wp[r] = wc[r] + (p[r] - nn[r]); }   /* a card: its centre is stored as the normal */
            else memcpy(wc, wp, 12);
            memcpy(o->pos, wp, 12); memcpy(o->ctr, wc, 12); memcpy(o->col, me->col + v * 4, 4);
            o->uv[0] = me->uv[v * 2]; o->uv[1] = me->uv[v * 2 + 1]; const float *u2 = me->uvLast ? me->uvLast : me->uv2 ? me->uv2 : me->uv; o->uv2[0] = u2[v * 2]; o->uv2[1] = u2[v * 2 + 1];
            float lt[3], am[3]; lightvtx *lo = &b->lv[b->nvert + v]; memset(lo, 0, sizeof *lo);
            if (lit) light_at(&L, wp, board ? (float[3]){ 0, 0, 0 } : wn, lt, am, lo); else if (amb) { lt[0] = lt[1] = lt[2] = 0; light_at(&L, wp, wn, (float[3]){ 0 }, am, lo); memset(lo, 0, sizeof *lo); } else { lt[0] = lt[1] = lt[2] = 1; am[0] = am[1] = am[2] = 0; }
            for (int c = 0; c < 3; c++) { o->lit[c] = q8(lt[c], 255 / 4.0f); b->ambSum[c] += am[c]; } o->lit[3] = 255;
            float reach = board ? sqrtf((wp[0] - wc[0]) * (wp[0] - wc[0]) + (wp[1] - wc[1]) * (wp[1] - wc[1]) + (wp[2] - wc[2]) * (wp[2] - wc[2])) : 0;
            for (int c = 0; c < 3; c++) { float lo = wc[c] - reach, hi = wc[c] + reach; if (!board) lo = hi = wp[c]; if (lo < pmin[c]) pmin[c] = lo; if (hi > pmax[c]) pmax[c] = hi; if (lo < b->bmin[c]) b->bmin[c] = lo; if (hi > b->bmax[c]) b->bmax[c] = hi; } }
        { b->p = realloc(b->p, (b->np + 1) * sizeof *b->p); part *pt = &b->p[b->np++]; pt->first = b->nidx; pt->count = me->ntri * 3; float r2 = 0;
            for (int c = 0; c < 3; c++) { pt->c[c] = (pmin[c] + pmax[c]) / 2; r2 += (pmax[c] - pmin[c]) * (pmax[c] - pmin[c]); } pt->r = sqrtf(r2) / 2; }
        for (int i = 0; i < me->ntri * 3; i++) b->i[b->nidx + i] = (uint16_t)(me->idx[i] + b->nvert);
        b->nvert += me->nvert; b->nidx += me->ntri * 3; R.meshes++; R.tris += me->ntri;
    }
    /* batches of different materials that draw the same (same pictures and numbers, ambient within 1/64) become one: fewer calls */
    int merged = 0;
    for (int i = 0; i < R.nb; i++) for (int j = i + 1; j < R.nb; j++) { batch *a = &R.b[i], *c = &R.b[j]; int same = a->tex == c->tex && a->smap == c->smap && a->blend == c->blend && a->board == c->board && a->colA == c->colA && a->wib == c->wib && a->hasAnim == c->hasAnim && a->crowd == c->crowd && a->lighter == c->lighter
            && !memcmp(a->colour, c->colour, sizeof a->colour) && a->alphaRef == c->alphaRef && a->tile == c->tile && !memcmp(a->anim, c->anim, sizeof a->anim) && !memcmp(a->wibU, c->wibU, sizeof a->wibU) && !memcmp(a->wibV, c->wibV, sizeof a->wibV) && !memcmp(a->wibR, c->wibR, sizeof a->wibR);
        for (int k = 0; k < 3 && same; k++) if (fabs(a->ambSum[k] / a->nvert - c->ambSum[k] / c->nvert) > 1 / 64.0) same = 0;
        if (!same || a->nvert + c->nvert > 65535) continue;
        a->v = realloc(a->v, (size_t)(a->nvert + c->nvert) * sizeof *a->v); memcpy(a->v + a->nvert, c->v, (size_t)c->nvert * sizeof *c->v); a->lv = realloc(a->lv, (size_t)(a->nvert + c->nvert) * sizeof *a->lv); memcpy(a->lv + a->nvert, c->lv, (size_t)c->nvert * sizeof *c->lv);
        a->i = realloc(a->i, (size_t)(a->nidx + c->nidx) * 2); for (int k = 0; k < c->nidx; k++) a->i[a->nidx + k] = (uint16_t)(c->i[k] + a->nvert);
        a->p = realloc(a->p, (size_t)(a->np + c->np) * sizeof *a->p); for (int k = 0; k < c->np; k++) { a->p[a->np + k] = c->p[k]; a->p[a->np + k].first += a->nidx; }
        for (int k = 0; k < 3; k++) { a->ambSum[k] += c->ambSum[k]; a->bmin[k] = fminf(a->bmin[k], c->bmin[k]); a->bmax[k] = fmaxf(a->bmax[k], c->bmax[k]); }
        a->nvert += c->nvert; a->nidx += c->nidx; a->np += c->np; a->capV = a->nvert; a->capI = a->nidx; free(c->v); free(c->lv); free(c->i); free(c->p);
        R.b[j] = R.b[--R.nb]; j--; merged++; }
    /* upload; the ambient of a batch is the mean over its points (the venues' ambient lights reach the whole room) */
    for (int i = 0; i < R.nb; i++) { batch *b = &R.b[i];
        GL(glGenBuffers)(1, &b->vbo); GL(glBindBuffer)(GL_ARRAY_BUFFER, b->vbo); GL(glBufferData)(GL_ARRAY_BUFFER, (GLsizeiptr)b->nvert * sizeof(vtx), b->v, GL_STATIC_DRAW);
        GL(glGenBuffers)(1, &b->lvbo); GL(glBindBuffer)(GL_ARRAY_BUFFER, b->lvbo); GL(glBufferData)(GL_ARRAY_BUFFER, (GLsizeiptr)b->nvert * sizeof(lightvtx), b->lv, GL_STATIC_DRAW);
        GL(glGenBuffers)(1, &b->ibo); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, b->ibo); GL(glBufferData)(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)b->nidx * 2, b->i, GL_STATIC_DRAW);
        R.vram += (size_t)b->nvert * (sizeof(vtx) + sizeof(lightvtx)) + (size_t)b->nidx * 2;
        for (int c = 0; c < 3; c++) { b->amb[c] = (float)(b->ambSum[c] / b->nvert); b->centre[c] = (b->bmin[c] + b->bmax[c]) / 2; }
        float dx = b->bmax[0] - b->bmin[0], dy = b->bmax[1] - b->bmin[1], dz = b->bmax[2] - b->bmin[2]; b->radius = sqrtf(dx * dx + dy * dy + dz * dz) / 2;
        free(b->v); free(b->lv); free(b->i); b->v = NULL; b->lv = NULL; b->i = NULL; }
    GL(glBindBuffer)(GL_ARRAY_BUFFER, 0); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, 0);
    /* the solid batches first, then the solid ones with an alpha test (cut-outs: their own program, after the walls that hide them),
     * then the see-through ones; by texture inside each group (the first two are put near to far at each new shot, the last far to near each frame) */
#define GROUP(b) ((b).blend ? 2 : (b).alphaRef > 0 ? 1 : 0)
    for (int i = 1; i < R.nb; i++) for (int j = i; j > 0 && GROUP(R.b[j]) * 1000000 + (int)R.b[j].tex < GROUP(R.b[j - 1]) * 1000000 + (int)R.b[j - 1].tex; j--) { batch t = R.b[j]; R.b[j] = R.b[j - 1]; R.b[j - 1] = t; }
    /* the camera shots: fixed in the room, or following a band member (lockto: placed against where that one stands; shot_ok() leaves them and the close-ups out while there is no band); the general lists, shuffled, for when no song is cueing */
    uint32_t gk[16]; int ng = 0; for (int i = 0; i < (int)(sizeof GENERAL / sizeof *GENERAL); i++) { char s[64]; snprintf(s, sizeof s, "%s_%s", zone, GENERAL[i]); gk[ng++] = gh3_qbkey(s); } R.kCameras = gk[0];
    static const char *ROLES[BAND_ROLES] = { "guitarist", "bassist", "drummer", "vocalist" };
    for (int i = 0; i < vq.nshots; i++) { const r3d_shot *s = &vq.shots[i]; int who = -1; if (s->a.lock) { for (int r = 0; r < BAND_ROLES; r++) if (s->a.lock == gh3_qbkey(ROLES[r])) who = r; if (who < 0) continue; }
        R.shots = realloc(R.shots, (R.nshots + 1) * sizeof *R.shots); shot *o = &R.shots[R.nshots++]; o->list = s->list; o->type = s->type; o->name = s->name; cam_of(&s->a, &o->a); o->hasB = s->hasB; if (s->hasB) cam_of(&s->b, &o->b); o->orbit = s->orbit * (float)M_PI / 180; o->who = who; }
    R.order = malloc((R.nshots ? R.nshots : 1) * sizeof *R.order); R.ngen = 0;
    for (int k = 0; k < ng; k++) for (int i = 0; i < R.nshots; i++) if (R.shots[i].list == gk[k]) R.order[R.ngen++] = i;
    if (!R.ngen) for (int i = 0; i < R.nshots; i++) R.order[R.ngen++] = i;
    for (int i = R.ngen - 1, r = 0x2545f491; i > 0; i--) { r = (uint32_t)((r ^ (r >> 15)) * 0x2c1b3c6du); int j = (int)(r % (uint32_t)(i + 1)), t = R.order[i]; R.order[i] = R.order[j]; R.order[j] = t; }   /* the browser port's fixed shuffle */
    for (int c = 0; c < 3; c++) R.ctr[c] = (sc.bmin[c] + sc.bmax[c]) / 2; R.span = fmaxf(sc.bmax[0] - sc.bmin[0], sc.bmax[2] - sc.bmin[2]);
    int cHits = C.hits, cMiss = C.miss; cache_close();
    SDL_Log("r3d: %s: %d meshes in %d batches (%d merged; %d skipped, %d crowd cards), %d triangles, %d textures (%d ETC1, %d 16-bit, %d DXT; %.1f MB; ETC1 cache %d found, %d made in %.0f ms), %.1f MB on the GPU, %d shots (%d general; %d lights, %d objects, %d hidden), %u ms",
        zone, R.meshes, R.nb, merged, R.skipped, panels, R.tris, R.textures, R.nEtc, R.n16, R.nDxt, R.texBytes / 1048576.0, cHits, cMiss, R.texMs, R.vram / 1048576.0, R.nshots, R.ngen, vq.nlights, vq.nobjects, vq.nhidden, SDL_GetTicks() - t0);
    R.loadMs = SDL_GetTicks() - t0; snprintf(R.zone, sizeof R.zone, "%s", zone); R.loaded = 1;
    free(L.l); r3d_venueqb_free(&vq); free(tents); r3d_scene_free(&sc); free(ents); free(pak); return 0;
}
const char *r3d_zone(void) { return R.zone; }
void r3d_select(const char *zone) {
    if (!R.ready) return; if (!zone || strncmp(zone, "z_", 2)) zone = "z_dive";
    if (!R.loaded || strcmp(R.zone, zone)) load_zone(zone);
    r3d_band_select();   /* who plays this song (a boss battle has the boss on stage) */
    r3d_crowd_select();   /* when the song has the crowd's lighters up */
}

/* ------------------------------------------------------------------ drawing */
typedef struct { GLint prog, abuf, ebuf, active, tex0, tex1, view[4], scissor[4], beq[2], bfn[4], attribs[8], nattr, fbo; GLfloat clear[4]; GLboolean blend, scis, depth, cull, dmask, cmask[4]; } glstate;
static void state_save(glstate *s) {
    GL(glGetIntegerv)(GL_CURRENT_PROGRAM, &s->prog); GL(glGetIntegerv)(GL_ARRAY_BUFFER_BINDING, &s->abuf); GL(glGetIntegerv)(GL_ELEMENT_ARRAY_BUFFER_BINDING, &s->ebuf);
    GL(glGetIntegerv)(GL_ACTIVE_TEXTURE, &s->active); GL(glActiveTexture)(GL_TEXTURE0); GL(glGetIntegerv)(GL_TEXTURE_BINDING_2D, &s->tex0); GL(glActiveTexture)(GL_TEXTURE1); GL(glGetIntegerv)(GL_TEXTURE_BINDING_2D, &s->tex1);
    GL(glGetIntegerv)(GL_VIEWPORT, s->view); GL(glGetIntegerv)(GL_SCISSOR_BOX, s->scissor); GL(glGetIntegerv)(GL_BLEND_EQUATION_RGB, &s->beq[0]); GL(glGetIntegerv)(GL_BLEND_EQUATION_ALPHA, &s->beq[1]);
    GL(glGetIntegerv)(GL_BLEND_SRC_RGB, &s->bfn[0]); GL(glGetIntegerv)(GL_BLEND_DST_RGB, &s->bfn[1]); GL(glGetIntegerv)(GL_BLEND_SRC_ALPHA, &s->bfn[2]); GL(glGetIntegerv)(GL_BLEND_DST_ALPHA, &s->bfn[3]);
    s->blend = GL(glIsEnabled)(GL_BLEND); s->scis = GL(glIsEnabled)(GL_SCISSOR_TEST); s->depth = GL(glIsEnabled)(GL_DEPTH_TEST); s->cull = GL(glIsEnabled)(GL_CULL_FACE);
    GL(glGetBooleanv)(GL_DEPTH_WRITEMASK, &s->dmask); GL(glGetBooleanv)(GL_COLOR_WRITEMASK, s->cmask); GL(glGetIntegerv)(GL_FRAMEBUFFER_BINDING, &s->fbo); GL(glGetFloatv)(GL_COLOR_CLEAR_VALUE, s->clear);   /* SDL keeps its clear colour cached */
    GLint n = 0; GL(glGetIntegerv)(GL_MAX_VERTEX_ATTRIBS, &n); s->nattr = n < 8 ? n : 8; for (int i = 0; i < s->nattr; i++) GL(glGetVertexAttribiv)(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &s->attribs[i]);
}
static void state_restore(const glstate *s) {
    for (int i = 0; i < s->nattr; i++) { if (s->attribs[i]) GL(glEnableVertexAttribArray)(i); else GL(glDisableVertexAttribArray)(i); }
    GL(glUseProgram)(s->prog); GL(glBindBuffer)(GL_ARRAY_BUFFER, s->abuf); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, s->ebuf);
    GL(glActiveTexture)(GL_TEXTURE1); GL(glBindTexture)(GL_TEXTURE_2D, s->tex1); GL(glActiveTexture)(GL_TEXTURE0); GL(glBindTexture)(GL_TEXTURE_2D, s->tex0); GL(glActiveTexture)(s->active);
    GL(glViewport)(s->view[0], s->view[1], s->view[2], s->view[3]); GL(glScissor)(s->scissor[0], s->scissor[1], s->scissor[2], s->scissor[3]);
    GL(glBlendEquationSeparate)(s->beq[0], s->beq[1]); GL(glBlendFuncSeparate)(s->bfn[0], s->bfn[1], s->bfn[2], s->bfn[3]);
    if (s->blend) GL(glEnable)(GL_BLEND); else GL(glDisable)(GL_BLEND); if (s->scis) GL(glEnable)(GL_SCISSOR_TEST); else GL(glDisable)(GL_SCISSOR_TEST);
    if (s->depth) GL(glEnable)(GL_DEPTH_TEST); else GL(glDisable)(GL_DEPTH_TEST); if (s->cull) GL(glEnable)(GL_CULL_FACE); else GL(glDisable)(GL_CULL_FACE);
    GL(glDepthMask)(s->dmask); GL(glColorMask)(s->cmask[0], s->cmask[1], s->cmask[2], s->cmask[3]); GL(glBindFramebuffer)(GL_FRAMEBUFFER, (GLuint)s->fbo); GL(glClearColor)(s->clear[0], s->clear[1], s->clear[2], s->clear[3]);
}
/* a song's camera cues (cameracuts_notemapping of qb.pak: a midi note names a shot list, and sometimes the shot's Type or NAME) */
typedef struct { int note; const char *prefix, *type; uint32_t name; } cuemap;
static const cuemap CUEMAP[] = { { 79, "cameras", 0, 0 }, { 80, "cameras_intro", 0, 0 }, { 81, "cameras_solo", 0, 0 }, { 90, "cameras_solo", 0, 0 }, { 82, "cameras_orbit", 0, 0xf9784194 }, { 83, "cameras_zoom", 0, 0 },
    { 84, "cameras", "closeup", 0 }, { 85, "cameras", "mid", 0 }, { 86, "cameras", "closeup", 0 }, { 87, "cameras", "Stage", 0 }, { 110, "cameras", "Stage", 0 }, { 88, "cameras_singer", 0, 0 }, { 89, "cameras_drummer", 0, 0 },
    { 91, "cameras_orbit", 0, 0xf9784194 }, { 92, "cameras_orbit", 0, 0x32998951 }, { 93, "cameras_orbit", 0, 0xfeab597e }, { 94, "cameras_orbit", 0, 0x665844b0 }, { 103, "cameras_orbit", 0, 0x89837c69 },
    { 95, "cameras_guitar_closeup", 0, 0x1c28fd2a }, { 96, "cameras_starpower", 0, 0 }, { 97, "cameras_bassist", 0, 0 }, { 98, "cameras_walk", 0, 0 }, { 99, "cameras_vert_guitarist", 0, 0 }, { 100, "cameras_vert_drummer", 0, 0 },
    { 101, "cameras_vert_stagefront", 0, 0 }, { 102, "cameras_vert_stagerear", 0, 0 }, { 104, "cameras_singer_closeup", 0, 0 }, { 105, "cameras_special", 0, 0 }, { 106, "cameras_special", 0, 0xf38b1ab0 }, { 107, "cameras_special", 0, 0x6a824b0a },
    { 108, "cameras_special", 0, 0x1d857b9c }, { 109, "cameras_special", 0, 0x83e1ee3f }, { 111, "cameras_zoom_slow", 0, 0 }, { 112, "cameras_zoom_fast", 0, 0 }, { 113, "cameras_zoom_out", 0, 0 }, { 114, "cameras_pan", 0, 0 },
    { 115, "cameras_boss_closeup", 0, 0 }, { 116, "cameras_player_closeup", 0, 0 }, { 117, "cameras_stagedive", 0, 0 } };
static const cuemap *cue_of(int note) { for (int i = 0; i < (int)(sizeof CUEMAP / sizeof *CUEMAP); i++) if (CUEMAP[i].note == note) return &CUEMAP[i]; return NULL; }
/* the shot a cue cuts to: one of its list (narrowed by Type / NAME when the cue says so), picked by hashing the cue's index as the browser port does; not the one already showing */
static int shot_ok(const shot *s) { return s->who >= 0 ? r3d_band_where(s->who, NULL, NULL) : s->a.fov >= 45 || r3d_band_where(-1, NULL, NULL); }   /* one that follows a member who is not on stage, or a close-up with no band: not shown */
static int cut_to(const cuemap *m, int i, int prev) {
    if (!m || !R.nshots) return prev; char nm[64]; snprintf(nm, sizeof nm, "%s_%s", R.zone, m->prefix); uint32_t lk = gh3_qbkey(nm), tk = m->type ? gh3_qbkey(m->type) : 0;
    static int cand[512]; int n = 0;
    for (int pass = 0; pass < 4 && !n; pass++) for (int k = 0; k < R.nshots && n < 512; k++) { const shot *s = &R.shots[k]; if (!shot_ok(s)) continue;
        if (pass == 0 && (s->list != lk || (tk && s->type != tk) || (m->name && s->name != m->name))) continue;
        if (pass == 1 && s->list != lk) continue; if (pass == 2 && s->list != R.kCameras) continue; if (pass == 3) { for (int g = 0; g < R.ngen; g++) if (R.order[g] == k) { cand[n++] = k; break; } continue; }
        cand[n++] = k; }
    if (!n) return prev; int k = (int)(((uint32_t)(i + 1) * 0x9e3779b1u) >> 8) % n; if (cand[k] == prev && n > 1) k = (k + 1) % n; return cand[k];
}
static void place(const shot *s, double u, double t, float *view, float *fov) {
    float pos[3], q[4], yaw = 0; double e = u * u * (3 - 2 * u);
    if (s->hasB) { for (int i = 0; i < 3; i++) pos[i] = s->a.pos[i] + (s->b.pos[i] - s->a.pos[i]) * (float)e; quat_nlerp(s->a.q, s->b.q, (float)e, q); *fov = s->a.fov + (s->b.fov - s->a.fov) * (float)e; }
    else { memcpy(pos, s->a.pos, 12); memcpy(q, s->a.q, 16); *fov = s->a.fov; yaw = sinf((float)t * 0.23f) * 0.012f; }
    if (s->orbit) { float a = s->orbit * (float)u, c = cosf(a / 2), sn = sinf(a / 2), qo[4] = { 0, sn, 0, c }, p2[3], q2[4]; quat_rot(qo, pos, p2); quat_mul(qo, q, q2); memcpy(pos, p2, 12); memcpy(q, q2, 16); }   /* circling shots turn about the room's middle (or the one they follow) */
    float at[3], ay; if (s->who >= 0 && r3d_band_where(s->who, at, &ay)) { float qy[4] = { 0, sinf(ay / 2), 0, cosf(ay / 2) }, p2[3], q2[4]; quat_rot(qy, pos, p2); quat_mul(qy, q, q2); for (int i = 0; i < 3; i++) pos[i] = p2[i] + at[i]; memcpy(q, q2, 16); }   /* placed against the band member it follows */
    mat_view(view, pos, q, yaw);
}
/* the camera at this moment: over a song, the song's own cue list cuts between the venue's shots; otherwise the general shots one after another
 * (a->b moves eased, single ones swaying); with no shots at all, a slow circle round the room */
static struct { int on, seq; float view[16], fov; } camOver;   /* --r3dtest --band: a camera on one member (seq: a new one each time it is set, a "cut" for the draw order) */
static void camera(double t, float *view, float *proj, int *shotId) {   /* shotId: changes when the camera cuts */
    float fov = 72; if (t < 0) t = 0; int placed = 0;
    if (camOver.on) { memcpy(view, camOver.view, 64); fov = camOver.fov; placed = 1; *shotId = 0x40000000 + camOver.seq; }
    const gh3_songdata *sd = A.screen == SC_PLAY && A.S.loaded && A.S.sd.ncams ? &A.S.sd : NULL;
    static const gh3_songdata *song; static int pCam = -1, cur = -1; static double t0, t1, lastMs;
    if (placed) {}
    else if (sd && R.nshots) { double ms = A.S.run.now; int n = sd->ncams;
        if (sd != song || ms + 1500 < lastMs) { song = sd; pCam = -1; cur = -1; }   /* a new song, or the clock wound back (practice, a restart) */
        lastMs = ms; int k = pCam < 0 ? 0 : pCam; while (k + 1 < n && sd->cams[k + 1][0] <= ms) k++;
        if (k != pCam) { pCam = k; int nx = cut_to(cue_of(sd->cams[k][1]), k, cur);
            if (nx != cur || cur < 0) { cur = nx >= 0 ? nx : R.order[0]; t0 = sd->cams[k][0]; int j = k + 1; while (j < n && !cue_of(sd->cams[j][1])) j++; t1 = j < n ? sd->cams[j][0] : t0 + (sd->cams[k][2] > 4000 ? sd->cams[k][2] : 4000); } }   /* a cue that lands on the shot already showing lets it carry on */
        if (cur >= 0) { double u = (ms - t0) / (t1 - t0 > 1 ? t1 - t0 : 1); place(&R.shots[cur], u < 0 ? 0 : u > 1 ? 1 : u, t, view, &fov); placed = 1; *shotId = cur * 7919 + (int)t0; } }
    else song = NULL;
    if (!placed && R.nshots) { double n = floor(t / (SHOT_MS / 1000)), u = t / (SHOT_MS / 1000) - n; int g = (int)((long)n % R.ngen); for (int k = 0; k < R.ngen && !shot_ok(&R.shots[R.order[g]]); k++) g = (g + 1) % R.ngen;
        place(&R.shots[R.order[g]], u, t, view, &fov); placed = 1; *shotId = -1 - (int)n; }
    if (!placed) { float pos[3], q[4], a = (float)t * 0.05f; *shotId = (int)(t / 2); pos[0] = R.ctr[0] + sinf(a) * R.span * 0.2f; pos[1] = R.ctr[1]; pos[2] = R.ctr[2] + cosf(a) * R.span * 0.2f;
        float d[3] = { R.ctr[0] - pos[0], 0, R.ctr[2] - pos[2] }, L = sqrtf(d[0] * d[0] + d[2] * d[2]); if (L < 1e-4f) { d[2] = 1; L = 1; } float yaw = atan2f(d[0] / L, d[2] / L); q[0] = 0; q[1] = sinf(yaw / 2); q[2] = 0; q[3] = cosf(yaw / 2);   /* the camera's +Z toward the middle */
        mat_view(view, pos, q, 0); }
    float vf = 2 * atanf(tanf(fov * (float)M_PI / 360) * 0.75f);   /* the stored angle is the width of a 4:3 picture */
    mat_perspective(proj, vf, (float)R.W / R.H, Z_NEAR, Z_FAR);
}
/* one batch: the runs of its parts that are in view and not under a pixel, each run one call (small hidden gaps drawn through) */
typedef struct { float pl[6][4], view[16], pxk; } cullctx;
static void draw_batch(const batch *b, const r3d_prog *P, const cullctx *cc) {
    if ((b->crowd && !r3d_crowd_shown(b->crowd - 1)) || (b->lighter && !r3d_crowd_lighters())) return;   /* Efeitos de palco off, or its figure not drawn yet */
    if (!sphere_in(cc->pl, b->centre, b->radius)) { R.culled += b->np; return; }
    static int (*run)[2], cap; if (b->np > cap) { cap = b->np; run = realloc(run, (size_t)cap * sizeof *run); } int nr = 0;
    for (int k = 0; k < b->np; k++) { const part *p = &b->p[k];
        float z = -(cc->view[2] * p->c[0] + cc->view[6] * p->c[1] + cc->view[10] * p->c[2] + cc->view[14]);   /* how far in front of the camera */
        if (!sphere_in(cc->pl, p->c, p->r) || (z > p->r && p->r * cc->pxk < LOD_PX * z)) { R.culled++; continue; }
        if (nr && p->first - run[nr - 1][1] <= RUN_GAP && p->first >= run[nr - 1][1]) run[nr - 1][1] = p->first + p->count; else { run[nr][0] = p->first; run[nr][1] = p->first + p->count; nr++; } }
    if (!nr) return; if (b->crowd) r3d_crowd_seen(b->crowd - 1);   /* on screen: its figure keeps moving */
    GL(glBindBuffer)(GL_ARRAY_BUFFER, b->lvbo); GL(glVertexAttribPointer)(6, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(lightvtx), (void *)0); GL(glVertexAttribPointer)(7, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(lightvtx), (void *)4);
    GL(glBindBuffer)(GL_ARRAY_BUFFER, b->vbo); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, b->ibo);
    GL(glVertexAttribPointer)(0, 3, GL_FLOAT, GL_FALSE, sizeof(vtx), (void *)0); GL(glVertexAttribPointer)(1, 3, GL_FLOAT, GL_FALSE, sizeof(vtx), (void *)12);
    GL(glVertexAttribPointer)(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vtx), (void *)24); GL(glVertexAttribPointer)(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vtx), (void *)28);
    GL(glVertexAttribPointer)(4, 2, GL_FLOAT, GL_FALSE, sizeof(vtx), (void *)32); GL(glVertexAttribPointer)(5, 2, GL_FLOAT, GL_FALSE, sizeof(vtx), (void *)40);
    GL(glActiveTexture)(GL_TEXTURE1); GL(glBindTexture)(GL_TEXTURE_2D, b->smap); GL(glActiveTexture)(GL_TEXTURE0); GL(glBindTexture)(GL_TEXTURE_2D, b->tex);
    GL(glUniform1f)(P->uBoard, (float)b->board); GL(glUniform4f)(P->uAnim, b->hasAnim ? b->anim[0] : 0, b->anim[1], b->anim[2], b->anim[3]);
    GL(glUniform1f)(P->uWib, (float)b->wib); if (b->wib) { GL(glUniform4f)(P->uWibU, b->wibU[0], b->wibU[1], b->wibU[2], b->wibU[3]); GL(glUniform4f)(P->uWibV, b->wibV[0], b->wibV[1], b->wibV[2], b->wibV[3]); GL(glUniform3f)(P->uWibR, b->wibR[0], b->wibR[1], b->wibR[2]); }
    float sw[3], sv[16]; int moved = b->lighter && r3d_crowd_sway(b->lighter, sw); if (moved) { memcpy(sv, cc->view, 64); for (int c = 0; c < 3; c++) sv[12 + c] += cc->view[c] * sw[0] + cc->view[4 + c] * sw[1] + cc->view[8 + c] * sw[2]; GL(glUniformMatrix4fv)(P->uView, 1, GL_FALSE, sv); }   /* a lighter group, over by its sway */
    GL(glUniform4f)(P->uColour, b->colour[0], b->colour[1], b->colour[2], b->colour[3]); r3d_lights_batch(P == &R.P[1], !b->colA, b->amb); GL(glUniform1f)(P->uAlphaRef, b->alphaRef); GL(glUniform1f)(P->uColA, (float)b->colA); GL(glUniform1f)(P->uTile, b->tile);
    if (b->blend == 1) { GL(glEnable)(GL_BLEND); GL(glBlendFunc)(GL_ONE, GL_ONE); GL(glDepthMask)(GL_FALSE); } else if (b->blend == 2) { GL(glEnable)(GL_BLEND); GL(glBlendFunc)(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); GL(glDepthMask)(GL_FALSE); } else { GL(glDisable)(GL_BLEND); GL(glDepthMask)(GL_TRUE); }
    for (int r = 0; r < nr; r++) { GL(glDrawElements)(GL_TRIANGLES, run[r][1] - run[r][0], GL_UNSIGNED_SHORT, (void *)(size_t)(run[r][0] * 2)); R.drawCalls++; R.drawnTris += (run[r][1] - run[r][0]) / 3; }
    if (moved) GL(glUniformMatrix4fv)(P->uView, 1, GL_FALSE, cc->view);
}

/* ---- the framebuffer the venue is drawn into below Alta ---- */
static int fbo_ready(int w, int h) {
    if (R.fboBad) return 0; if (R.fbo && R.fw == w && R.fh == h) return 1;
    if (!R.fbo) { GL(glGenFramebuffers)(1, &R.fbo); GL(glGenTextures)(1, &R.fboTex); GL(glGenRenderbuffers)(1, &R.fboDepth); }
    GL(glBindTexture)(GL_TEXTURE_2D, R.fboTex); GL(glTexImage2D)(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);   /* stretched smooth */
    GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL(glBindRenderbuffer)(GL_RENDERBUFFER, R.fboDepth); GL(glRenderbufferStorage)(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, w, h); GL(glBindRenderbuffer)(GL_RENDERBUFFER, 0);
    GL(glBindFramebuffer)(GL_FRAMEBUFFER, R.fbo); GL(glFramebufferTexture2D)(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, R.fboTex, 0); GL(glFramebufferRenderbuffer)(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, R.fboDepth);
    GLenum st = GL(glCheckFramebufferStatus)(GL_FRAMEBUFFER);
    if (st != GL_FRAMEBUFFER_COMPLETE) { SDL_Log("r3d: the venue's framebuffer %dx%d is not complete (0x%x): drawn at the screen's size", w, h, st); R.fboBad = 1;
        GL(glDeleteFramebuffers)(1, &R.fbo); GL(glDeleteTextures)(1, &R.fboTex); GL(glDeleteRenderbuffers)(1, &R.fboDepth); R.fbo = R.fboTex = R.fboDepth = 0; return 0; }
    R.fw = w; R.fh = h; SDL_Log("r3d: venue drawn at %dx%d, stretched to %dx%d", w, h, R.W, R.H); return 1;
}
static int res_mode(void) { const char *e = getenv("R3D_RES"); int r = e && *e ? atoi(e) : SET.stage3dRes; return r < 0 ? 0 : r > 2 ? 2 : r; }   /* 0 Alta, 1 Média, 2 Baixa */
static const char *RES_NAME[3] = { "Alta", "Média", "Baixa" };

/* ---- GPU time of the venue's pass (timer queries, read a few frames later so nothing waits) ---- */
static void timer_poll(int i) {
    if (!R.qBusy[i]) return; GLuint av = 0; R.getQ(R.q[i], GL_QUERY_RESULT_AVAILABLE_EXT, &av); if (!av) return;
    GLuint ns = 0; R.getQ(R.q[i], GL_QUERY_RESULT_EXT, &ns); R.qBusy[i] = 0;
    GLint dj = 0; if (R.disjoint) { GL(glGetIntegerv)(GL_GPU_DISJOINT_EXT, &dj); if (dj) return; }   /* the clock jumped (a frequency change): this one does not count */
    if (R.pw.t0) { R.pw.sumGpu += ns / 1e6; R.pw.nGpu++; }
}

/* the venue's light arrays (aLI, aLW) on for its batches, off for anything else: with its array off aLW reads 0, no share of any light, so
 * a part drawn with r3d_program() and r3d_vtx alone keeps its own aLit under the show */
static void light_arrays(int on) {
    if (on) { GL(glEnableVertexAttribArray)(6); GL(glEnableVertexAttribArray)(7); return; }
    GL(glDisableVertexAttribArray)(6); GL(glDisableVertexAttribArray)(7); if (R.attrib4f) R.attrib4f(7, 0, 0, 0, 0);
}
static int draw(double perf, int sp, float energy) {
    double t = perf / 1000; float view[16], proj[16], vp[16]; int shotId = 0; camera(t, view, proj, &shotId); mat_mul(proj, view, vp); r3d_lights_update(t);
    R.flush(R.ren); glstate st; state_save(&st); R.drawCalls = R.drawnTris = R.culled = 0;
    r3d_crowd_update(perf, sp); R.drawCalls += r3d_crowd_stats()->draws;   /* the crowd's figures due a redraw, into their own pictures, before the venue's pass */
    int res = res_mode(), fw = R.W, fh = R.H; if (res && R.blitProg) { float k = res == 2 ? 0.5f : 0.75f; fw = ((int)(R.W * k + 0.5f) + 1) & ~1; fh = ((int)(R.H * k + 0.5f) + 1) & ~1; }
    int off = (fw != R.W || fh != R.H) && fbo_ready(fw, fh); if (!off) { fw = R.W; fh = R.H; }
    int qi = -1; if (R.timer) { for (int i = 0; i < 4; i++) timer_poll(i); if (!R.qBusy[R.qi]) { qi = R.qi; R.beginQ(GL_TIME_ELAPSED_EXT, R.q[qi]); } }
    GL(glDisable)(GL_SCISSOR_TEST); GL(glEnable)(GL_DEPTH_TEST); GL(glDepthFunc)(GL_LEQUAL); GL(glDepthMask)(GL_TRUE); GL(glColorMask)(1, 1, 1, 1); GL(glDisable)(GL_CULL_FACE); GL(glDisable)(GL_BLEND);
    if (off) { GL(glBindFramebuffer)(GL_FRAMEBUFFER, R.fbo); GL(glViewport)(0, 0, fw, fh); GL(glClearColor)(18 / 255.0f, 16 / 255.0f, 24 / 255.0f, 1); GL(glClear)(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); }   /* all of it cleared: a tiler reads nothing back */
    else { GL(glViewport)(0, 0, R.W, R.H); GL(glClear)(GL_DEPTH_BUFFER_BIT); }
    GL(glBlendEquationSeparate)(GL_FUNC_ADD, GL_FUNC_ADD);
    float k = 1 + 0.12f * energy, tint[3] = { k, k, k }; if (sp) { tint[0] = k * 0.78f; tint[1] = k * 0.98f; tint[2] = k * 1.18f; }   /* star power: the room goes blue */
    for (int p = 0; p < 2; p++) { const r3d_prog *P = &R.P[p]; GL(glUseProgram)(P->prog); GL(glUniformMatrix4fv)(P->uProj, 1, GL_FALSE, proj); GL(glUniformMatrix4fv)(P->uView, 1, GL_FALSE, view);
        GL(glUniform1f)(P->uTime, (float)fmod(t, 4096)); GL(glUniform1i)(P->uMap, 0); GL(glUniform1i)(P->uSmap, 1); GL(glUniform3f)(P->uTint, tint[0], tint[1], tint[2]); r3d_lights_uniforms(p); }
    for (int i = 0; i < st.nattr; i++) GL(glDisableVertexAttribArray)(i);
    for (int i = 0; i < NATTR; i++) GL(glEnableVertexAttribArray)(i);
    float cam[3] = { -(view[0] * view[12] + view[1] * view[13] + view[2] * view[14]), -(view[4] * view[12] + view[5] * view[13] + view[6] * view[14]), -(view[8] * view[12] + view[9] * view[13] + view[10] * view[14]) };
    memcpy(R.V.view, view, 64); memcpy(R.V.proj, proj, 64); memcpy(R.V.vp, vp, 64); memcpy(R.V.cam, cam, 12); memcpy(R.V.tint, tint, 12); R.V.t = t; R.V.w = fw; R.V.h = fh;
    cullctx cc; frustum(vp, cc.pl); memcpy(cc.view, view, 64); cc.pxk = proj[5] * R.H / 2;
    /* the solid groups near to far, put in order again at each cut (not every frame: an order that flips mid-shot could make touching surfaces flicker) */
    int g1 = 0, g2 = 0; while (g1 < R.nb && GROUP(R.b[g1]) == 0) g1++; g2 = g1; while (g2 < R.nb && GROUP(R.b[g2]) == 1) g2++;
    if (shotId != R.sortedSeq) { R.sortedSeq = shotId;
        for (int i = 0; i < g2; i++) { batch *b = &R.b[i]; float dx = b->centre[0] - cam[0], dy = b->centre[1] - cam[1], dz = b->centre[2] - cam[2]; b->order = sqrtf(dx * dx + dy * dy + dz * dz) - b->radius; }
        for (int g = 0; g < 2; g++) { int lo = g ? g1 : 0, hi = g ? g2 : g1; for (int i = lo + 1; i < hi; i++) for (int j = i; j > lo && R.b[j].order < R.b[j - 1].order; j--) { batch t2 = R.b[j]; R.b[j] = R.b[j - 1]; R.b[j - 1] = t2; } } }
    GL(glUseProgram)(R.P[0].prog); for (int i = 0; i < g1; i++) draw_batch(&R.b[i], &R.P[0], &cc);
    GL(glUseProgram)(R.P[1].prog); for (int i = g1; i < g2; i++) draw_batch(&R.b[i], &R.P[1], &cc);
    light_arrays(0);   /* what is drawn inside the pass besides the venue's batches sees the six arrays of r3d_vtx only */
    if (r3d_band_draw(view, proj, perf, sp, energy)) for (int i = 0; i < 6; i++) GL(glEnableVertexAttribArray)(i);   /* the band, after the solid parts, at the pass's size and depth; it leaves its own program and arrays: the venue's six again (the program is set per batch below) */
    if (g2 < R.nb) { light_arrays(1);   /* the see-through parts, farthest first */
        static float *dist; static int *idx, cap; int n = R.nb - g2; if (n > cap) { cap = n; dist = realloc(dist, cap * sizeof *dist); idx = realloc(idx, cap * sizeof *idx); }
        for (int i = 0; i < n; i++) { const batch *b = &R.b[g2 + i]; float dx = b->centre[0] - cam[0], dy = b->centre[1] - cam[1], dz = b->centre[2] - cam[2]; dist[i] = dx * dx + dy * dy + dz * dz; idx[i] = g2 + i; }
        for (int i = 1; i < n; i++) for (int j = i; j > 0 && dist[idx[j] - g2] > dist[idx[j - 1] - g2]; j--) { int t2 = idx[j]; idx[j] = idx[j - 1]; idx[j - 1] = t2; }
        int cur = -1; for (int i = 0; i < n; i++) { const batch *b = &R.b[idx[i]]; int p = b->alphaRef > 0; if (p != cur) { cur = p; GL(glUseProgram)(R.P[p].prog); } draw_batch(b, &R.P[p], &cc); } light_arrays(0); }
    GL(glBindBuffer)(GL_ARRAY_BUFFER, 0); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, 0); for (int i = 0; i < 6; i++) GL(glDisableVertexAttribArray)(i);
    r3d_lights_draw(vp, view);   /* beams, the housings' glow, pyro sparks: additive, depth tested against the venue and the band, at the pass's size */
    GL(glDepthMask)(GL_TRUE);
    if (off) {   /* the depth is not needed past the pass (Mali then never writes it out); the picture stretched over the window */
        if (R.discard) { GLenum att = GL_DEPTH_ATTACHMENT; R.discard(GL_FRAMEBUFFER, 1, &att); }
        GL(glBindFramebuffer)(GL_FRAMEBUFFER, (GLuint)st.fbo); GL(glViewport)(0, 0, R.W, R.H); GL(glDisable)(GL_DEPTH_TEST); GL(glDisable)(GL_BLEND);
        GL(glEnableVertexAttribArray)(0);
        GL(glUseProgram)(R.blitProg); GL(glUniform1i)(R.bTex, 0); GL(glActiveTexture)(GL_TEXTURE0); GL(glBindTexture)(GL_TEXTURE_2D, R.fboTex);
        GL(glBindBuffer)(GL_ARRAY_BUFFER, R.blitVbo); GL(glVertexAttribPointer)(0, 2, GL_FLOAT, GL_FALSE, 8, (void *)0); GL(glDrawArrays)(GL_TRIANGLES, 0, 3); R.drawCalls++; }
    if (qi >= 0) { R.endQ(GL_TIME_ELAPSED_EXT); R.qBusy[qi] = 1; R.qi = (qi + 1) & 3; }
    GL(glBindBuffer)(GL_ARRAY_BUFFER, 0); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, 0);
    for (int i = 0; i < 6; i++) GL(glDisableVertexAttribArray)(i);
    state_restore(&st); return 1;
}

/* ---- the timings line: every PERF_MS while a song plays (log-3d.txt on the device) ---- */
static void perf_frame(Uint64 c0) {
    Uint64 c1 = SDL_GetPerformanceCounter(); double f = (double)SDL_GetPerformanceFrequency(), cpu = (c1 - c0) * 1000.0 / f; perfwin *w = &R.pw;
    if (A.screen != SC_PLAY) { memset(w, 0, sizeof *w); return; }   /* menus, pause, results: not counted; a new window starts with the song */
    if (!w->t0) { w->t0 = w->last = c0; return; }
    double fr = (c0 - w->last) * 1000.0 / f; w->last = c0;
    w->frames++; w->sumFrame += fr; if (fr > w->maxFrame) w->maxFrame = fr; w->sumCpu += cpu; if (cpu > w->maxCpu) w->maxCpu = cpu; w->calls += R.drawCalls; w->tris += R.drawnTris;
    static double every; if (!every) { const char *e = getenv("R3D_PERF_MS"); every = e && atoi(e) > 0 ? atoi(e) : PERF_MS; }
    double span = (c0 - w->t0) * 1000.0 / f; if (span < every || !w->frames) return;
    int n = w->frames, res = res_mode(); char gpu[32]; if (w->nGpu) snprintf(gpu, sizeof gpu, "%.1f ms", w->sumGpu / w->nGpu); else snprintf(gpu, sizeof gpu, R.timer ? "(no result)" : "n/a");
    SDL_Log("r3d perf: %s %dx%d (%s) | %.1f fps, frame %.1f ms avg %.1f max | venue cpu %.2f ms avg %.2f max, gpu %s | %ld draws %ld tris per frame | tex %.1f MB (%d ETC1, %d 16-bit, %d DXT) vram %.1f MB",
        R.zone, res && R.fbo && !R.fboBad ? R.fw : R.W, res && R.fbo && !R.fboBad ? R.fh : R.H, RES_NAME[res], n * 1000.0 / span, w->sumFrame / n, w->maxFrame, w->sumCpu / n, w->maxCpu, gpu, w->calls / n, w->tris / n,
        R.texBytes / 1048576.0, R.nEtc, R.n16, R.nDxt, R.vram / 1048576.0);
    Uint64 last = w->last; memset(w, 0, sizeof *w); w->t0 = w->last = last;
}
int r3d_draw_stage(double perf, int sp, float energy) {
    if (!R.ready || !R.loaded || !SET.stage3d) return 0;
    Uint64 c0 = SDL_GetPerformanceCounter(); int r = draw(perf, sp, energy); perf_frame(c0); return r;
}
void r3d_shutdown(void) {
    if (!R.ready) return; r3d_band_shutdown(); unload(); r3d_crowd_shutdown(); r3d_lights_shutdown(); for (int p = 0; p < 2; p++) GL(glDeleteProgram)(R.P[p].prog); GL(glDeleteTextures)(1, &R.white);
    if (R.blitProg) { GL(glDeleteProgram)(R.blitProg); GL(glDeleteBuffers)(1, &R.blitVbo); }
    if (R.fbo) { GL(glDeleteFramebuffers)(1, &R.fbo); GL(glDeleteTextures)(1, &R.fboTex); GL(glDeleteRenderbuffers)(1, &R.fboDepth); }
    R.ready = 0;
}

/* ------------------------------------------------------------------ --r3dtest */
static void save_frame(const char *outdir, const char *name, int W, int H, uint8_t *px) {
    GL(glPixelStorei)(GL_PACK_ALIGNMENT, 1); GL(glReadPixels)(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);
    for (int y = 0; y < H / 2; y++) for (int x = 0; x < W * 4; x++) { uint8_t t = px[(size_t)y * W * 4 + x]; px[(size_t)y * W * 4 + x] = px[(size_t)(H - 1 - y) * W * 4 + x]; px[(size_t)(H - 1 - y) * W * 4 + x] = t; }
    SDL_Surface *sf = SDL_CreateRGBSurfaceFrom(px, W, H, 32, W * 4, 0x000000FF, 0x0000FF00, 0x00FF0000, 0); char p[700]; snprintf(p, sizeof p, "%s/%s.bmp", outdir && *outdir ? outdir : ".", name); SDL_SaveBMP(sf, p); SDL_FreeSurface(sf);
}
int r3d_test_band; char r3d_test_song[64];
static void band_test_select(void) {   /* --band [--song NAME]: the band that song would have (a boss song: the boss), its cues on a clock of its own */
    if (A.lib) for (int i = 0; i < A.lib->nsongs; i++) if (!strcmp(A.lib->songs[i].name, r3d_test_song)) A.selSong = i;
    if (!r3d_test_song[0]) A.selSong = -1; r3d_band_test_clock(0, r3d_test_song); Uint32 t0 = SDL_GetTicks(); r3d_band_select();
    const r3d_bandstats *b = r3d_band_stats(); printf("band: %s / %s / %s / %s: %d points, %.1f MB on the GPU, %d animations (%.1f MB), %d bones a draw, loaded in %u ms\n",
        b->who[0], b->who[1], b->who[2], b->who[3], b->verts, b->vram / 1048576.0, b->clips, b->animBytes / 1048576.0, b->pal, SDL_GetTicks() - t0);
}
static void look_at(float *m, const float e[3], const float at[3]) {
    float f[3] = { at[0] - e[0], at[1] - e[1], at[2] - e[2] }, l = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]); for (int i = 0; i < 3; i++) f[i] /= l;
    float s[3] = { -f[2], 0, f[0] }; l = sqrtf(s[0] * s[0] + s[2] * s[2]); s[0] /= l; s[2] /= l; float u[3] = { s[1] * f[2] - s[2] * f[1], s[2] * f[0] - s[0] * f[2], s[0] * f[1] - s[1] * f[0] };
    m[0] = s[0]; m[4] = s[1]; m[8] = s[2]; m[12] = -(s[0] * e[0] + s[1] * e[1] + s[2] * e[2]); m[1] = u[0]; m[5] = u[1]; m[9] = u[2]; m[13] = -(u[0] * e[0] + u[1] * e[1] + u[2] * e[2]);
    m[2] = -f[0]; m[6] = -f[1]; m[10] = -f[2]; m[14] = f[0] * e[0] + f[1] * e[1] + f[2] * e[2]; m[3] = m[7] = m[11] = 0; m[15] = 1;
}
static void band_shots(SDL_Renderer *ren, const char *zone, const char *outdir, int W, int H, uint8_t *px, double ms, const char *tag);
static int band_front(float dist, float fov) {   /* camOver on the whole band from the front, dist away; 0 when nobody is on stage */
    float c[3] = { 0, 0, 0 }, fwd[3] = { 0, 0, 0 }; int n = 0;
    for (int r = 0; r < BAND_ROLES; r++) { float p[3], yaw; if (!r3d_band_where(r, p, &yaw)) continue; for (int i = 0; i < 3; i++) c[i] += p[i]; fwd[0] += sinf(yaw); fwd[2] += cosf(yaw); n++; }
    if (!n) return 0; for (int i = 0; i < 3; i++) c[i] /= n; float L = sqrtf(fwd[0] * fwd[0] + fwd[2] * fwd[2]); if (L < 1e-3f) { fwd[2] = 1; L = 1; }
    float e[3] = { c[0] + fwd[0] / L * dist, c[1] + 2.0f, c[2] + fwd[2] / L * dist }, at[3] = { c[0], c[1] + 1.0f, c[2] }; look_at(camOver.view, e, at); camOver.fov = fov; camOver.on = 1; camOver.seq++; return 1;
}
static void band_test_frames(SDL_Renderer *ren, const char *zone, const char *outdir, int W, int H, uint8_t *px) {
    /* the song's clock from 0, a frame every 1/60 s; at two moments, a picture of each member from the front and one of the whole band */
    static const double AT[3] = { 6000, 21000, 47000 }; double ms = 0, poseMs = 0, drawMs = 0; int nf = 0, maxDraws = 0, maxTris = 0; char tag[16];
    for (int k = 0; k < 3; k++) {
        for (; ms < AT[k]; ms += 1000.0 / 60) { r3d_band_test_clock(ms, r3d_test_song); camOver.on = 0; SDL_RenderClear(ren); draw(ms, 0, 0.5f); R.flush(ren); GL(glFinish)(); const r3d_bandstats *b = r3d_band_stats(); poseMs += b->poseMs; drawMs += b->drawMs; nf++; if (b->draws > maxDraws) { maxDraws = b->draws; maxTris = b->tris; } }
        snprintf(tag, sizeof tag, "%d", k); band_shots(ren, zone, outdir, W, H, px, ms, tag); }
    printf("band over %d frames: %.3f ms/frame animating + skinning matrices, %.3f ms/frame submitting (host CPU); up to %d draws, %d triangles a frame\n", nf, poseMs / nf, drawMs / nf, maxDraws, maxTris);
    /* a battle (the boss where the bassist stands): an attack each way, as battle.c sends them; then the song's end, won and lost */
    float p[3];
    if (A.lib && A.selSong >= 0 && battle_song(A.selSong) && r3d_band_where(BAND_BASSIST, p, NULL)) for (int side = 0; side < 2; side++) {
        r3d_band_attack(side, gh3_qbkey("lightning")); double until = ms + 900; for (; ms < until; ms += 1000.0 / 60) { r3d_band_test_clock(ms, r3d_test_song); camOver.on = 0; SDL_RenderClear(ren); draw(ms, 0, 0.5f); R.flush(ren); }
        band_shots(ren, zone, outdir, W, H, px, ms, side ? "attack_p2" : "attack_p1"); }
    for (int how = 1; how <= 2; how++) { r3d_band_test_finale(how); double until = ms + 2500;
        for (; ms < until; ms += 1000.0 / 60) { r3d_band_test_clock(ms, r3d_test_song); camOver.on = 0; SDL_RenderClear(ren); draw(ms, 0, 0.5f); R.flush(ren); }
        band_shots(ren, zone, outdir, W, H, px, ms, how == 1 ? "win" : "lose"); r3d_band_test_finale(0); }
}
static void band_shots(SDL_Renderer *ren, const char *zone, const char *outdir, int W, int H, uint8_t *px, double ms, const char *tag) {
    static const char *RN[BAND_ROLES] = { "guitarist", "bassist", "drummer", "vocalist" };
    {   float c[3] = { 0, 0, 0 }, fwd[3] = { 0, 0, 0 }; int n = 0;
        for (int r = 0; r <= BAND_ROLES; r++) { float p[3], yaw; char path[96];
            if (r < BAND_ROLES) { if (!r3d_band_where(r, p, &yaw)) continue; c[0] += p[0]; c[1] += p[1]; c[2] += p[2]; fwd[0] += sinf(yaw); fwd[2] += cosf(yaw); n++;
                float d = r == BAND_DRUMMER ? 3.6f : 2.6f, e[3] = { p[0] + sinf(yaw) * d, p[1] + 1.35f, p[2] + cosf(yaw) * d }, at[3] = { p[0], p[1] + 1.0f, p[2] }; look_at(camOver.view, e, at); camOver.fov = 50;
                snprintf(path, sizeof path, "%s_band_%s_%s", zone, RN[r], tag); }
            else { if (!n) break; for (int i = 0; i < 3; i++) c[i] /= n; float L = sqrtf(fwd[0] * fwd[0] + fwd[2] * fwd[2]); if (L < 1e-3f) { fwd[2] = 1; L = 1; } float e[3] = { c[0] + fwd[0] / L * 7.5f, c[1] + 2.4f, c[2] + fwd[2] / L * 7.5f }, at[3] = { c[0], c[1] + 1.0f, c[2] };
                look_at(camOver.view, e, at); camOver.fov = 72; snprintf(path, sizeof path, "%s_band_all_%s", zone, tag); }
            camOver.on = 1; camOver.seq++; SDL_SetRenderDrawColor(ren, 18, 16, 24, 255); SDL_RenderClear(ren); draw(ms, 0, 0.5f); R.flush(ren); GL(glFinish)();
            save_frame(outdir, path, W, H, px);
            const r3d_bandstats *b = r3d_band_stats(); printf("  band %s at %.1f s: %d draws, %d triangles -> %s/%s.bmp\n", r < BAND_ROLES ? RN[r] : "all", ms / 1000, b->draws, b->tris, outdir && *outdir ? outdir : ".", path); } }
    camOver.on = 0;
}
int r3d_test(const char *zone, const char *outdir, int W, int H) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);   /* as main() asks for the game's window */
    SDL_Window *win = SDL_CreateWindow("r3dtest", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN); if (!win) { fprintf(stderr, "window: %s\n", SDL_GetError()); return 1; }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED); if (!ren) { fprintf(stderr, "renderer: %s\n", SDL_GetError()); return 1; }
    SDL_RendererInfo ri; SDL_GetRendererInfo(ren, &ri); printf("renderer: %s\n", ri.name);
    if (r3d_init(ren)) { puts("r3d: no GL (see the log above)"); return 1; }
    if (load_zone(zone)) { printf("could not load %s\n", zone); return 1; }
    if (r3d_test_band) band_test_select();
    r3d_crowd_test(zone, outdir);   /* the crowd's figures in each mood, and their cost */
    printf("%s: %d meshes in %d batches, %d skipped, %d triangles, %d textures (%d ETC1, %d 16-bit, %d DXT; %.1f MB of textures, %.0f ms encoding ETC1, up to %d px), %.1f MB on the GPU, %d camera shots, loaded in %.0f ms\n",
        zone, R.meshes, R.nb, R.skipped, R.tris, R.textures, R.nEtc, R.n16, R.nDxt, R.texBytes / 1048576.0, R.texMs, R.texMax, R.vram / 1048576.0, R.nshots, R.loadMs);
    uint8_t *px = malloc((size_t)W * H * 4); int nsave = R.nshots < 4 ? (R.nshots ? R.nshots : 1) : 4, keep = SET.stage3dRes; char nm[96];
    /* one picture per shot (the first few) at Média, the game's default, read back from the window; the first shot also at Alta and Baixa */
    SET.stage3dRes = 1;
    for (int s = 0; s < nsave; s++) { double perf = s * SHOT_MS + SHOT_MS * 0.35;
        SDL_SetRenderDrawColor(ren, 18, 16, 24, 255); SDL_RenderClear(ren); draw(perf, 0, 0.5f); R.flush(ren); GL(glFinish)();
        snprintf(nm, sizeof nm, "%s_%d", zone, s); save_frame(outdir, nm, W, H, px);
        printf("  shot %d: %d draw calls, %d triangles, %d parts left out -> %s/%s.bmp\n", s, R.drawCalls, R.drawnTris, R.culled, outdir && *outdir ? outdir : ".", nm); SDL_RenderPresent(ren); }
    for (int m = 0; m < 3; m += 2) { SET.stage3dRes = m; SDL_RenderClear(ren); draw(SHOT_MS * 0.35, 0, 0.5f); R.flush(ren); GL(glFinish)(); snprintf(nm, sizeof nm, "%s_0_%s", zone, m ? "baixa" : "alta"); save_frame(outdir, nm, W, H, px); SDL_RenderPresent(ren); }
    /* the band and the light show at the size asked (R3D_RES=0/1/2, else palco_3d_resolucao of the settings) */
    SET.stage3dRes = keep;
    if (r3d_test_band) band_test_frames(ren, zone, outdir, W, H, px);
    /* the light show over a song (--song NAME, else the first song of the set played here): frames at the first cue of each kind, three shots each, the show stepped from the song's start */
    static gh3_songdata sd; uint8_t *spak = NULL; double showAt = -1; const gh3_library *lib = A.lib; const char *song = r3d_test_song;
    if (!*song && lib) for (int t = 0; t < lib->ntiers && !*song; t++) if (lib->shop.tierZone && !strcmp(lib->shop.tierZone[t], zone) && lib->tiers[t].nsongs) song = lib->songs[lib->tiers[t].songIdx[0]].name;
    if (*song && lib && lib->hasLightNotes) { char sn[96]; size_t len; snprintf(sn, sizeof sn, "%s_song.pak.xen", song); spak = app_read_data("songs", sn, &len);
        if (!spak || gh3_songpak_read(spak, len, song, &sd)) printf("light show: no song %s\n", song);
        else { const char *what[10] = { "intro", "verse", "chorus", "solo", "strobe", "colour", "pyro", "chorus_poor", "flash", "off" }; double at[10]; uint32_t mk[5] = { gh3_qbkey("intro"), gh3_qbkey("verse"), gh3_qbkey("chorus"), gh3_qbkey("solo"), gh3_qbkey("strobe") };
            for (int i = 0; i < 9; i++) at[i] = -1;
            for (int i = 0; i < sd.nlights; i++) { const gh3_lightnote *w = &lib->lightNote[sd.lights[i][1] & 127]; double t = sd.lights[i][0];
                if (w->kind == LN_MOOD) for (int k = 0; k < 5; k++) if (w->mood == mk[k] && at[k] < 0) at[k] = t + 400;
                if (w->kind == LN_COLOUR && w->rgb[0] >= 0 && at[5] < 0) at[5] = t + 600; if (w->kind == LN_PYRO && at[6] < 0) at[6] = t + 450; }
            at[7] = at[2]; at[8] = at[4] > 0 ? at[4] - 390 : -1; at[9] = 0; showAt = at[2] > 0 ? at[2] : at[1];
            if (getenv("R3D_LDEBUG")) for (int i = 0; i < sd.nlights; i++) if (fabs(sd.lights[i][0] - at[4]) < 3000) printf("    cue %6d note %3d kind %d\n", sd.lights[i][0], sd.lights[i][1], lib->lightNote[sd.lights[i][1] & 127].kind);
            printf("light show: %s, %d cues\n", song, sd.nlights);
            SET.stage3d = 1;
            for (int i = 0; i < 10; i++) { if (at[i] < 0) { printf("  %-11s: no cue\n", what[i]); continue; }
                for (int c = 0; c < 4; c++) { r3d_lights_sim(NULL, 0, 0); SET.stageFx = i < 9; if (i < 9) r3d_lights_sim(&sd, at[i], i == 7 ? 0.3 : 1.6);   /* "off": Efeitos de palco off, the baked look */
                    if (c == 3 && !(r3d_test_band && band_front(5.5f, 60))) break;   /* and the band from the front (--band), lit by the show */
                    SDL_RenderClear(ren); draw((c * c + c) * SHOT_MS + SHOT_MS * 0.35, 0, 0.5f); R.flush(ren); GL(glFinish)(); camOver.on = 0;
                    snprintf(nm, sizeof nm, c == 3 ? "%s_show_%s_band" : "%s_show_%s_%d", zone, what[i], c); save_frame(outdir, nm, W, H, px);
                    if (!c) printf("  %-11s at %6.0f ms: %s -> %s/%s.bmp\n", what[i], at[i], r3d_lights_info(), outdir && *outdir ? outdir : ".", nm); } } } }
    /* timing at each size, with the band (--band) and the light show (frozen at the first chorus) on as in a song: frames across all the shots, the GPU made to finish each, and the CPU time of the
     * venue's calls alone; then the show off at Média, to see what it costs. R3D_RES is set aside here (it would pin every size to one) */
    char envRes[8] = ""; { const char *e = getenv("R3D_RES"); if (e) snprintf(envRes, sizeof envRes, "%s", e); unsetenv("R3D_RES"); }
    for (int m = 0; m < 4; m++) { int show = m < 3 && showAt >= 0; SET.stage3dRes = m < 3 ? m : 1; r3d_lights_sim(NULL, 0, 0); SET.stageFx = show; if (show) r3d_lights_sim(&sd, showAt, 1.6);
        int N = 120; double cpu = 0; long calls = 0, tris = 0; draw(0, 0, 0.5f); GL(glFinish)(); Uint64 f0 = SDL_GetPerformanceCounter();
        for (int f = 0; f < N; f++) { SDL_RenderClear(ren); Uint64 c0 = SDL_GetPerformanceCounter(); draw(f * 1000.0 / 60 * 7, 0, 0.5f); cpu += (SDL_GetPerformanceCounter() - c0) * 1000.0 / SDL_GetPerformanceFrequency(); R.flush(ren); GL(glFinish)(); calls += R.drawCalls; tris += R.drawnTris; }
        double ms = (SDL_GetPerformanceCounter() - f0) * 1000.0 / SDL_GetPerformanceFrequency() / N; int r = SET.stage3dRes;
        printf("%-6s %dx%d, band %s, light show %s: %d frames, %.2f ms/frame on this GPU (%s), venue calls %.2f ms CPU, %.0f draw calls, %.0f triangles per frame\n", RES_NAME[r], r && R.fbo ? R.fw : W, r && R.fbo ? R.fh : H,
            r3d_test_band && r3d_band_where(-1, NULL, NULL) ? "on" : "off", show ? "on" : "off", N, ms, GL(glGetString)(GL_RENDERER), cpu / N, (double)calls / N, (double)tris / N); }
    if (envRes[0]) setenv("R3D_RES", envRes, 1);
    /* the log line as a song gets it (window shortened to 1 s), at the size asked, the show on */
    r3d_lights_sim(NULL, 0, 0); SET.stageFx = 1; if (showAt >= 0) r3d_lights_sim(&sd, showAt, 1.6);
    { SET.stage3dRes = keep; int scr = A.screen; A.screen = SC_PLAY; setenv("R3D_PERF_MS", "1000", 0); Uint64 t0 = SDL_GetPerformanceCounter(); int f = 0;
        while ((SDL_GetPerformanceCounter() - t0) * 1000.0 / SDL_GetPerformanceFrequency() < 2100) { SDL_RenderClear(ren); r3d_draw_stage(f++ * 1000.0 / 60 * 7, 0, 0.5f); SDL_RenderPresent(ren); }
        A.screen = scr; }
    SET.stage3dRes = keep; r3d_lights_sim(NULL, 0, 0); gh3_songdata_free(&sd); free(spak);
    free(px); r3d_shutdown(); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit(); return 0;
}
