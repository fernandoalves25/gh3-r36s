/* The band on the 3D stage. A port of the browser port's band (index.html: bandRead, bandPlan, bandMember, charMeshes,
 * charArm, charClip, T.tick), docs/research/band.md being the recipe:
 *   who:   musician_profiles of qb.pak; the guitarist is the game's default character (Axel: what the game falls back to
 *          for player 1), then Bassist, Drummer, Singer; in a boss battle the boss (Boss_*_Props character_profile)
 *          stands where the bassist would and the bassist stays home.
 *   look:  the largest .skin and .tex of <body>.pak and <instrument>.pak; the guitar and the bass hang on bone 119 with the
 *          guitar skeleton's rest pose as their bind (0xe5d20452); the drum kit and the mic on their player's own bones.
 *   moves: the profile's anim set (stance / kind / mood / speed lists, 0.3 s cross-fades, round robin), the hand shapes
 *          of the chord being played (finger_anims), the fretting hand's place on the neck (fret_anims from _anim_notes),
 *          the song's _performance events (stance changes, solos, jumps, walks, the singer's words), the drummer's hits
 *          from _drums_notes started 0.13 s early so the stick lands on the note, and the two-bone arm reach onto the
 *          guitar's hand targets (bones 122 / 124) with the strumming hand's +-2.8 cm stroke on each note.
 *   draw:  one draw per mesh (split when a mesh needs more bones than the shader's palette holds), skinned on the GPU
 *          with up to three bones a point, lit by one key light and the ambient of the venue's band lights. */
#include "r3d_band.h"
#include "r3d_lights.h"
#include "r3d_gl.h"
#include "r3d_skin.h"
#include "app.h"
#include "settings.h"
#include "battle.h"
#include "charsel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define GL(name) r3d_##name
#define NB 128               /* bones a skeleton may have here (the body ones have 125) */
#define PAL_MAX 80           /* bones one draw may use (3 vec4 each): every mesh of the game fits (the singer's largest uses 79) */
#define FADE 0.3f
#define HIT_LEAD 0.13f
#define HIT_WEIGHT 16.0f
#define BAND_TEX 512         /* textures above this take a smaller level (a 1024 body map at 512: what the screen shows of it at 640x480) */
#define MAXACT 24
#define GL_MAX_VERTEX_UNIFORM_VECTORS 0x8DFB
#define GL_MAX_VERTEX_UNIFORM_COMPONENTS 0x8B4A
#define GL_BYTE 0x1400

static void (*glUniform4fv_)(GLint, GLsizei, const GLfloat *); static void (*glDrawArrays_)(GLenum, GLint, GLsizei);   /* not in r3d_gl.h's list: looked up here */
static const char *ROLE[BAND_ROLES] = { "guitarist", "bassist", "drummer", "vocalist" };
static int dbg(void) { static int lv = -1; if (lv < 0) { const char *e = getenv("R3D_BAND_DEBUG"); lv = e ? (atoi(e) > 0 ? atoi(e) : 1) : 0; } return lv; }   /* R3D_BAND_DEBUG=1: what each one plays; 2: and every drum hit */

/* ------------------------------------------------------------------ the game's lists (qb.pak), read once */
typedef struct { char name[24]; uint32_t nameKey; int role; uint32_t ske, set, stance, fingers, frets; float scale; char body[48], anims[48], inst[48]; } profile;
typedef struct { uint32_t id; qbv *v; } keptsec;
static struct { int tried, ok; profile *pf; int npf; keptsec *sec; int nsec; } L;
static uint32_t K_IDLE, K_INTRO, K_STARPOWER, K_TRANS, K_FACIAL, K_WALKING, K_STANCE_A, K_SLOW, K_MED, K_FAST, K_BAD, K_NORMAL, K_GOOD, K_EV_STANCE, K_EV_PLAY, K_EV_FACE, K_EV_WALK, F_CYCLE, F_NOARMS, K_SKA, K_SKIN, K_TEX, K_SKE, K_QB;
static uint32_t K_WIN, K_LOSE, K_WIN_SM, K_LOSE_SM, K_INTRO_SM, K_BTL1, K_BTL2, K_ATK_ANIM, K_RESP_ANIM, K_NONE;
static void keys_init(void) {
    if (K_IDLE) return;
    K_WIN = gh3_qbkey("win"); K_LOSE = gh3_qbkey("lose"); K_WIN_SM = gh3_qbkey("win_smstg"); K_LOSE_SM = gh3_qbkey("lose_smstg"); K_INTRO_SM = gh3_qbkey("intro_smstg");
    K_BTL1 = gh3_qbkey("player1_battlemode_anims"); K_BTL2 = gh3_qbkey("player2_battlemode_anims"); K_ATK_ANIM = gh3_qbkey("attack_anim"); K_RESP_ANIM = gh3_qbkey("response_anim"); K_NONE = gh3_qbkey("none");
    K_IDLE = gh3_qbkey("idle"); K_INTRO = gh3_qbkey("intro"); K_STARPOWER = gh3_qbkey("starpower"); K_TRANS = gh3_qbkey("transitions"); K_FACIAL = gh3_qbkey("facial_anims"); K_WALKING = gh3_qbkey("walking"); K_STANCE_A = gh3_qbkey("stance_a");
    K_SLOW = gh3_qbkey("slow"); K_MED = gh3_qbkey("med"); K_FAST = gh3_qbkey("fast"); K_BAD = gh3_qbkey("bad"); K_NORMAL = gh3_qbkey("normal"); K_GOOD = gh3_qbkey("good");
    K_EV_STANCE = gh3_qbkey("band_changestance"); K_EV_PLAY = gh3_qbkey("band_playanim"); K_EV_FACE = gh3_qbkey("band_playfacialanim"); K_EV_WALK = gh3_qbkey("band_walktonode");
    F_CYCLE = gh3_qbkey("cycle"); F_NOARMS = gh3_qbkey("disable_auto_arms"); K_SKA = gh3_qbkey(".ska"); K_SKIN = gh3_qbkey(".skin"); K_TEX = gh3_qbkey(".tex"); K_SKE = gh3_qbkey(".ske"); K_QB = gh3_qbkey(".qb");
}
static qbv *qdup(const qbv *v) {
    if (!v) return NULL; qbv *o = malloc(sizeof *o); *o = *v; o->s = v->s ? strdup(v->s) : NULL; o->items = NULL; o->arr = NULL;
    if (v->nitems > 0 && v->items) { o->items = malloc(v->nitems * sizeof *o->items); for (int i = 0; i < v->nitems; i++) { o->items[i] = v->items[i]; o->items[i].v = qdup(v->items[i].v); } } else o->nitems = 0;
    if (v->narr > 0 && v->arr) { o->arr = malloc(v->narr * sizeof *o->arr); for (int i = 0; i < v->narr; i++) o->arr[i] = qdup(v->arr[i]); } else o->narr = 0;
    return o;
}
static const qbv *sec(uint32_t id) { for (int i = 0; i < L.nsec; i++) if (L.sec[i].id == id) return L.sec[i].v; return NULL; }
static const qbv *qsub(const qbv *st, uint32_t k) { return qb_getk(st, k); }
static int qkeys(const qbv *v, uint32_t *out, int max) { int n = 0; uint32_t k;
    if (v && v->kind == QB_ARRAY) { for (int i = 0; i < v->narr && n < max; i++) if (qb_keyof(v->arr[i], &k)) out[n++] = k; } else if (qb_keyof(v, &k) && max > 0) out[n++] = k; return n; }
static void basename_of(const char *path, char *out, size_t n) {   /* "pak\models\guitarists\axel_1\axel_1.pak" -> "axel_1" */
    const char *b = path; for (const char *p = path; *p; p++) if (*p == '\\' || *p == '/') b = p + 1;
    snprintf(out, n, "%s", b); char *d = strstr(out, ".pak"); if (!d) d = strstr(out, ".PAK"); if (d) *d = 0;
}
static const qbv *desc_of(const qbv *list, uint32_t id) { uint32_t k; if (list && list->kind == QB_ARRAY) for (int i = 0; i < list->narr; i++) if (qb_keyof(qb_get(list->arr[i], "desc_id"), &k) && k == id) return list->arr[i]; return NULL; }
static int lib_load(void) {
    if (L.tried) return L.ok; L.tried = 1; keys_init(); Uint32 t0 = SDL_GetTicks();
    size_t pl, bl; uint8_t *pak = app_read_data("pak", "qb.pak.xen", &pl), *pab = app_read_data("pak", "qb.pab.xen", &bl);
    if (!pak || !pab) { free(pak); free(pab); SDL_Log("band: no PAK/qb.pak.xen + qb.pab.xen"); return 0; }
    gh3_pakent *ents; int ne = gh3_pak_parse(pak, pl, pab, bl, &ents);
    qbsection **fs = calloc(ne ? ne : 1, sizeof *fs); int *fn = calloc(ne ? ne : 1, sizeof *fn);
    for (int e = 0; e < ne; e++) if (ents[e].type == K_QB && ents[e].data) { fn[e] = gh3_qb_parse(ents[e].data, ents[e].len, &fs[e]); if (fn[e] < 0) { fn[e] = 0; fs[e] = NULL; } }
#define FINDSEC(KEY_, out) do { out = NULL; for (int e_ = 0; e_ < ne && !out; e_++) for (int s_ = 0; s_ < fn[e_]; s_++) if (fs[e_][s_].id == (KEY_)) { out = fs[e_][s_].v; break; } } while (0)
#define KEEP(KEY_) do { uint32_t id_ = (KEY_); if (id_ && !sec(id_)) { const qbv *v_; FINDSEC(id_, v_); if (v_) { L.sec = realloc(L.sec, (L.nsec + 1) * sizeof *L.sec); L.sec[L.nsec].id = id_; L.sec[L.nsec].v = qdup(v_); L.nsec++; } } } while (0)
    const qbv *mp, *mb, *mi; FINDSEC(gh3_qbkey("musician_profiles"), mp); FINDSEC(gh3_qbkey("musician_body"), mb); FINDSEC(gh3_qbkey("musician_instrument"), mi);
    if (mp && mp->kind == QB_ARRAY) for (int i = 0; i < mp->narr; i++) { const qbv *st = mp->arr[i]; if (!st || st->kind != QB_STRUCT) continue;
        profile p; memset(&p, 0, sizeof p); snprintf(p.name, sizeof p.name, "%s", qb_str(qb_get(st, "name"))); p.nameKey = gh3_qbkey(p.name); uint32_t ty = 0, id;
        qb_keyof(qb_get(st, "type"), &ty); p.role = -1; for (int r = 0; r < BAND_ROLES; r++) if (ty == gh3_qbkey(ROLE[r])) p.role = r;
        qb_keyof(qb_get(st, "skeleton"), &p.ske); qb_keyof(qb_get(st, "anim_set"), &p.set); if (!qb_keyof(qb_get(st, "stance"), &p.stance)) p.stance = K_STANCE_A;
        qb_keyof(qb_get(st, "finger_anims"), &p.fingers); qb_keyof(qb_get(st, "fret_anims"), &p.frets);
        const qbv *sc = qb_get(st, "scale"); p.scale = sc && sc->kind == QB_VEC3 && sc->f[0] > 0.1f ? sc->f[0] : 1;
        if (qb_keyof(qb_get(qb_get(st, "musician_body"), "desc_id"), &id)) { const qbv *d = desc_of(mb, id); if (d) { basename_of(qb_str(qb_get(d, "pak")), p.body, sizeof p.body); basename_of(qb_str(qb_get(d, "anims_pak")), p.anims, sizeof p.anims); } }
        if (qb_keyof(qb_get(qb_get(st, "musician_instrument"), "desc_id"), &id)) { const qbv *d = desc_of(mi, id); if (d) basename_of(qb_str(qb_get(d, "pak")), p.inst, sizeof p.inst); }
        if (p.role < 0 || !p.name[0]) continue;
        L.pf = realloc(L.pf, (L.npf + 1) * sizeof *L.pf); L.pf[L.npf++] = p; KEEP(p.set); KEEP(p.fingers); KEEP(p.frets); }
    KEEP(gh3_qbkey("drummer_anims")); KEEP(gh3_qbkey("cymbal_anims")); KEEP(K_BTL1); KEEP(K_BTL2);   /* who plays what when a battle's attack lands */
    if (A.lib) for (int s = 0; s < A.lib->nsongs; s++) if (A.lib->songs[s].isBoss) KEEP(A.lib->songs[s].bossKey);
#undef KEEP
#undef FINDSEC
    for (int e = 0; e < ne; e++) if (fs[e]) gh3_qb_free_sections(fs[e], fn[e]); free(fs); free(fn); free(ents); free(pak); free(pab);
    L.ok = L.npf > 0; SDL_Log("band: %d musician profiles, %d lists kept, %u ms", L.npf, L.nsec, SDL_GetTicks() - t0); return L.ok;
}
static const profile *profile_named(const char *name, int role) { for (int i = 0; i < L.npf; i++) if (!strcasecmp(L.pf[i].name, name) && (role < 0 || L.pf[i].role == role)) return &L.pf[i]; return NULL; }
static const profile *profile_key(uint32_t k) { for (int i = 0; i < L.npf; i++) if (L.pf[i].nameKey == k) return &L.pf[i]; return NULL; }

