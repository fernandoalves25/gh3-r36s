#include "r3d_zone.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static uint32_t be16(const uint8_t *p) { return ((uint32_t)p[0] << 8) | p[1]; }
static float bef32(const uint8_t *p) { uint32_t u = be32(p); float f; memcpy(&f, &u, 4); return f; }
static uint32_t le32(const uint8_t *p) { return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0]; }

/* ------------------------------------------------------------------ .scn */
/* the strip, cut wherever the "start again" mark (0x7fff / 0xffff) stands, turned into separate triangles */
static uint16_t *strip_tris(const uint8_t *s, size_t n, size_t iAt, int nIdx, int nVert, int *ntri) {
    uint16_t *tri = malloc((size_t)nIdx * 3 * sizeof *tri); int t = 0, a = -1, b = -1, flip = 0;
    for (int k = 0; k < nIdx && iAt + (size_t)k * 2 + 2 <= n; k++) { int c = (int)be16(s + iAt + (size_t)k * 2);
        if (c == 0x7FFF || c == 0xFFFF || c >= nVert) { a = b = -1; flip = 0; continue; }
        if (a >= 0 && b >= 0 && a != b && b != c && a != c) { if (flip) { tri[t++] = (uint16_t)b; tri[t++] = (uint16_t)a; } else { tri[t++] = (uint16_t)a; tri[t++] = (uint16_t)b; } tri[t++] = (uint16_t)c; }
        if (a >= 0 && b >= 0) flip = !flip; a = b; b = c; }
    *ntri = t / 3; return tri;
}
int r3d_scene_read(const uint8_t *s, size_t n, r3d_scene *out) {
    memset(out, 0, sizeof *out); if (n < 0x100) return -1;
    size_t matEnd = 0x20 + be32(s + 0x24); if (matEnd + 0x100 > n) return -1;
    int cap = 32; out->mats = malloc(cap * sizeof *out->mats);
    for (size_t p = 0x20 + be32(s + 0x28); p + 0x100 <= matEnd; ) {
        uint32_t size = be32(s + p + 0xc4); if (size < 0x100 || p + size > matEnd) break;
        r3d_mat m; memset(&m, 0, sizeof m); m.key = be32(s + p); m.template = be32(s + p + 0xa0);
        uint32_t nt = be32(s + p + 0xb8), tOff = p + be32(s + p + 0xbc); if (nt > 8) nt = 8;
        for (uint32_t i = 0; i < nt && tOff + i * 4 + 4 <= p + size; i++) m.tex[m.ntex++] = be32(s + tOff + i * 4);
        uint32_t np = be32(s + p + 0xb0), pAt = p + be32(s + p + 0xb4); if (np > 16) np = 16;
        for (uint32_t i = 0; i < np * 4 && pAt + i * 4 + 4 <= p + size; i++) m.props[m.nprops++] = bef32(s + pAt + i * 4);
        uint32_t nv = be32(s + p + 0xa8), vAt = p + be32(s + p + 0xac); if (nv > 16) nv = 16;
        for (uint32_t i = 0; i < nv && vAt + i * 16 + 4 <= p + size; i++) m.vsp[m.nvsp++] = bef32(s + vAt + i * 16);
        m.flags = be32(s + p + 0xcc); m.blend = be32(s + p + 0xd8); m.glow = be32(s + p + 0xdc); m.alphaRef = s[p + 0xe8]; memcpy(m.clamp, s + p + 0xea, 4);
        if (out->nmats == cap) { cap *= 2; out->mats = realloc(out->mats, cap * sizeof *out->mats); }
        out->mats[out->nmats++] = m; p += size;
    }
    if (be32(s + matEnd) != 0xBABEFACE) return -1;
    size_t base = matEnd + 8 + be32(s + matEnd + 4); if (base + 0x90 > n) return -1;
    uint32_t nMesh = be32(s + base + 0x4c); size_t meshAt = base + be32(s + base + 0x80);
    uint32_t nSec = be32(s + base + 0x6c); size_t secAt = base + be32(s + base + 0x70), geomAt = base + be32(s + base + 0x74);
    for (int i = 0; i < 3; i++) { out->bmin[i] = bef32(s + base + i * 4); out->bmax[i] = bef32(s + base + 0x10 + i * 4); }
    if (nMesh > 100000) return -1;
    out->meshes = calloc(nMesh ? nMesh : 1, sizeof *out->meshes);
    for (uint32_t i = 0; i < nMesh; i++) {
        size_t m = meshAt + (size_t)i * 0x80; if (m + 0x80 > n) break;
        int nIdx = (int)be16(s + m + 0x24), nVert = (int)be16(s + m + 0x26); uint32_t vo = be32(s + m + 0x40), io = be32(s + m + 0x5c), so = be32(s + m + 0x60);
        if (!nVert || !nIdx || vo == 0xFFFFFFFFu || io == 0xFFFFFFFFu) continue;
        size_t vAt = base + vo + 0x20, iAt = base + io + 0x20; uint32_t vSize = be32(s + m + 0x4c);
        if (so != 0xFFFFFFFFu && base + so + 4 <= n && be32(s + base + so) == 0xCAFEBAB4) continue;   /* a character's skinned mesh: not a venue part */
        int stride = (int)(vSize / (uint32_t)nVert), uvOff = s[m + 0x1a], colOff = s[m + 0x1b];
        if (stride < 12 || vAt + (size_t)nVert * stride > n || iAt + (size_t)nIdx * 2 > n) continue;
        r3d_mesh me; memset(&me, 0, sizeof me); me.n = (int)i; me.mat = be32(s + m + 0x14); me.flags = be32(s + m + 0x10);
        for (int k = 0; k < 3; k++) me.centre[k] = bef32(s + m + k * 4); me.radius = bef32(s + m + 12);
        me.nvert = nVert; me.nuv = uvOff >= 12 ? (stride - uvOff) / 8 : 0; if (me.nuv > 4) me.nuv = 4;
        me.pos = malloc((size_t)nVert * 12); me.nrm = calloc((size_t)nVert, 12); me.col = malloc((size_t)nVert * 4); me.uv = calloc((size_t)nVert, 8);
        if (me.nuv > 1) me.uv2 = malloc((size_t)nVert * 8); if (me.nuv > 2) me.uvLast = malloc((size_t)nVert * 8);
        for (int v = 0; v < nVert; v++) { const uint8_t *q = s + vAt + (size_t)v * stride;
            for (int k = 0; k < 3; k++) me.pos[v * 3 + k] = bef32(q + k * 4);
            if (colOff >= 24) for (int k = 0; k < 3; k++) me.nrm[v * 3 + k] = bef32(q + 12 + k * 4);
            if (colOff >= 12 && colOff + 4 <= stride) { me.col[v * 4] = q[colOff + 1]; me.col[v * 4 + 1] = q[colOff + 2]; me.col[v * 4 + 2] = q[colOff + 3]; me.col[v * 4 + 3] = q[colOff]; } /* stored alpha first */
            else memset(me.col + v * 4, 255, 4);
            if (me.nuv > 0) { me.uv[v * 2] = bef32(q + uvOff); me.uv[v * 2 + 1] = bef32(q + uvOff + 4); }
            if (me.uv2) { me.uv2[v * 2] = bef32(q + uvOff + 8); me.uv2[v * 2 + 1] = bef32(q + uvOff + 12); }
            if (me.uvLast) { me.uvLast[v * 2] = bef32(q + uvOff + (me.nuv - 1) * 8); me.uvLast[v * 2 + 1] = bef32(q + uvOff + (me.nuv - 1) * 8 + 4); } }
        me.idx = strip_tris(s, n, iAt, nIdx, nVert, &me.ntri);
        if (!me.ntri) { free(me.pos); free(me.nrm); free(me.col); free(me.uv); free(me.uv2); free(me.uvLast); free(me.idx); continue; }
        out->meshes[out->nmeshes++] = me;
    }
    /* each sector owns a run of meshes: its record in the second table gives the first one and how many */
    for (uint32_t i = 0; i < nSec && secAt + (size_t)i * 0x60 + 0x60 <= n && geomAt + (size_t)i * 0x60 + 0x60 <= n; i++) {
        uint32_t key = be32(s + secAt + (size_t)i * 0x60 + 4), first = be32(s + geomAt + (size_t)i * 0x60 + 0x48), cnt = be32(s + geomAt + (size_t)i * 0x60 + 0x4c); if (cnt > 4096) cnt = 4096;
        for (int k = 0; k < out->nmeshes; k++) if ((uint32_t)out->meshes[k].n >= first && (uint32_t)out->meshes[k].n < first + cnt) out->meshes[k].sector = key; }
    return 0;
}
void r3d_scene_free(r3d_scene *s) {
    for (int i = 0; i < s->nmeshes; i++) { r3d_mesh *m = &s->meshes[i]; free(m->pos); free(m->nrm); free(m->col); free(m->uv); free(m->uv2); free(m->uvLast); free(m->idx); }
    free(s->meshes); free(s->mats); memset(s, 0, sizeof *s);
}
const r3d_mat *r3d_scene_mat(const r3d_scene *s, uint32_t key) { for (int i = 0; i < s->nmats; i++) if (s->mats[i].key == key) return &s->mats[i]; return NULL; }

