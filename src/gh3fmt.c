#include "gh3fmt.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

const char *GH3_PART_INFIX[4] = { "", "_rhythm", "_guitarcoop", "_rhythmcoop" };
const char *GH3_PART_NAME[4] = { "guitar", "rhythm", "guitarcoop", "rhythmcoop" };
const char *GH3_DIFF_NAME[4] = { "easy", "medium", "hard", "expert" };

static uint32_t CRC[256]; static int crc_ready = 0;
static void crc_setup(void) { if (crc_ready) return; for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = (c & 1) ? (c >> 1) ^ 0xEDB88320u : (c >> 1); CRC[i] = c; } crc_ready = 1; }
uint32_t gh3_qbkey(const char *s) {
    crc_setup(); uint32_t c = 0xFFFFFFFFu;
    for (; *s; s++) { unsigned ch = (unsigned char)*s; if (ch == '/') ch = '\\'; ch = (unsigned)tolower((int)ch); c = (c >> 8) ^ CRC[(c ^ ch) & 0xFF]; }
    return c;
}
static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static uint32_t le32(const uint8_t *p) { return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0]; }
static uint32_t le16(const uint8_t *p) { return ((uint32_t)p[1] << 8) | p[0]; }

/* ------------------------------------------------------------------ PAK */
int gh3_pak_parse(const uint8_t *pak, size_t pakLen, const uint8_t *pab, size_t pabLen, gh3_pakent **out) {
    uint32_t K_LAST = gh3_qbkey(".last"), K_LAST2 = gh3_qbkey("last");
    int cap = 64, n = 0; gh3_pakent *v = malloc(cap * sizeof *v);
    size_t p = 0;
    while (p + 32 <= pakLen) {
        uint32_t type = be32(pak + p), off = be32(pak + p + 4), len = be32(pak + p + 8);
        if (type == K_LAST || type == K_LAST2 || type == 0) break;
        gh3_pakent e; memset(&e, 0, sizeof e);
        e.type = type; e.len = len; e.pakKey = be32(pak + p + 12); e.fullKey = be32(pak + p + 16); e.nameKey = be32(pak + p + 20); e.flags = be32(pak + p + 28);
        int64_t abs = (int64_t)p + off; const uint8_t *src = pak; size_t srcLen = pakLen;
        if (pab) { abs -= (int64_t)pakLen; src = pab; srcLen = pabLen; }
        p += 32;
        if (e.flags & 0x20) { for (size_t i = 0; i < 160 && p + i < pakLen && pak[p + i]; i++) e.filename[i] = (char)pak[p + i]; p += 160; }
        if (abs >= 0 && (uint64_t)abs + len <= srcLen) e.data = src + abs;
        if (n == cap) { cap *= 2; v = realloc(v, cap * sizeof *v); }
        v[n++] = e;
    }
    *out = v; return n;
}