/* ------------------------------------------------------------------ files shared by everyone: the skeletons, perm_anims */
static struct { int tried; r3d_pakfile global, perm; int hasGlobal, hasPerm; r3d_ske guitar; int hasGuitar; } F;
static void files_open(void) {
    if (F.tried) return; F.tried = 1;
    const char *gp = app_data_path("zones", "global.pak.xen"), *gb = app_data_path("zones", "global.pab.xen");
    char gpc[700] = "", gbc[700] = ""; if (gp) snprintf(gpc, sizeof gpc, "%s", gp); if (gb) snprintf(gbc, sizeof gbc, "%s", gb);
    F.hasGlobal = gp && gb && r3d_pak_open(&F.global, gpc, gbc) == 0;
    const char *pp = app_data_path("pak", "perm_anims.pak.xen"); F.hasPerm = pp && r3d_pak_open(&F.perm, pp, NULL) == 0;
    if (F.hasGlobal) { int i = r3d_pak_find(&F.global, K_SKE, 0xe5d20452u); size_t n; uint8_t *b = r3d_pak_read(&F.global, i, &n); if (b) { F.hasGuitar = r3d_ske_read(b, n, &F.guitar) == 0; free(b); } }   /* the rest pose every guitar and bass is built on */
}
static int ske_load(uint32_t key, r3d_ske *o) { if (!F.hasGlobal) return -1; int i = r3d_pak_find(&F.global, K_SKE, key); size_t n; uint8_t *b = r3d_pak_read(&F.global, i, &n); if (!b) return -1; int r = r3d_ske_read(b, n, o); free(b); return r; }

/* ------------------------------------------------------------------ GPU side */
static struct { int ready; GLuint prog, sprog; GLint aPos, aNrm, aUv, aIdx, aW, uVP, uB, uLdir, uLcol, uAmb, uTint, uMap, uAref, saPos, suVP, suC; int pal, s3tc; } G;
/* a soft dark patch on the floor under each member (the browser port flattens the whole band onto the floor instead: a second skinned pass) */
static const char *SVS = "uniform mat4 uVP; uniform vec4 uC; attribute vec2 aPos; varying vec2 vP;\n"
    "void main() { vP = aPos; gl_Position = uVP * vec4(uC.x + aPos.x * uC.w, uC.y, uC.z + aPos.y * uC.w, 1.0); }\n";
static const char *SFS = "#ifdef GL_ES\nprecision mediump float;\n#endif\nvarying vec2 vP;\n"
    "void main() { float d = length(vP); gl_FragColor = vec4(0.0, 0.0, 0.0, 0.42 * (1.0 - smoothstep(0.35, 1.0, d))); }\n";
static const char *VS_FMT =
    "#define NPAL %d\n"
    "uniform mat4 uVP; uniform vec4 uB[NPAL * 3]; uniform vec3 uLdir; uniform vec3 uLcol; uniform vec3 uAmb;\n"
    "attribute vec3 aPos; attribute vec3 aNrm; attribute vec2 aUv; attribute vec3 aIdx; attribute vec3 aW;\n"
    "varying vec2 vUv; varying vec3 vL;\n"
    "void main() {\n"
    "  int i0 = int(aIdx.x), i1 = int(aIdx.y), i2 = int(aIdx.z);\n"   /* each a palette slot times three: the rows of a 4x3 matrix */
    "  vec4 r0 = uB[i0] * aW.x + uB[i1] * aW.y + uB[i2] * aW.z;\n"
    "  vec4 r1 = uB[i0 + 1] * aW.x + uB[i1 + 1] * aW.y + uB[i2 + 1] * aW.z;\n"
    "  vec4 r2 = uB[i0 + 2] * aW.x + uB[i1 + 2] * aW.y + uB[i2 + 2] * aW.z;\n"
    "  vec4 p = vec4(aPos, 1.0);\n"
    "  vec3 wp = vec3(dot(r0, p), dot(r1, p), dot(r2, p));\n"
    "  vec3 n = vec3(dot(r0.xyz, aNrm), dot(r1.xyz, aNrm), dot(r2.xyz, aNrm));\n"
    "  float ln = length(n); n = ln > 1e-5 ? n / ln : vec3(0.0, 1.0, 0.0);\n"
    "  float d = dot(n, uLdir);\n"
    "  vL = uAmb + uLcol * (max(d, 0.0) * 0.8 + 0.2 * (0.5 + 0.5 * d));\n"   /* the key light, softened round the dark side */
    "  vUv = aUv; gl_Position = uVP * vec4(wp, 1.0);\n"
    "}\n";
static const char *FS_SRC =
    "#ifdef GL_ES\nprecision mediump float;\n#endif\n"
    "uniform sampler2D uMap; uniform float uAref; uniform vec3 uTint;\n"
    "varying vec2 vUv; varying vec3 vL;\n"
    "void main() {\n"
    "  vec4 t = texture2D(uMap, vUv);\n"
    "  if (t.a < uAref) discard;\n"
    "  gl_FragColor = vec4(t.rgb * vL * uTint, t.a);\n"
    "}\n";
static GLuint shader(GLenum kind, const char *src) {
    GLuint s = GL(glCreateShader)(kind); GL(glShaderSource)(s, 1, &src, NULL); GL(glCompileShader)(s); GLint ok = 0; GL(glGetShaderiv)(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[1024]; GL(glGetShaderInfoLog)(s, sizeof log, NULL, log); SDL_Log("band: shader: %s", log); GL(glDeleteShader)(s); return 0; }
    return s;
}
int r3d_band_init(void) {
    memset(&G, 0, sizeof G); keys_init();
    glUniform4fv_ = (void (*)(GLint, GLsizei, const GLfloat *))SDL_GL_GetProcAddress("glUniform4fv"); glDrawArrays_ = (void (*)(GLenum, GLint, GLsizei))SDL_GL_GetProcAddress("glDrawArrays"); if (!glUniform4fv_) { SDL_Log("band: no glUniform4fv"); return -1; }
    GLint mv = 0; while (GL(glGetError)() != GL_NO_ERROR) {} GL(glGetIntegerv)(GL_MAX_VERTEX_UNIFORM_VECTORS, &mv);
    if (GL(glGetError)() != GL_NO_ERROR || mv <= 0) { GLint mc = 0; GL(glGetIntegerv)(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &mc); while (GL(glGetError)() != GL_NO_ERROR) {} mv = mc / 4; }
    if (mv <= 0) mv = 128;   /* GLES2's least */
    G.pal = (mv - 16) / 3; if (G.pal > PAL_MAX) G.pal = PAL_MAX; if (G.pal < 12) { SDL_Log("band: %d vertex uniforms: too few for skinning", mv); return -1; }
    char vs[2600]; snprintf(vs, sizeof vs, VS_FMT, G.pal);
    GLuint v = shader(GL_VERTEX_SHADER, vs), f = shader(GL_FRAGMENT_SHADER, FS_SRC); if (!v || !f) return -1;
    G.prog = GL(glCreateProgram)(); GL(glAttachShader)(G.prog, v); GL(glAttachShader)(G.prog, f); GL(glLinkProgram)(G.prog); GL(glDeleteShader)(v); GL(glDeleteShader)(f);
    GLint ok = 0; GL(glGetProgramiv)(G.prog, GL_LINK_STATUS, &ok); if (!ok) { char log[1024]; GL(glGetProgramInfoLog)(G.prog, sizeof log, NULL, log); SDL_Log("band: link: %s", log); return -1; }
    G.aPos = GL(glGetAttribLocation)(G.prog, "aPos"); G.aNrm = GL(glGetAttribLocation)(G.prog, "aNrm"); G.aUv = GL(glGetAttribLocation)(G.prog, "aUv"); G.aIdx = GL(glGetAttribLocation)(G.prog, "aIdx"); G.aW = GL(glGetAttribLocation)(G.prog, "aW");
#define U(n) G.n = GL(glGetUniformLocation)(G.prog, #n)
    U(uVP); U(uLdir); U(uLcol); U(uAmb); U(uTint); U(uMap); U(uAref); G.uB = GL(glGetUniformLocation)(G.prog, "uB[0]"); if (G.uB < 0) G.uB = GL(glGetUniformLocation)(G.prog, "uB");
#undef U
    { GLuint a = shader(GL_VERTEX_SHADER, SVS), b = shader(GL_FRAGMENT_SHADER, SFS); if (a && b) { G.sprog = GL(glCreateProgram)(); GL(glAttachShader)(G.sprog, a); GL(glAttachShader)(G.sprog, b); GL(glLinkProgram)(G.sprog); GL(glGetProgramiv)(G.sprog, GL_LINK_STATUS, &ok);
        if (!ok) { GL(glDeleteProgram)(G.sprog); G.sprog = 0; } else { G.saPos = GL(glGetAttribLocation)(G.sprog, "aPos"); G.suVP = GL(glGetUniformLocation)(G.sprog, "uVP"); G.suC = GL(glGetUniformLocation)(G.sprog, "uC"); } }
      if (a) GL(glDeleteShader)(a); if (b) GL(glDeleteShader)(b); }
    const char *ext = (const char *)GL(glGetString)(GL_EXTENSIONS); G.s3tc = ext && (strstr(ext, "GL_EXT_texture_compression_s3tc") || (strstr(ext, "GL_EXT_texture_compression_dxt1") && strstr(ext, "GL_ANGLE_texture_compression_dxt"))); if (getenv("R3D_NODXT")) G.s3tc = 0;
    SDL_Log("band: skinning shader up, %d vertex uniforms -> %d bones a draw", mv, G.pal); G.ready = 1; return 0;
}

/* ------------------------------------------------------------------ one member */
typedef struct { int first, count; GLuint tex; int add; float aref; int npal; uint8_t pal[PAL_MAX]; } bdraw;
typedef struct { GLuint vbo, ibo; bdraw *d; int nd; float (*inv)[12]; uint8_t used[NB]; int verts, tris; } gmodel;
typedef struct { const r3d_clip *c; float t, w, wTo, rate, t0; int slot, loop; } act;
enum { S_BODY, S_HAND, S_FRET, S_FACE, S_HIT };
typedef struct { uint32_t key; GLuint id; } btex;
typedef struct { int go; float x, y, z, yaw, head, v; } walk_t;
typedef struct {
    int on, role; const profile *pf; r3d_ske ske; gmodel body, inst; int hasInst;
    r3d_clip *clips; int nclips; btex *tex; int ntex; size_t vram, animBytes;
    uint32_t chord[32], fret[128], drum[128], face[16]; int nface; uint32_t walkFwd, walkBack;
    int bBicep[2], bFore[2], bPalm[2], bIk[2], bNeck, armBone; float a1[2], a2[2];
    act acts[MAXACT]; int nacts;
    uint32_t stance, cycle, played; int curLoop, curIdle, curSpeed, noArms, n, faceN, introN, spWas, lastNote; float faceWait, strokeTo, stroke; walk_t walk;
    float lq[NB][4], lp[NB][3], wq[NB][4], wp[NB][3], skin[NB][12];
    float home[3], homeYaw, pos[3], yaw; float ldir[3], lcol[3], amb[3];
} member;
static member M[BAND_ROLES];
static r3d_bandstats ST;

