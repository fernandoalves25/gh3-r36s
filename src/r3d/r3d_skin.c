/* The band's file readers (see r3d_skin.h). Each one follows the browser port's reader line by line; the offsets are in
 * docs/research/band.md section 2. */
#include "r3d_skin.h"
#include "gh3fmt.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static uint32_t u32(const uint8_t *b) { return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3]; }
static uint16_t u16(const uint8_t *b) { return (uint16_t)((b[0] << 8) | b[1]); }
static int16_t s16(const uint8_t *b) { return (int16_t)u16(b); }
static float f32(const uint8_t *b) { uint32_t v = u32(b); float f; memcpy(&f, &v, 4); return f; }

/* ------------------------------------------------------------------ pak on disk */
static void pak_index(r3d_pakfile *p, const uint8_t *h, size_t n, long base, int *done) {
    uint32_t K_LAST = gh3_qbkey(".last"), K_LAST2 = gh3_qbkey("last"); size_t q = 0; int cap = 0; *done = 0; p->n = 0;
    while (q + 32 <= n) { uint32_t ty = u32(h + q); if (ty == K_LAST || ty == K_LAST2 || ty == 0) { *done = 1; break; }
        uint32_t fl = u32(h + q + 28); if (fl & 0x20 && q + 32 + 160 > n) break;
        if (p->n == cap) { cap = cap ? cap * 2 : 64; p->e = realloc(p->e, cap * sizeof *p->e); }
        r3d_pkent *e = &p->e[p->n++]; e->type = ty; e->off = (long)q + (long)u32(h + q + 4) - base; e->len = u32(h + q + 8); e->fullKey = u32(h + q + 16); e->nameKey = u32(h + q + 20);
        q += 32 + (fl & 0x20 ? 160 : 0); }
}
int r3d_pak_open(r3d_pakfile *p, const char *pakPath, const char *pabPath) {
    memset(p, 0, sizeof *p); if (!pakPath) return -1;
    FILE *f = fopen(pakPath, "rb"); if (!f) return -1;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    if (pabPath) {   /* the index is the whole .pak; the entries sit in the .pab at (their place in the index + off - the index's size) */
        uint8_t *h = malloc(len > 0 ? len : 1); int done; if (fread(h, 1, len, f) != (size_t)len) { free(h); fclose(f); return -1; } fclose(f);
        pak_index(p, h, len, len, &done); free(h); p->f = fopen(pabPath, "rb"); if (!p->f) { free(p->e); p->e = NULL; return -1; } return 0; }
    size_t have = 0, cap = 0; uint8_t *h = NULL; int done = 0;   /* one file: its list at the start, read in growing pieces until the end mark */
    while (!done && have < (size_t)len) { cap = cap ? cap * 2 : 65536; if (cap > (size_t)len) cap = len; h = realloc(h, cap); size_t got = fread(h + have, 1, cap - have, f); have += got; pak_index(p, h, have, 0, &done); if (!got) break; }
    free(h); p->f = f; return p->n ? 0 : -1;
}
int r3d_pak_find(const r3d_pakfile *p, uint32_t type, uint32_t fullKey) { for (int i = 0; i < p->n; i++) if (p->e[i].type == type && p->e[i].fullKey == fullKey) return i; return -1; }
int r3d_pak_biggest(const r3d_pakfile *p, uint32_t type) { int b = -1; for (int i = 0; i < p->n; i++) if (p->e[i].type == type && (b < 0 || p->e[i].len > p->e[b].len)) b = i; return b; }
uint8_t *r3d_pak_read(r3d_pakfile *p, int i, size_t *n) {
    if (!p->f || i < 0 || i >= p->n || p->e[i].off < 0) return NULL; const r3d_pkent *e = &p->e[i];
    uint8_t *b = malloc(e->len ? e->len : 1); if (fseek(p->f, e->off, SEEK_SET) || fread(b, 1, e->len, p->f) != e->len) { free(b); return NULL; }
    if (n) *n = e->len; return b;
}
void r3d_pak_close(r3d_pakfile *p) { if (p->f) fclose(p->f); free(p->e); memset(p, 0, sizeof *p); }