/* ------------------------------------------------------------------ QB */
typedef struct { const uint8_t *b; size_t N; size_t maxEnd; int err; int depth; } qbctx;
static uint32_t u32c(qbctx *c, size_t o) { if (o + 4 > c->N) { c->err = 1; return 0; } return be32(c->b + o); }
static float f32c(qbctx *c, size_t o) { uint32_t u = u32c(c, o); float f; memcpy(&f, &u, 4); return f; }
static void mark(qbctx *c, size_t o) { if (o > c->maxEnd) c->maxEnd = o; }
static size_t al4(size_t o) { return (o + 3) & ~(size_t)3; }
static qbv *newv(int kind) { qbv *v = calloc(1, sizeof *v); v->kind = kind; return v; }
static char *strc(qbctx *c, size_t o) {
    if (o >= c->N) { mark(c, al4(o + 1)); return strdup(""); }
    size_t e = o; while (e < c->N && c->b[e]) e++;
    char *s = malloc(e - o + 1); memcpy(s, c->b + o, e - o); s[e - o] = 0; mark(c, al4(e + 1)); return s;
}
static char *wstrc(qbctx *c, size_t o) {
    size_t cap = 64, n = 0; char *s = malloc(cap); size_t e = o;
    for (; e + 1 < c->N; e += 2) { uint32_t ch = ((uint32_t)c->b[e] << 8) | c->b[e + 1]; if (!ch) break; if (n + 2 > cap) { cap *= 2; s = realloc(s, cap); } s[n++] = ch < 128 ? (char)ch : '?'; }
    s[n] = 0; mark(c, al4(e + 2)); return s;
}
static qbv *value(qbctx *c, int kind, uint32_t raw, size_t at);
static qbv *structc(qbctx *c, size_t o) {
    qbv *r = newv(QB_STRUCT); if (c->depth > 64) { c->err = 1; return r; } c->depth++;
    u32c(c, o); size_t p = u32c(c, o + 4); mark(c, o + 8);
    int guard = 0, cap = 8; r->items = malloc(cap * sizeof *r->items);
    while (p && guard++ < 100000 && !c->err) {
        uint32_t t = u32c(c, p), id = u32c(c, p + 4), raw = u32c(c, p + 8), next = u32c(c, p + 12); mark(c, p + 16); if (c->err) break;
        int kind = (t >> 16) & 0x7F; if (!kind) kind = (t >> 8) & 0xFF;
        qbv *v = value(c, kind, raw, p + 8);
        if (r->nitems == cap) { cap *= 2; r->items = realloc(r->items, cap * sizeof *r->items); }
        r->items[r->nitems].id = id; r->items[r->nitems].kind = kind; r->items[r->nitems].v = v; r->nitems++;
        p = next;
    }
    c->depth--; return r;
}
static qbv *arrayc(qbctx *c, size_t o) {
    qbv *r = newv(QB_ARRAY); if (c->depth > 64) { c->err = 1; return r; } c->depth++;
    uint32_t t = u32c(c, o), cnt = u32c(c, o + 4); mark(c, o + 8); int kind = (t >> 8) & 0xFF; r->arrKind = kind;
    if (cnt == 0 || kind == 0) { mark(c, o + 12 < c->N ? o + 12 : c->N); c->depth--; return r; }
    if (cnt > 4000000 || c->err) { c->err = 1; c->depth--; return r; }
    size_t base; if (cnt == 1) { base = o + 8; mark(c, o + 12); } else { base = u32c(c, o + 8); mark(c, o + 12); mark(c, base + (size_t)cnt * 4); }
    r->arr = calloc(cnt, sizeof(qbv *)); r->narr = (int)cnt;
    for (uint32_t i = 0; i < cnt && !c->err; i++) r->arr[i] = value(c, kind, u32c(c, base + (size_t)i * 4), base + (size_t)i * 4);
    c->depth--; return r;
}
static qbv *value(qbctx *c, int kind, uint32_t raw, size_t at) {
    qbv *v;
    switch (kind) {
    case 0x01: v = newv(QB_INT); v->i = (int32_t)raw; return v;
    case 0x02: v = newv(QB_FLOAT); v->f[0] = f32c(c, at); return v;
    case 0x03: case 0x1B: v = newv(QB_STR); v->s = raw ? strc(c, raw) : strdup(""); return v;
    case 0x04: case 0x1C: v = newv(QB_STR); v->s = raw ? wstrc(c, raw) : strdup(""); return v;
    case 0x05: v = newv(QB_VEC2); if (raw) { v->f[0] = f32c(c, raw + 4); v->f[1] = f32c(c, raw + 8); mark(c, raw + 12); } return v;
    case 0x06: v = newv(QB_VEC3); if (raw) { v->f[0] = f32c(c, raw + 4); v->f[1] = f32c(c, raw + 8); v->f[2] = f32c(c, raw + 12); mark(c, raw + 16); } return v;
    case 0x0A: return raw ? structc(c, raw) : newv(QB_STRUCT);
    case 0x0C: return raw ? arrayc(c, raw) : newv(QB_ARRAY);
    case 0x0D: case 0x1A: case 0x09: v = newv(QB_KEY); v->key = raw; return v;
    default: v = newv(QB_NONE); v->i = kind; v->key = raw; return v;
    }
}
void qb_free(qbv *v) {
    if (!v) return;
    free(v->s);
    for (int i = 0; i < v->nitems; i++) qb_free(v->items[i].v);
    free(v->items);
    for (int i = 0; i < v->narr; i++) qb_free(v->arr[i]);
    free(v->arr); free(v);
}
int gh3_qb_parse(const uint8_t *buf, size_t N, qbsection **out) {
    qbctx c = { buf, N, 0, 0, 0 }; int cap = 32, n = 0; qbsection *s = malloc(cap * sizeof *s);
    size_t p = 28;
    while (p + 20 <= N) {
        uint32_t t = u32c(&c, p); if ((t >> 16) != 0x0020) break;
        int kind = (t >> 8) & 0xFF; uint32_t id = u32c(&c, p + 4), raw = u32c(&c, p + 12); c.maxEnd = p + 20;
        qbv *v;
        if (kind == 0x07) { uint32_t csize = u32c(&c, (size_t)raw + 8); if (c.err) break; mark(&c, al4((size_t)raw + 12 + csize)); v = newv(QB_SCRIPT); }
        else v = value(&c, kind, raw, p + 12);
        if (c.err) { qb_free(v); break; }
        if (n == cap) { cap *= 2; s = realloc(s, cap * sizeof *s); }
        s[n].id = id; s[n].kind = kind; s[n].v = v; n++;
        p = al4(c.maxEnd);
    }
    if (c.err) { gh3_qb_free_sections(s, n); *out = NULL; return -1; }
    *out = s; return n;
}
void gh3_qb_free_sections(qbsection *s, int n) { for (int i = 0; i < n; i++) qb_free(s[i].v); free(s); }
const qbv *qb_getk(const qbv *st, uint32_t key) { if (!st || st->kind != QB_STRUCT) return NULL; for (int i = 0; i < st->nitems; i++) if (st->items[i].id == key) return st->items[i].v; return NULL; }
const qbv *qb_get(const qbv *st, const char *name) { return qb_getk(st, gh3_qbkey(name)); }
double qb_num(const qbv *v, double def) { if (!v) return def; if (v->kind == QB_INT) return v->i; if (v->kind == QB_FLOAT) return v->f[0]; return def; }
const char *qb_str(const qbv *v) { return v && v->kind == QB_STR && v->s ? v->s : ""; }
int qb_keyof(const qbv *v, uint32_t *key) { if (v && v->kind == QB_KEY) { *key = v->key; return 1; } return 0; }