static GLuint upload_tex(const uint8_t *bank, size_t bankLen, const r3d_texent *ents, int nents, uint32_t key, int clamp, size_t *bytes) {
    const r3d_texent *e = NULL; for (int i = 0; i < nents; i++) if (ents[i].key == key) { e = &ents[i]; break; }
    r3d_dds d; if (!e || e->off + e->size > bankLen || r3d_dds_info(bank + e->off, e->size, &d)) return 0;
    const uint8_t *b = bank + e->off; int l0 = 0; while (l0 + 1 < d.nlev && (d.lev[l0].w > BAND_TEX || d.lev[l0].h > BAND_TEX)) l0++;
    int w = d.lev[l0].w, h = d.lev[l0].h, full = d.lev[d.nlev - 1].w == 1 && d.lev[d.nlev - 1].h == 1, gpu = G.s3tc && d.cc != DDS_RGBA32, mips = 0;
    GLuint id; GL(glGenTextures)(1, &id); GL(glBindTexture)(GL_TEXTURE_2D, id);
    if (gpu) { GLenum fmt = d.cc == DDS_DXT1 ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT : d.cc == DDS_DXT3 ? GL_COMPRESSED_RGBA_S3TC_DXT3_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; int nl = full ? d.nlev - l0 : 1;
        for (int l = 0; l < nl; l++) { GL(glCompressedTexImage2D)(GL_TEXTURE_2D, l, fmt, d.lev[l0 + l].w, d.lev[l0 + l].h, 0, (GLsizei)d.lev[l0 + l].size, b + d.lev[l0 + l].off); *bytes += d.lev[l0 + l].size; } mips = nl > 1; }
    else {   /* decoded: 565 when opaque, 4444 when it has holes (hair, lashes) */
        uint8_t *rgba = malloc((size_t)w * h * 4); int trans = r3d_dds_decode(b, e->size, &d, l0, rgba); uint16_t *p16 = malloc((size_t)w * h * 2); GL(glPixelStorei)(GL_UNPACK_ALIGNMENT, 1);
        if (!trans) { for (int i = 0; i < w * h; i++) p16[i] = (uint16_t)(((rgba[i * 4] >> 3) << 11) | ((rgba[i * 4 + 1] >> 2) << 5) | (rgba[i * 4 + 2] >> 3)); GL(glTexImage2D)(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, p16); }
        else { for (int i = 0; i < w * h; i++) p16[i] = (uint16_t)(((rgba[i * 4] >> 4) << 12) | ((rgba[i * 4 + 1] >> 4) << 8) | ((rgba[i * 4 + 2] >> 4) << 4) | (rgba[i * 4 + 3] >> 4)); GL(glTexImage2D)(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, p16); }
        GL(glPixelStorei)(GL_UNPACK_ALIGNMENT, 4); free(rgba); free(p16); *bytes += (size_t)w * h * 2;
        if (!(w & (w - 1)) && !(h & (h - 1))) { GL(glGenerateMipmap)(GL_TEXTURE_2D); mips = 1; *bytes += (size_t)w * h * 2 / 3; } }
    GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mips ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR); GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp & 1 ? GL_CLAMP_TO_EDGE : GL_REPEAT); GL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp & 2 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    GL(glBindTexture)(GL_TEXTURE_2D, 0); return id;
}
static GLuint member_tex(member *m, const uint8_t *bank, size_t bankLen, const r3d_texent *ents, int nents, uint32_t key, int clamp) {
    for (int i = 0; i < m->ntex; i++) if (m->tex[i].key == key) return m->tex[i].id;
    size_t bytes = 0; GLuint id = upload_tex(bank, bankLen, ents, nents, key, clamp, &bytes); m->vram += bytes;
    m->tex = realloc(m->tex, (m->ntex + 1) * sizeof *m->tex); m->tex[m->ntex].key = key; m->tex[m->ntex].id = id; m->ntex++; return id;
}
/* the world pose of a skeleton at rest, and its inverse (the bind) as 4x3 rows */
static void rest_world(const r3d_ske *s, float (*wq)[4], float (*wp)[3]) {
    for (int i = 0; i < s->n && i < NB; i++) { int p = s->parent[i]; if (p < 0) { memcpy(wq[i], s->q[i], 16); memcpy(wp[i], s->pos[i], 12); continue; }
        r3q_mul(wq[p], s->q[i], wq[i]); float v[3]; r3q_rot(wq[p], s->pos[i], v); for (int c = 0; c < 3; c++) wp[i][c] = wp[p][c] + v[c]; }
}
static void rows_of(const float q[4], const float p[3], float m[12]) {   /* the rotation and the place as three rows (x y z t) */
    float x = q[0], y = q[1], z = q[2], w = q[3];
    m[0] = 1 - 2 * (y * y + z * z); m[1] = 2 * (x * y - w * z); m[2] = 2 * (x * z + w * y); m[3] = p[0];
    m[4] = 2 * (x * y + w * z); m[5] = 1 - 2 * (x * x + z * z); m[6] = 2 * (y * z - w * x); m[7] = p[1];
    m[8] = 2 * (x * z - w * y); m[9] = 2 * (y * z + w * x); m[10] = 1 - 2 * (x * x + y * y); m[11] = p[2];
}
static void rows_inv(const float a[12], float o[12]) { for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) o[r * 4 + c] = a[c * 4 + r]; for (int r = 0; r < 3; r++) o[r * 4 + 3] = -(o[r * 4] * a[3] + o[r * 4 + 1] * a[7] + o[r * 4 + 2] * a[11]); }
static void rows_mul(const float a[12], const float b[12], float o[12]) { float t[12]; for (int r = 0; r < 3; r++) { for (int c = 0; c < 3; c++) t[r * 4 + c] = a[r * 4] * b[c] + a[r * 4 + 1] * b[4 + c] + a[r * 4 + 2] * b[8 + c]; t[r * 4 + 3] = a[r * 4] * b[3] + a[r * 4 + 1] * b[7] + a[r * 4 + 2] * b[11] + a[r * 4 + 3]; } memcpy(o, t, 48); }

typedef struct { float pos[3]; int8_t nrm[4]; float uv[2]; uint8_t idx[4], w[4]; } bvtx;   /* 32 bytes */
/* a model's meshes into one vertex and one index buffer: a draw per mesh, or more when one needs more bones than the palette holds */
static void build_model(member *m, const r3d_skin *sk, const uint8_t *bank, size_t bankLen, const r3d_texent *ents, int nents, const r3d_ske *bind, gmodel *g) {
    memset(g, 0, sizeof *g); int nb = m->ske.n; g->inv = calloc(NB, sizeof *g->inv);
    { float rq[NB][4], rp[NB][3], q2[NB][4], p2[NB][3]; rest_world(&m->ske, rq, rp); if (bind && bind != &m->ske) rest_world(bind, q2, p2);
      for (int i = 0; i < nb; i++) { float r[12]; if (bind && bind != &m->ske && i < bind->n) rows_of(q2[i], p2[i], r); else rows_of(rq[i], rp[i], r); rows_inv(r, g->inv[i]); } }
    bvtx *V = NULL; uint16_t *I = NULL; int nv = 0, ni = 0, capV = 0, capI = 0; int *remap = NULL, remapN = 0;
    for (int mi = 0; mi < sk->nm; mi++) { const r3d_skinmesh *me = &sk->m[mi]; const r3d_skinmat *mat = r3d_skin_mat(sk, me->mat); if (!mat || !mat->ntex) continue;
        int pic = 0; if (mat->ntex > 1) { const r3d_texent *e = NULL; for (int k = 0; k < nents; k++) if (ents[k].key == mat->tex[0]) e = &ents[k];   /* a body lists its surface-detail map first and its picture second */
            if (e) { r3d_look lk = r3d_tex_look(bank + e->off, e->size); if (lk.normal || (lk.b > lk.r + 35 && lk.b > lk.g + 35 && fabsf(lk.r - lk.g) < 25)) pic = 1; } }
        GLuint tex = member_tex(m, bank, bankLen, ents, nents, mat->tex[pic], mat->clamp[pic]); if (!tex) continue;
        int add = mat->blend == 1; float aref = mat->alphaRef > 0 ? mat->alphaRef / 255.0f : 0; if (mat->blend && !add && aref < 0.35f) aref = 0.35f;   /* hair and the like: cut out, not blended */
        if (me->nvert > remapN) { remapN = me->nvert; remap = realloc(remap, remapN * sizeof *remap); }
        int t = 0; while (t < me->ntri) {   /* triangles in order, a new draw whenever the next would need one bone too many */
            bdraw d; memset(&d, 0, sizeof d); d.tex = tex; d.add = add; d.aref = aref; d.first = ni; int slot[NB]; for (int i = 0; i < NB; i++) slot[i] = -1;
            for (int v = 0; v < me->nvert; v++) remap[v] = -1;
            for (; t < me->ntri && nv + 3 <= 65535; t++) { int need[9], nn = 0;
                for (int k = 0; k < 3; k++) { int v = me->idx[t * 3 + k]; for (int j = 0; j < 3; j++) { int bb = me->bone[v * 3 + j]; if (bb >= nb) bb = 0; if (j && me->w[v * 3 + j] <= 0) continue; if (slot[bb] >= 0) continue; int dup = 0; for (int q = 0; q < nn; q++) if (need[q] == bb) dup = 1; if (!dup) need[nn++] = bb; } }
                if (d.npal + nn > G.pal) break;
                for (int q = 0; q < nn; q++) { slot[need[q]] = d.npal; d.pal[d.npal++] = (uint8_t)need[q]; g->used[need[q]] = 1; }
                if (ni + 3 > capI) { capI = capI ? capI * 2 : 4096; I = realloc(I, capI * 2); }
                for (int k = 0; k < 3; k++) { int v = me->idx[t * 3 + k];
                    if (remap[v] < 0) { if (nv == capV) { capV = capV ? capV * 2 : 4096; V = realloc(V, capV * sizeof *V); } bvtx *o = &V[nv]; memcpy(o->pos, me->pos + v * 3, 12);
                        for (int c = 0; c < 3; c++) { float x = me->nrm[v * 3 + c] * 127; o->nrm[c] = (int8_t)(x < -127 ? -127 : x > 127 ? 127 : lrintf(x)); } o->nrm[3] = 0; o->uv[0] = me->uv[v * 2]; o->uv[1] = me->uv[v * 2 + 1];
                        int q0 = (int)lrintf(me->w[v * 3] * 255), q1 = (int)lrintf(me->w[v * 3 + 1] * 255), q2 = 255 - q0 - q1; if (q2 < 0) { q1 += q2; q2 = 0; } if (q1 < 0) { q0 += q1; q1 = 0; }
                        if (me->w[v * 3 + 2] <= 0) { q0 += q2; q2 = 0; }
                        o->w[0] = (uint8_t)q0; o->w[1] = (uint8_t)q1; o->w[2] = (uint8_t)q2; o->w[3] = 0;
                        for (int j = 0; j < 3; j++) { int bb = me->bone[v * 3 + j]; if (bb >= nb) bb = 0; int s = slot[bb] >= 0 ? slot[bb] : 0; o->idx[j] = (uint8_t)(s * 3); } o->idx[3] = 0;
                        remap[v] = nv++; }
                    I[ni++] = (uint16_t)remap[v]; } }
            d.count = ni - d.first; if (d.count) { g->d = realloc(g->d, (g->nd + 1) * sizeof *g->d); g->d[g->nd++] = d; } else break; } }
    for (int i = 1; i < g->nd; i++) for (int j = i; j > 0 && g->d[j].add < g->d[j - 1].add; j--) { bdraw t = g->d[j]; g->d[j] = g->d[j - 1]; g->d[j - 1] = t; }   /* added light last */
    if (nv) { GL(glGenBuffers)(1, &g->vbo); GL(glBindBuffer)(GL_ARRAY_BUFFER, g->vbo); GL(glBufferData)(GL_ARRAY_BUFFER, (GLsizeiptr)nv * sizeof *V, V, GL_STATIC_DRAW);
        GL(glGenBuffers)(1, &g->ibo); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, g->ibo); GL(glBufferData)(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)ni * 2, I, GL_STATIC_DRAW);
        GL(glBindBuffer)(GL_ARRAY_BUFFER, 0); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, 0); m->vram += (size_t)nv * sizeof *V + (size_t)ni * 2; }
    g->verts = nv; g->tris = ni / 3; free(V); free(I); free(remap);
}
static void model_free(gmodel *g) { if (g->vbo) GL(glDeleteBuffers)(1, &g->vbo); if (g->ibo) GL(glDeleteBuffers)(1, &g->ibo); free(g->d); free(g->inv); memset(g, 0, sizeof *g); }
static void member_free(member *m) {
    if (!m->on) return; model_free(&m->body); model_free(&m->inst); for (int i = 0; i < m->ntex; i++) if (m->tex[i].id) GL(glDeleteTextures)(1, &m->tex[i].id); free(m->tex);
    for (int i = 0; i < m->nclips; i++) r3d_clip_free(&m->clips[i]); free(m->clips); r3d_ske_free(&m->ske); memset(m, 0, sizeof *m);
}
static int clip_cmp(const void *a, const void *b) { uint32_t x = ((const r3d_clip *)a)->key, y = ((const r3d_clip *)b)->key; return x < y ? -1 : x > y; }
/* the song's own animations (the singer's mouth for the words), by key */
static struct { char name[64]; int loaded; struct { int t; uint32_t scr, who, anim, stance; uint32_t flags[6]; int nflags; char node[32]; } *ev; int nev; int (*drums)[3]; int ndr; int (*an)[3]; int nan; int32_t *fb; int nfb; r3d_clip *faces; int nfaces; } SG;
static const r3d_clip *clip_of(const member *m, uint32_t key) {
    int lo = 0, hi = m->nclips - 1; while (lo <= hi) { int mid = (lo + hi) >> 1; if (m->clips[mid].key == key) return &m->clips[mid]; if (m->clips[mid].key < key) lo = mid + 1; else hi = mid - 1; }
    for (int i = 0; i < SG.nfaces; i++) if (SG.faces[i].key == key && SG.faces[i].nb <= m->ske.n + 4) return &SG.faces[i];
    return NULL;
}
static void want_keys(const qbv *v, uint32_t **keys, int *n, int *cap, int depth) {   /* every animation a list names */
    if (!v || depth > 12) return; uint32_t k;
    if (v->kind == QB_ARRAY) { for (int i = 0; i < v->narr; i++) want_keys(v->arr[i], keys, n, cap, depth + 1); return; }
    if (v->kind == QB_STRUCT) { for (int i = 0; i < v->nitems; i++) want_keys(v->items[i].v, keys, n, cap, depth + 1); return; }
    if (qb_keyof(v, &k)) { for (int i = 0; i < *n; i++) if ((*keys)[i] == k) return; if (*n == *cap) { *cap = *cap ? *cap * 2 : 256; *keys = realloc(*keys, *cap * 4); } (*keys)[(*n)++] = k; }
}
static void add_key(uint32_t k, uint32_t **keys, int *n, int *cap) { if (!k) return; qbv v; memset(&v, 0, sizeof v); v.kind = QB_KEY; v.key = k; want_keys(&v, keys, n, cap, 0); }
static int member_load(member *m, int role, const profile *pf) {
    member_free(m); memset(m, 0, sizeof *m); m->role = role; m->pf = pf; Uint32 t0 = SDL_GetTicks();
    if (ske_load(pf->ske, &m->ske) || m->ske.n > NB) { SDL_Log("band: %s: no skeleton %08x", pf->name, pf->ske); r3d_ske_free(&m->ske); return -1; }
    char fn[96]; snprintf(fn, sizeof fn, "%s.pak.xen", pf->body); const char *bp = app_data_path("pak", fn); r3d_pakfile body, inst, anims; int hasAnims = 0;
    if (!bp || r3d_pak_open(&body, bp, NULL)) { SDL_Log("band: %s: no PAK/%s", pf->name, fn); r3d_ske_free(&m->ske); return -1; }
    m->on = 1;
    /* the body and its instrument */
    for (int pass = 0; pass < 2; pass++) { r3d_pakfile *pk = &body; if (pass) { if (!pf->inst[0]) break; snprintf(fn, sizeof fn, "%s.pak.xen", pf->inst); const char *ip = app_data_path("pak", fn); if (!ip || r3d_pak_open(&inst, ip, NULL)) break; pk = &inst; }
        size_t sn = 0, tn = 0; uint8_t *sb = r3d_pak_read(pk, r3d_pak_biggest(pk, K_SKIN), &sn), *tb = r3d_pak_read(pk, r3d_pak_biggest(pk, K_TEX), &tn); r3d_skin sk; r3d_texent *te = NULL; int nt = tb ? r3d_texbank_read(tb, tn, &te) : 0;
        if (sb && r3d_skin_read(sb, sn, &sk) == 0) { const r3d_ske *bind = pass && (role == BAND_GUITARIST || role == BAND_BASSIST) && F.hasGuitar ? &F.guitar : &m->ske; build_model(m, &sk, tb, tn, te, nt, bind, pass ? &m->inst : &m->body); if (pass) m->hasInst = 1; r3d_skin_free(&sk); }
        free(sb); free(tb); free(te); if (pass) r3d_pak_close(&inst); }
    /* what it may play: every animation its lists name */
    const qbv *set = sec(pf->set), *fing = sec(pf->fingers), *fret = sec(pf->frets); uint32_t *keys = NULL; int nk = 0, ck = 0; want_keys(set, &keys, &nk, &ck, 0);
    if (fing) { static const char *COL[5] = { "green", "red", "yellow", "blue", "orange" }; for (int mask = 1; mask < 32; mask++) { char nm[64] = ""; for (int i = 0; i < 5; i++) if (mask & (1 << i)) { if (nm[0]) strcat(nm, "_"); strcat(nm, COL[i]); } uint32_t k; if (qb_keyof(qsub(fing, gh3_qbkey(nm)), &k)) { m->chord[mask] = k; add_key(k, &keys, &nk, &ck); } } }
    if (fret) for (int nt = 101; nt <= 127; nt++) { char nm[16]; snprintf(nm, sizeof nm, "track_%d", nt); uint32_t k; if (qb_keyof(qsub(fret, gh3_qbkey(nm)), &k)) { m->fret[nt] = k; add_key(k, &keys, &nk, &ck); } }
    if (role == BAND_DRUMMER) { const qbv *da = sec(gh3_qbkey("drummer_anims")), *cy = sec(gh3_qbkey("cymbal_anims")); uint32_t k[8];
        for (int nt = 36; nt <= 59; nt++) { char nm[16]; snprintf(nm, sizeof nm, "track_%d", nt); if (qkeys(qsub(da, gh3_qbkey(nm)), k, 1)) m->drum[nt] = k[0]; }
        if (!m->drum[47] && qb_keyof(qsub(da, gh3_qbkey("sticktap_l")), &k[0])) m->drum[47] = k[0]; if (!m->drum[59] && qb_keyof(qsub(da, gh3_qbkey("sticktap_r")), &k[0])) m->drum[59] = k[0];
        int nc = qkeys(cy, k, 8); static const int CY[5][2] = { { 68, 0 }, { 69, 1 }, { 67, 2 }, { 65, 3 }, { 66, 3 } }; for (int i = 0; i < 5; i++) if (CY[i][1] < nc) m->drum[CY[i][0]] = k[CY[i][1]];   /* the cymbals and the hi-hat swing when struck */
        for (int nt = 0; nt < 128; nt++) add_key(m->drum[nt], &keys, &nk, &ck); }
    m->nface = qkeys(qsub(qsub(set, K_FACIAL), K_IDLE), m->face, 16);
    { const qbv *w = qsub(set, K_WALKING); uint32_t k[2]; if (qkeys(qsub(qsub(qsub(w, gh3_qbkey("forward")), gh3_qbkey("cycle_anim")), K_MED), k, 1)) m->walkFwd = k[0]; if (qkeys(qsub(qsub(qsub(w, gh3_qbkey("backward")), gh3_qbkey("cycle_anim")), K_MED), k, 1)) m->walkBack = k[0]; }
    snprintf(fn, sizeof fn, "%s.pak.xen", pf->anims); const char *ap = pf->anims[0] ? app_data_path("pak", fn) : NULL; hasAnims = ap && r3d_pak_open(&anims, ap, NULL) == 0;
    m->clips = calloc(nk ? nk : 1, sizeof *m->clips);
    for (int i = 0; i < nk; i++) { r3d_pakfile *where[3] = { hasAnims ? &anims : NULL, &body, F.hasPerm ? &F.perm : NULL };   /* its own file, then its body's, then the shared one */
        for (int w = 0; w < 3; w++) { if (!where[w]) continue; int e = r3d_pak_find(where[w], K_SKA, keys[i]); if (e < 0) continue; size_t n; uint8_t *b = r3d_pak_read(where[w], e, &n); if (!b) continue;
            r3d_clip c; if (r3d_ska_read(b, n, keys[i], &c) == 0) { if (c.nb > m->ske.n + 4) r3d_clip_free(&c); else { m->clips[m->nclips++] = c; m->animBytes += r3d_clip_bytes(&c); } } free(b); break; } }
    qsort(m->clips, m->nclips, sizeof *m->clips, clip_cmp);
    if (role == BAND_DRUMMER) for (int nt = 0; nt < 68; nt++) if (m->drum[nt] && nt < 65) { r3d_clip *c = (r3d_clip *)clip_of(m, m->drum[nt]); if (c && c->land == 0.13f) r3d_clip_land(c, &m->ske); }
    if (hasAnims) r3d_pak_close(&anims); r3d_pak_close(&body); free(keys);
    /* the bones the arms reach with */
    static const char *SIDE[2] = { "l", "r" }; char nm[48];
    for (int s = 0; s < 2; s++) { snprintf(nm, sizeof nm, "bone_bicep_%s", SIDE[s]); m->bBicep[s] = r3d_ske_bone(&m->ske, nm); snprintf(nm, sizeof nm, "bone_forearm_%s", SIDE[s]); m->bFore[s] = r3d_ske_bone(&m->ske, nm);
        snprintf(nm, sizeof nm, "bone_palm_%s", SIDE[s]); m->bPalm[s] = r3d_ske_bone(&m->ske, nm); snprintf(nm, sizeof nm, "bone_ik_hand_guitar_%s", SIDE[s]); m->bIk[s] = r3d_ske_bone(&m->ske, nm);
        if (m->bFore[s] >= 0) { const float *p = m->ske.pos[m->bFore[s]]; m->a1[s] = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]); } if (m->bPalm[s] >= 0) { const float *p = m->ske.pos[m->bPalm[s]]; m->a2[s] = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]); } }
    m->bNeck = r3d_ske_bone(&m->ske, "bone_guitar_neck"); m->armBone = m->bBicep[0];
    m->stance = pf->stance; m->cycle = K_IDLE; m->strokeTo = m->stroke = 0.028f; m->lastNote = -1;
    for (int i = 0; i < m->ske.n; i++) { memcpy(m->lq[i], m->ske.q[i], 16); memcpy(m->lp[i], m->ske.pos[i], 12); }
    SDL_Log("band: %s (%s): %d+%d points, %d+%d triangles, %d+%d draws, %d textures, %.1f MB on the GPU; %d of %d animations (%.1f MB), %u ms", pf->name, ROLE[role], m->body.verts, m->inst.verts, m->body.tris, m->inst.tris, m->body.nd, m->inst.nd, m->ntex, m->vram / 1048576.0, m->nclips, nk, m->animBytes / 1048576.0, SDL_GetTicks() - t0);
    return 0;
}

