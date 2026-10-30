/* The venue drawn in 3D behind the highway. A zone's meshes are read once (r3d_zone.c), lit on the CPU from the venue's
 * own lights (baked into the vertex colours), merged per material into one vertex buffer each (world space, objects
 * already placed), and the textures uploaded as DXT where the GPU takes it or decoded to 16/32-bit when it does not
 * (the Mali-G31). Every frame: SDL_RenderFlush, the GL state SDL relies on saved, the venue drawn through one shader
 * (lit / unlit / ambient-only are the same formula with different numbers), the state put back. The camera walks the
 * venue's own shots (cameras, _pan, _dolly…) one after another, as the game does between songs. */
#include "r3d.h"
#include "r3d_gl.h"
#include "r3d_zone.h"
#include "r3d_band.h"
#include "app.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dlfcn.h>

#define R3D_GL_DEF(ret, name, args) ret (*r3d_##name) args;
R3D_GL_FUNCS(R3D_GL_DEF)
#undef R3D_GL_DEF
int r3d_gl_load(void) {
    int missing = 0;
#define R3D_GL_GET(ret, name, args) r3d_##name = (ret (*) args)SDL_GL_GetProcAddress(#name); if (!r3d_##name) { missing++; SDL_Log("r3d: no %s", #name); }
    R3D_GL_FUNCS(R3D_GL_GET)
#undef R3D_GL_GET
    return missing ? -1 : 0;
}
#define GL(name) r3d_##name

#define TEX_MAX 512          /* textures above this take a smaller mip level: VRAM */
#define VRAM_MAX (64u << 20)
#define SHOT_MS 8000.0       /* with no cue list, the next shot every eight seconds */
#define Z_NEAR 0.25f
#define Z_FAR 1500.0f

/* ---- one vertex: 48 bytes ---- */
typedef struct { float pos[3], ctr[3]; uint8_t col[4], lit[4]; float uv[2], uv2[2]; } vtx;
typedef struct { uint32_t key; int clamp; GLuint id; } texref;
typedef struct {
    uint32_t mat; int smapped; GLuint vbo, ibo, tex, smap; int nidx, nvert, blend /* 0 solid, 1 added, 2 see-through */, board, colA;
    float colour[4], alphaRef, tile, anim[4], wibU[4], wibV[4], wibR[3]; int wib, hasAnim; float amb[3], centre[3], radius;
    vtx *v; uint16_t *i; int capV, capI; double ambSum[3]; float bmin[3], bmax[3];   /* build-time only */
} batch;
typedef struct { float pos[3], q[4], fov; } camend;
typedef struct { uint32_t list, type, name; camend a, b; int hasB; float orbit; int who; } shot;   /* who: the band member the shot follows (BAND_*), -1 fixed in the room */
typedef struct {
    int ready, loaded; SDL_Renderer *ren; char zone[32]; int W, H;
    GLuint prog, white; GLint aPos, aCtr, aCol, aLit, aUv, aUv2, uProj, uView, uBoard, uAnim, uWibU, uWibV, uWibR, uWib, uTime, uMap, uSmap, uColour, uAmb, uTint, uAlphaRef, uColA, uTile;
    int s3tc, npot; int (*flush)(SDL_Renderer *);
    batch *b; int nb; texref *tex; int ntex; shot *shots; int nshots; int *order, ngen; uint32_t kCameras; float ctr[3], span;   /* order: the general shots, shuffled */
    /* counts, for --r3dtest and the log */
    int meshes, skipped, tris, textures; size_t vram; int drawCalls, drawnTris; double loadMs;
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
/* a sphere against the six planes of a projection*view matrix */
static int sphere_visible(const float *vp, const float c[3], float rad) {
    for (int i = 0; i < 6; i++) { int row = i >> 1; float s = (i & 1) ? -1.0f : 1.0f, p[4]; for (int k = 0; k < 4; k++) p[k] = vp[k * 4 + 3] + s * vp[k * 4 + row];
        float L = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]); if (L < 1e-6f) continue; if ((p[0] * c[0] + p[1] * c[1] + p[2] * c[2] + p[3]) / L < -rad) return 0; }
    return 1;
}

/* ------------------------------------------------------------------ shaders */
static const char *VS =
    "uniform mat4 uProj; uniform mat4 uView; uniform float uBoard; uniform vec4 uAnim; uniform vec4 uWibU; uniform vec4 uWibV; uniform vec3 uWibR; uniform float uWib; uniform float uTime;\n"
    "attribute vec3 aPos; attribute vec3 aCtr; attribute vec4 aCol; attribute vec4 aLit; attribute vec2 aUv; attribute vec2 aUv2;\n"
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
    "  vUv = uv; vUv2 = aUv2; vCol = aCol; vLit = aLit.rgb * 4.0;\n"
    "}\n";
static const char *FS =
    "#ifdef GL_ES\nprecision highp float;\n#endif\n"
    "uniform sampler2D uMap; uniform sampler2D uSmap; uniform vec4 uColour; uniform vec3 uAmb; uniform vec3 uTint; uniform float uAlphaRef; uniform float uColA; uniform float uTile;\n"
    "varying vec2 vUv; varying vec2 vUv2; varying vec4 vCol; varying vec3 vLit;\n"
    "void main() {\n"
    "  vec4 t = texture2D(uMap, vUv * uTile) * uColour;\n"
    "  vec3 L = vLit * texture2D(uSmap, vUv2).rgb + uAmb;\n"   /* the game's sum: the lights dimmed by the baked map, plus ambient (unlit parts: lit 1, amb 0, map white) */
    "  float a = t.a * mix(1.0, vCol.a, uColA);\n"
    "  if (a < uAlphaRef) discard;\n"
    "  gl_FragColor = vec4(t.rgb * L * vCol.rgb * uTint, a);\n"
    "}\n";