/* ------------------------------------------------------------------ .tex */
int r3d_texbank_read(const uint8_t *b, size_t n, r3d_texent **out) {
    *out = NULL; if (n < 16 || b[0] != 0xFA || b[1] != 0xCE) return 0;
    int cnt = (b[6] << 8) | b[7]; size_t meta = be32(b + 8); r3d_texent *t = malloc((cnt ? cnt : 1) * sizeof *t); int k = 0;
    for (int i = 0; i < cnt && meta + (size_t)i * 40 + 40 <= n; i++) { const uint8_t *p = b + meta + (size_t)i * 40; uint32_t off = be32(p + 28), size = be32(p + 32);
        if ((size_t)off + size <= n) { t[k].key = be32(p + 4); t[k].w = (int)be16(p + 8); t[k].h = (int)be16(p + 10); t[k].off = off; t[k].size = size; k++; } }
    *out = t; return k;
}

/* ------------------------------------------------------------------ DDS */
int r3d_dds_info(const uint8_t *b, size_t n, r3d_dds *d) {
    memset(d, 0, sizeof *d); if (n < 128 || b[0] != 'D' || b[1] != 'D' || b[2] != 'S') return -1;
    d->h = (int)le32(b + 12); d->w = (int)le32(b + 16); uint32_t nm = le32(b + 28); if (nm < 1) nm = 1; if (nm > 16) nm = 16;
    if (d->w <= 0 || d->h <= 0 || d->w > 8192 || d->h > 8192) return -1;
    if (!memcmp(b + 84, "DXT1", 4)) d->cc = DDS_DXT1; else if (!memcmp(b + 84, "DXT3", 4) || !memcmp(b + 84, "DXT2", 4)) d->cc = DDS_DXT3; else if (!memcmp(b + 84, "DXT5", 4) || !memcmp(b + 84, "DXT4", 4)) d->cc = DDS_DXT5;
    else if (le32(b + 88) == 32 && 128 + (size_t)d->w * d->h * 4 <= n) { d->cc = DDS_RGBA32; d->nlev = 1; d->lev[0].w = d->w; d->lev[0].h = d->h; d->lev[0].off = 128; d->lev[0].size = (uint32_t)d->w * d->h * 4; d->alpha = 1; return 0; }
    else return -1;
    int blk = d->cc == DDS_DXT1 ? 8 : 16; size_t p = 128; int lw = d->w, lh = d->h;
    for (uint32_t i = 0; i < nm; i++) { size_t size = (size_t)((lw + 3) >> 2 ? (lw + 3) >> 2 : 1) * ((lh + 3) >> 2 ? (lh + 3) >> 2 : 1) * blk; if (p + size > n) break;
        d->lev[d->nlev].w = lw; d->lev[d->nlev].h = lh; d->lev[d->nlev].off = (uint32_t)p; d->lev[d->nlev].size = (uint32_t)size; d->nlev++; p += size; if (lw == 1 && lh == 1) break; lw = lw > 1 ? lw >> 1 : 1; lh = lh > 1 ? lh >> 1 : 1; }
    d->alpha = d->cc != DDS_DXT1; return d->nlev ? 0 : -1;
}
int r3d_dds_decode(const uint8_t *b, size_t n, const r3d_dds *d, int level, uint8_t *out) {
    if (level < 0 || level >= d->nlev) return 0; int w = d->lev[level].w, h = d->lev[level].h; size_t p = d->lev[level].off; int trans = 0;
    if (d->cc == DDS_RGBA32) { for (int i = 0; i < w * h; i++) { out[i * 4] = b[p + i * 4 + 2]; out[i * 4 + 1] = b[p + i * 4 + 1]; out[i * 4 + 2] = b[p + i * 4]; out[i * 4 + 3] = b[p + i * 4 + 3]; if (out[i * 4 + 3] != 255) trans = 1; } return trans; }
    uint8_t col[16], al[8];
    for (int by = 0; by < h; by += 4) for (int bx = 0; bx < w; bx += 4) {
        size_t aoff = 0; if (d->cc != DDS_DXT1) { aoff = p; p += 8; }
        if (p + 8 > n) return trans;
        uint32_t c0 = b[p] | (b[p + 1] << 8), c1 = b[p + 2] | (b[p + 3] << 8), bits = b[p + 4] | (b[p + 5] << 8) | ((uint32_t)b[p + 6] << 16) | ((uint32_t)b[p + 7] << 24); p += 8;
        col[0] = (uint8_t)((c0 >> 11) * 255 / 31); col[1] = (uint8_t)(((c0 >> 5) & 63) * 255 / 63); col[2] = (uint8_t)((c0 & 31) * 255 / 31); col[3] = 255;
        col[4] = (uint8_t)((c1 >> 11) * 255 / 31); col[5] = (uint8_t)(((c1 >> 5) & 63) * 255 / 63); col[6] = (uint8_t)((c1 & 31) * 255 / 31); col[7] = 255;
        if (c0 > c1 || d->cc != DDS_DXT1) { for (int k = 0; k < 3; k++) { col[8 + k] = (uint8_t)((2 * col[k] + col[4 + k]) / 3); col[12 + k] = (uint8_t)((col[k] + 2 * col[4 + k]) / 3); } col[11] = 255; col[15] = 255; }
        else { for (int k = 0; k < 3; k++) { col[8 + k] = (uint8_t)((col[k] + col[4 + k]) / 2); col[12 + k] = 0; } col[11] = 255; col[15] = 0; }
        uint32_t ab0 = 0, ab1 = 0;
        if (d->cc == DDS_DXT5) { int a0 = b[aoff], a1 = b[aoff + 1]; al[0] = (uint8_t)a0; al[1] = (uint8_t)a1;
            if (a0 > a1) for (int k = 1; k < 7; k++) al[k + 1] = (uint8_t)(((7 - k) * a0 + k * a1) / 7); else { for (int k = 1; k < 5; k++) al[k + 1] = (uint8_t)(((5 - k) * a0 + k * a1) / 5); al[6] = 0; al[7] = 255; }
            ab0 = b[aoff + 2] | (b[aoff + 3] << 8) | ((uint32_t)b[aoff + 4] << 16); ab1 = b[aoff + 5] | (b[aoff + 6] << 8) | ((uint32_t)b[aoff + 7] << 16); }
        for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) { int x = bx + i, y = by + j; if (x >= w || y >= h) continue;
            int nn = j * 4 + i, ci = (bits >> (nn * 2)) & 3; uint8_t *o = out + ((size_t)y * w + x) * 4; o[0] = col[ci * 4]; o[1] = col[ci * 4 + 1]; o[2] = col[ci * 4 + 2];
            if (d->cc == DDS_DXT1) o[3] = col[ci * 4 + 3]; else if (d->cc == DDS_DXT3) o[3] = (uint8_t)(((b[aoff + (nn >> 1)] >> ((nn & 1) * 4)) & 15) * 17); else o[3] = al[(nn < 8 ? ab0 >> (nn * 3) : ab1 >> ((nn - 8) * 3)) & 7];
            if (o[3] != 255) trans = 1; }
    }
    return trans;
}
r3d_look r3d_tex_look(const uint8_t *b, size_t n) {
    r3d_look L = { 128, 128, 128, 0, 0, 0, 0 }; r3d_dds d; if (r3d_dds_info(b, n, &d)) return L; L.w = d.w; L.h = d.h; if (d.cc == DDS_RGBA32) return L;
    int blk = d.cc == DDS_DXT1 ? 8 : 16, cnt = (int)(d.lev[0].size / blk), step = cnt / 97 > 1 ? cnt / 97 : 1; float r = 0, g = 0, bl = 0, lo = 255, hi = 0; int c = 0;
    for (int i = 0; i < cnt; i += step) { size_t q = d.lev[0].off + (size_t)i * blk + (blk - 8); if (q + 4 > n) break; uint32_t cs[2] = { b[q] | (b[q + 1] << 8), b[q + 2] | (b[q + 3] << 8) };
        for (int k = 0; k < 2; k++) { float R = (cs[k] >> 11) * 255.0f / 31, G = ((cs[k] >> 5) & 63) * 255.0f / 63, B = (cs[k] & 31) * 255.0f / 31; r += R; g += G; bl += B; c++; float y = (R + G + B) / 3; if (y < lo) lo = y; if (y > hi) hi = y; } }
    if (!c) return L; L.r = r / c; L.g = g / c; L.b = bl / c; L.flat = hi - lo < 12; L.normal = L.b > 170 && fabsf(L.r - 128) < 45 && fabsf(L.g - 128) < 45 && L.b - fmaxf(L.r, L.g) > 45;
    return L;
}