/* ------------------------------------------------------------------ the venue: places and lights */
typedef struct { float pos[3], yaw; int has; } wpt;
static struct { char zone[32]; wpt role[BAND_ROLES]; struct { uint32_t key; wpt p; } *pts; int npts; r3d_light *lights; int nlights; } Z;
static void light_member(member *m);
static void place_all(void) {
    for (int r = 0; r < BAND_ROLES; r++) { member *m = &M[r]; if (!m->on) continue; const wpt *w = &Z.role[r]; float p[3] = { 0, 0, 0 }, yaw = 0; if (w->has) { memcpy(p, w->pos, 12); yaw = w->yaw; }
        memcpy(m->home, p, 12); m->homeYaw = yaw; memcpy(m->pos, p, 12); m->yaw = yaw; m->walk.go = 0; light_member(m); }
}
void r3d_band_zone(const char *zone, const gh3_pakent *ents, int n, const r3d_light *lights, int nlights) {
    keys_init(); free(Z.pts); free(Z.lights); memset(&Z, 0, sizeof Z); snprintf(Z.zone, sizeof Z.zone, "%s", zone);
    uint32_t want[BAND_ROLES], K_NQB = gh3_qbkey(".nqb"), K_NAME = gh3_qbkey("name"), K_POS = gh3_qbkey("pos"), K_ANG = gh3_qbkey("angles"), K_CLASS = gh3_qbkey("class"), K_WAY = gh3_qbkey("waypoint");
    for (int r = 0; r < BAND_ROLES; r++) { char nm[96]; snprintf(nm, sizeof nm, "%s_trg_waypoint_%s_start", zone, ROLE[r]); want[r] = gh3_qbkey(nm); }
    for (int e = 0; e < n; e++) { if ((ents[e].type != K_QB && ents[e].type != K_NQB) || !ents[e].data) continue; qbsection *s; int ns = gh3_qb_parse(ents[e].data, ents[e].len, &s); if (ns < 0) continue;
        for (int k = 0; k < ns; k++) { const qbv *v = s[k].v; if (!v || v->kind != QB_ARRAY) continue;
            for (int i = 0; i < v->narr; i++) { const qbv *st = v->arr[i]; if (!st || st->kind != QB_STRUCT) continue; uint32_t nk, ck = 0; if (!qb_keyof(qb_getk(st, K_NAME), &nk)) continue;
                const qbv *pv = qb_getk(st, K_POS), *av = qb_getk(st, K_ANG); if (!pv || pv->kind != QB_VEC3) continue; qb_keyof(qb_getk(st, K_CLASS), &ck);
                wpt w; memcpy(w.pos, pv->f, 12); w.yaw = av && av->kind == QB_VEC3 ? av->f[1] : 0; w.has = 1;
                for (int r = 0; r < BAND_ROLES; r++) if (nk == want[r]) Z.role[r] = w;
                if (ck == K_WAY) { Z.pts = realloc(Z.pts, (Z.npts + 1) * sizeof *Z.pts); Z.pts[Z.npts].key = nk; Z.pts[Z.npts].p = w; Z.npts++; } } }
        gh3_qb_free_sections(s, ns); }
    for (int i = 0; i < nlights; i++) if (lights[i].band) { Z.lights = realloc(Z.lights, (Z.nlights + 1) * sizeof *Z.lights); Z.lights[Z.nlights++] = lights[i]; }
    SDL_Log("band: %s: places %s%s%s%s, %d waypoints, %d band lights", zone, Z.role[0].has ? "G" : "-", Z.role[1].has ? "B" : "-", Z.role[2].has ? "D" : "-", Z.role[3].has ? "V" : "-", Z.npts, Z.nlights);
    place_all();
}
/* one key light and the ambient at the member's chest, from the venue's band lights (as light_at does for the room), as the light show has
 * them now (r3d_lights_get: the presets' colour and strength, cross-faded as the room's, the strobe's flash), else as the venue sets them.
 * Every frame (r3d_band_draw): the show moves them and the member walks */