/* ------------------------------------------------------------------ library */
typedef struct { uint32_t id; const qbv *v; } secref;
typedef struct { secref *a; int n, cap; } secmap;
static void secmap_add(secmap *m, uint32_t id, const qbv *v) { if (m->n == m->cap) { m->cap = m->cap ? m->cap * 2 : 1024; m->a = realloc(m->a, m->cap * sizeof *m->a); } m->a[m->n].id = id; m->a[m->n].v = v; m->n++; }
static const qbv *secmap_get(const secmap *m, uint32_t id) { for (int i = 0; i < m->n; i++) if (m->a[i].id == id) return m->a[i].v; return NULL; }
static const qbv *sec(const secmap *m, const char *name) { return secmap_get(m, gh3_qbkey(name)); }
static double num(const secmap *m, const char *name, double def) { return qb_num(sec(m, name), def); }
static void cpystr(char *dst, size_t cap, const char *s) { size_t n = strlen(s); if (n >= cap) n = cap - 1; memcpy(dst, s, n); dst[n] = 0; }
static void lower(char *s) { for (; *s; s++) *s = (char)tolower((unsigned char)*s); }

const char *gh3_library_text(const gh3_library *lib, uint32_t key) { for (int i = 0; i < lib->ntexts; i++) if (lib->texts[i].id == key) return lib->texts[i].s; return ""; }
void gh3_default_tuning(gh3_tuning *t) {
    static const gh3_diff D[4] = { { 2.33, 0.029, -0.053, 0.058 }, { 1.5, 0.0145, -0.0267, 0.058 }, { 1.29, 0.0135, -0.0374, 0.058 }, { 1.11, 0.012, -0.048, 0.058 } };
    t->hmScale = 2.95; t->early = 116; t->late = 100; t->hamEarly = 83; t->hamLate = 83; t->strumBefore = 34; t->formBefore = 25; t->susShorten = 0.25; t->split = 0.5; t->spDrain = 12.5;
    for (int i = 0; i < 4; i++) t->diffs[i] = D[i];
}
static int song_index(const gh3_library *lib, uint32_t key) { for (int i = 0; i < lib->nsongs; i++) if (lib->songs[i].key == key) return i; return -1; }
/* boss battles, as the browser port reads them: the power-ups, each battle song's boss and the per-difficulty numbers */
static int truthy(const qbv *v) { return v && !(v->kind == QB_INT && !v->i) && !(v->kind == QB_FLOAT && !v->f[0]); }
static void perd(const qbv *st, const char *post, double *out) { for (int d = 0; d < 4; d++) { char nm[48]; snprintf(nm, sizeof nm, "%s%s", GH3_DIFF_NAME[d], post); out[d] = qb_num(qb_get(st, nm), 0); } }
static void battle_read(const secmap *m, gh3_library *lib) {
    static const char *IDS[PU_KINDS] = { "lightning", "difficultyup", "doublenotes", "powerupsteal", "leftynotes", "brokenstring", "whammyattack", "deathlick" };
    gh3_battledata *B = &lib->battle; const qbv *pu = sec(m, "battlemode_powerups"); if (!pu || pu->kind != QB_ARRAY || !pu->narr) return;
    B->pu = calloc(pu->narr, sizeof *B->pu);
    for (int i = 0; i < pu->narr; i++) { const qbv *st = pu->arr[i]; uint32_t k; if (!qb_keyof(qb_get(st, "name"), &k)) continue; gh3_powerup *p = &B->pu[B->npu++];
        p->key = k; p->id = -1; for (int j = 0; j < PU_KINDS; j++) if (gh3_qbkey(IDS[j]) == k) p->id = j;
        cpystr(p->title, sizeof p->title, qb_str(qb_get(st, "name_text"))); cpystr(p->alt, sizeof p->alt, qb_str(qb_get(st, "alt_name_text")));
        p->weight = qb_num(qb_get(st, "weight"), 0); p->weightLow = qb_num(qb_get(st, "weight_low"), 0); p->drain = qb_num(qb_get(st, "drain_time"), 0); if (p->drain <= 0) p->drain = 5000;
        p->immediate = truthy(qb_get(st, "immediate")); p->bolt = truthy(qb_get(st, "fire_bolt")); p->noAi = truthy(qb_get(st, "no_ai_damage"));
        perd(st, "_repair", p->repair); perd(st, "_flicker", p->flicker); perd(st, "_kill_rate", p->kill); }
    B->bosses = calloc(lib->nsongs ? lib->nsongs : 1, sizeof *B->bosses);
    for (int s = 0; s < lib->nsongs; s++) { if (!lib->songs[s].isBoss) continue; const qbv *st = secmap_get(m, lib->songs[s].bossKey); if (!st || st->kind != QB_STRUCT) continue;
        gh3_boss *b = &B->bosses[B->nbosses]; b->songIdx = s; const char *nm = qb_str(qb_get(st, "character_name")); cpystr(b->name, sizeof b->name, *nm ? nm : "O chefe");
        const qbv *l = qb_get(st, "powerups"); if (l && l->kind == QB_ARRAY) for (int k = 0; k < l->narr && b->npowerups < 16; k++) { uint32_t pk; if (!qb_keyof(l->arr[k], &pk)) continue; for (int j = 0; j < B->npu; j++) if (B->pu[j].key == pk) { b->powerups[b->npowerups++] = pk; break; } }
        perd(qb_get(st, "gainpernote"), "", b->gain); perd(qb_get(st, "losspernote"), "", b->loss); perd(qb_get(st, "powerupmissednote"), "", b->miss);
        perd(qb_get(st, "whammyspeed"), "", b->whammySpeed); perd(qb_get(st, "brokenstringspeed"), "", b->stringSpeed); perd(qb_get(st, "brokenstringmissednote"), "", b->stringMiss);
        B->nbosses++; }
    B->max = (int)num(m, "max_num_powerups", 3);
    for (int d = 0; d < 4; d++) { char nm[64];
        snprintf(nm, sizeof nm, "battle_%s_drain_offset", GH3_DIFF_NAME[d]); B->offset[d] = num(m, nm, 0);
        snprintf(nm, sizeof nm, "health_change_good_boss_%s", GH3_DIFF_NAME[d]); B->good[d] = num(m, nm, 0.015);
        snprintf(nm, sizeof nm, "health_change_bad_boss_%s", GH3_DIFF_NAME[d]); B->bad[d] = num(m, nm, -0.04); }
}
const gh3_boss *gh3_library_boss(const gh3_library *lib, int si) { for (int i = 0; i < lib->battle.nbosses; i++) if (lib->battle.bosses[i].songIdx == si) return lib->battle.bosses[i].npowerups ? &lib->battle.bosses[i] : NULL; return NULL; }