/* ------------------------------------------------------------------ the venue's script data */
static int qb_vec(const qbv *v, float *out, int n) {   /* a vec3 or a list of numbers */
    if (!v) return 0; if (v->kind == QB_VEC3 && n <= 3) { for (int i = 0; i < n; i++) out[i] = v->f[i]; return 1; } if (v->kind == QB_VEC2 && n <= 2) { for (int i = 0; i < n; i++) out[i] = v->f[i]; return 1; }
    if (v->kind == QB_ARRAY && v->narr >= n) { for (int i = 0; i < n; i++) out[i] = (float)qb_num(v->arr[i], 0); return 1; } return 0;
}
static uint32_t K_CLASS, K_CAS, K_POS, K_QUAT, K_FOV, K_LOCKTO, K_WORLD, K_CAM1, K_CAM2, K_ORBIT, K_NAME, K_ANGLES, K_LEVELOBJECT, K_LEVELLIGHT, K_LIGHTCOLOR, K_TYPE, K_LIGHTGROUP, K_INTENSITY, K_ATTENSTART, K_ATTENEND, K_AMBIENT, K_POINT, K_DIR, K_BAND, K_ALTBAND, K_CROWD, K_QB, K_NQB;
#define K_LQUAT 0x786e947eu
static void keys_init(void) {
    if (K_CLASS) return;
    K_CLASS = gh3_qbkey("class"); K_CAS = gh3_qbkey("createdatstart"); K_POS = gh3_qbkey("pos"); K_QUAT = gh3_qbkey("quat"); K_FOV = gh3_qbkey("fov"); K_LOCKTO = gh3_qbkey("lockto"); K_WORLD = gh3_qbkey("world");
    K_CAM1 = gh3_qbkey("cam1"); K_CAM2 = gh3_qbkey("cam2"); K_ORBIT = gh3_qbkey("orbitangle"); K_NAME = gh3_qbkey("name"); K_ANGLES = gh3_qbkey("angles"); K_LEVELOBJECT = gh3_qbkey("levelobject"); K_LEVELLIGHT = gh3_qbkey("levellight");
    K_LIGHTCOLOR = gh3_qbkey("lightcolor"); K_TYPE = gh3_qbkey("type"); K_LIGHTGROUP = gh3_qbkey("lightgroup"); K_INTENSITY = gh3_qbkey("intensity"); K_ATTENSTART = gh3_qbkey("attenstart"); K_ATTENEND = gh3_qbkey("attenend");
    K_AMBIENT = gh3_qbkey("ambient"); K_POINT = gh3_qbkey("point"); K_DIR = gh3_qbkey("dir"); K_BAND = gh3_qbkey("band"); K_ALTBAND = gh3_qbkey("alt_band"); K_CROWD = gh3_qbkey("crowd"); K_QB = gh3_qbkey(".qb"); K_NQB = gh3_qbkey(".nqb");
}
static int made_at_start(const qbv *st) { uint32_t k; for (int i = 0; i < st->nitems; i++) if (st->items[i].id == 0 && qb_keyof(st->items[i].v, &k) && k == K_CAS) return 1; return 0; }
static int cam_end(const qbv *st, r3d_camend *o) {
    if (!st || st->kind != QB_STRUCT) return 0; memset(o, 0, sizeof *o);
    if (!qb_vec(qb_getk(st, K_POS), o->pos, 3) || !qb_vec(qb_getk(st, K_QUAT), o->quat, 3)) return 0;
    double f = qb_num(qb_getk(st, K_FOV), 0); o->fov = f > 5 && f < 150 ? (float)f : 72; uint32_t lk; o->lock = qb_keyof(qb_getk(st, K_LOCKTO), &lk) && lk != K_WORLD ? lk : 0; return 1;
}
typedef struct { r3d_venueqb *v; int capS, capH, capO, capL; } ctx;
#define PUSH(arr, n, cap, item) do { if ((n) == (cap)) { (cap) = (cap) ? (cap) * 2 : 16; (arr) = realloc((arr), (cap) * sizeof *(arr)); } (arr)[(n)++] = (item); } while (0)
static void walk_cams(ctx *c, const qbv *v, uint32_t list, int depth, float orbit, uint32_t tyUp, uint32_t nmUp) {
    if (!v || depth > 6) return;
    if (v->kind == QB_ARRAY) { for (int i = 0; i < v->narr; i++) walk_cams(c, v->arr[i], list, depth + 1, 0, 0, 0); return; }
    if (v->kind != QB_STRUCT) return;
    const qbv *ob = qb_getk(v, K_ORBIT); float orb = ob && (ob->kind == QB_INT || ob->kind == QB_FLOAT) ? (float)qb_num(ob, 0) : orbit;
    uint32_t ty = 0, nm = 0; if (!qb_keyof(qb_getk(v, K_TYPE), &ty)) ty = tyUp; if (!qb_keyof(qb_getk(v, K_NAME), &nm)) nm = nmUp;   /* the shot's Type (closeup, mid, Stage) and NAME, from the struct or the one above */
    const qbv *c1 = qb_getk(v, K_CAM1), *c2 = qb_getk(v, K_CAM2); r3d_shot sh; memset(&sh, 0, sizeof sh); sh.list = list; sh.orbit = orb; sh.type = ty; sh.name = nm;
    if (c1 && c1->kind == QB_STRUCT) { if (cam_end(c1, &sh.a)) { sh.hasB = c2 && cam_end(c2, &sh.b) && sh.b.lock == sh.a.lock; PUSH(c->v->shots, c->v->nshots, c->capS, sh); } return; }
    if (cam_end(v, &sh.a)) { PUSH(c->v->shots, c->v->nshots, c->capS, sh); return; }
    for (int i = 0; i < v->nitems; i++) if (v->items[i].v && (v->items[i].v->kind == QB_STRUCT || v->items[i].v->kind == QB_ARRAY)) walk_cams(c, v->items[i].v, list, depth + 1, orb, ty, nm);
}
static void turn_vec(const float q[4], float out[3]) {   /* (0,0,1) turned by the quaternion x y z w */
    float x = q[0], y = q[1], z = q[2], w = q[3];
    out[0] = 2 * (x * z + w * y); out[1] = 2 * (y * z - w * x); out[2] = 1 - 2 * (x * x + y * y);
}
static void read_struct(ctx *c, const qbv *st) {
    uint32_t cls = 0, name = 0; int hasCls = 0; const qbv *cv = qb_getk(st, K_CLASS); if (cv) { hasCls = 1; qb_keyof(cv, &cls); }
    int hasName = qb_keyof(qb_getk(st, K_NAME), &name), made = made_at_start(st);
    if (hasName && hasCls && !made) PUSH(c->v->hidden, c->v->nhidden, c->capH, name);
    if (cls == K_LEVELOBJECT && hasName) { r3d_object o; memset(&o, 0, sizeof o); o.key = name; o.shown = made;
        if (qb_vec(qb_getk(st, K_POS), o.pos, 3)) { qb_vec(qb_getk(st, K_ANGLES), o.angles, 3); PUSH(c->v->objects, c->v->nobjects, c->capO, o); } }
    if (cls == K_LEVELLIGHT && hasName && made) { r3d_light l; memset(&l, 0, sizeof l); uint32_t ty = 0, grp = 0; float q[4] = { 0, 0, 0, 1 };
        if (!qb_vec(qb_getk(st, K_POS), l.pos, 3) || !qb_vec(qb_getk(st, K_LIGHTCOLOR), l.colour, 3)) return;
        qb_keyof(qb_getk(st, K_TYPE), &ty); qb_keyof(qb_getk(st, K_LIGHTGROUP), &grp); qb_vec(qb_getk(st, K_LQUAT), q, 4); turn_vec(q, l.dir);
        l.kind = ty == K_AMBIENT ? L_AMBIENT : ty == K_POINT ? L_POINT : ty == K_DIR ? L_DIR : L_SPOT; l.band = grp == K_BAND || grp == K_ALTBAND || grp == K_CROWD;
        l.intensity = (float)qb_num(qb_getk(st, K_INTENSITY), 0.5); l.near = (float)qb_num(qb_getk(st, K_ATTENSTART), 0); l.far = (float)qb_num(qb_getk(st, K_ATTENEND), 10);
        PUSH(c->v->lights, c->v->nlights, c->capL, l); }
}
void r3d_venueqb_read(const gh3_pakent *ents, int n, r3d_venueqb *out) {
    keys_init(); memset(out, 0, sizeof *out); ctx c = { out, 0, 0, 0, 0 };
    for (int e = 0; e < n; e++) { if ((ents[e].type != K_QB && ents[e].type != K_NQB) || !ents[e].data) continue;
        qbsection *secs; int ns = gh3_qb_parse(ents[e].data, ents[e].len, &secs); if (ns < 0) continue;
        for (int s = 0; s < ns; s++) { const qbv *v = secs[s].v; if (!v || v->kind != QB_ARRAY) continue;
            if (ents[e].type == K_QB) walk_cams(&c, v, secs[s].id, 0, 0, 0, 0);
            for (int i = 0; i < v->narr; i++) if (v->arr[i] && v->arr[i]->kind == QB_STRUCT) read_struct(&c, v->arr[i]); }
        gh3_qb_free_sections(secs, ns); }
}
void r3d_venueqb_free(r3d_venueqb *v) { free(v->shots); free(v->hidden); free(v->objects); free(v->lights); memset(v, 0, sizeof *v); }
const r3d_object *r3d_venueqb_object(const r3d_venueqb *v, uint32_t key) { for (int i = 0; i < v->nobjects; i++) if (v->objects[i].key == key) return &v->objects[i]; return NULL; }
int r3d_venueqb_hidden(const r3d_venueqb *v, uint32_t key) { for (int i = 0; i < v->nhidden; i++) if (v->hidden[i] == key) return 1; return 0; }