static void light_member(member *m) {
    float P[3] = { m->pos[0], m->pos[1] + 1.2f, m->pos[2] }, amb[3] = { 0, 0, 0 }, best = -1, key[3] = { 0, 0, 0 }, dir[3] = { 0, 1, 0 }, fill[3] = { 0, 0, 0 };
    for (int i = 0; i < Z.nlights; i++) { r3d_light lv = Z.lights[i]; const r3d_light *l = &lv; float sv[6]; if (r3d_lights_get(lv.key, sv)) { memcpy(lv.colour, sv, 12); lv.intensity = sv[3]; lv.far = sv[4]; lv.near = sv[5]; }
        float dx = l->pos[0] - P[0], dy = l->pos[1] - P[1], dz = l->pos[2] - P[2], dd = sqrtf(dx * dx + dy * dy + dz * dz);
        float far = l->far > 0.01f ? l->far : 0.01f, nr = l->near < far * 0.999f ? (l->near > 0 ? l->near : 0) : far * 0.999f, att = (far - dd) / (far - nr); if (att > 1) att = 1; if (att <= 0) continue;
        float k = l->intensity * att, c[3] = { l->colour[0] * k, l->colour[1] * k, l->colour[2] * k };
        if (l->kind == L_AMBIENT) { for (int j = 0; j < 3; j++) amb[j] += c[j]; continue; }
        float d[3]; if (l->kind == L_DIR) { d[0] = -l->dir[0]; d[1] = -l->dir[1]; d[2] = -l->dir[2]; } else { float q = dd > 1e-4f ? dd : 1; d[0] = dx / q; d[1] = dy / q; d[2] = dz / q; }
        float w = fmaxf(c[0], fmaxf(c[1], c[2])); if (w > best) { if (best >= 0) for (int j = 0; j < 3; j++) fill[j] += key[j]; best = w; memcpy(key, c, 12); memcpy(dir, d, 12); } else for (int j = 0; j < 3; j++) fill[j] += c[j]; }
    if (best < 0.05f) {   /* a venue with no light for the band here: from the front and above, warm, as the game's default */
        float a = m->yaw; dir[0] = sinf(a) * 0.6f; dir[1] = 0.7f; dir[2] = cosf(a) * 0.6f; key[0] = 0.85f; key[1] = 0.75f; key[2] = 0.62f; }
    float L = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]); if (L < 1e-5f) L = 1;
    for (int j = 0; j < 3; j++) { m->ldir[j] = dir[j] / L; float kk = key[j] > 1.6f ? 1.6f : key[j]; m->lcol[j] = kk; m->amb[j] = fminf(0.9f, 0.16f + amb[j] * 0.8f + fill[j] * 0.3f); }
}
static int shown(int r) { return G.ready && SET.band3d && M[r].on && Z.role[r].has && !(A.screen == SC_PLAY && A.mode == MODE_PRACTICE); }   /* the game leaves out a member with no place in the venue, and the whole band in practice */
int r3d_band_where(int role, float pos[3], float *yaw) {
    if (role < 0) { for (int r = 0; r < BAND_ROLES; r++) if (shown(r)) return 1; return 0; }
    if (role >= BAND_ROLES || !shown(role)) return 0; if (pos) memcpy(pos, M[role].pos, 12); if (yaw) *yaw = M[role].yaw; return 1;
}

/* ------------------------------------------------------------------ the song's cues (<song>_song.pak) */
static void song_free(void) {
    for (int r = 0; r < BAND_ROLES; r++) for (int i = 0; i < M[r].nacts; ) { const r3d_clip *c = M[r].acts[i].c; if (SG.nfaces && c >= SG.faces && c < SG.faces + SG.nfaces) M[r].acts[i] = M[r].acts[--M[r].nacts]; else i++; }   /* the old song's faces stop */
    free(SG.ev); free(SG.drums); free(SG.an); free(SG.fb); for (int i = 0; i < SG.nfaces; i++) r3d_clip_free(&SG.faces[i]); free(SG.faces); memset(&SG, 0, sizeof SG); }