int gh3_library_read(const uint8_t *pak, size_t pakLen, const uint8_t *pab, size_t pabLen, gh3_library *lib) {
    memset(lib, 0, sizeof *lib); gh3_default_tuning(&lib->tuning);
    gh3_pakent *ents; int ne = gh3_pak_parse(pak, pakLen, pab, pabLen, &ents);
    uint32_t K_QB = gh3_qbkey(".qb");
    secmap m = { 0 }; qbsection **keep = calloc(ne ? ne : 1, sizeof *keep); int *keepn = calloc(ne ? ne : 1, sizeof *keepn);
    for (int i = 0; i < ne; i++) {
        if (ents[i].type != K_QB || !ents[i].data) continue;
        qbsection *secs; int ns = gh3_qb_parse(ents[i].data, ents[i].len, &secs); if (ns < 0) continue;
        keep[i] = secs; keepn[i] = ns;
        for (int k = 0; k < ns; k++) if (secs[k].kind != 0x07 && !secmap_get(&m, secs[k].id)) secmap_add(&m, secs[k].id, secs[k].v);
    }
    /* every string section, kept for look-ups by key (section names of the charts live here) */
    { int cap2 = 1024; lib->texts = malloc(cap2 * sizeof *lib->texts);
      for (int i = 0; i < m.n; i++) { const qbv *v = m.a[i].v; if (!v || v->kind != QB_STR || !v->s || !v->s[0]) continue;
          if (lib->ntexts == cap2) { cap2 *= 2; lib->texts = realloc(lib->texts, cap2 * sizeof *lib->texts); }
          lib->texts[lib->ntexts].id = m.a[i].id; lib->texts[lib->ntexts].s = strdup(v->s); lib->ntexts++; } }
    /* songs */
    int cap = 128; lib->songs = malloc(cap * sizeof *lib->songs);
    const char *lists[2] = { "permanent_songlist_props", "download_songlist_props" };
    for (int L = 0; L < 2; L++) {
        const qbv *st = sec(&m, lists[L]); if (!st || st->kind != QB_STRUCT) continue;
        for (int i = 0; i < st->nitems; i++) {
            const qbitem *it = &st->items[i]; if (it->kind != 0x0A || !it->v) continue;
            const char *name = qb_str(qb_get(it->v, "name")); if (!*name) continue;
            gh3_song s; memset(&s, 0, sizeof s); s.key = it->id;
            cpystr(s.name, sizeof s.name, name); lower(s.name);
            const char *title = qb_str(qb_get(it->v, "title")); cpystr(s.title, sizeof s.title, *title ? title : name);
            cpystr(s.artist, sizeof s.artist, qb_str(qb_get(it->v, "artist")));
            const char *year = qb_str(qb_get(it->v, "year")); while (*year == ',' || *year == ' ') year++; cpystr(s.year, sizeof s.year, year);
            s.hmScale = (float)qb_num(qb_get(it->v, "hammer_on_measure_scale"), 0);
            s.bandVol = (float)qb_num(qb_get(it->v, "band_playback_volume"), 0);
            s.guitarVol = (float)qb_num(qb_get(it->v, "guitar_playback_volume"), 0);
            const qbv *rt = qb_get(it->v, "rhythm_track"); s.hasRhythm = !(rt && rt->kind == QB_INT && rt->i == 0);
            s.isBoss = qb_keyof(qb_get(it->v, "boss"), &s.bossKey);
            if (lib->nsongs == cap) { cap *= 2; lib->songs = realloc(lib->songs, cap * sizeof *lib->songs); }
            lib->songs[lib->nsongs++] = s;
        }
    }
    /* tiers */
    int *placed = calloc(lib->nsongs ? lib->nsongs : 1, sizeof *placed);
    int tcap = 32; lib->tiers = malloc(tcap * sizeof *lib->tiers);
    /* the career (gh3_career_songs: 8 tiers of 4 songs, a boss battle in some, an encore last), then the bonus and
     * downloaded lists; gh3_general_songs only adds the co-op encores, which go in a last set */
    const char *tl[4] = { "gh3_career_songs", "gh3_bonus_songs", "gh3_download_songs", "gh3_general_songs" };
    uint32_t K_ENCORE_FLAG = 0xbd963c89u; /* the anonymous flag every career tier but the last carries (the last has no encore) */
    for (int L = 0; L < 4; L++) {
        const qbv *st = sec(&m, tl[L]); if (!st || st->kind != QB_STRUCT) continue;
        for (int i = 0; i < st->nitems; i++) {
            const qbitem *it = &st->items[i]; if (it->kind != 0x0A || !it->v) continue;
            const qbv *arr = qb_get(it->v, "songs"); if (!arr || arr->kind != QB_ARRAY) continue;
            gh3_tier t; memset(&t, 0, sizeof t); t.tab = L; t.career = L == 0; t.encoreIdx = t.bossIdx = -1; t.songIdx = malloc((arr->narr + 1) * sizeof(int));
            for (int k = 0; k < arr->narr; k++) { uint32_t key; if (!qb_keyof(arr->arr[k], &key)) continue; int si = song_index(lib, key); if (si >= 0 && !placed[si]) { placed[si] = 1; t.songIdx[t.nsongs++] = si; } }
            if (!t.nsongs) { free(t.songIdx); continue; }
            const char *title = qb_str(qb_get(it->v, "title")); cpystr(t.title, sizeof t.title, *title ? title : tl[L]);
            if (t.career) {
                t.defaultUnlocked = (int)qb_num(qb_get(it->v, "defaultunlocked"), 0);
                for (int k = 0; k < it->v->nitems; k++) { uint32_t fk; if (it->v->items[k].id == 0 && qb_keyof(it->v->items[k].v, &fk) && fk == K_ENCORE_FLAG) t.hasEncore = 1; }
                for (int k = 0; k < t.nsongs; k++) if (lib->songs[t.songIdx[k]].isBoss) t.bossIdx = t.songIdx[k];
                if (t.hasEncore) for (int k = t.nsongs - 1; k >= 0; k--) if (!lib->songs[t.songIdx[k]].isBoss) { t.encoreIdx = t.songIdx[k]; break; }
            }
            if (lib->ntiers == tcap) { tcap *= 2; lib->tiers = realloc(lib->tiers, tcap * sizeof *lib->tiers); }
            lib->tiers[lib->ntiers++] = t;
        }
    }
    /* the general list's own tiers were only mined for their extra songs: merge them into one set */
    { int first = -1; for (int i = 0; i < lib->ntiers; i++) if (lib->tiers[i].tab == 3) { first = i; break; }
      if (first >= 0) { gh3_tier *g = &lib->tiers[first]; g->songIdx = realloc(g->songIdx, (lib->nsongs + 1) * sizeof(int));
          for (int i = first + 1; i < lib->ntiers; i++) { for (int k = 0; k < lib->tiers[i].nsongs; k++) g->songIdx[g->nsongs++] = lib->tiers[i].songIdx[k]; free(lib->tiers[i].songIdx); }
          lib->ntiers = first + 1; cpystr(g->title, sizeof g->title, "Co-op encores"); } }
    { gh3_tier t; memset(&t, 0, sizeof t); t.tab = 3; t.encoreIdx = t.bossIdx = -1; t.songIdx = malloc((lib->nsongs + 1) * sizeof(int)); for (int i = 0; i < lib->nsongs; i++) if (!placed[i]) t.songIdx[t.nsongs++] = i;
      if (t.nsongs) { cpystr(t.title, sizeof t.title, "Everything else"); if (lib->ntiers == tcap) { tcap *= 2; lib->tiers = realloc(lib->tiers, tcap * sizeof *lib->tiers); } lib->tiers[lib->ntiers++] = t; } else free(t.songIdx); }
    free(placed);
    /* tuning */
    gh3_tuning *tn = &lib->tuning;
    const qbv *dl = sec(&m, "difficulty_list_props");
    for (int d = 0; d < 4; d++) {
        const qbv *st = qb_get(dl, GH3_DIFF_NAME[d]); double sc = qb_num(qb_get(st, "scroll_time"), -1), gs = qb_num(qb_get(st, "game_speed"), -1);
        if (sc >= 0 && gs > 0) tn->diffs[d].lookahead = sc / gs;
        char nm[64];
        snprintf(nm, sizeof nm, "health_change_good_%s", GH3_DIFF_NAME[d]); tn->diffs[d].good = num(&m, nm, 0.013);
        snprintf(nm, sizeof nm, "health_change_bad_%s", GH3_DIFF_NAME[d]); tn->diffs[d].bad = num(&m, nm, -0.04);
        snprintf(nm, sizeof nm, "health_change_star_%s", GH3_DIFF_NAME[d]); tn->diffs[d].star = num(&m, nm, 0.058);
    }
    tn->hmScale = num(&m, "default_hammer_on_measure_scale", 2.95);
    tn->early = num(&m, "check_time_early", 0.116) * 1000; tn->late = num(&m, "check_time_late", 0.1) * 1000;
    tn->hamEarly = num(&m, "hammer_time_early", 0.083) * 1000; tn->hamLate = num(&m, "hammer_time_late", 0.083) * 1000;
    tn->strumBefore = num(&m, "strum_before_forming", 0.034) * 1000; tn->formBefore = num(&m, "form_before_strumming", 0.025) * 1000;
    tn->susShorten = num(&m, "whammy_shorten", 0.25); tn->split = num(&m, "split_late_percent", 0.5); tn->spDrain = num(&m, "star_power_drain_rate", 12.5);
    battle_read(&m, lib);
    for (int i = 0; i < ne; i++) if (keep[i]) gh3_qb_free_sections(keep[i], keepn[i]);
    free(keep); free(keepn); free(m.a); free(ents);
    return lib->nsongs ? 0 : -1;
}