/* ------------------------------------------------------------------ templates */
static const char *UNLIT[] = { "AlphaOnly", "AnimatedTexture", "AnimatedTexture_Billboard", "Atmosphere", "Billboard_Fast", "Color", "Color_and_Texture", "Color_and_Texture_Decal", "D_1VertexColorOnly", "D_4UWibble_VertexColorOnly", "D_4UWibble_VertexColorOnly_infinity",
    "Diffuse_Cubemap_Decal", "Flare", "Fog", "Sky", "Texture_Decal", "TranslucentFloatyShadow", "Wibble_DynamicMask", "Wibble_Pos", "Wibble_Pos_Advanced_VCOnly", "Wibble_Pos_Simple", "Wibble_Simple", "Wibble_Simple_Billboard", "Wibble_Simple_Billboard_Pass0",
    "Wibble_VC_Simple", "Wibble_VC_Simple_Frame2", "Wibble_VC_Simple_Frame2_Frame3", "Wibble_VC_Simple_Frame3" };
static const char *AMB[] = { "Flare_AmbientLit", "Flare_AmbientLit_infinity", "D_1VertexColorAmbientLit" };
static const char *BOARD[] = { "AnimatedTexture_Billboard", "Billboard", "Billboard_Normal", "Billboard_Fast", "Flare", "Flare_AmbientLit", "Flare_AmbientLit_infinity", "Wibble_D_N_S_Billboard", "Wibble_D_N_S_Billboard_Pass0", "Wibble_D_N_S_Billboard_N_S", "Wibble_D_N_S_Billboard_N_S_Pass0", "Wibble_Simple_Billboard", "Wibble_Simple_Billboard_Pass0" };
static const char *ANIM[] = { "AnimatedTexture", "AnimatedTexture_Lit", "AnimatedTexture_Billboard" };
static const char *WIB[] = { "Wibble_Simple", "Wibble_Simple_Billboard", "Wibble_Simple_Billboard_Pass0", "Wibble_VC_Simple" };
#define N(a) (int)(sizeof(a) / sizeof *(a))
static uint32_t kUnlit[N(UNLIT)], kAmb[N(AMB)], kBoard[N(BOARD)], kAnim[N(ANIM)], kWib[N(WIB)]; static int tplReady;
static int in(const uint32_t *k, int n, uint32_t t) { for (int i = 0; i < n; i++) if (k[i] == t) return 1; return 0; }
int r3d_template_flags(uint32_t t) {
    if (!tplReady) { for (int i = 0; i < N(UNLIT); i++) kUnlit[i] = gh3_qbkey(UNLIT[i]); for (int i = 0; i < N(AMB); i++) kAmb[i] = gh3_qbkey(AMB[i]); for (int i = 0; i < N(BOARD); i++) kBoard[i] = gh3_qbkey(BOARD[i]);
        for (int i = 0; i < N(ANIM); i++) kAnim[i] = gh3_qbkey(ANIM[i]); for (int i = 0; i < N(WIB); i++) kWib[i] = gh3_qbkey(WIB[i]); tplReady = 1; }
    return (in(kUnlit, N(UNLIT), t) ? TPL_UNLIT : 0) | (in(kAmb, N(AMB), t) ? TPL_AMB : 0) | (in(kBoard, N(BOARD), t) ? TPL_BOARD : 0) | (in(kAnim, N(ANIM), t) ? TPL_ANIM : 0) | (in(kWib, N(WIB), t) ? TPL_WIB : 0);
}
int r3d_diffuse_slot(uint32_t t) { switch (t) { case 0x16d7efb7: return 4; case 0xa699384e: return 2; case 0x852105c8: case 0x6f3f03f3: case 0x4d7335f8: return 1; default: return -1; } }