/* ------------------------------------------------------------------ .ske (readSkeleton) */
int r3d_ske_read(const uint8_t *b, size_t n, r3d_ske *o) {
    memset(o, 0, sizeof *o); if (n < 0x30) return -1;
    int nb = (int)u32(b + 4); uint32_t na = u32(b + 0x10), pa = u32(b + 0x14), posAt = u32(b + 0x28), qAt = u32(b + 0x2c);
    if (nb < 1 || nb > 512 || na + nb * 4 > n || pa + nb * 4 > n || posAt + nb * 16 > n || qAt + nb * 16 > n) return -1;
    o->n = nb; o->key = malloc(nb * 4); o->parent = malloc(nb * sizeof(int)); o->pos = malloc(nb * sizeof *o->pos); o->q = malloc(nb * sizeof *o->q);
    for (int i = 0; i < nb; i++) o->key[i] = u32(b + na + i * 4);
    for (int i = 0; i < nb; i++) { uint32_t pk = u32(b + pa + i * 4); int par = -1; if (pk) for (int k = 0; k < nb; k++) if (o->key[k] == pk) { par = k; break; } o->parent[i] = par < i ? par : -1;
        for (int c = 0; c < 3; c++) o->pos[i][c] = f32(b + posAt + i * 16 + c * 4);
        o->q[i][0] = -f32(b + qAt + i * 16); o->q[i][1] = -f32(b + qAt + i * 16 + 4); o->q[i][2] = -f32(b + qAt + i * 16 + 8); o->q[i][3] = f32(b + qAt + i * 16 + 12); }   /* stored turns run the other way */
    return 0;
}
int r3d_ske_bone(const r3d_ske *s, const char *name) { uint32_t k = gh3_qbkey(name); for (int i = 0; i < s->n; i++) if (s->key[i] == k) return i; return -1; }
void r3d_ske_free(r3d_ske *s) { free(s->key); free(s->parent); free(s->pos); free(s->q); memset(s, 0, sizeof *s); }