/* ------------------------------------------------------------------ song pak */
static const qbv *arrsec(const qbsection *secs, int ns, const char *name, const char *suffix) {
    char nm[160]; snprintf(nm, sizeof nm, "%s%s", name, suffix); uint32_t k = gh3_qbkey(nm);
    for (int i = 0; i < ns; i++) if (secs[i].id == k) { const qbv *v = secs[i].v; return v && v->kind == QB_ARRAY && v->narr > 0 ? v : NULL; }
    return NULL;
}
static int arr_int(const qbv *a, int i) { const qbv *v = a->arr[i]; return v ? (v->kind == QB_INT ? v->i : (int)qb_num(v, 0)) : 0; }
int gh3_songpak_read(const uint8_t *pak, size_t len, const char *name, gh3_songdata *sd) {
    memset(sd, 0, sizeof *sd); cpystr(sd->name, sizeof sd->name, name); lower(sd->name);
    gh3_pakent *ents; int ne = gh3_pak_parse(pak, len, NULL, 0, &ents);
    uint32_t K_QB = gh3_qbkey(".qb"); char nm[160]; snprintf(nm, sizeof nm, "%s_fretbars", sd->name); uint32_t kfb = gh3_qbkey(nm);
    qbsection *best = NULL; int bestn = 0;
    for (int i = 0; i < ne; i++) {
        if (ents[i].type != K_QB || !ents[i].data) continue;
        qbsection *s; int ns = gh3_qb_parse(ents[i].data, ents[i].len, &s); if (ns < 0) continue;
        int has = 0; for (int k = 0; k < ns; k++) if (s[k].id == kfb) { has = 1; break; }
        if (has) { if (best) gh3_qb_free_sections(best, bestn); best = s; bestn = ns; break; }
        if (!best || ns > bestn) { if (best) gh3_qb_free_sections(best, bestn); best = s; bestn = ns; } else gh3_qb_free_sections(s, ns);
    }
    free(ents);
    if (!best) return -1;
    const qbv *a;
    if ((a = arrsec(best, bestn, sd->name, "_fretbars"))) { sd->fretbars = malloc(a->narr * sizeof(int32_t)); for (int i = 0; i < a->narr; i++) sd->fretbars[i] = arr_int(a, i); sd->nfret = a->narr; }
    if ((a = arrsec(best, bestn, sd->name, "_timesig"))) { sd->timesig = calloc(a->narr, sizeof *sd->timesig); for (int i = 0; i < a->narr; i++) { const qbv *e = a->arr[i]; if (e && e->kind == QB_ARRAY) for (int k = 0; k < 3 && k < e->narr; k++) sd->timesig[i][k] = arr_int(e, k); } sd->nsig = a->narr; }
    if ((a = arrsec(best, bestn, sd->name, "_markers"))) { sd->markers = calloc(a->narr, sizeof *sd->markers);
        for (int i = 0; i < a->narr; i++) { const qbv *e = a->arr[i]; const qbv *t = qb_get(e, "time"); if (!t) continue; gh3_marker *mk = &sd->markers[sd->nmarkers++]; mk->t = (int)qb_num(t, 0); const qbv *k = qb_get(e, "marker"); cpystr(mk->text, sizeof mk->text, qb_str(k)); qb_keyof(k, &mk->key); } }
    for (int p = 0; p < 4; p++) for (int d = 0; d < 4; d++) {
        char suf[96]; snprintf(suf, sizeof suf, "_song%s_%s", GH3_PART_INFIX[p], GH3_DIFF_NAME[d]);
        a = arrsec(best, bestn, sd->name, suf); if (!a || a->narr < 3) continue;
        gh3_partdiff *pd = &sd->parts[p][d]; pd->raw = malloc(a->narr * sizeof(int32_t)); for (int i = 0; i < a->narr; i++) pd->raw[i] = arr_int(a, i); pd->nraw = a->narr;
        snprintf(suf, sizeof suf, "%s_%s_star", GH3_PART_INFIX[p], GH3_DIFF_NAME[d]);
        const qbv *s = arrsec(best, bestn, sd->name, suf);
        if (s) { pd->star = calloc(s->narr, sizeof *pd->star); for (int i = 0; i < s->narr; i++) { const qbv *e = s->arr[i]; if (e && e->kind == QB_ARRAY && e->narr >= 2) { pd->star[pd->nstar][0] = arr_int(e, 0); pd->star[pd->nstar][1] = arr_int(e, 1); pd->nstar++; } } }
        snprintf(suf, sizeof suf, "%s_%s_starbattlemode", GH3_PART_INFIX[p], GH3_DIFF_NAME[d]);
        if ((s = arrsec(best, bestn, sd->name, suf))) { pd->battle = calloc(s->narr, sizeof *pd->battle); for (int i = 0; i < s->narr; i++) { const qbv *e = s->arr[i]; if (e && e->kind == QB_ARRAY && e->narr >= 2) { pd->battle[pd->nbattle][0] = arr_int(e, 0); pd->battle[pd->nbattle][1] = arr_int(e, 1); pd->nbattle++; } } }
    }
    if ((a = arrsec(best, bestn, sd->name, "_scripts"))) { sd->cues = calloc(a->narr, sizeof *sd->cues);
        for (int i = 0; i < a->narr; i++) { const qbv *e = a->arr[i]; const qbv *t = qb_get(e, "time"); uint32_t k; if (!t || !qb_keyof(qb_get(e, "scr"), &k)) continue; sd->cues[sd->ncues].t = (int)qb_num(t, 0); sd->cues[sd->ncues].scr = k; sd->ncues++; }
        for (int i = 1; i < sd->ncues; i++) { gh3_cue c = sd->cues[i]; int j = i; while (j > 0 && sd->cues[j - 1].t > c.t) { sd->cues[j] = sd->cues[j - 1]; j--; } sd->cues[j] = c; } }
    gh3_qb_free_sections(best, bestn);
    return sd->nfret ? 0 : -1;
}
void gh3_songdata_free(gh3_songdata *sd) { free(sd->fretbars); free(sd->timesig); free(sd->markers); free(sd->cues); for (int p = 0; p < 4; p++) for (int d = 0; d < 4; d++) { free(sd->parts[p][d].raw); free(sd->parts[p][d].star); free(sd->parts[p][d].battle); } memset(sd, 0, sizeof *sd); }