static GLuint shader(GLenum kind, const char *src) {
    GLuint s = GL(glCreateShader)(kind); GL(glShaderSource)(s, 1, &src, NULL); GL(glCompileShader)(s); GLint ok = 0; GL(glGetShaderiv)(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[1024]; GL(glGetShaderInfoLog)(s, sizeof log, NULL, log); SDL_Log("r3d: shader: %s", log); GL(glDeleteShader)(s); return 0; }
    return s;
}
static int build_program(void) {
    GLuint vs = shader(GL_VERTEX_SHADER, VS), fs = shader(GL_FRAGMENT_SHADER, FS); if (!vs || !fs) return -1;
    R.prog = GL(glCreateProgram)(); GL(glAttachShader)(R.prog, vs); GL(glAttachShader)(R.prog, fs); GL(glLinkProgram)(R.prog); GL(glDeleteShader)(vs); GL(glDeleteShader)(fs);
    GLint ok = 0; GL(glGetProgramiv)(R.prog, GL_LINK_STATUS, &ok); if (!ok) { char log[1024]; GL(glGetProgramInfoLog)(R.prog, sizeof log, NULL, log); SDL_Log("r3d: link: %s", log); return -1; }
    R.aPos = GL(glGetAttribLocation)(R.prog, "aPos"); R.aCtr = GL(glGetAttribLocation)(R.prog, "aCtr"); R.aCol = GL(glGetAttribLocation)(R.prog, "aCol"); R.aLit = GL(glGetAttribLocation)(R.prog, "aLit"); R.aUv = GL(glGetAttribLocation)(R.prog, "aUv"); R.aUv2 = GL(glGetAttribLocation)(R.prog, "aUv2");
#define U(n) R.n = GL(glGetUniformLocation)(R.prog, #n)
    U(uProj); U(uView); U(uBoard); U(uAnim); U(uWibU); U(uWibV); U(uWibR); U(uWib); U(uTime); U(uMap); U(uSmap); U(uColour); U(uAmb); U(uTint); U(uAlphaRef); U(uColA); U(uTile);
#undef U
    return 0;
}

/* ------------------------------------------------------------------ init */
int r3d_ready(void) { return R.ready; }
int r3d_init(SDL_Renderer *ren) {
    memset(&R, 0, sizeof R); R.ren = ren; if (!ren) return -1;
    SDL_RendererInfo ri; SDL_GetRendererInfo(ren, &ri); if (!strstr(ri.name, "opengl")) { SDL_Log("r3d: renderer %s is not GL: 2D stage", ri.name); return -1; }
    if (!SDL_GL_GetCurrentContext()) { SDL_Log("r3d: no current GL context: 2D stage"); return -1; }
    R.flush = (int (*)(SDL_Renderer *))dlsym(RTLD_DEFAULT, "SDL_RenderFlush"); if (!R.flush) { SDL_Log("r3d: this SDL has no SDL_RenderFlush: 2D stage"); return -1; }   /* 2.0.10+; the sysroot headers (2.0.8) do not declare it */
    if (r3d_gl_load()) return -1;
    if (build_program()) return -1;
    if (r3d_band_init()) SDL_Log("r3d: no band (skinning shader)");   /* the venue still shows without it */
    const char *ext = (const char *)GL(glGetString)(GL_EXTENSIONS); if (!ext) ext = "";
    R.s3tc = strstr(ext, "GL_EXT_texture_compression_s3tc") != NULL || (strstr(ext, "GL_EXT_texture_compression_dxt1") && strstr(ext, "GL_ANGLE_texture_compression_dxt"));
    if (getenv("R3D_NODXT")) R.s3tc = 0;   /* R3D_NODXT=1: the textures decoded on the CPU as on the Mali-G31, to check that path on a desktop GPU */
    const char *ver = (const char *)GL(glGetString)(GL_VERSION); R.npot = strstr(ext, "GL_OES_texture_npot") != NULL || (ver && !strstr(ver, "ES"));   /* desktop GL: always */
    GL(glGenTextures)(1, &R.white); GL(glBindTexture)(GL_TEXTURE_2D, R.white); uint8_t px[4] = { 255, 255, 255, 255 }; GL(glTexImage2D)(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); GL(glBindTexture)(GL_TEXTURE_2D, 0);
    SDL_GetRendererOutputSize(ren, &R.W, &R.H);
    SDL_Log("r3d: %s / %s, %s, s3tc %d npot %d", GL(glGetString)(GL_RENDERER), ver ? ver : "?", ri.name, R.s3tc, R.npot);
    R.ready = 1; return 0;
}

/* ------------------------------------------------------------------ textures */
static int pot(int x) { return x > 0 && !(x & (x - 1)); }
static GLuint tex_get(const uint8_t *bank, size_t bankLen, const r3d_texent *ents, int nents, uint32_t key, int clamp) {
    for (int i = 0; i < R.ntex; i++) if (R.tex[i].key == key && R.tex[i].clamp == clamp) return R.tex[i].id;
    const r3d_texent *e = NULL; for (int i = 0; i < nents; i++) if (ents[i].key == key) { e = &ents[i]; break; }
    GLuint id = 0; r3d_dds d;
    if (e && r3d_dds_info(bank + e->off, e->size, &d) == 0) {
        const uint8_t *b = bank + e->off; size_t n = e->size; int l0 = 0; while (l0 + 1 < d.nlev && (d.lev[l0].w > TEX_MAX || d.lev[l0].h > TEX_MAX)) l0++;
        int gpu = R.s3tc && d.cc != DDS_RGBA32;   /* over the budget, a smaller level still (decoded pictures take 16 bits a texel, plus a third for the mips) */
        while (l0 + 1 < d.nlev && R.vram + (gpu ? (size_t)d.lev[l0].size : (size_t)d.lev[l0].w * d.lev[l0].h * 2 * 4 / 3) > VRAM_MAX) l0++;
        int w = d.lev[l0].w, h = d.lev[l0].h, full = d.lev[d.nlev - 1].w == 1 && d.lev[d.nlev - 1].h == 1, isPot = pot(w) && pot(h);
        GL(glGenTextures)(1, &id); GL(glBindTexture)(GL_TEXTURE_2D, id); int mips = 0; size_t bytes = 0;
        if (gpu) {
            GLenum fmt = d.cc == DDS_DXT1 ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT : d.cc == DDS_DXT3 ? GL_COMPRESSED_RGBA_S3TC_DXT3_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            int nl = full ? d.nlev - l0 : 1; for (int l = 0; l < nl; l++) { GL(glCompressedTexImage2D)(GL_TEXTURE_2D, l, fmt, d.lev[l0 + l].w, d.lev[l0 + l].h, 0, (GLsizei)d.lev[l0 + l].size, b + d.lev[l0 + l].off); bytes += d.lev[l0 + l].size; }
            mips = nl > 1;
        } else {   /* decoded on the CPU, 16 bits a texel: 565 when the picture is opaque, 4444 when it has holes or soft edges */
            uint8_t *rgba = malloc((size_t)w * h * 4); int trans = r3d_dds_decode(b, n, &d, l0, rgba); uint16_t *p16 = malloc((size_t)w * h * 2);
            GL(glPixelStorei)(GL_UNPACK_ALIGNMENT, 1);
            if (!trans) { for (int i = 0; i < w * h; i++) p16[i] = (uint16_t)(((rgba[i * 4] >> 3) << 11) | ((rgba[i * 4 + 1] >> 2) << 5) | (rgba[i * 4 + 2] >> 3)); GL(glTexImage2D)(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, p16); }
            else { for (int i = 0; i < w * h; i++) p16[i] = (uint16_t)(((rgba[i * 4] >> 4) << 12) | ((rgba[i * 4 + 1] >> 4) << 8) | ((rgba[i * 4 + 2] >> 4) << 4) | (rgba[i * 4 + 3] >> 4)); GL(glTexImage2D)(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, p16); }
            bytes = (size_t)w * h * 2; GL(glPixelStorei)(GL_UNPACK_ALIGNMENT, 4); free(rgba); free(p16);
            if (isPot) { GL(glGenerateMipmap)(GL_TEXTURE_2D); mips = 1; bytes += bytes / 3; }
        }
        GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mips ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR); GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        int rep = isPot || R.npot; GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (clamp & 1) || !rep ? GL_CLAMP_TO_EDGE : GL_REPEAT); GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (clamp & 2) || !rep ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        GL(glBindTexture)(GL_TEXTURE_2D, 0); R.vram += bytes; R.textures++;
    }
    R.tex = realloc(R.tex, (R.ntex + 1) * sizeof *R.tex); R.tex[R.ntex].key = key; R.tex[R.ntex].clamp = clamp; R.tex[R.ntex].id = id; R.ntex++;
    return id;
}