/* ------------------------------------------------------------------ .skin (readScene + skinnedMesh) */
static int strip(const uint8_t *b, size_t iAt, int nIdx, int nVert, uint16_t **out) {   /* the strip, cut at 0x7fff / 0xffff, as separate triangles, every other one turned */
    uint16_t *t = malloc((size_t)nIdx * 3 * 2 + 6); int nt = 0, a = -1, c0 = -1, flip = 0;
    for (int k = 0; k < nIdx; k++) { int c = u16(b + iAt + k * 2); if (c == 0x7FFF || c == 0xFFFF || c >= nVert) { a = c0 = -1; flip = 0; continue; }
        if (a >= 0 && c0 >= 0) { if (a != c0 && c0 != c && a != c) { if (flip) { t[nt * 3] = c0; t[nt * 3 + 1] = a; } else { t[nt * 3] = a; t[nt * 3 + 1] = c0; } t[nt * 3 + 2] = c; nt++; } flip = !flip; }
        a = c0; c0 = c; }
    *out = t; return nt;
}
int r3d_skin_read(const uint8_t *b, size_t n, r3d_skin *o) {
    memset(o, 0, sizeof *o); if (n < 0x100) return -1;
    size_t matEnd = 0x20 + u32(b + 0x24); if (matEnd + 0x100 > n) return -1;
    for (size_t p = 0x20 + u32(b + 0x28); p + 0x100 <= matEnd; ) {
        uint32_t size = u32(b + p + 0xc4); if (size < 0x100 || p + size > matEnd) break;
        o->mats = realloc(o->mats, (o->nmats + 1) * sizeof *o->mats); r3d_skinmat *m = &o->mats[o->nmats++]; memset(m, 0, sizeof *m);
        int nt = (int)u32(b + p + 0xb8); if (nt > 8) nt = 8; size_t tOff = p + u32(b + p + 0xbc);
        for (int i = 0; i < nt && tOff + i * 4 + 4 <= p + size; i++) m->tex[m->ntex++] = u32(b + tOff + i * 4);
        m->key = u32(b + p); m->template = u32(b + p + 0xa0); m->flags = u32(b + p + 0xcc); m->blend = u32(b + p + 0xd8); m->alphaRef = b[p + 0xe8]; memcpy(m->clamp, b + p + 0xea, 4);
        p += size; }
    if (u32(b + matEnd) != 0xBABEFACEu) return -1;
    size_t base = matEnd + 8 + u32(b + matEnd + 4); if (base + 0x84 > n) return -1;
    int nMesh = (int)u32(b + base + 0x4c); size_t meshAt = base + u32(b + base + 0x80);
    for (int i = 0; i < nMesh; i++) {
        size_t m = meshAt + (size_t)i * 0x80; if (m + 0x80 > n) break;
        int nIdx = u16(b + m + 0x24), nVert = u16(b + m + 0x26); uint32_t o40 = u32(b + m + 0x40), o5c = u32(b + m + 0x5c), o60 = u32(b + m + 0x60), vSize = u32(b + m + 0x4c);
        if (!nVert || !nIdx || o40 == 0xFFFFFFFFu || o5c == 0xFFFFFFFFu || o60 == 0xFFFFFFFFu) continue;
        size_t sAt = base + o60, vAt = base + o40 + 0x20, iAt = base + o5c + 0x20; if (sAt + 0x20 > n || u32(b + sAt) != 0xCAFEBAB4u) continue;   /* only skinned meshes */
        int uvStride = (int)(vSize / nVert); if (uvStride < 8 || vAt + (size_t)nVert * uvStride > n || iAt + (size_t)nIdx * 2 > n) continue;
        int uvOff = -1, cur = 0; for (int k = 2; k < 16; k++) { int ty = b[vAt - 0x20 + k]; if (!ty) break; if (ty == 0x0A) cur += 4; else if (ty == 0x05) { if (uvOff < 0) uvOff = cur; cur += 8; } }   /* this block's own list: 0x0a a colour, 0x05 a pair of coordinates */
        if (cur != uvStride || uvOff < 0) uvOff = 0;
        r3d_skinmesh me; memset(&me, 0, sizeof me); me.nvert = nVert; me.mat = u32(b + m + 0x14); for (int c = 0; c < 3; c++) me.centre[c] = f32(b + m + c * 4); me.radius = f32(b + m + 12);
        me.pos = malloc(nVert * 12); me.nrm = malloc(nVert * 12); me.uv = malloc(nVert * 8); me.bone = malloc(nVert * 3); me.w = malloc(nVert * 12);
        for (int v = 0; v < nVert; v++) { size_t q = vAt + (size_t)v * uvStride; me.uv[v * 2] = f32(b + q + uvOff); me.uv[v * 2 + 1] = f32(b + q + uvOff + 4); }
        size_t p = sAt + 0x20; int v = 0, bad = 0;
        for (int w = 0; w < 4 && !bad; w++) for (uint32_t g = u32(b + sAt + 0x10 + w * 4); g > 0 && !bad; g--) {
            if (p + 16 > n) { bad = 1; break; } uint32_t num = u32(b + p); int b0 = b[p + 4], b1 = b[p + 5], b2 = b[p + 6]; p += 16;
            if (v + num > (uint32_t)nVert || p + (size_t)num * 56 > n) { bad = 1; break; }
            for (uint32_t k = 0; k < num; k++, v++, p += 56) {
                me.pos[v * 3] = f32(b + p); me.pos[v * 3 + 1] = f32(b + p + 16); me.pos[v * 3 + 2] = f32(b + p + 32); me.nrm[v * 3] = f32(b + p + 12); me.nrm[v * 3 + 1] = f32(b + p + 28); me.nrm[v * 3 + 2] = f32(b + p + 44);
                float w0 = w <= 1 ? 1 : f32(b + p + 48), w1 = w <= 1 ? 0 : f32(b + p + 52), w2 = w >= 3 ? fmaxf(0, 1 - w0 - w1) : 0;
                me.bone[v * 3] = (uint8_t)b0; me.bone[v * 3 + 1] = (uint8_t)(w >= 2 ? b1 : b0); me.bone[v * 3 + 2] = (uint8_t)(w >= 3 ? b2 : b0); me.w[v * 3] = w0; me.w[v * 3 + 1] = w1; me.w[v * 3 + 2] = w2; } }
        if (!bad && v == nVert) me.ntri = strip(b, iAt, nIdx, nVert, &me.idx);
        if (bad || v != nVert || !me.ntri) { free(me.pos); free(me.nrm); free(me.uv); free(me.bone); free(me.w); free(me.idx); continue; }
        o->m = realloc(o->m, (o->nm + 1) * sizeof *o->m); o->m[o->nm++] = me;
    }
    return o->nm ? 0 : -1;
}
const r3d_skinmat *r3d_skin_mat(const r3d_skin *s, uint32_t key) { for (int i = 0; i < s->nmats; i++) if (s->mats[i].key == key) return &s->mats[i]; return NULL; }
void r3d_skin_free(r3d_skin *s) { for (int i = 0; i < s->nm; i++) { free(s->m[i].pos); free(s->m[i].nrm); free(s->m[i].uv); free(s->m[i].bone); free(s->m[i].w); free(s->m[i].idx); } free(s->m); free(s->mats); memset(s, 0, sizeof *s); }