/* ------------------------------------------------------------------ chart */
double gh3_beatpos(const int32_t *fb, int nfb, double t) {
    if (nfb < 2) return 0; if (t <= fb[0]) return 0;
    int lo = 0, hi = nfb - 2; /* the largest i <= nfb-2 with fb[i] <= t */
    while (lo < hi) { int m = (lo + hi + 1) >> 1; if (fb[m] <= t) lo = m; else hi = m - 1; }
    double a = fb[lo], c = fb[lo + 1]; double gap = c - a; if (gap < 1) gap = 1;
    return lo + (t - a) / gap;
}
static int popcount5(int m) { int c = 0; for (int i = 0; i < 5; i++) if (m & (1 << i)) c++; return c; }
int gh3_chart_build(const gh3_songdata *sd, int part, int diff, double hmScale, double shorten, gh3_chart *out) {
    memset(out, 0, sizeof *out);
    const gh3_partdiff *src = &sd->parts[part][diff]; if (!src->raw || src->nraw < 3) return -1;
    static const int32_t FB2[2] = { 0, 500 };
    const int32_t *fb = sd->nfret > 1 ? sd->fretbars : FB2; int nfb = sd->nfret > 1 ? sd->nfret : 2;
    out->fretbars = fb; out->nfret = nfb;
    double beat0 = fb[1] - fb[0]; if (beat0 < 1) beat0 = 1;
    double cut = (shorten > 0 ? shorten : shorten == 0 ? 0 : 0.25) * beat0;
    static const int SIG1[1][3] = { { 0, 4, 4 } };
    const int (*sigs)[3] = sd->nsig ? (const int (*)[3])sd->timesig : SIG1; int nsig = sd->nsig ? sd->nsig : 1;
    out->notes = malloc((src->nraw / 3 + 1) * sizeof *out->notes);
    int b = 0, sg = -1; gh3_note *prev = NULL;
    for (int i = 0; i + 2 < src->nraw; i += 3) {
        int t = src->raw[i], len = src->raw[i + 1], bits = src->raw[i + 2];
        int mask = bits & 31; if (!mask) continue;
        while (b + 2 < nfb && fb[b + 1] <= t) b++;
        while (sg + 1 < nsig && sigs[sg + 1][0] <= fb[b]) sg++;
        double beat = fb[b + 1] - fb[b]; if (beat < 1) beat = 1;
        int lowerv = sg >= 0 && sigs[sg][2] > 0 ? sigs[sg][2] : 4;
        double reach = beat / hmScale * (double)(lowerv / 4);
        int n = popcount5(mask);
        int hopo = prev && n == 1 && mask != prev->mask && (t - prev->t) <= reach;
        if (bits & 32) hopo = prev ? !hopo : 0;
        gh3_note *nt = &out->notes[out->nnotes++]; memset(nt, 0, sizeof *nt);
        nt->t = t; nt->len = len; nt->mask = mask; nt->n = n; nt->hopo = hopo; nt->sus = len > beat0 * 0.5 ? (int)fmax(0, len - cut) : 0; nt->star = -1;
        prev = nt;
    }
    out->phrases = calloc(src->nstar + 1, sizeof *out->phrases);
    for (int k = 0; k < src->nstar; k++) {
        int st = src->star[k][0], sl = src->star[k][1]; gh3_phrase *ph = &out->phrases[out->nphrases]; ph->t = st; ph->end = st + sl;
        for (int i = 0; i < out->nnotes; i++) if (out->notes[i].t >= st && out->notes[i].t < st + sl) { out->notes[i].star = out->nphrases; ph->total++; }
        out->nphrases++;
    }
    out->bars = malloc((nfb + 1) * sizeof *out->bars);
    { int si = 0, cnt = 0; for (int i = 0; i < nfb; i++) { while (si + 1 < nsig && sigs[si + 1][0] <= fb[i] + 1) { si++; cnt = 0; } int numv = sigs[si][1] > 0 ? sigs[si][1] : 1; out->bars[i].t = fb[i]; out->bars[i].strong = (cnt % numv) == 0; cnt++; } out->nbars = nfb; }
    double base = 0;
    for (int i = 0; i < out->nnotes; i++) { gh3_note *nt = &out->notes[i]; base += 50 * nt->n; if (nt->sus) base += 25 * nt->n * (gh3_beatpos(fb, nfb, nt->t + nt->sus) - gh3_beatpos(fb, nfb, nt->t)); }
    out->base = base;
    int last = out->nnotes ? out->notes[out->nnotes - 1].t + out->notes[out->nnotes - 1].len : 0;
    out->end = fb[nfb - 1] > last ? fb[nfb - 1] : last;
    return out->nnotes ? 0 : -1;
}
void gh3_chart_free(gh3_chart *c) { free(c->notes); free(c->phrases); free(c->bars); memset(c, 0, sizeof *c); }