/* ------------------------------------------------------------------ lights, baked per vertex at load */
typedef struct { r3d_light *l; int n; } lightset;
/* the game's sum, as the browser port does it per part: the strongest light falling on the point lights it by n.l, the
 * others softly from their side (half-Lambert), each faded between its attenstart and attenend; ambient apart */
static void light_at(const lightset *L, const float p[3], const float n[3], float lit[3], float amb[3]) {
    lit[0] = lit[1] = lit[2] = amb[0] = amb[1] = amb[2] = 0; int hasN = n[0] * n[0] + n[1] * n[1] + n[2] * n[2] > 1e-8f; float best = -1; int bi = -1;
    float k[64], d[64][3];
    for (int i = 0; i < L->n && i < 64; i++) { const r3d_light *l = &L->l[i]; float dx = l->pos[0] - p[0], dy = l->pos[1] - p[1], dz = l->pos[2] - p[2], dd = sqrtf(dx * dx + dy * dy + dz * dz);
        float far = l->far > 0.01f ? l->far : 0.01f, near = l->near < far * 0.999f ? (l->near > 0 ? l->near : 0) : far * 0.999f, att = (far - dd) / (far - near); if (att > 1) att = 1; if (att <= 0) { k[i] = 0; continue; }
        k[i] = l->intensity * att;
        if (l->kind == L_AMBIENT) { for (int c = 0; c < 3; c++) amb[c] += l->colour[c] * k[i]; k[i] = 0; continue; }
        if (l->kind == L_DIR) { d[i][0] = -l->dir[0]; d[i][1] = -l->dir[1]; d[i][2] = -l->dir[2]; } else { float q = dd > 1e-4f ? dd : 1; d[i][0] = dx / q; d[i][1] = dy / q; d[i][2] = dz / q; }
        float w = fmaxf(l->colour[0], fmaxf(l->colour[1], l->colour[2])) * k[i]; if (w > best) { best = w; bi = i; } }
    for (int i = 0; i < L->n && i < 64; i++) { if (k[i] <= 0) continue; float ndl = hasN ? n[0] * d[i][0] + n[1] * d[i][1] + n[2] * d[i][2] : 0;
        float f = i == bi ? (hasN ? fmaxf(ndl, 0) : 0.5f) : (hasN ? 0.5f * (1 + ndl) : 0.5f) * 0.6f;
        for (int c = 0; c < 3; c++) lit[c] += L->l[i].colour[c] * k[i] * f; }
}
static void default_lights(lightset *L) {
    static r3d_light def[2]; def[0].kind = L_AMBIENT; def[0].colour[0] = 135 / 255.0f; def[0].colour[1] = 130 / 255.0f; def[0].colour[2] = 125 / 255.0f; def[0].intensity = 1; def[0].near = 1e5f; def[0].far = 1e6f;
    def[1].kind = L_DIR; def[1].colour[0] = 1; def[1].colour[1] = 173 / 255.0f; def[1].colour[2] = 97 / 255.0f; def[1].intensity = 0.6f; def[1].dir[0] = 0.5f; def[1].dir[1] = -0.7f; def[1].dir[2] = 0.5f; def[1].near = 1e5f; def[1].far = 1e6f;
    L->l = realloc(L->l, (L->n + 2) * sizeof *L->l); L->l[L->n++] = def[0]; L->l[L->n++] = def[1];
}