static int cmp3(const void *a, const void *b) { return ((const int *)a)[0] - ((const int *)b)[0]; }
static int track3(const qbsection *s, int ns, const char *name, const char *suf, int (**out)[3]) {
    char nm[128]; snprintf(nm, sizeof nm, "%s%s", name, suf); uint32_t k = gh3_qbkey(nm); *out = NULL;
    for (int i = 0; i < ns; i++) if (s[i].id == k && s[i].v && s[i].v->kind == QB_ARRAY) { const qbv *a = s[i].v; int n = 0; *out = calloc(a->narr ? a->narr : 1, sizeof **out);
        for (int j = 0; j < a->narr; j++) { const qbv *e = a->arr[j]; if (!e || e->kind != QB_ARRAY || e->narr < 2) continue; for (int c = 0; c < 3 && c < e->narr; c++) (*out)[n][c] = (int)qb_num(e->arr[c], 0); n++; }
        qsort(*out, n, sizeof **out, cmp3); return n; }
    return 0;
}
static void song_load(const char *name) {
    if (SG.loaded && !strcmp(SG.name, name)) return; song_free(); snprintf(SG.name, sizeof SG.name, "%s", name); SG.loaded = 1; if (!name[0]) return;
    char fn[128]; snprintf(fn, sizeof fn, "%s_song.pak.xen", name); size_t len; uint8_t *pak = app_read_data("songs", fn, &len); if (!pak) return;
    gh3_pakent *ents; int ne = gh3_pak_parse(pak, len, NULL, 0, &ents); char nm[128]; snprintf(nm, sizeof nm, "%s_fretbars", name); uint32_t kfb = gh3_qbkey(nm);
    qbsection *best = NULL; int bestn = 0;
    for (int i = 0; i < ne; i++) { if (ents[i].type != K_QB || !ents[i].data) continue; qbsection *s; int ns = gh3_qb_parse(ents[i].data, ents[i].len, &s); if (ns < 0) continue;
        int has = 0; for (int k = 0; k < ns; k++) if (s[k].id == kfb) has = 1;
        if (has || !best || ns > bestn) { if (best) gh3_qb_free_sections(best, bestn); best = s; bestn = ns; if (has) break; } else gh3_qb_free_sections(s, ns); }
    if (best) {
        SG.ndr = track3(best, bestn, name, "_drums_notes", &SG.drums); SG.nan = track3(best, bestn, name, "_anim_notes", &SG.an);
        for (int i = 0; i < bestn; i++) if (best[i].id == kfb && best[i].v && best[i].v->kind == QB_ARRAY) { const qbv *a = best[i].v; SG.fb = malloc(a->narr * 4 + 4); for (int j = 0; j < a->narr; j++) SG.fb[j] = (int32_t)qb_num(a->arr[j], 0); SG.nfb = a->narr; }
        snprintf(nm, sizeof nm, "%s_performance", name); uint32_t kp = gh3_qbkey(nm), K_TIME = gh3_qbkey("time"), K_SCR = gh3_qbkey("scr"), K_PAR = gh3_qbkey("params"), K_NAME = gh3_qbkey("name"), K_ANIM = gh3_qbkey("anim"), K_ST = gh3_qbkey("stance"), K_NODE = gh3_qbkey("node");
        for (int i = 0; i < bestn; i++) if (best[i].id == kp && best[i].v && best[i].v->kind == QB_ARRAY) { const qbv *a = best[i].v; SG.ev = calloc(a->narr ? a->narr : 1, sizeof *SG.ev);
            for (int j = 0; j < a->narr; j++) { const qbv *e = a->arr[j], *t = qb_getk(e, K_TIME), *p = qb_getk(e, K_PAR); if (!t || !qb_keyof(qb_getk(e, K_SCR), &SG.ev[SG.nev].scr)) continue;
                SG.ev[SG.nev].t = (int)qb_num(t, 0); qb_keyof(qb_getk(p, K_NAME), &SG.ev[SG.nev].who); qb_keyof(qb_getk(p, K_ANIM), &SG.ev[SG.nev].anim); qb_keyof(qb_getk(p, K_ST), &SG.ev[SG.nev].stance);
                const qbv *nd = qb_getk(p, K_NODE); if (nd && nd->kind == QB_STR && nd->s) { snprintf(SG.ev[SG.nev].node, sizeof SG.ev[SG.nev].node, "%s", nd->s); for (char *c = SG.ev[SG.nev].node; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32; }
                if (p && p->kind == QB_STRUCT) for (int k = 0; k < p->nitems && SG.ev[SG.nev].nflags < 6; k++) { uint32_t fk; if (p->items[k].id == 0 && qb_keyof(p->items[k].v, &fk)) SG.ev[SG.nev].flags[SG.ev[SG.nev].nflags++] = fk; }
                SG.nev++; }
            for (int x = 1; x < SG.nev; x++) for (int y = x; y > 0 && SG.ev[y].t < SG.ev[y - 1].t; y--) { __typeof__(*SG.ev) tmp = SG.ev[y]; SG.ev[y] = SG.ev[y - 1]; SG.ev[y - 1] = tmp; } }
        gh3_qb_free_sections(best, bestn); }
    for (int i = 0; i < ne; i++) if (ents[i].type == K_SKA && ents[i].data) { int used = 0; for (int k = 0; k < SG.nev; k++) if (SG.ev[k].scr == K_EV_FACE && SG.ev[k].anim == ents[i].fullKey) used = 1; if (!used) continue;   /* the singer's mouth for the words */
        SG.faces = realloc(SG.faces, (SG.nfaces + 1) * sizeof *SG.faces); if (r3d_ska_read(ents[i].data, ents[i].len, ents[i].fullKey, &SG.faces[SG.nfaces]) == 0) SG.nfaces++; }
    free(ents); free(pak);
    SDL_Log("band: %s: %d events, %d drum notes, %d hand cues, %d face clips", name, SG.nev, SG.ndr, SG.nan, SG.nfaces);
    if (dbg()) for (int i = 0; i < SG.nev; i++) printf("band: event %6d ms %08x who %08x anim %08x stance %08x node '%s' flags %d\n", SG.ev[i].t, SG.ev[i].scr, SG.ev[i].who, SG.ev[i].anim, SG.ev[i].stance, SG.ev[i].node, SG.ev[i].nflags);
}

/* ------------------------------------------------------------------ who plays */
static const profile *guitarist(void) {   /* the player's rocker, outfit and guitar from the character hub (charsel.c; R3D_BAND_GUITARIST=Name overrides the rocker); Axel when missing, as the game falls back */
    static profile **made; static int nmade; const char *e = getenv("R3D_BAND_GUITARIST"); const profile *p = profile_named(e && *e ? e : charsel_rocker(), BAND_GUITARIST); if (!p) p = profile_named("Axel", -1); if (!p) return NULL;
    profile q = *p; char body[48], anims[48], inst[48]; charsel_paks(q.name, body, anims, inst, sizeof body);
    if (*body) { snprintf(q.body, sizeof q.body, "%s", body); snprintf(q.anims, sizeof q.anims, "%s", anims); } if (*inst) snprintf(q.inst, sizeof q.inst, "%s", inst);
    for (int i = 0; i < nmade; i++) if (!memcmp(made[i], &q, sizeof q)) return made[i];   /* one copy per combination: the same pointer while nothing changes */
    made = realloc(made, (nmade + 1) * sizeof *made); made[nmade] = malloc(sizeof q); *made[nmade] = q; return made[nmade++];
}
void r3d_band_select(void) {
    if (!G.ready || !SET.band3d) return; Uint32 t0 = SDL_GetTicks(); keys_init();
    if (!lib_load()) return; files_open(); if (!F.hasGlobal) { SDL_Log("band: no ZONES/global.pak + global.pab: no skeletons"); return; }
    const profile *want[BAND_ROLES] = { NULL }; want[BAND_GUITARIST] = guitarist();
    for (int r = 1; r < BAND_ROLES; r++) for (int i = 0; i < L.npf && !want[r]; i++) if (L.pf[i].role == r) want[r] = &L.pf[i];
    if (SET.bretMichaels) { static int said; const profile *b = profile_named("Singer_Bret", BAND_VOCALIST); char fn[96] = "";   /* the cheat (Cheat_BretMichaels: "singer_bret" for the singer) */
        if (b) snprintf(fn, sizeof fn, "%s.pak.xen", b->body); if (b && app_data_path("pak", fn)) want[BAND_VOCALIST] = b; else if (!said++) SDL_Log("band: Bret Michaels: no PAK/%s in this copy of the game, the usual singer sings", b ? fn : "(Singer_Bret profile)"); }
    int si = A.selSong; const char *song = "";
    if (A.lib && si >= 0 && si < A.lib->nsongs) { song = A.lib->songs[si].name;
        if (battle_song(si)) { const qbv *bp = sec(A.lib->songs[si].bossKey); uint32_t ck; const profile *b = qb_keyof(qb_get(bp, "character_profile"), &ck) ? profile_key(ck) : NULL; if (b) want[BAND_BASSIST] = b; } }   /* the boss in the bassist's place */
    for (int r = 0; r < BAND_ROLES; r++) { member *m = &M[r]; if (m->on && m->pf == want[r]) continue; member_free(m); if (want[r] && member_load(m, r, want[r])) member_free(m);
        if (!m->on && r == BAND_VOCALIST && want[r] && strcasecmp(want[r]->name, "Singer")) { const profile *s = profile_named("Singer", BAND_VOCALIST); if (s && !member_load(m, r, s)) continue; member_free(m); } }   /* (a singer that would not load: the usual one) */
    song_load(song); place_all();
    memset(&ST, 0, sizeof ST); for (int r = 0; r < BAND_ROLES; r++) if (M[r].on) { ST.members++; snprintf(ST.who[r], sizeof ST.who[r], "%s", M[r].pf->name); ST.vram += M[r].vram; ST.animBytes += M[r].animBytes; ST.clips += M[r].nclips; ST.verts += M[r].body.verts + M[r].inst.verts; }
    ST.pal = G.pal; ST.loadMs = SDL_GetTicks() - t0;
    SDL_Log("band: %s / %s / %s / %s, %.1f MB on the GPU, %.1f MB of animations, %.0f ms", ST.who[0], ST.who[1], ST.who[2], ST.who[3], ST.vram / 1048576.0, ST.animBytes / 1048576.0, ST.loadMs);
}

/* ------------------------------------------------------------------ the mixer (three.js AnimationMixer, as the browser port uses it) */
static act *act_add(member *m, const r3d_clip *c, int slot, float fade, int loop, float t0) {
    int had = 0; for (int i = 0; i < m->nacts; i++) { act *a = &m->acts[i]; if (a->slot != slot || slot == S_HIT || a->wTo <= 0) continue; had = 1; a->wTo = 0; a->rate = fade > 0 ? a->w / fade : 1e9f; }
    if (dbg() && slot != S_BODY) { int nr = 0; for (int i = 0; i < c->nb; i++) nr += c->b[i].nr > 0; printf("band: %s layer %d %08x (%.2f s, %d bones with turns, arm %d, delta %d)\n", m->pf->name, slot, c->key, c->dur, nr, m->armBone >= 0 && m->armBone < c->nb ? c->b[m->armBone].nr : -1, c->delta); }
    if (m->nacts == MAXACT) { int lo = 0; for (int i = 1; i < m->nacts; i++) if (m->acts[i].w < m->acts[lo].w) lo = i; m->acts[lo] = m->acts[--m->nacts]; }
    act *a = &m->acts[m->nacts++]; memset(a, 0, sizeof *a); a->c = c; a->slot = slot; a->loop = loop; a->t = a->t0 = t0;
    if (slot == S_HIT) { a->w = 0; a->wTo = 0; } else if (had && fade > 0) { a->w = 0; a->wTo = 1; a->rate = 1 / fade; } else { a->w = a->wTo = 1; }
    return a;
}
static act *cur_body(member *m) { act *c = NULL; for (int i = 0; i < m->nacts; i++) if (m->acts[i].slot == S_BODY && m->acts[i].wTo > 0) c = &m->acts[i]; return c; }
static act *cur_slot(member *m, int slot) { act *c = NULL; for (int i = 0; i < m->nacts; i++) if (m->acts[i].slot == slot && m->acts[i].wTo > 0) c = &m->acts[i]; return c; }
static void mixer_update(member *m, float dt) {
    for (int i = 0; i < m->nacts; i++) { act *a = &m->acts[i]; float d = a->c->dur;
        if (a->slot != S_HIT) { if (a->w < a->wTo) { a->w += a->rate * dt; if (a->w > a->wTo) a->w = a->wTo; } else if (a->w > a->wTo) { a->w -= a->rate * dt; if (a->w < a->wTo) a->w = a->wTo; } }
        a->t += dt; if (a->loop) { if (a->t >= d) a->t = fmodf(a->t, d); } else if (a->t > d) a->t = d; }
    for (int i = 0; i < m->nacts; ) { act *a = &m->acts[i]; if ((a->slot != S_HIT && a->w <= 0 && a->wTo <= 0) || (a->slot == S_HIT && a->t >= a->c->dur - 0.005f)) m->acts[i] = m->acts[--m->nacts]; else i++; }
}

/* ------------------------------------------------------------------ the member's state (bandMember) */
static int anim_keys(const qbv *kind, int mood, int speed, uint32_t *out, int max) {
    static uint32_t *MOOD[3] = { &K_BAD, &K_NORMAL, &K_GOOD }, *SPEED[3] = { &K_SLOW, &K_MED, &K_FAST };
    const qbv *md = qsub(kind, *MOOD[mood]); if (!md) md = qsub(kind, K_NORMAL); if (!md) md = qsub(kind, K_GOOD); if (!md) return 0;
    const qbv *l = qsub(md, *SPEED[speed]); if (!l) l = qsub(md, K_MED); return qkeys(l, out, max);
}
static const qbv *set_of(const member *m, uint32_t st) { return qsub(sec(m->pf->set), st); }
static const qbv *any_stance(const member *m) { const qbv *s = set_of(m, m->pf->stance); if (!s) s = set_of(m, K_STANCE_A); const qbv *all = sec(m->pf->set); if (!s && all && all->nitems) s = all->items[0].v; return s; }
static int keys_for(member *m, uint32_t kind, int mood, int speed, uint32_t *out) {
    uint32_t l[16]; int n = anim_keys(qsub(set_of(m, m->stance), kind), mood, speed, l, 16), k = 0; for (int i = 0; i < n; i++) if (clip_of(m, l[i])) out[k++] = l[i];
    if (!k && m->stance != m->pf->stance) { n = anim_keys(qsub(any_stance(m), kind), mood, speed, l, 16); for (int i = 0; i < n; i++) if (clip_of(m, l[i])) out[k++] = l[i]; }
    return k;
}
static void begin(member *m, const r3d_clip *c, float fade, int loop, int speed) { act_add(m, c, S_BODY, fade, loop, 0);
    if (dbg()) { int nd = 0; for (int i = 0; i < c->nb; i++) nd += c->driven[i]; printf("band: %s plays %08x (%.2f s, %d bones moved, arm %d, stance %08x)\n", m->pf->name, c->key, c->dur, nd, m->armBone >= 0 && m->armBone < c->nb ? c->driven[m->armBone] : -1, m->stance); } m->curLoop = loop; m->curIdle = 0; m->curSpeed = speed; m->noArms = 0; }
static int play(member *m, uint32_t kind, int mood, int speed) {
    uint32_t l[16]; int n = keys_for(m, kind, mood, speed, l); if (!n) return 0; const r3d_clip *c = clip_of(m, l[(m->n++) % n]); if (!c) return 0;
    begin(m, c, FADE, 0, speed); m->curIdle = kind == K_IDLE; m->played = kind; return 1;
}
static void next(member *m, int mood, int speed) { if (!play(m, m->cycle, mood, speed)) { m->cycle = K_IDLE; play(m, K_IDLE, mood, speed); } }
static void layer(member *m, int slot, uint32_t key, float fade) { const r3d_clip *c = key ? clip_of(m, key) : NULL; if (!c) return; act *p = cur_slot(m, slot); if (p && p->c == c) return; act_add(m, c, slot, fade, 1, 0); }
static void face_to(member *m, uint32_t key, float from) { const r3d_clip *c = clip_of(m, key);
    if (dbg()) { printf("band: %s face %08x from %.2f: %s", m->pf->name, key, from, c ? "found" : "missing"); for (int i = 0; i < SG.nfaces; i++) if (SG.faces[i].key == key) printf(" (song clip, %d bones, %.1f s)", SG.faces[i].nb, SG.faces[i].dur); printf("\n"); } if (!c || from >= c->dur - 0.3f) return; act_add(m, c, S_FACE, 0.25f, 0, from); }
static void member_reset(member *m, int mood, int speed) {
    m->stance = m->pf->stance; m->cycle = K_IDLE; memset(&m->walk, 0, sizeof m->walk); m->spWas = 0; memcpy(m->pos, m->home, 12); m->yaw = m->homeYaw;
    for (int i = 0; i < m->nacts; ) if (m->acts[i].slot == S_HIT) m->acts[i] = m->acts[--m->nacts]; else i++;
    if (cur_body(m)) next(m, mood, speed);
}
static int small_stage(void) {   /* the venues the scripts give the _smstg intro / win / lose stances (a list in their small-venue check) */
    static const char *SM[5] = { "z_party", "z_dive", "z_video", "z_prison", "z_hell" }; for (int i = 0; i < 5; i++) if (!strcmp(Z.zone, SM[i])) return 1; return 0; }
static uint32_t stage_stance(const member *m, uint32_t big, uint32_t sm) { return m->role != BAND_DRUMMER && small_stage() && set_of(m, sm) ? sm : big; }   /* (the drummer has only the big ones) */
static void member_intro(member *m, int mood, int speed) {
    uint32_t l[16], ok[16]; int n = anim_keys(qsub(set_of(m, stage_stance(m, K_INTRO, K_INTRO_SM)), K_IDLE), mood, speed, l, 16), k = 0; for (int i = 0; i < n; i++) if (clip_of(m, l[i])) ok[k++] = l[i];
    if (!k) return; const r3d_clip *c = clip_of(m, ok[(m->introN++) % k]); if (c) { begin(m, c, 0.2f, 0, speed); m->played = K_INTRO; }
}
static float wrap_angle(float a) { a = fmodf(a + (float)M_PI, (float)(2 * M_PI)); if (a < 0) a += (float)(2 * M_PI); return a - (float)M_PI; }
static void walk_to(member *m, const wpt *pt, int quiet, int speed) {
    if (!pt) return; if (quiet) { memcpy(m->pos, pt->pos, 12); m->yaw = pt->yaw; m->walk.go = 0; return; }
    float dx = pt->pos[0] - m->pos[0], dz = pt->pos[2] - m->pos[2], dist = hypotf(dx, dz); if (dist < 0.2f) { m->walk.go = 0; m->walk.yaw = pt->yaw; m->walk.v = -1; return; }
    float head = atan2f(dx, dz); int back = fabsf(wrap_angle(head - m->yaw)) > 1.75f; uint32_t key = back ? m->walkBack : m->walkFwd; const r3d_clip *c = key ? clip_of(m, key) : NULL;
    m->walk.go = 1; m->walk.x = pt->pos[0]; m->walk.y = pt->pos[1]; m->walk.z = pt->pos[2]; m->walk.yaw = pt->yaw; m->walk.head = back ? wrap_angle(head + (float)M_PI) : head; m->walk.v = c && c->travel > 0.2f ? c->travel / c->dur : 1.3f;
    if (c) { begin(m, c, 0.25f, 1, speed); m->played = K_WALKING; }
}
static void member_event(member *m, int i, int quiet, float late, int mood, int speed) {
    const __typeof__(*SG.ev) *ev = &SG.ev[i];
    if (dbg()) printf("band: %s event %d at %d ms: %08x%s\n", m->pf->name, i, ev->t, ev->scr, quiet ? " (quiet)" : "");
    if (ev->scr == K_EV_STANCE) { if (!ev->stance || ev->stance == m->stance || !set_of(m, ev->stance)) return; uint32_t old = m->stance; m->stance = ev->stance; m->cycle = K_IDLE; if (quiet || m->walk.go) return;
        uint32_t tk; const r3d_clip *c = qb_keyof(qsub(qsub(set_of(m, old), K_TRANS), ev->stance), &tk) ? clip_of(m, tk) : NULL; if (c) { begin(m, c, FADE, 0, speed); m->played = K_TRANS; } else play(m, K_IDLE, mood, speed); }
    else if (ev->scr == K_EV_PLAY) { if (!ev->anim || quiet || m->walk.go) return; if (play(m, ev->anim, mood, speed)) { int cyc = 0, na = 0; for (int k = 0; k < ev->nflags; k++) { if (ev->flags[k] == F_CYCLE) cyc = 1; if (ev->flags[k] == F_NOARMS) na = 1; } m->cycle = cyc ? ev->anim : K_IDLE; m->noArms = na; } }
    else if (ev->scr == K_EV_FACE) { if (ev->anim) face_to(m, ev->anim, late > 0 ? late : 0); }
    else if (ev->scr == K_EV_WALK) { char nm[96]; snprintf(nm, sizeof nm, "%s_trg_waypoint_%s", Z.zone, ev->node); uint32_t k = gh3_qbkey(nm); for (int p = 0; p < Z.npts; p++) if (Z.pts[p].key == k) { walk_to(m, &Z.pts[p].p, quiet, speed); break; } }
}
static void member_hit(member *m, int note, int struck) {
    if (note < 0 || note > 127 || !m->drum[note]) return; const r3d_clip *c = clip_of(m, m->drum[note]); if (!c) return;
    float t0 = struck ? 0 : fmaxf(0, c->land - HIT_LEAD); act_add(m, c, S_HIT, 0, 0, t0);
    if (dbg() >= 2) printf("band: hit %d clip %08x land %.3f dur %.2f\n", note, c->key, c->land, c->dur);   /* struck: a cymbal's own swing, which begins as it is hit */
}
/* the song's end: the win or lose stance's idle at once (the scripts' anim_struct { stance, anim = idle, blendduration = 0 }:
 * a cut hides it there; here a short fade), round robin from then on; the guitarist and the bassist take the _smstg ones in
 * the small venues, the drummer and the singer never */
static void member_finale(member *m, int win, int mood, int speed) {
    uint32_t st = m->role == BAND_GUITARIST || m->role == BAND_BASSIST ? stage_stance(m, win ? K_WIN : K_LOSE, win ? K_WIN_SM : K_LOSE_SM) : win ? K_WIN : K_LOSE;
    m->walk.go = 0; m->walk.v = 0; m->noArms = 0; m->spWas = 0;
    for (int i = 0; i < m->nacts; ) { int s = m->acts[i].slot; if (s == S_HIT) m->acts[i] = m->acts[--m->nacts]; else { if (s == S_FACE) { m->acts[i].wTo = 0; m->acts[i].rate = 4; } i++; } }   /* the sticks stop, the words too */
    if (!set_of(m, st)) return; m->stance = st; m->cycle = K_IDLE;
    uint32_t l[16]; int n = keys_for(m, K_IDLE, mood, speed, l); const r3d_clip *c = n ? clip_of(m, l[(m->n++) % n]) : NULL; if (!c) return;
    begin(m, c, 0.25f, 0, speed); m->curIdle = 1; m->played = K_IDLE;
}
typedef struct { int speed, mood, frets, note, sp; } bandstate;
static void member_tick(member *m, float dt, const bandstate *S) {
    int strings = m->role == BAND_GUITARIST || m->role == BAND_BASSIST;
    if (strings && m->chord[1]) layer(m, S_HAND, m->chord[S->frets & 31] ? m->chord[S->frets & 31] : m->chord[1], 0.08f);   /* the fretting hand takes the shape of the buttons played */
    if (m->walk.go) { float d = wrap_angle(m->walk.head - m->yaw), s = 4.5f * dt; int facing = fabsf(d) < 0.9f; m->yaw += fabsf(d) <= s ? d : (d > 0 ? s : -s);
        if (facing) { float dx = m->walk.x - m->pos[0], dz = m->walk.z - m->pos[2], dd = hypotf(dx, dz), st = m->walk.v * dt;
            if (dd <= st + 0.02f) { m->pos[0] = m->walk.x; m->pos[1] = m->walk.y; m->pos[2] = m->walk.z; m->walk.go = 0; m->walk.v = -1; m->cycle = K_IDLE; play(m, K_IDLE, S->mood, S->speed); }
            else { m->pos[0] += dx / dd * st; m->pos[2] += dz / dd * st; } } }
    else if (m->walk.v < 0) { float d = wrap_angle(m->walk.yaw - m->yaw), s = 2.4f * dt; if (fabsf(d) <= s) { m->yaw = m->walk.yaw; m->walk.v = 0; } else m->yaw += d > 0 ? s : -s; }   /* arrived: turn back to the hall */
    if (S->sp && !m->spWas && m->role != BAND_DRUMMER && !m->walk.go) play(m, K_STARPOWER, S->mood, S->speed); m->spWas = S->sp;   /* the move that goes with star power */
    act *cur = cur_body(m);
    if (!cur || (!m->curLoop && cur->t >= cur->c->dur - FADE - 0.02f) || (m->curIdle && m->curSpeed != S->speed && cur->t > 1.5f)) next(m, S->mood, S->speed);
    if (m->faceWait > 0) m->faceWait -= dt; else { act *f = cur_slot(m, S_FACE); if (!f || f->t >= f->c->dur - 0.27f) { int ok = 0; if (m->nface) { for (int k = 0; k < m->nface && !ok; k++) { const r3d_clip *c = clip_of(m, m->face[(m->faceN++) % m->nface]); if (c && c->dur > 0.3f) { act_add(m, c, S_FACE, 0.25f, 0, 0); ok = 1; } } } if (!ok) m->faceWait = 1; } }
    for (int i = 0; i < m->nacts; i++) { act *a = &m->acts[i]; if (a->slot != S_HIT) continue; float d = a->c->dur, t = a->t, e = fminf((t - a->t0 + 0.015f) / 0.05f, (d - t) / fminf(0.16f, d * 0.45f)); a->w = HIT_WEIGHT * (e < 0 ? 0 : e > 1 ? 1 : e); }
    mixer_update(m, dt);
    if (S->note != m->lastNote) { m->lastNote = S->note; m->strokeTo = -m->strokeTo; } m->stroke += (m->strokeTo - m->stroke) * fminf(1, dt * 28);   /* each note a stroke, down then up */
}

/* ------------------------------------------------------------------ the pose: clips blended, FK, the arms' reach, the skinning matrices */
static void fk(member *m) {
    for (int i = 0; i < m->ske.n; i++) { int p = m->ske.parent[i]; if (p < 0) { memcpy(m->wq[i], m->lq[i], 16); memcpy(m->wp[i], m->lp[i], 12); continue; }
        r3q_mul(m->wq[p], m->lq[i], m->wq[i]); float v[3]; r3q_rot(m->wq[p], m->lp[i], v); for (int c = 0; c < 3; c++) m->wp[i][c] = m->wp[p][c] + v[c]; }
}
static void norm3(float v[3]) { float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l > 1e-8f) { v[0] /= l; v[1] /= l; v[2] /= l; } }
static void cross3(const float a[3], const float b[3], float o[3]) { float x = a[1] * b[2] - a[2] * b[1], y = a[2] * b[0] - a[0] * b[2], z = a[0] * b[1] - a[1] * b[0]; o[0] = x; o[1] = y; o[2] = z; }
static float dot3(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static void reach(member *m, int s, float stroke) {   /* charArm: the upper arm and forearm turned so the palm lands on the guitar's hand target, the elbow low and behind */
    int up = m->bBicep[s], lo = m->bFore[s], palm = m->bPalm[s], tg = m->bIk[s]; if (up < 0 || lo < 0 || palm < 0 || tg < 0) return; int par = m->ske.parent[up]; if (par < 0) return;
    float S[3], G[3], Qt[4], u[3], p[3], E[3], x[3], y[3], z[3], t[3], Qb[4], Qf[4], Qp[4], iq[4]; memcpy(S, m->wp[up], 12); memcpy(G, m->wp[tg], 12); memcpy(Qt, m->wq[tg], 16);
    float a1 = m->a1[s], a2 = m->a2[s];
    if (stroke != 0 && m->bNeck >= 0) { float ax[3] = { 1, 0, 0 }, xd[3]; r3q_rot(m->wq[m->bNeck], ax, xd); float yd[3] = { -xd[1] * xd[0], 1 - xd[1] * xd[1], -xd[1] * xd[2] }; if (dot3(yd, yd) > 1e-4f) { norm3(yd); for (int c = 0; c < 3; c++) G[c] += yd[c] * stroke; } }   /* across the strings */
    for (int c = 0; c < 3; c++) u[c] = G[c] - S[c]; float d = sqrtf(dot3(u, u)); if (d < 1e-4f) return; for (int c = 0; c < 3; c++) u[c] /= d; if (d > (a1 + a2) * 0.999f) d = (a1 + a2) * 0.999f;
    float a = (a1 * a1 - a2 * a2 + d * d) / (2 * d), hh = sqrtf(fmaxf(0, a1 * a1 - a * a));
    float h[3] = { s == 0 ? 0.45f : -0.45f, -0.8f, -0.4f }; norm3(h); float hu = dot3(h, u); for (int c = 0; c < 3; c++) p[c] = h[c] - u[c] * hu;
    if (dot3(p, p) < 1e-6f) { p[0] = u[0] * u[1]; p[1] = -1 + u[1] * u[1]; p[2] = u[2] * u[1]; } norm3(p);
    for (int c = 0; c < 3; c++) E[c] = S[c] + u[c] * a + p[c] * hh;
    memcpy(Qp, m->wq[par], 16);
    for (int c = 0; c < 3; c++) { x[c] = E[c] - S[c]; y[c] = G[c] - E[c]; } norm3(x); norm3(y); cross3(x, y, z); if (dot3(z, z) < 1e-8f) cross3(u, p, z); norm3(z); for (int c = 0; c < 3; c++) z[c] = -z[c];   /* the elbow is a hinge about the forearm's -z */
    cross3(z, x, t); r3q_from_basis(x, t, z, Qb); cross3(z, y, t); r3q_from_basis(y, t, z, Qf);
    r3q_inv(Qp, iq); r3q_mul(iq, Qb, m->lq[up]); r3q_inv(Qb, iq); r3q_mul(iq, Qf, m->lq[lo]); r3q_inv(Qf, iq); r3q_mul(iq, Qt, m->lq[palm]);
}
static void pose(member *m) {
    int nb = m->ske.n; float qa[NB][4], wqa[NB], pa[NB][3], wpa[NB]; memset(qa, 0, sizeof qa); memset(wqa, 0, sizeof wqa); memset(pa, 0, sizeof pa); memset(wpa, 0, sizeof wpa);
    for (int k = 0; k < m->nacts; k++) { const act *a = &m->acts[k]; if (a->w <= 1e-4f) continue; int n = a->c->nb < nb ? a->c->nb : nb;
        for (int b = 0; b < n; b++) { const r3d_track *tr = &a->c->b[b]; if (!tr->nr && !(tr->nt && b > 0)) continue; float q[4], p[3]; int got = r3d_clip_bone(a->c, &m->ske, b, a->t, q, p);
            if (got & 1) { if (wqa[b] > 0 && dot3(qa[b], q) + qa[b][3] * q[3] < 0) for (int c = 0; c < 4; c++) q[c] = -q[c]; for (int c = 0; c < 4; c++) qa[b][c] += q[c] * a->w; wqa[b] += a->w; }
            if (got & 2) { for (int c = 0; c < 3; c++) pa[b][c] += p[c] * a->w; wpa[b] += a->w; } } }
    for (int b = 0; b < nb; b++) { const float *rq = m->ske.q[b], *rp = m->ske.pos[b];
        if (wqa[b] > 0) { float q[4]; memcpy(q, qa[b], 16); if (wqa[b] < 1) { float r = 1 - wqa[b], s = dot3(q, rq) + q[3] * rq[3] < 0 ? -r : r; for (int c = 0; c < 4; c++) q[c] += rq[c] * s; }
            float l = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]); if (l > 1e-8f) for (int c = 0; c < 4; c++) m->lq[b][c] = q[c] / l; else memcpy(m->lq[b], rq, 16); }
        else memcpy(m->lq[b], rq, 16);
        if (wpa[b] > 0) { if (wpa[b] >= 1) for (int c = 0; c < 3; c++) m->lp[b][c] = pa[b][c] / wpa[b]; else for (int c = 0; c < 3; c++) m->lp[b][c] = pa[b][c] + rp[c] * (1 - wpa[b]); }
        else memcpy(m->lp[b], rp, 12); }
    fk(m);
    const act *cur = cur_body(m);
    if ((m->role == BAND_GUITARIST || m->role == BAND_BASSIST) && cur && !m->noArms && !(m->armBone >= 0 && m->armBone < cur->c->nb && cur->c->driven[m->armBone])) { reach(m, 0, 0); reach(m, 1, m->stroke); fk(m); }   /* arms the clip leaves alone */
    /* holder: place, turn about +Y, the profile's scale */
    float k = m->pf->scale, c = cosf(m->yaw), s = sinf(m->yaw), H[12] = { c * k, 0, s * k, m->pos[0], 0, k, 0, m->pos[1], -s * k, 0, c * k, m->pos[2] };
    for (int b = 0; b < nb; b++) { if (!m->body.used[b] && !m->inst.used[b]) continue; float W[12]; rows_of(m->wq[b], m->wp[b], W); rows_mul(H, W, m->skin[b]); }
}
static int visible(const float *vp, const float c[3], float rad) {
    for (int i = 0; i < 6; i++) { int row = i >> 1; float sg = (i & 1) ? -1.0f : 1.0f, p[4]; for (int k = 0; k < 4; k++) p[k] = vp[k * 4 + 3] + sg * vp[k * 4 + row];
        float L = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]); if (L < 1e-6f) continue; if ((p[0] * c[0] + p[1] * c[1] + p[2] * c[2] + p[3]) / L < -rad) return 0; }
    return 1;
}