/* ------------------------------------------------------------------ FSB3 */
static const uint8_t FSB_KEY[10] = { 0x35, 0x61, 0x74, 0x75, 0x36, 0x77, 0x34, 0x7A, 0x61, 0x77 };
static uint8_t REV[256]; static int rev_ready = 0;
static void rev_setup(void) { if (rev_ready) return; for (int i = 0; i < 256; i++) { int r = 0; for (int k = 0; k < 8; k++) if (i & (1 << k)) r |= 0x80 >> k; REV[i] = (uint8_t)r; } rev_ready = 1; }
static void fsb_dec(uint8_t *b, size_t from, size_t to) { rev_setup(); for (size_t i = from; i < to; i++) b[i] = REV[b[i] ^ FSB_KEY[i % 10]]; }
void gh3_fsb_descramble(uint8_t *b, size_t n, size_t at) { rev_setup(); for (size_t i = 0; i < n; i++) b[i] = REV[b[i] ^ FSB_KEY[(at + i) % 10]]; }
static const char *ROLES[FSB_ROLES] = { "guitar", "rhythm", "song", "preview", "coop_guitar", "coop_rhythm", "coop_song" };
int gh3_fsb_read(uint8_t *bytes, size_t N, const uint8_t *dat, size_t datLen, const char *songName, gh3_fsb *out) {
    memset(out, 0, sizeof *out); out->bytes = bytes; out->n = N; if (N < 24) return -1;
    out->plain = bytes[0] == 'F' && bytes[1] == 'S' && bytes[2] == 'B';
    if (!out->plain) fsb_dec(bytes, 0, 24);
    if (memcmp(bytes, "FSB3", 4) != 0) return -1;
    uint32_t count = le32(bytes + 4), shSize = le32(bytes + 8), mode = le32(bytes + 20);
    size_t dataStart = 24 + shSize; if (dataStart > N) return -1;
    if (!out->plain) fsb_dec(bytes, 24, dataStart);
    out->streams = calloc(count + 1, sizeof *out->streams);
    size_t p = 24, off = dataStart;
    for (uint32_t i = 0; i < count && p + 8 <= dataStart; i++) {
        gh3_fsbstream *s = &out->streams[out->nstreams]; s->role = -1; s->freq = 44100; s->channels = 2;
        uint32_t size;
        if (i == 0 || !(mode & 2)) {
            uint32_t hs = le16(bytes + p); if (p + 64 > dataStart) break;
            for (int k = 0; k < 30 && bytes[p + 2 + k]; k++) s->name[k] = (char)tolower(bytes[p + 2 + k]);
            s->samples = le32(bytes + p + 32); size = le32(bytes + p + 36); s->freq = le32(bytes + p + 52); s->channels = le16(bytes + p + 62);
            p += hs ? hs : 80;
        } else { s->samples = le32(bytes + p); size = le32(bytes + p + 4); p += 8; if (out->nstreams) { s->freq = out->streams[0].freq; s->channels = out->streams[0].channels; } }
        s->offset = (uint32_t)off; s->size = off + size <= N ? size : (uint32_t)(N > off ? N - off : 0);
        off += size; out->nstreams++;
    }
    int mapped = 0;
    if (dat && datLen >= 8 && songName) {
        uint32_t n = be32(dat);
        for (int r = 0; r < FSB_ROLES; r++) {
            char nm[96]; snprintf(nm, sizeof nm, "%s_%s", songName, ROLES[r]); uint32_t k = gh3_qbkey(nm);
            for (uint32_t i = 0; i < n && 8 + (size_t)i * 20 + 8 <= datLen; i++) if (be32(dat + 8 + i * 20) == k) { uint32_t ix = be32(dat + 12 + i * 20); if (ix < (uint32_t)out->nstreams) { out->streams[ix].role = r; mapped = 1; } break; }
        }
    }
    if (!mapped) for (int i = 0; i < out->nstreams; i++) {
        char *nm = out->streams[i].name, *dot = strrchr(nm, '.'); size_t L = dot ? (size_t)(dot - nm) : strlen(nm);
        for (int r = 0; r < FSB_ROLES; r++) { size_t rl = strlen(ROLES[r]); if (L > rl + 1 && nm[L - rl - 1] == '_' && !strncmp(nm + L - rl, ROLES[r], rl)) { out->streams[i].role = r; break; } }
    }
    return 0;
}
int gh3_fsb_has(const gh3_fsb *f, int role) { for (int i = 0; i < f->nstreams; i++) if (f->streams[i].role == role && f->streams[i].size > 0) return 1; return 0; }
const uint8_t *gh3_fsb_extract(gh3_fsb *f, int role, size_t *size) {
    for (int i = 0; i < f->nstreams; i++) { gh3_fsbstream *s = &f->streams[i]; if (s->role != role || !s->size) continue;
        if (!f->plain && !s->done) { fsb_dec(f->bytes, s->offset, (size_t)s->offset + s->size); s->done = 1; }
        *size = s->size; return f->bytes + s->offset; }
    *size = 0; return NULL;
}