/* ------------------------------------------------------------------ building the batches */
static void unload(void) {
    for (int i = 0; i < R.nb; i++) { GL(glDeleteBuffers)(1, &R.b[i].vbo); GL(glDeleteBuffers)(1, &R.b[i].ibo); free(R.b[i].v); free(R.b[i].i); }
    for (int i = 0; i < R.ntex; i++) if (R.tex[i].id) GL(glDeleteTextures)(1, &R.tex[i].id);
    free(R.b); free(R.tex); free(R.shots); free(R.order); R.b = NULL; R.tex = NULL; R.shots = NULL; R.order = NULL; R.nb = R.ntex = R.nshots = R.ngen = 0; R.loaded = 0; R.zone[0] = 0;
    R.meshes = R.skipped = R.tris = R.textures = 0; R.vram = 0;
}
static uint8_t q8(float v, float scale) { float x = v * scale; return (uint8_t)(x < 0 ? 0 : x > 255 ? 255 : x + 0.5f); }
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
    r3d_texent *tents; int nt = r3d_texbank_read(te->data, te->len, &tents);
    r3d_venueqb vq; r3d_venueqb_read(ents, ne, &vq);
    r3d_band_zone(zone, ents, ne, vq.lights, vq.nlights);   /* where the band stands, its lights */
    lightset L = { NULL, 0 }; int roomLit = 0;   /* the lights that fall on the room: not the ones kept for the band or the crowd */
    for (int i = 0; i < vq.nlights && L.n < 32; i++) if (!vq.lights[i].band) { L.l = realloc(L.l, (L.n + 1) * sizeof *L.l); L.l[L.n++] = vq.lights[i]; if (vq.lights[i].kind != L_AMBIENT) roomLit = 1; }
    if (!roomLit) default_lights(&L);   /* a venue with no lights of its own: the game's default daylight */
    const gh3_zonecrowd *crowd = A.lib ? gh3_library_crowd(A.lib, zone) : NULL; int panels = 0;
    for (int mi = 0; mi < sc.nmeshes; mi++) {
        const r3d_mesh *me = &sc.meshes[mi]; if (me->nvert > 65535) { R.skipped++; continue; }
        { const char *sk = getenv("R3D_SKIP"); if (sk) { int hit = 0; for (const char *q = sk; *q; ) { if (atoi(q) == me->n) hit = 1; while (*q && *q != ',') q++; if (*q) q++; } if (hit) continue; } }   /* R3D_SKIP=3,16: parts left out, to tell what is what */
        if (me->sector && r3d_venueqb_hidden(&vq, me->sector)) continue;   /* a part the venue's scripts put up later (or never) */
        const r3d_mat *m = r3d_scene_mat(&sc, me->mat); if (!m || !m->ntex) { R.skipped++; continue; }
        { int isPanel = 0; if (crowd) for (int i = 0; i < m->ntex && !isPanel; i++) for (int k = 0; k < crowd->n; k++) if (crowd->tex[k] == m->tex[i]) isPanel = 1; if (isPanel) { panels++; continue; } }   /* a crowd stand-in card: the game paints a moving figure on it; here there is no crowd yet */
        /* which texture is the picture, and which the baked light (venueSlots) */
        int n = m->ntex, light = n >= 2 ? n - 1 : -1, pic = r3d_diffuse_slot(m->template);
        if (pic < 0 || pic >= n) { pic = -1; for (int i = 0; i < n && pic < 0; i++) { if (i == light) continue; const r3d_texent *e = NULL; for (int k = 0; k < nt; k++) if (tents[k].key == m->tex[i]) e = &tents[k];
                if (!e) continue; r3d_look lk = r3d_tex_look(te->data + e->off, e->size); if (!lk.normal && !(lk.flat && lk.w <= 32)) pic = i; }
            if (pic < 0) { pic = 0; const r3d_texent *e = NULL; for (int k = 0; k < nt; k++) if (tents[k].key == m->tex[0]) e = &tents[k]; r3d_look lk = e ? r3d_tex_look(te->data + e->off, e->size) : (r3d_look){ 0 };
                if (e && lk.flat && lk.w <= 32) { R.skipped++; continue; } } }   /* a part with only a flat filler for a picture: a stand-in (the grey figure on the dive bar's stage), not scenery */
        int tf = r3d_template_flags(m->template), unlit = tf & TPL_UNLIT, amb = tf & TPL_AMB, lit = !unlit && !amb, board = tf & TPL_BOARD;
        int smapOk = 0; if (lit && light >= 0 && (me->uvLast || me->uv2) && m->tex[light] != m->tex[pic]) { const r3d_texent *e = NULL; for (int k = 0; k < nt; k++) if (tents[k].key == m->tex[light]) e = &tents[k]; if (e && e->w > 32) smapOk = 1; }
        GLuint tex = tex_get(te->data, te->len, tents, nt, m->tex[pic], m->clamp[pic < 3 ? pic : 3]); if (!tex) { R.skipped++; continue; }   /* a screen or sign whose picture the game makes while it runs */
        GLuint smap = smapOk ? tex_get(te->data, te->len, tents, nt, m->tex[light], 3) : 0;
        if (getenv("R3D_DEBUG")) { const r3d_texent *e = NULL; for (int k = 0; k < nt; k++) if (tents[k].key == m->tex[pic]) e = &tents[k]; r3d_look lk = e ? r3d_tex_look(te->data + e->off, e->size) : (r3d_look){ 0 };
            printf("mesh %3d sector %08x mat %08x tpl %08x tf %02x blend %u aref %u ntex %d pic %08x(%dx%d avg %.0f,%.0f,%.0f%s) tris %5d at %.1f %.1f %.1f r %.1f%s\n", me->n, me->sector, m->key, m->template, tf, m->blend, m->alphaRef, m->ntex, m->tex[pic], lk.w, lk.h, lk.r, lk.g, lk.b, lk.flat ? " flat" : "", me->ntri, me->centre[0], me->centre[1], me->centre[2], me->radius, me->sector && r3d_venueqb_object(&vq, me->sector) ? " (object)" : ""); }
        /* the batch of this material (and whether its parts carry a light map) */
        batch *b = NULL; for (int i = 0; i < R.nb; i++) if (R.b[i].mat == m->key && R.b[i].smapped == (smap != 0) && R.b[i].nvert + me->nvert <= 65535) { b = &R.b[i]; break; }
        if (!b) { R.b = realloc(R.b, (R.nb + 1) * sizeof *R.b); b = &R.b[R.nb++]; memset(b, 0, sizeof *b); b->mat = m->key; b->smapped = smap != 0; b->tex = tex; b->smap = smap ? smap : R.white;
            b->blend = m->blend == 1 ? 1 : m->blend ? 2 : 0; b->board = board; b->colA = lit ? 0 : 1; b->tile = 1;
            int at = m->nprops >= 8 && m->props[1] == 0 && m->props[2] == 0 && m->props[3] == 0 ? 4 : 0; for (int i = 0; i < 4; i++) b->colour[i] = m->nprops >= at + 4 ? fminf(8, fmaxf(0, m->props[at + i])) : 1;
            b->alphaRef = m->alphaRef > 0 ? m->alphaRef / 255.0f : b->blend == 2 ? 0.02f : 0;
            if ((tf & TPL_ANIM) && m->nvsp >= 3 && m->vsp[0] >= 1 && m->vsp[1] >= 1) { b->hasAnim = 1; for (int i = 0; i < 4; i++) b->anim[i] = i < m->nvsp ? m->vsp[i] : 0; }
            if (tf & TPL_WIB) { int any = 0; for (int i = 0; i < 11 && i < m->nvsp; i++) if (m->vsp[i]) any = 1; if (m->nvsp >= 11 && any) { b->wib = 1; memcpy(b->wibU, m->vsp, 16); memcpy(b->wibV, m->vsp + 4, 16); memcpy(b->wibR, m->vsp + 8, 12); }
                if (m->nprops >= 6 && m->props[4]) b->tile = m->props[4]; }
            for (int i = 0; i < 3; i++) { b->bmin[i] = 1e30f; b->bmax[i] = -1e30f; } }
        /* where the part stands: a movable object's parts are stored about its own centre */
        float rot[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 }, tr[3] = { 0, 0, 0 }; const r3d_object *ob = me->sector ? r3d_venueqb_object(&vq, me->sector) : NULL; if (ob) { euler_xyz(ob->angles, rot); memcpy(tr, ob->pos, 12); }
        if (b->nvert + me->nvert > b->capV) { b->capV = (b->nvert + me->nvert) * 2; if (b->capV > 65535) b->capV = 65535; b->v = realloc(b->v, (size_t)b->capV * sizeof *b->v); }
        if (b->nidx + me->ntri * 3 > b->capI) { b->capI = (b->nidx + me->ntri * 3) * 2; b->i = realloc(b->i, (size_t)b->capI * sizeof *b->i); }
        for (int v = 0; v < me->nvert; v++) { vtx *o = &b->v[b->nvert + v]; const float *p = me->pos + v * 3, *nn = me->nrm + v * 3; float wp[3], wn[3], wc[3];
            for (int r = 0; r < 3; r++) { wp[r] = rot[r * 3] * p[0] + rot[r * 3 + 1] * p[1] + rot[r * 3 + 2] * p[2] + tr[r]; wn[r] = rot[r * 3] * nn[0] + rot[r * 3 + 1] * nn[1] + rot[r * 3 + 2] * nn[2]; }
            if (board) { for (int r = 0; r < 3; r++) wc[r] = rot[r * 3] * nn[0] + rot[r * 3 + 1] * nn[1] + rot[r * 3 + 2] * nn[2] + tr[r]; for (int r = 0; r < 3; r++) wp[r] = wc[r] + (p[r] - nn[r]); }   /* a card: its centre is stored as the normal */
            else memcpy(wc, wp, 12);
            memcpy(o->pos, wp, 12); memcpy(o->ctr, wc, 12); memcpy(o->col, me->col + v * 4, 4);
            o->uv[0] = me->uv[v * 2]; o->uv[1] = me->uv[v * 2 + 1]; const float *u2 = me->uvLast ? me->uvLast : me->uv2 ? me->uv2 : me->uv; o->uv2[0] = u2[v * 2]; o->uv2[1] = u2[v * 2 + 1];
            float lt[3], am[3]; if (lit) light_at(&L, wp, board ? (float[3]){ 0, 0, 0 } : wn, lt, am); else if (amb) { lt[0] = lt[1] = lt[2] = 0; light_at(&L, wp, wn, (float[3]){ 0 }, am); } else { lt[0] = lt[1] = lt[2] = 1; am[0] = am[1] = am[2] = 0; }
            for (int c = 0; c < 3; c++) { o->lit[c] = q8(lt[c], 255 / 4.0f); b->ambSum[c] += am[c]; } o->lit[3] = 255;
            for (int c = 0; c < 3; c++) { if (wp[c] < b->bmin[c]) b->bmin[c] = wp[c]; if (wp[c] > b->bmax[c]) b->bmax[c] = wp[c]; } }
        for (int i = 0; i < me->ntri * 3; i++) b->i[b->nidx + i] = (uint16_t)(me->idx[i] + b->nvert);
        b->nvert += me->nvert; b->nidx += me->ntri * 3; R.meshes++; R.tris += me->ntri;
    }
    /* upload; the ambient of a batch is the mean over its points (the venues' ambient lights reach the whole room) */
    for (int i = 0; i < R.nb; i++) { batch *b = &R.b[i];
        GL(glGenBuffers)(1, &b->vbo); GL(glBindBuffer)(GL_ARRAY_BUFFER, b->vbo); GL(glBufferData)(GL_ARRAY_BUFFER, (GLsizeiptr)b->nvert * sizeof(vtx), b->v, GL_STATIC_DRAW);
        GL(glGenBuffers)(1, &b->ibo); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, b->ibo); GL(glBufferData)(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)b->nidx * 2, b->i, GL_STATIC_DRAW);
        R.vram += (size_t)b->nvert * sizeof(vtx) + (size_t)b->nidx * 2;
        for (int c = 0; c < 3; c++) { b->amb[c] = (float)(b->ambSum[c] / b->nvert); b->centre[c] = (b->bmin[c] + b->bmax[c]) / 2; }
        float dx = b->bmax[0] - b->bmin[0], dy = b->bmax[1] - b->bmin[1], dz = b->bmax[2] - b->bmin[2]; b->radius = sqrtf(dx * dx + dy * dy + dz * dz) / 2;
        free(b->v); free(b->i); b->v = NULL; b->i = NULL; }
    GL(glBindBuffer)(GL_ARRAY_BUFFER, 0); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, 0);
    /* the opaque batches first (by texture), the see-through ones after: those are sorted back to front when drawn */
    for (int i = 1; i < R.nb; i++) for (int j = i; j > 0 && (R.b[j].blend != 0) * 1000000 + (int)R.b[j].tex < (R.b[j - 1].blend != 0) * 1000000 + (int)R.b[j - 1].tex; j--) { batch t = R.b[j]; R.b[j] = R.b[j - 1]; R.b[j - 1] = t; }
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
    SDL_Log("r3d: %s: %d meshes in %d batches (%d skipped, %d crowd cards), %d triangles, %d textures, %.1f MB on the GPU, %d shots (%d general; %d lights, %d objects, %d hidden), %u ms",
        zone, R.meshes, R.nb, R.skipped, panels, R.tris, R.textures, R.vram / 1048576.0, R.nshots, R.ngen, vq.nlights, vq.nobjects, vq.nhidden, SDL_GetTicks() - t0);
    R.loadMs = SDL_GetTicks() - t0; snprintf(R.zone, sizeof R.zone, "%s", zone); R.loaded = 1;
    free(L.l); r3d_venueqb_free(&vq); free(tents); r3d_scene_free(&sc); free(ents); free(pak); return 0;
}
const char *r3d_zone(void) { return R.zone; }
void r3d_select(const char *zone) {
    if (!R.ready) return; if (!zone || strncmp(zone, "z_", 2)) zone = "z_dive";
    if (!R.loaded || strcmp(R.zone, zone)) load_zone(zone);
    r3d_band_select();   /* who plays this song (a boss battle has the boss on stage) */
}