/* ------------------------------------------------------------------ every frame */
static struct { double lastPerf, lastMs; int pPerf, pDrum, pSwing, pAnim; const void *song; float bpm; double lastBeat; int hasBeat; double testMs; int test, testFin; char testSong[64]; int mood, speed; } T;
static struct { int how, battle; } FIN;   /* the song's end: 1 won, 2 lost (battle: the boss in the bassist's place takes the other stance) */
void r3d_band_test_clock(double ms, const char *song) { T.test = song && *song; T.testMs = ms; if (song) snprintf(T.testSong, sizeof T.testSong, "%s", song); }
static void finale(int how) {
    FIN.how = how; FIN.battle = M[BAND_BASSIST].on && M[BAND_BASSIST].pf->role == BAND_GUITARIST;   /* a guitarist's profile where the bassist stands: the boss */
    if (dbg()) printf("band: the song is over: %s%s, %s venue\n", how == 1 ? "won" : "lost", FIN.battle ? " (a battle)" : "", small_stage() ? "a small" : "a big");
    for (int r = 0; r < BAND_ROLES; r++) if (M[r].on) member_finale(&M[r], r == BAND_BASSIST && FIN.battle ? how != 1 : how == 1, T.mood, T.speed);
}
void r3d_band_test_finale(int how) { T.testFin = how; if (how) finale(how); }
static void update(double perf) {
    int live = (A.screen == SC_PLAY || A.screen == SC_PAUSE) && A.S.loaded && A.S.song; double ms; float dt;   /* (paused: the song's clock stands, so does the band) */
    if (FIN.how) { int keep = T.test ? T.testFin != 0 : live ? A.S.run.finished || A.S.run.failed : A.screen == SC_RESULTS || A.screen == SC_BATTLE;
        if (!keep) { FIN.how = 0; T.song = &FIN; }   /* a new song or the menus: the reset below */
        else { float d = (float)((perf - T.lastPerf) / 1000); T.lastPerf = perf; if (d < 0 || d > 0.25f) d = d < 0 ? 0 : 0.25f;
            bandstate S; memset(&S, 0, sizeof S); S.mood = T.mood; S.speed = T.speed; S.note = -1; for (int r = 0; r < BAND_ROLES; r++) if (M[r].on) member_tick(&M[r], d, &S); return; } }
    else if (live && (A.S.run.finished || A.S.run.failed) && !T.test) { T.lastPerf = perf; finale(BTL.on ? (BTL.won ? 1 : 2) : A.S.run.failed ? 2 : 1); return; }   /* (a battle is won only by making the boss fail) */
    if (live) { ms = A.S.run.now; dt = (float)((ms - T.lastMs) / 1000); } else if (T.test) { ms = T.testMs; dt = (float)((ms - T.lastMs) / 1000); } else { ms = 0; dt = (float)((perf - T.lastPerf) / 1000); }
    if (dt < 0 || dt > 0.25f) dt = dt < 0 ? 0 : 0.25f; T.lastPerf = perf;
    const void *song = live ? (const void *)A.S.song : T.test ? (const void *)T.testSong : NULL; int cues = song && SG.loaded && ((live && !strcmp(SG.name, A.S.song->name)) || (T.test && !strcmp(SG.name, T.testSong)));
    /* the pace: beats a second, smoothed, picks slow / medium / fast; the mood from the rock meter */
    const int32_t *fb = live ? A.S.sd.fretbars : SG.fb; int nfb = live ? A.S.sd.nfret : SG.nfb;
    if (song != T.song || ms < T.lastMs - 1500) { T.hasBeat = 0; if (fb && nfb > 2) { double b0 = gh3_beatpos(fb, nfb, ms), b1 = gh3_beatpos(fb, nfb, ms + 2000); T.bpm = (float)((b1 - b0) * 30); if (T.bpm < 40) T.bpm = 120; } else T.bpm = 120; }
    if (fb && nfb > 2) { double beat = gh3_beatpos(fb, nfb, ms); if (T.hasBeat && dt > 0 && beat > T.lastBeat) T.bpm += (float)(((beat - T.lastBeat) / dt * 60 - T.bpm) * fminf(1, dt * 0.5f)); T.lastBeat = beat; T.hasBeat = 1; }
    bandstate S; memset(&S, 0, sizeof S); S.speed = T.bpm < 105 ? 0 : T.bpm < 150 ? 1 : 2; S.mood = 1; S.note = -1;
    if (live) { double h = A.S.run.health; S.mood = h < 0.66 ? 0 : h < 1.33 ? 1 : 2; S.sp = A.S.run.spOn;
        const gh3_chart *ch = &A.S.chart; int lo = 0, hi = ch->nnotes - 1, k = -1; while (lo <= hi) { int mid = (lo + hi) >> 1; if (ch->notes[mid].t <= ms) { k = mid; lo = mid + 1; } else hi = mid - 1; }
        if (k >= 0) { S.note = k; if (ms - ch->notes[k].t <= fmax(ch->notes[k].sus, 450)) S.frets = ch->notes[k].mask; } }
    else if (T.test) { S.note = (int)(ms / 240); S.frets = 1 << ((int)(ms / 960) % 3); }
    T.mood = S.mood; T.speed = S.speed;
    /* a new song (or the clock wound back): everyone home, the intro move */
    if (song != T.song || ms < T.lastMs - 1500) { T.song = song; T.pPerf = T.pDrum = T.pSwing = T.pAnim = 0; for (int r = 0; r < BAND_ROLES; r++) if (M[r].on) { member_reset(&M[r], S.mood, S.speed); if (song && ms < 4000) member_intro(&M[r], S.mood, S.speed); } }
    T.lastMs = ms;
    if (cues) {
        while (T.pPerf < SG.nev && SG.ev[T.pPerf].t <= ms) { int i = T.pPerf++; int r = -1; for (int k = 0; k < BAND_ROLES; k++) if (SG.ev[i].who == gh3_qbkey(ROLE[k])) r = k; if (r < 0 || !M[r].on) continue; member_event(&M[r], i, ms - SG.ev[i].t > 3000, (float)((ms - SG.ev[i].t) / 1000), S.mood, S.speed); }
        member *dr = M[BAND_DRUMMER].on ? &M[BAND_DRUMMER] : NULL;
        while (T.pDrum < SG.ndr && SG.drums[T.pDrum][0] - 130 <= ms) { if (dr && SG.drums[T.pDrum][1] < 67 && ms - SG.drums[T.pDrum][0] < 150) member_hit(dr, SG.drums[T.pDrum][1], 0); T.pDrum++; }   /* started early, so the stick lands on the note */
        while (T.pSwing < SG.ndr && SG.drums[T.pSwing][0] <= ms) { if (dr && SG.drums[T.pSwing][1] >= 67 && ms - SG.drums[T.pSwing][0] < 300) member_hit(dr, SG.drums[T.pSwing][1], 1); T.pSwing++; }
        while (T.pAnim < SG.nan && SG.an[T.pAnim][0] <= ms) { int nt = SG.an[T.pAnim][1]; if (nt >= 118 && nt <= 127 && M[BAND_GUITARIST].on) layer(&M[BAND_GUITARIST], S_FRET, M[BAND_GUITARIST].fret[nt], 0.18f); else if (nt >= 101 && nt <= 110 && M[BAND_BASSIST].on) layer(&M[BAND_BASSIST], S_FRET, M[BAND_BASSIST].fret[nt], 0.18f); T.pAnim++; } }
    for (int r = 0; r < BAND_ROLES; r++) if (M[r].on) member_tick(&M[r], dt, &S);
}
void r3d_band_attack(int from, uint32_t pu) {   /* the battle scripts' attack / response anim scripts: the kind from the side's table, played once (no_wait) */
    if (!G.ready || FIN.how || !pu) return; keys_init();
    for (int s = 0; s < 2; s++) { member *m = &M[s ? BAND_BASSIST : BAND_GUITARIST]; uint32_t kind = 0; if (!m->on || m->walk.go) continue;
        if (!qb_keyof(qsub(qsub(sec(s ? K_BTL2 : K_BTL1), pu), s == from ? K_ATK_ANIM : K_RESP_ANIM), &kind) || !kind || kind == K_NONE) continue;
        int ok = play(m, kind, T.mood, T.speed); if (ok) { m->cycle = K_IDLE; m->noArms = 0; }
        if (dbg()) printf("band: battle: %s %s %08x (%s): %s\n", m->pf->name, s == from ? "attacks with" : "takes", pu, s == from ? "attack_anim" : "response_anim", ok ? "playing" : "no clip in this stance"); }
}
static void mat4_mul(const float *a, const float *b, float *o) { for (int c = 0; c < 4; c++) for (int r = 0; r < 4; r++) { float s = 0; for (int k = 0; k < 4; k++) s += a[k * 4 + r] * b[c * 4 + k]; o[c * 4 + r] = s; } }
static void draw_model(const member *m, const gmodel *g, int add) {
    if (!g->vbo) return; static float pal[PAL_MAX * 12];
    GL(glBindBuffer)(GL_ARRAY_BUFFER, g->vbo); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, g->ibo);
    GL(glVertexAttribPointer)(G.aPos, 3, GL_FLOAT, GL_FALSE, sizeof(bvtx), (void *)0); GL(glVertexAttribPointer)(G.aNrm, 3, GL_BYTE, GL_TRUE, sizeof(bvtx), (void *)12);
    GL(glVertexAttribPointer)(G.aUv, 2, GL_FLOAT, GL_FALSE, sizeof(bvtx), (void *)16); GL(glVertexAttribPointer)(G.aIdx, 3, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(bvtx), (void *)24); GL(glVertexAttribPointer)(G.aW, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(bvtx), (void *)28);
    for (int i = 0; i < g->nd; i++) { const bdraw *d = &g->d[i]; if (d->add != add) continue;
        for (int k = 0; k < d->npal; k++) { float o[12]; rows_mul(m->skin[d->pal[k]], g->inv[d->pal[k]], o); memcpy(pal + k * 12, o, 48); }
        glUniform4fv_(G.uB, d->npal * 3, pal); GL(glBindTexture)(GL_TEXTURE_2D, d->tex); GL(glUniform1f)(G.uAref, d->aref);
        GL(glDrawElements)(GL_TRIANGLES, d->count, GL_UNSIGNED_SHORT, (void *)(size_t)(d->first * 2)); ST.draws++; ST.tris += d->count / 3; }
}
int r3d_band_draw(const float *view, const float *proj, double perf, int sp, float energy) {
    if (r3d_band_where(-1, NULL, NULL) == 0) return 0;   /* nobody on stage (practice: the stage without the band, as in the game) */
    Uint64 c0 = SDL_GetPerformanceCounter(); ST.draws = ST.tris = 0; ST.bones = 0;
    update(perf);
    float vp[16]; mat4_mul(proj, view, vp); int vis[BAND_ROLES];
    for (int r = 0; r < BAND_ROLES; r++) { member *m = &M[r]; vis[r] = 0; if (!shown(r)) continue; float c[3] = { m->pos[0], m->pos[1] + 1.0f, m->pos[2] }; if (!visible(vp, c, 1.8f)) continue; vis[r] = 1; pose(m); ST.bones += m->ske.n; }
    Uint64 c1 = SDL_GetPerformanceCounter();
    GL(glDisable)(GL_CULL_FACE); GL(glEnable)(GL_DEPTH_TEST); GL(glDepthFunc)(GL_LEQUAL);
    if (G.sprog && glDrawArrays_) {   /* the patches on the floor first, blended, not written to depth */
        static const float quad[12] = { -1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1 }; GL(glUseProgram)(G.sprog); GL(glUniformMatrix4fv)(G.suVP, 1, GL_FALSE, vp);
        GL(glBindBuffer)(GL_ARRAY_BUFFER, 0); GL(glEnableVertexAttribArray)(G.saPos); GL(glVertexAttribPointer)(G.saPos, 2, GL_FLOAT, GL_FALSE, 8, quad);
        GL(glEnable)(GL_BLEND); GL(glBlendFunc)(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); GL(glDepthMask)(GL_FALSE);
        for (int r = 0; r < BAND_ROLES; r++) { if (!vis[r]) continue; const member *m = &M[r]; GL(glUniform4f)(G.suC, m->pos[0], m->pos[1] + 0.03f, m->pos[2], r == BAND_DRUMMER ? 0.95f : 0.6f * m->pf->scale); glDrawArrays_(GL_TRIANGLES, 0, 6); }
        GL(glDisableVertexAttribArray)(G.saPos); }
    GL(glUseProgram)(G.prog); GL(glUniformMatrix4fv)(G.uVP, 1, GL_FALSE, vp); GL(glUniform1i)(G.uMap, 0); GL(glActiveTexture)(GL_TEXTURE0);
    float k = 1 + 0.12f * energy; if (sp) GL(glUniform3f)(G.uTint, k * 0.78f, k * 0.98f, k * 1.18f); else GL(glUniform3f)(G.uTint, k, k, k);
    GL(glEnableVertexAttribArray)(G.aPos); GL(glEnableVertexAttribArray)(G.aNrm); GL(glEnableVertexAttribArray)(G.aUv); GL(glEnableVertexAttribArray)(G.aIdx); GL(glEnableVertexAttribArray)(G.aW);
    GL(glDisable)(GL_CULL_FACE); GL(glEnable)(GL_DEPTH_TEST); GL(glDepthFunc)(GL_LEQUAL);
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 0) { GL(glDisable)(GL_BLEND); GL(glDepthMask)(GL_TRUE); } else { GL(glEnable)(GL_BLEND); GL(glBlendFunc)(GL_ONE, GL_ONE); GL(glDepthMask)(GL_FALSE); }
        for (int r = 0; r < BAND_ROLES; r++) { if (!vis[r]) continue; member *m = &M[r];
            if (!pass) light_member(m);
            GL(glUniform3f)(G.uLdir, m->ldir[0], m->ldir[1], m->ldir[2]); GL(glUniform3f)(G.uLcol, m->lcol[0], m->lcol[1], m->lcol[2]); GL(glUniform3f)(G.uAmb, m->amb[0], m->amb[1], m->amb[2]);
            draw_model(m, &m->body, pass); if (!(SET.airGuitar && (r == BAND_GUITARIST || r == BAND_BASSIST))) draw_model(m, &m->inst, pass); } }   /* Air Guitar: the scripts dress both with musician_instrument none (the boss too) */
    GL(glDisable)(GL_BLEND); GL(glDepthMask)(GL_TRUE);
    GL(glDisableVertexAttribArray)(G.aPos); GL(glDisableVertexAttribArray)(G.aNrm); GL(glDisableVertexAttribArray)(G.aUv); GL(glDisableVertexAttribArray)(G.aIdx); GL(glDisableVertexAttribArray)(G.aW);
    GL(glBindBuffer)(GL_ARRAY_BUFFER, 0); GL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, 0);
    Uint64 c2 = SDL_GetPerformanceCounter(); double f = 1000.0 / SDL_GetPerformanceFrequency(); ST.poseMs = (c1 - c0) * f; ST.drawMs = (c2 - c1) * f;
    return 1;
}
const r3d_bandstats *r3d_band_stats(void) { return &ST; }
void r3d_band_shutdown(void) {
    for (int r = 0; r < BAND_ROLES; r++) member_free(&M[r]); song_free(); free(Z.pts); free(Z.lights); memset(&Z, 0, sizeof Z);
    if (G.prog) GL(glDeleteProgram)(G.prog); memset(&G, 0, sizeof G);
    if (F.hasGlobal) r3d_pak_close(&F.global); if (F.hasPerm) r3d_pak_close(&F.perm); if (F.hasGuitar) r3d_ske_free(&F.guitar); memset(&F, 0, sizeof F);
}