/* ------------------------------------------------------------------ .ska (readSka) */
static int16_t q16(float v) { float x = v * 16384.0f; x = x < -32767 ? -32767 : x > 32767 ? 32767 : x; return (int16_t)lrintf(x); }
int r3d_ska_read(const uint8_t *b, size_t n, uint32_t key, r3d_clip *o) {
    memset(o, 0, sizeof *o); o->key = key; if (n < 0x80 || u32(b + 0x20) != 0x48) return -1;
    uint32_t flags = u32(b + 0x24); int nb = u16(b + 0x2c); size_t qAt = u32(b + 0x58), tAt = u32(b + 0x5c), qS = u32(b + 0x60), tS = u32(b + 0x64), mAt = u32(b + 0x18);
    if (!nb || nb > 512 || qS + nb * 2 > n || tS + nb * 2 > n) return -1;
    o->dur = f32(b + 0x28); if (!(o->dur > 0.05f)) o->dur = 0.05f; o->nb = nb; o->plain = (flags >> 28) == 1; o->delta = (flags & 0x200) != 0; o->land = 0.13f; int wide = (flags & 0x100) != 0;
    int hasMask = mAt >= 0x80 && mAt + 4 + ((nb + 31) / 32) * 4 <= n && u32(b + mAt) == (uint32_t)nb;
    o->b = calloc(nb, sizeof *o->b); o->driven = calloc(nb, 1);
    size_t qp = qAt, tp = tAt; int lead = 1;
    uint16_t *rf = malloc(4096 * 2); int16_t *rq = malloc(4096 * 6); int capR = 4096;
    for (int i = 0; i < nb; i++) {
        size_t qEnd = qp + u16(b + qS + i * 2), tEnd = tp + u16(b + tS + i * 2); int head = lead && tEnd > tp; if (head) lead = 0;
        int uses = !hasMask || ((u32(b + mAt + 4 + (i >> 5) * 4) >> (i & 31)) & 1);
        r3d_track *tr = &o->b[i]; int nr = 0;
        if (!uses || qEnd > n || tEnd > n) { qp = qEnd; tp = tEnd; continue; }
        if (o->plain) {   /* one pose: 16-byte floats, the first entry */
            if (qp + 16 <= qEnd) { float x = -f32(b + qp), y = -f32(b + qp + 4), z = -f32(b + qp + 8), w = f32(b + qp + 12); if (w < 0) { x = -x; y = -y; z = -z; } rf[0] = 0; rq[0] = q16(x); rq[1] = q16(y); rq[2] = q16(z); nr = 1; }
            if (tp + 16 <= tEnd) { tr->nt = 1; tr->tf = calloc(1, 2); tr->tv = malloc(12); for (int c = 0; c < 3; c++) tr->tv[c] = f32(b + tp + c * 4); }
        } else {
            size_t p = qp; int last = 0, wrap = 0, step = wide ? 4 : 2;
            while (p + step <= qEnd) {
                int frame, h; if (wide) { frame = u16(b + p); h = u16(b + p + 2); p += 4; } else { h = u16(b + p); p += 2; frame = (h & 0x7FF) + wrap; if (frame < last) { wrap += 0x800; frame += 0x800; } last = frame; }
                int v[3] = { 0, 0, 0 }, packed = h & 0x4000;
                if (packed && !(h & 0x3800)) p += 2;
                else for (int c = 0; c < 3; c++) { if (packed && (h & (0x2000 >> c))) { v[c] = b[p]; p += 1; } else { if (p & 1) p++; v[c] = s16(b + p); p += 2; } }
                if (p & 1) p++;
                if (p > qEnd) break;
                if (nr == capR) { capR *= 2; rf = realloc(rf, capR * 2); rq = realloc(rq, capR * 6); }
                rf[nr] = (uint16_t)(frame > 65535 ? 65535 : frame); for (int c = 0; c < 3; c++) { int x = -v[c]; rq[nr * 3 + c] = (int16_t)(x < -32767 ? -32767 : x > 32767 ? 32767 : x); } nr++; }
            p = tp; int first = 1, nt = 0, capT = 0;
            while (p + 16 <= tEnd) { int h0 = b[p], frame = (h0 & 0x40) ? (h0 & 0x3F) : ((b[p + 1] << 8) | b[p + 2]);
                if (nt == capT) { capT = capT ? capT * 2 : 16; tr->tf = realloc(tr->tf, capT * 2); tr->tv = realloc(tr->tv, capT * 12); }
                tr->tf[nt] = (uint16_t)frame; for (int c = 0; c < 3; c++) tr->tv[nt * 3 + c] = f32(b + p + 4 + c * 4); nt++; p += 16;
                if (first && head) p += 12; first = 0; }
            tr->nt = nt; }
        if (nr) { tr->nr = nr; tr->rf = malloc(nr * 2); tr->rq = malloc(nr * 6); memcpy(tr->rf, rf, nr * 2); memcpy(tr->rq, rq, nr * 6); }
        o->driven[i] = nr > 1;
        qp = qEnd; tp = tEnd; }
    free(rf); free(rq);
    if (o->b[0].nt > 1) { const float *t = o->b[0].tv; int k = o->b[0].nt - 1; o->travel = hypotf(t[k * 3] - t[0], t[k * 3 + 2] - t[2]); }
    return 0;
}
void r3d_clip_free(r3d_clip *c) { if (c->b) for (int i = 0; i < c->nb; i++) { free(c->b[i].rf); free(c->b[i].rq); free(c->b[i].tf); free(c->b[i].tv); } free(c->b); free(c->driven); memset(c, 0, sizeof *c); }
size_t r3d_clip_bytes(const r3d_clip *c) { size_t s = sizeof *c + c->nb * (sizeof *c->b + 1); for (int i = 0; i < c->nb; i++) s += c->b[i].nr * 8 + c->b[i].nt * 14; return s; }
static void rkey(const int16_t *k, float q[4]) { q[0] = k[0] / 16384.0f; q[1] = k[1] / 16384.0f; q[2] = k[2] / 16384.0f; float w = 1 - q[0] * q[0] - q[1] * q[1] - q[2] * q[2]; q[3] = w > 0 ? sqrtf(w) : 0; }
static int keyat(const uint16_t *f, int n, float fr) { int lo = 0, hi = n - 1; if (fr <= f[0]) return 0; if (fr >= f[n - 1]) return n - 1; while (hi - lo > 1) { int mid = (lo + hi) >> 1; if (f[mid] <= fr) lo = mid; else hi = mid; } return lo; }
int r3d_clip_bone(const r3d_clip *c, const r3d_ske *rest, int bone, float t, float q[4], float p[3]) {
    memcpy(q, rest->q[bone], 16); memcpy(p, rest->pos[bone], 12); if (bone >= c->nb) return 0;
    const r3d_track *tr = &c->b[bone]; int got = 0; float fr = t * 60;
    if (tr->nr) { int k = keyat(tr->rf, tr->nr, fr); float a[4]; rkey(tr->rq + k * 3, a);
        if (k + 1 < tr->nr && fr > tr->rf[k]) { float bq[4], u = (fr - tr->rf[k]) / (float)(tr->rf[k + 1] - tr->rf[k] > 0 ? tr->rf[k + 1] - tr->rf[k] : 1); rkey(tr->rq + (k + 1) * 3, bq); r3q_nlerp(a, bq, u > 1 ? 1 : u, a); }
        if (c->delta) r3q_mul(a, rest->q[bone], q); else memcpy(q, a, 16); got |= 1; }
    if (tr->nt && bone > 0) { int k = keyat(tr->tf, tr->nt, fr); const float *a = tr->tv + k * 3; float v[3] = { a[0], a[1], a[2] };   /* an offset from the rest place; the root's own travel is not applied */
        if (k + 1 < tr->nt && fr > tr->tf[k]) { const float *bb = tr->tv + (k + 1) * 3; float u = (fr - tr->tf[k]) / (float)(tr->tf[k + 1] - tr->tf[k] > 0 ? tr->tf[k + 1] - tr->tf[k] : 1); if (u > 1) u = 1; for (int i = 0; i < 3; i++) v[i] = a[i] + (bb[i] - a[i]) * u; }
        for (int i = 0; i < 3; i++) p[i] += v[i]; got |= 2; }
    return got;
}
void r3d_clip_land(r3d_clip *c, const r3d_ske *rest) {
    static const char *NAMES[4] = { "bone_ik_hand_slave_l", "bone_ik_hand_slave_r", "bone_ik_foot_master_r", "bone_ik_foot_master_l" };
    for (int s = 0; s < 4; s++) { int i = r3d_ske_bone(rest, NAMES[s]); if (i < 0 || i >= c->nb) continue; const r3d_track *tr = &c->b[i]; if (tr->nt < 2) continue;
        int best = 0; float low = 1e30f;
        for (int k = 0; k < tr->nt; k++) { float v[3] = { tr->tv[k * 3], tr->tv[k * 3 + 1], tr->tv[k * 3 + 2] };
            if (s < 2 && tr->nr) { int q = 0; while (q + 1 < tr->nr && tr->rf[q + 1] <= tr->tf[k] + 0.006f) q++; float qq[4], tip[3], d[3] = { 0.35f, 0, 0 }; rkey(tr->rq + q * 3, qq); r3q_rot(qq, d, tip); for (int a = 0; a < 3; a++) v[a] += tip[a]; }   /* the stick's tip */
            float w[3]; r3q_rot(rest->q[0], v, w); if (w[1] < low) { low = w[1]; best = k; } }
        float tb = tr->tf[best] / 60.0f; if (tb > 0.02f && tb < c->dur * 0.7f) c->land = tb; break; }
}