/* ------------------------------------------------------------------ drawing */
typedef struct { GLint prog, abuf, ebuf, active, tex0, tex1, view[4], scissor[4], beq[2], bfn[4], attribs[8], nattr; GLboolean blend, scis, depth, cull, dmask, cmask[4]; } glstate;
static void state_save(glstate *s) {
    GL(glGetIntegerv)(GL_CURRENT_PROGRAM, &s->prog); GL(glGetIntegerv)(GL_ARRAY_BUFFER_BINDING, &s->abuf); GL(glGetIntegerv)(GL_ELEMENT_ARRAY_BUFFER_BINDING, &s->ebuf);
    GL(glGetIntegerv)(GL_ACTIVE_TEXTURE, &s->active); GL(glActiveTexture)(GL_TEXTURE0); GL(glGetIntegerv)(GL_TEXTURE_BINDING_2D, &s->tex0); GL(glActiveTexture)(GL_TEXTURE1); GL(glGetIntegerv)(GL_TEXTURE_BINDING_2D, &s->tex1);
    GL(glGetIntegerv)(GL_VIEWPORT, s->view); GL(glGetIntegerv)(GL_SCISSOR_BOX, s->scissor); GL(glGetIntegerv)(GL_BLEND_EQUATION_RGB, &s->beq[0]); GL(glGetIntegerv)(GL_BLEND_EQUATION_ALPHA, &s->beq[1]);
    GL(glGetIntegerv)(GL_BLEND_SRC_RGB, &s->bfn[0]); GL(glGetIntegerv)(GL_BLEND_DST_RGB, &s->bfn[1]); GL(glGetIntegerv)(GL_BLEND_SRC_ALPHA, &s->bfn[2]); GL(glGetIntegerv)(GL_BLEND_DST_ALPHA, &s->bfn[3]);
    s->blend = GL(glIsEnabled)(GL_BLEND); s->scis = GL(glIsEnabled)(GL_SCISSOR_TEST); s->depth = GL(glIsEnabled)(GL_DEPTH_TEST); s->cull = GL(glIsEnabled)(GL_CULL_FACE);
    GL(glGetBooleanv)(GL_DEPTH_WRITEMASK, &s->dmask); GL(glGetBooleanv)(GL_COLOR_WRITEMASK, s->cmask);
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
    GL(glDepthMask)(s->dmask); GL(glColorMask)(s->cmask[0], s->cmask[1], s->cmask[2], s->cmask[3]);
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
static struct { int on; float view[16], fov; } camOver;   /* --r3dtest --band: a camera on one member */
static void camera(double t, float *view, float *proj) {
    float fov = 72; if (t < 0) t = 0; int placed = 0;
    if (camOver.on) { memcpy(view, camOver.view, 64); fov = camOver.fov; placed = 1; }
    const gh3_songdata *sd = A.screen == SC_PLAY && A.S.loaded && A.S.sd.ncams ? &A.S.sd : NULL;
    static const gh3_songdata *song; static int pCam = -1, cur = -1; static double t0, t1, lastMs;
    if (placed) {}
    else if (sd && R.nshots) { double ms = A.S.run.now; int n = sd->ncams;
        if (sd != song || ms + 1500 < lastMs) { song = sd; pCam = -1; cur = -1; }   /* a new song, or the clock wound back (practice, a restart) */
        lastMs = ms; int k = pCam < 0 ? 0 : pCam; while (k + 1 < n && sd->cams[k + 1][0] <= ms) k++;
        if (k != pCam) { pCam = k; int nx = cut_to(cue_of(sd->cams[k][1]), k, cur);
            if (nx != cur || cur < 0) { cur = nx >= 0 ? nx : R.order[0]; t0 = sd->cams[k][0]; int j = k + 1; while (j < n && !cue_of(sd->cams[j][1])) j++; t1 = j < n ? sd->cams[j][0] : t0 + (sd->cams[k][2] > 4000 ? sd->cams[k][2] : 4000); } }   /* a cue that lands on the shot already showing lets it carry on */
        if (cur >= 0) { double u = (ms - t0) / (t1 - t0 > 1 ? t1 - t0 : 1); place(&R.shots[cur], u < 0 ? 0 : u > 1 ? 1 : u, t, view, &fov); placed = 1; } }
    else song = NULL;
    if (!placed && R.nshots) { double n = floor(t / (SHOT_MS / 1000)), u = t / (SHOT_MS / 1000) - n; int g = (int)((long)n % R.ngen); for (int k = 0; k < R.ngen && !shot_ok(&R.shots[R.order[g]]); k++) g = (g + 1) % R.ngen;
        place(&R.shots[R.order[g]], u, t, view, &fov); placed = 1; }
    if (!placed) { float pos[3], q[4], a = (float)t * 0.05f; pos[0] = R.ctr[0] + sinf(a) * R.span * 0.2f; pos[1] = R.ctr[1]; pos[2] = R.ctr[2] + cosf(a) * R.span * 0.2f;
        float d[3] = { R.ctr[0] - pos[0], 0, R.ctr[2] - pos[2] }, L = sqrtf(d[0] * d[0] + d[2] * d[2]); if (L < 1e-4f) { d[2] = 1; L = 1; } float yaw = atan2f(d[0] / L, d[2] / L); q[0] = 0; q[1] = sinf(yaw / 2); q[2] = 0; q[3] = cosf(yaw / 2);   /* the camera's +Z toward the middle */
        mat_view(view, pos, q, 0); }
    float vf = 2 * atanf(tanf(fov * (float)M_PI / 360) * 0.75f);   /* the stored angle is the width of a 4:3 picture */
    mat_perspective(proj, vf, (float)R.W / R.H, Z_NEAR, Z_FAR);
}
static void draw_batch(const batch *b) {
    GL(glBindBuffer)(GL_ARRAY_BUFFER, b->vbo); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, b->ibo);
    GL(glVertexAttribPointer)(R.aPos, 3, GL_FLOAT, GL_FALSE, sizeof(vtx), (void *)0); GL(glVertexAttribPointer)(R.aCtr, 3, GL_FLOAT, GL_FALSE, sizeof(vtx), (void *)12);
    GL(glVertexAttribPointer)(R.aCol, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vtx), (void *)24); GL(glVertexAttribPointer)(R.aLit, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vtx), (void *)28);
    GL(glVertexAttribPointer)(R.aUv, 2, GL_FLOAT, GL_FALSE, sizeof(vtx), (void *)32); GL(glVertexAttribPointer)(R.aUv2, 2, GL_FLOAT, GL_FALSE, sizeof(vtx), (void *)40);
    GL(glActiveTexture)(GL_TEXTURE1); GL(glBindTexture)(GL_TEXTURE_2D, b->smap); GL(glActiveTexture)(GL_TEXTURE0); GL(glBindTexture)(GL_TEXTURE_2D, b->tex);
    GL(glUniform1f)(R.uBoard, (float)b->board); GL(glUniform4f)(R.uAnim, b->hasAnim ? b->anim[0] : 0, b->anim[1], b->anim[2], b->anim[3]);
    GL(glUniform1f)(R.uWib, (float)b->wib); if (b->wib) { GL(glUniform4f)(R.uWibU, b->wibU[0], b->wibU[1], b->wibU[2], b->wibU[3]); GL(glUniform4f)(R.uWibV, b->wibV[0], b->wibV[1], b->wibV[2], b->wibV[3]); GL(glUniform3f)(R.uWibR, b->wibR[0], b->wibR[1], b->wibR[2]); }
    GL(glUniform4f)(R.uColour, b->colour[0], b->colour[1], b->colour[2], b->colour[3]); GL(glUniform3f)(R.uAmb, b->amb[0], b->amb[1], b->amb[2]); GL(glUniform1f)(R.uAlphaRef, b->alphaRef); GL(glUniform1f)(R.uColA, (float)b->colA); GL(glUniform1f)(R.uTile, b->tile);
    if (b->blend == 1) { GL(glEnable)(GL_BLEND); GL(glBlendFunc)(GL_ONE, GL_ONE); GL(glDepthMask)(GL_FALSE); } else if (b->blend == 2) { GL(glEnable)(GL_BLEND); GL(glBlendFunc)(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); GL(glDepthMask)(GL_FALSE); } else { GL(glDisable)(GL_BLEND); GL(glDepthMask)(GL_TRUE); }
    GL(glDrawElements)(GL_TRIANGLES, b->nidx, GL_UNSIGNED_SHORT, (void *)0); R.drawCalls++; R.drawnTris += b->nidx / 3;
}
static int draw(double perf, int sp, float energy) {
    double t = perf / 1000; float view[16], proj[16], vp[16]; camera(t, view, proj); mat_mul(proj, view, vp);
    R.flush(R.ren); glstate st; state_save(&st); R.drawCalls = R.drawnTris = 0;
    GL(glDisable)(GL_SCISSOR_TEST); GL(glViewport)(0, 0, R.W, R.H); GL(glEnable)(GL_DEPTH_TEST); GL(glDepthFunc)(GL_LEQUAL); GL(glDepthMask)(GL_TRUE); GL(glColorMask)(1, 1, 1, 1); GL(glDisable)(GL_CULL_FACE);
    GL(glClear)(GL_DEPTH_BUFFER_BIT); GL(glBlendEquationSeparate)(GL_FUNC_ADD, GL_FUNC_ADD);
    GL(glUseProgram)(R.prog); GL(glUniformMatrix4fv)(R.uProj, 1, GL_FALSE, proj); GL(glUniformMatrix4fv)(R.uView, 1, GL_FALSE, view); GL(glUniform1f)(R.uTime, (float)fmod(t, 4096)); GL(glUniform1i)(R.uMap, 0); GL(glUniform1i)(R.uSmap, 1);
    float k = 1 + 0.12f * energy; if (sp) GL(glUniform3f)(R.uTint, k * 0.78f, k * 0.98f, k * 1.18f); else GL(glUniform3f)(R.uTint, k, k, k);   /* star power: the room goes blue */
    for (int i = 0; i < st.nattr; i++) GL(glDisableVertexAttribArray)(i);
    GL(glEnableVertexAttribArray)(R.aPos); GL(glEnableVertexAttribArray)(R.aCtr); GL(glEnableVertexAttribArray)(R.aCol); GL(glEnableVertexAttribArray)(R.aLit); GL(glEnableVertexAttribArray)(R.aUv); GL(glEnableVertexAttribArray)(R.aUv2);
    float cam[3] = { -(view[0] * view[12] + view[1] * view[13] + view[2] * view[14]), -(view[4] * view[12] + view[5] * view[13] + view[6] * view[14]), -(view[8] * view[12] + view[9] * view[13] + view[10] * view[14]) };
    int first = R.nb; for (int i = 0; i < R.nb; i++) { if (R.b[i].blend) { if (first == R.nb) first = i; continue; } if (R.b[i].board || sphere_visible(vp, R.b[i].centre, R.b[i].radius)) draw_batch(&R.b[i]); }
    if (r3d_band_draw(view, proj, perf, sp, energy)) { GL(glUseProgram)(R.prog);   /* the band, after the solid parts and before the see-through ones; then the venue's program and arrays again */
        GL(glEnableVertexAttribArray)(R.aPos); GL(glEnableVertexAttribArray)(R.aCtr); GL(glEnableVertexAttribArray)(R.aCol); GL(glEnableVertexAttribArray)(R.aLit); GL(glEnableVertexAttribArray)(R.aUv); GL(glEnableVertexAttribArray)(R.aUv2); }
    if (first < R.nb) {   /* the see-through parts, farthest first */
        static float *dist; static int *idx, cap; int n = R.nb - first; if (n > cap) { cap = n; dist = realloc(dist, cap * sizeof *dist); idx = realloc(idx, cap * sizeof *idx); }
        for (int i = 0; i < n; i++) { const batch *b = &R.b[first + i]; float dx = b->centre[0] - cam[0], dy = b->centre[1] - cam[1], dz = b->centre[2] - cam[2]; dist[i] = dx * dx + dy * dy + dz * dz; idx[i] = first + i; }
        for (int i = 1; i < n; i++) for (int j = i; j > 0 && dist[idx[j] - first] > dist[idx[j - 1] - first]; j--) { int t2 = idx[j]; idx[j] = idx[j - 1]; idx[j - 1] = t2; }
        for (int i = 0; i < n; i++) if (R.b[idx[i]].board || sphere_visible(vp, R.b[idx[i]].centre, R.b[idx[i]].radius)) draw_batch(&R.b[idx[i]]); }
    GL(glBindBuffer)(GL_ARRAY_BUFFER, 0); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, 0); GL(glDepthMask)(GL_TRUE);
    GL(glDisableVertexAttribArray)(R.aPos); GL(glDisableVertexAttribArray)(R.aCtr); GL(glDisableVertexAttribArray)(R.aCol); GL(glDisableVertexAttribArray)(R.aLit); GL(glDisableVertexAttribArray)(R.aUv); GL(glDisableVertexAttribArray)(R.aUv2);
    state_restore(&st); return 1;
}
int r3d_draw_stage(double perf, int sp, float energy) {
    if (!R.ready || !R.loaded || !SET.stage3d) return 0;
    return draw(perf, sp, energy);
}
void r3d_shutdown(void) { if (!R.ready) return; r3d_band_shutdown(); unload(); GL(glDeleteProgram)(R.prog); GL(glDeleteTextures)(1, &R.white); R.ready = 0; }

/* ------------------------------------------------------------------ --r3dtest */
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
static void band_test_frames(SDL_Renderer *ren, const char *zone, const char *outdir, int W, int H, uint8_t *px) {
    /* the song's clock from 0, a frame every 1/60 s; at two moments, a picture of each member from the front and one of the whole band */
    static const double AT[3] = { 6000, 21000, 47000 }; static const char *RN[BAND_ROLES] = { "guitarist", "bassist", "drummer", "vocalist" }; double ms = 0, poseMs = 0, drawMs = 0; int nf = 0, maxDraws = 0, maxTris = 0;
    for (int k = 0; k < 3; k++) {
        for (; ms < AT[k]; ms += 1000.0 / 60) { r3d_band_test_clock(ms, r3d_test_song); camOver.on = 0; SDL_RenderClear(ren); draw(ms, 0, 0.5f); R.flush(ren); GL(glFinish)(); const r3d_bandstats *b = r3d_band_stats(); poseMs += b->poseMs; drawMs += b->drawMs; nf++; if (b->draws > maxDraws) { maxDraws = b->draws; maxTris = b->tris; } }
        float c[3] = { 0, 0, 0 }, fwd[3] = { 0, 0, 0 }; int n = 0;
        for (int r = 0; r <= BAND_ROLES; r++) { float p[3], yaw; char path[700];
            if (r < BAND_ROLES) { if (!r3d_band_where(r, p, &yaw)) continue; c[0] += p[0]; c[1] += p[1]; c[2] += p[2]; fwd[0] += sinf(yaw); fwd[2] += cosf(yaw); n++;
                float d = r == BAND_DRUMMER ? 3.6f : 2.6f, e[3] = { p[0] + sinf(yaw) * d, p[1] + 1.35f, p[2] + cosf(yaw) * d }, at[3] = { p[0], p[1] + 1.0f, p[2] }; look_at(camOver.view, e, at); camOver.fov = 50;
                snprintf(path, sizeof path, "%s/%s_band_%s_%d.bmp", outdir && *outdir ? outdir : ".", zone, RN[r], k); }
            else { if (!n) break; for (int i = 0; i < 3; i++) c[i] /= n; float L = sqrtf(fwd[0] * fwd[0] + fwd[2] * fwd[2]); if (L < 1e-3f) { fwd[2] = 1; L = 1; } float e[3] = { c[0] + fwd[0] / L * 7.5f, c[1] + 2.4f, c[2] + fwd[2] / L * 7.5f }, at[3] = { c[0], c[1] + 1.0f, c[2] };
                look_at(camOver.view, e, at); camOver.fov = 72; snprintf(path, sizeof path, "%s/%s_band_all_%d.bmp", outdir && *outdir ? outdir : ".", zone, k); }
            camOver.on = 1; SDL_SetRenderDrawColor(ren, 18, 16, 24, 255); SDL_RenderClear(ren); draw(ms, 0, 0.5f); R.flush(ren); GL(glFinish)();
            GL(glPixelStorei)(GL_PACK_ALIGNMENT, 1); GL(glReadPixels)(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);
            for (int y = 0; y < H / 2; y++) for (int x = 0; x < W * 4; x++) { uint8_t t = px[(size_t)y * W * 4 + x]; px[(size_t)y * W * 4 + x] = px[(size_t)(H - 1 - y) * W * 4 + x]; px[(size_t)(H - 1 - y) * W * 4 + x] = t; }
            SDL_Surface *sf = SDL_CreateRGBSurfaceFrom(px, W, H, 32, W * 4, 0x000000FF, 0x0000FF00, 0x00FF0000, 0); SDL_SaveBMP(sf, path); SDL_FreeSurface(sf);
            const r3d_bandstats *b = r3d_band_stats(); printf("  band %s at %.1f s: %d draws, %d triangles -> %s\n", r < BAND_ROLES ? RN[r] : "all", ms / 1000, b->draws, b->tris, path); } }
    camOver.on = 0;
    printf("band over %d frames: %.3f ms/frame animating + skinning matrices, %.3f ms/frame submitting (host CPU); up to %d draws, %d triangles a frame\n", nf, poseMs / nf, drawMs / nf, maxDraws, maxTris);
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
    printf("%s: %d meshes in %d batches, %d skipped, %d triangles, %d textures, %.1f MB on the GPU (%s), %d camera shots, loaded in %.0f ms\n", zone, R.meshes, R.nb, R.skipped, R.tris, R.textures, R.vram / 1048576.0, R.s3tc ? "DXT" : "decoded", R.nshots, R.loadMs);
    /* one picture per shot (the first few), read back from the window */
    uint8_t *px = malloc((size_t)W * H * 4); int nsave = R.nshots < 4 ? (R.nshots ? R.nshots : 1) : 4;
    for (int s = 0; s < nsave; s++) { double perf = s * SHOT_MS + SHOT_MS * 0.35;
        SDL_SetRenderDrawColor(ren, 18, 16, 24, 255); SDL_RenderClear(ren); draw(perf, 0, 0.5f); R.flush(ren); GL(glFinish)();
        GL(glPixelStorei)(GL_PACK_ALIGNMENT, 1); GL(glReadPixels)(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);
        for (int y = 0; y < H / 2; y++) for (int x = 0; x < W * 4; x++) { uint8_t t = px[(size_t)y * W * 4 + x]; px[(size_t)y * W * 4 + x] = px[(size_t)(H - 1 - y) * W * 4 + x]; px[(size_t)(H - 1 - y) * W * 4 + x] = t; }
        SDL_Surface *sf = SDL_CreateRGBSurfaceFrom(px, W, H, 32, W * 4, 0x000000FF, 0x0000FF00, 0x00FF0000, 0); char p[700]; snprintf(p, sizeof p, "%s/%s_%d.bmp", outdir && *outdir ? outdir : ".", zone, s); SDL_SaveBMP(sf, p); SDL_FreeSurface(sf);
        printf("  shot %d: %d draw calls, %d triangles -> %s\n", s, R.drawCalls, R.drawnTris, p); SDL_RenderPresent(ren); }
    if (r3d_test_band) band_test_frames(ren, zone, outdir, W, H, px);
    /* timing: frames across all the shots, the GPU made to finish each */
    int N = 120; Uint64 f0 = SDL_GetPerformanceCounter(); long calls = 0;
    for (int f = 0; f < N; f++) { SDL_RenderClear(ren); draw(f * 1000.0 / 60 * 7, 0, 0.5f); R.flush(ren); GL(glFinish)(); calls += R.drawCalls; }
    double ms = (SDL_GetPerformanceCounter() - f0) * 1000.0 / SDL_GetPerformanceFrequency() / N;
    printf("%d frames: %.2f ms/frame on this GPU (%s), %.0f draw calls/frame\n", N, ms, GL(glGetString)(GL_RENDERER), (double)calls / N);
    free(px); r3d_shutdown(); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit(); return 0;
}