/* ------------------------------------------------------------------ quats */
void r3q_mul(const float a[4], const float b[4], float o[4]) {
    float x = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], y = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
    float z = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3], w = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]; o[0] = x; o[1] = y; o[2] = z; o[3] = w;
}
void r3q_rot(const float q[4], const float v[3], float o[3]) {
    float x = q[0], y = q[1], z = q[2], w = q[3], tx = 2 * (y * v[2] - z * v[1]), ty = 2 * (z * v[0] - x * v[2]), tz = 2 * (x * v[1] - y * v[0]);
    float a = v[0] + w * tx + (y * tz - z * ty), b = v[1] + w * ty + (z * tx - x * tz), c = v[2] + w * tz + (x * ty - y * tx); o[0] = a; o[1] = b; o[2] = c;
}
void r3q_nlerp(const float a[4], const float b[4], float t, float o[4]) {
    float d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3], s = d < 0 ? -1.0f : 1.0f, r[4], n = 0;
    for (int i = 0; i < 4; i++) { r[i] = a[i] * (1 - t) + b[i] * s * t; n += r[i] * r[i]; } n = sqrtf(n); if (n < 1e-8f) { memcpy(o, a, 16); return; } for (int i = 0; i < 4; i++) o[i] = r[i] / n;
}
void r3q_inv(const float q[4], float o[4]) { o[0] = -q[0]; o[1] = -q[1]; o[2] = -q[2]; o[3] = q[3]; }
void r3q_from_basis(const float x[3], const float y[3], const float z[3], float q[4]) {   /* three.js setFromRotationMatrix, columns x y z */
    float m00 = x[0], m10 = x[1], m20 = x[2], m01 = y[0], m11 = y[1], m21 = y[2], m02 = z[0], m12 = z[1], m22 = z[2], tr = m00 + m11 + m22, s;
    if (tr > 0) { s = 0.5f / sqrtf(tr + 1); q[3] = 0.25f / s; q[0] = (m21 - m12) * s; q[1] = (m02 - m20) * s; q[2] = (m10 - m01) * s; }
    else if (m00 > m11 && m00 > m22) { s = 2 * sqrtf(1 + m00 - m11 - m22); q[3] = (m21 - m12) / s; q[0] = 0.25f * s; q[1] = (m01 + m10) / s; q[2] = (m02 + m20) / s; }
    else if (m11 > m22) { s = 2 * sqrtf(1 + m11 - m00 - m22); q[3] = (m02 - m20) / s; q[0] = (m01 + m10) / s; q[1] = 0.25f * s; q[2] = (m12 + m21) / s; }
    else { s = 2 * sqrtf(1 + m22 - m00 - m11); q[3] = (m10 - m01) / s; q[0] = (m02 + m20) / s; q[1] = (m12 + m21) / s; q[2] = 0.25f * s; }
}
