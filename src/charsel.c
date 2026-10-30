/* Your rocker, outfit, guitar and finish (see charsel.h). As GH3's character hub (menu_character_hub, the select
 * character / outfit / style / guitar / finish menus): after Career or Quickplay the hub lists START, CHANGE ROCKER,
 * CHANGE OUTFIT and CHANGE GUITAR; each opens its list beside the rocker's photo, the guitar's list opens that guitar's
 * finishes. What is locked shows its price, and A on it goes to the store's shelf with it (B there comes back here).
 *   * Rockers: the guitarists of musician_profiles (fullname, blurb); the store's secret list (Slash and Tom Morello
 *     $10.000, Lou $15.000, God of Rock $20.000, the Grim Ripper $10.000) are bought, the eight others are yours.
 *   * Outfits: each profile's outfits x styles, their files Guitarist_<name>_Outfit<o>_Style<s> in musician_body; only
 *     the ones whose file is on disk can be worn (the PC copy carries the first style of the first outfit of each).
 *   * Guitars: musician_instrument, one guitar per name (its finishes listed under it). The player starts with the
 *     three of the store's default list (X-Plorer Alpine White, SG Worn Red, Les Paul Ebony); the guitars and special
 *     guitars for sale and the finishes for sale cost what the store's lists say ($200 to $10.000; a finish $250-$500,
 *     sold once the guitar is yours). "The rocker's own" plays the profile's guitar. Guitars with no file on disk can be
 *     bought and chosen; the 3D stage then puts the rocker's own guitar in their hands.
 * The stories and the outfit, style and finish names are shown in Portuguese (charsel_pt.c), the data's English where
 * it has none. The choice is kept in gh3.cfg (roqueiro / visual / guitarra); the 3D band (r3d_band.c) and the results' magazine
 * cover (orig_results.c) use it. */
#include "charsel.h"
#include "app.h"
#include "store.h"
#include "gamearts.h"
#include "original.h"
#include "render.h"
#include "audio.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define K_ID 0x40c698afu        /* id = <desc_id> in the store's lists */
#define K_CHAR 0xf8b75377u      /* the rocker a store item is for */
#define K_LINE 0x0c17c7b9u      /* a guitar's model line ("Standard", "Custom") */
#define K_MAKER 0x1b7165ecu     /* its maker ("Gibson", "Epiphone") */
#define K_STYLEIDS 0x53a9fd23u  /* an outfit's style ids ("axel_01s1") */
#define L_DEFAULT 0xb0a6a7f4u   /* the guitars a player starts with */
#define L_GUITARS 0x6b999e82u   /* guitars for sale */
#define L_FINISHES 0x5e190e34u  /* finishes for sale */
#define L_SPECIAL 0xbc02c6adu   /* special guitars for sale */
#define L_ROCKERS 0x1c11400au   /* secret rockers for sale */

static cs_data D; static int built, demo;
static uint32_t be32(const uint8_t *b) { return (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3]; }
static const qbv *S(const char *name) { return gh3_library_section(A.lib, gh3_qbkey(name)); }
static void base_of(const char *path, char *out, size_t n) {   /* "pak\models\guitars\SG_WornRed\SG_WornRed.pak" -> "SG_WornRed" */
    const char *b = path; for (const char *p = path; *p; p++) if (*p == '\\' || *p == '/') b = p + 1;
    snprintf(out, n, "%s", b); char *d = strstr(out, ".pak"); if (!d) d = strstr(out, ".PAK"); if (d) *d = 0;
}
static int on_disk(const char *base) { char fn[96]; snprintf(fn, sizeof fn, "%s.pak.xen", base); return *base && app_data_path("pak", fn) != NULL; }
static const qbv *by_desc(const qbv *list, uint32_t id) { uint32_t k; if (list && list->kind == QB_ARRAY) for (int i = 0; i < list->narr; i++) if (qb_keyof(qb_get(list->arr[i], "desc_id"), &k) && k == id) return list->arr[i]; return NULL; }
static int listed(uint32_t sec, uint32_t id, int *price) {   /* id in one of the store's lists (and its price) */
    const qbv *l = gh3_library_section(A.lib, sec); uint32_t k; if (!l || l->kind != QB_ARRAY) return 0;
    for (int i = 0; i < l->narr; i++) { const qbv *e = l->arr[i]; if ((e && e->kind == QB_KEY && e->key == id) || (qb_keyof(qb_getk(e, K_ID), &k) && k == id)) { if (price) *price = (int)qb_num(qb_get(e, "price"), 0); return 1; } }
    return 0;
}
static const char *clean(const qbv *v) {   /* a blurb: its "\n" (written out in the data) as spaces, in place; NULL when empty */
    if (!v || v->kind != QB_STR || !v->s || !*v->s) return NULL; char *o = v->s;
    for (const char *p = v->s; *p; p++) { if (p[0] == '\\' && p[1] == 'n') { p++; if (o > v->s && o[-1] != ' ') *o++ = ' '; continue; } if (*p == ' ' && o > v->s && o[-1] == ' ') continue; *o++ = *p; }
    *o = 0; return v->s;
}
static void maker_line(const cs_guitar *g, char *out, size_t n) {   /* "Gibson Custom" (the line left out when it is the finish, the maker when the line names it) */
    const char *ln = strcmp(g->line, g->finish) ? g->line : ""; snprintf(out, n, "%s%s%s", strncmp(ln, g->maker, strlen(g->maker)) || !*g->maker ? g->maker : "", *ln && strncmp(ln, g->maker, strlen(g->maker)) ? " " : "", ln);
}
static void trim(const char *s, char *out, size_t n) { while (*s == ' ') s++; snprintf(out, n, "%s", s); size_t L = strlen(out); while (L && out[L - 1] == ' ') out[--L] = 0; }
static void say(int kind, char *s, size_t n) { const char *pt = charsel_pt_name(kind, s); if (pt != s) snprintf(s, n, "%s", pt); }   /* a name in Portuguese, in place */
static const char *blurb_of(const char *pt, const qbv *v) { return pt ? pt : clean(v); }   /* the Portuguese story, else the data's English */
static void money(int n, char *out, size_t cap) {   /* $10.000 */
    char raw[24]; snprintf(raw, sizeof raw, "%d", n < 0 ? 0 : n); int L = (int)strlen(raw), o = 0; out[o++] = '$';
    for (int i = 0; i < L && o + 1 < (int)cap; i++) { out[o++] = raw[i]; int left = L - 1 - i; if (left && left % 3 == 0 && o + 1 < (int)cap) out[o++] = '.'; } out[o] = 0;
}

/* ------------------------------------------------------------------ the data */
static void read_outfits(int ri, const qbv *prof, const qbv *bodies) {
    const cs_rocker *r = &D.r[ri]; const qbv *outs = qb_get(prof, "outfits");
    for (int o = 1; o <= 2; o++) for (int s = 1; s <= 4; s++) { char nm[96]; snprintf(nm, sizeof nm, "Guitarist_%s_Outfit%d_Style%d", r->name, o, s); uint32_t id = gh3_qbkey(nm); const qbv *d = by_desc(bodies, id);
        if (!d || D.no >= (int)(sizeof D.o / sizeof *D.o)) continue; cs_outfit *c = &D.o[D.no++]; memset(c, 0, sizeof *c); c->id = id; c->rocker = ri; c->outfit = o; c->style = s;
        base_of(qb_str(qb_get(d, "pak")), c->pak, sizeof c->pak); base_of(qb_str(qb_get(d, "anims_pak")), c->anims, sizeof c->anims); c->hasPak = on_disk(c->pak);
        char tag[16]; snprintf(tag, sizeof tag, "_0%ds%d", o, s);   /* the outfit whose style ids say it ("axel_01s1"): its name and that style's */
        if (outs && outs->kind == QB_ARRAY) for (int k = 0; k < outs->narr; k++) { const qbv *ids = qb_getk(outs->arr[k], K_STYLEIDS), *sty = qb_get(outs->arr[k], "styles"); if (!ids || ids->kind != QB_ARRAY) continue;
            for (int j = 0; j < ids->narr; j++) if (strstr(qb_str(ids->arr[j]), tag)) { trim(qb_str(qb_get(outs->arr[k], "name")), c->oname, sizeof c->oname); if (sty && sty->kind == QB_ARRAY && j < sty->narr) trim(qb_str(sty->arr[j]), c->sname, sizeof c->sname); } }
        if (!c->oname[0]) snprintf(c->oname, sizeof c->oname, o == 1 ? "Visual de sempre" : "Visual %d", o); if (!c->sname[0]) snprintf(c->sname, sizeof c->sname, "cor %d", s);
        say(CS_PT_OUTFIT, c->oname, sizeof c->oname); say(CS_PT_STYLE, c->sname, sizeof c->sname); }
}
const cs_data *charsel_data(void) {
    if (built || !A.lib) return &D; built = 1; memset(&D, 0, sizeof D);
    const qbv *mp = S("musician_profiles"), *mb = S("musician_body"), *mi = S("musician_instrument"); if (!mp || mp->kind != QB_ARRAY || !mb || !mi || mi->kind != QB_ARRAY) return &D;
    uint32_t KG = gh3_qbkey("guitarist"), KGT = gh3_qbkey("guitar"), k;
    for (int i = 0; i < mp->narr && D.nr < (int)(sizeof D.r / sizeof *D.r); i++) { const qbv *st = mp->arr[i]; const char *nm = qb_str(qb_get(st, "name"));
        if (!qb_keyof(qb_get(st, "type"), &k) || k != KG || !*nm || !strncasecmp(nm, "boneman", 7)) continue;
        cs_rocker *r = &D.r[D.nr]; memset(r, 0, sizeof *r); snprintf(r->name, sizeof r->name, "%s", nm); trim(qb_str(qb_get(st, "fullname")), r->full, sizeof r->full); if (!r->full[0]) snprintf(r->full, sizeof r->full, "%s", nm);
        r->blurb = blurb_of(charsel_pt_rocker(nm), qb_get(st, "blurb"));
        qb_keyof(qb_get(qb_get(st, "musician_body"), "desc_id"), &r->body); qb_keyof(qb_get(qb_get(st, "musician_instrument"), "desc_id"), &r->inst);
        const qbv *d = by_desc(mb, r->body); if (d) { base_of(qb_str(qb_get(d, "pak")), r->pak, sizeof r->pak); base_of(qb_str(qb_get(d, "anims_pak")), r->anims, sizeof r->anims); } r->hasPak = on_disk(r->pak);
        const qbv *sl = gh3_library_section(A.lib, L_ROCKERS); uint32_t nk = gh3_qbkey(nm);   /* for sale: by the rocker named, or its body */
        if (sl && sl->kind == QB_ARRAY) for (int j = 0; j < sl->narr; j++) { uint32_t c = 0, id = 0; qb_keyof(qb_getk(sl->arr[j], K_CHAR), &c); qb_keyof(qb_getk(sl->arr[j], K_ID), &id); if (c == nk || id == r->body) { r->secret = 1; r->price = (int)qb_num(qb_get(sl->arr[j], "price"), 0); } }
        D.nr++; read_outfits(D.nr - 1, st, mb); }
    /* the guitars: one per name, in the order the list first names them, each with its finishes */
    for (int i = 0; i < mi->narr && D.nm < (int)(sizeof D.m / sizeof *D.m); i++) { const qbv *e = mi->arr[i]; const char *nm = qb_str(qb_get(e, "name")); if (!qb_keyof(qb_get(e, "type"), &k) || k != KGT || !*nm) continue;
        int found = 0; for (int m = 0; m < D.nm && !found; m++) found = !strcmp(D.m[m].name, nm); if (!found) { snprintf(D.m[D.nm].name, sizeof D.m[D.nm].name, "%s", nm); D.nm++; } }
    for (int m = 0; m < D.nm; m++) { D.m[m].first = D.ng;
        for (int i = 0; i < mi->narr && D.ng < (int)(sizeof D.g / sizeof *D.g); i++) { const qbv *e = mi->arr[i]; if (!qb_keyof(qb_get(e, "type"), &k) || k != KGT || strcmp(qb_str(qb_get(e, "name")), D.m[m].name)) continue;
            cs_guitar *g = &D.g[D.ng]; memset(g, 0, sizeof *g); if (!qb_keyof(qb_get(e, "desc_id"), &g->id)) continue; g->model = m;
            trim(qb_str(qb_get(e, "finish")), g->finish, sizeof g->finish); trim(qb_str(qb_getk(e, K_LINE)), g->line, sizeof g->line); trim(qb_str(qb_getk(e, K_MAKER)), g->maker, sizeof g->maker);
            int same = !strcmp(g->line, g->finish); say(CS_PT_FINISH, g->finish, sizeof g->finish); say(CS_PT_MAKER, g->maker, sizeof g->maker);   /* (a line that repeats the finish stays the same) */
            if (same) memcpy(g->line, g->finish, sizeof g->line); else say(CS_PT_LINE, g->line, sizeof g->line);
            g->blurb = blurb_of(charsel_pt_guitar(g->id), qb_get(e, "blurb")); base_of(qb_str(qb_get(e, "pak")), g->pak, sizeof g->pak); g->hasPak = on_disk(g->pak);
            g->deflt = listed(L_DEFAULT, g->id, NULL); g->cat = listed(L_GUITARS, g->id, &g->price) || listed(L_SPECIAL, g->id, &g->price) ? CS_GUITARS : listed(L_FINISHES, g->id, &g->price) ? CS_FINISHES : -1;
            D.ng++; D.m[m].n++; } }
    char tm[40]; for (int m = 0; m < D.nm; m++) { trim(D.m[m].name, tm, sizeof tm); snprintf(D.m[m].name, sizeof D.m[m].name, "%s", tm); }   /* "  Les Paul  " (the game tells its Les Pauls apart by spaces) */
    D.ok = D.nr > 0 && D.ng > 0;
    if (getenv("GH3_CHARSEL_LOG")) { printf("charsel: %d rockers, %d outfits, %d guitars, %d finishes\n", D.nr, D.no, D.nm, D.ng);
        for (int i = 0; i < D.nr; i++) printf("  rocker %-8s %-14s body %-10s %s%s price %d\n", D.r[i].name, D.r[i].full, D.r[i].pak, D.r[i].hasPak ? "on disk" : "MISSING", D.r[i].secret ? ", secret" : "", D.r[i].price);
        for (int i = 0; i < D.ng; i++) printf("  guitar %-12s %-22s %-14s %-10s %s%s %s $%d\n", D.m[D.g[i].model].name, D.g[i].finish, D.g[i].line, D.g[i].pak, D.g[i].hasPak ? "on disk" : "-", D.g[i].deflt ? " default" : "", D.g[i].cat == CS_GUITARS ? "guitar" : D.g[i].cat == CS_FINISHES ? "finish" : "unsold", D.g[i].price); }
    if (getenv("GH3_CHARSEL_LOG")) { int n = 0; for (int i = 0; i < D.nr; i++) if (!charsel_pt_rocker(D.r[i].name)) { printf("  no Portuguese story: rocker %s\n", D.r[i].name); n++; }   /* charsel_pt.c: what is still shown in English */
        for (int i = 0; i < D.ng; i++) if (!charsel_pt_guitar(D.g[i].id) && D.g[i].blurb) { printf("  no Portuguese story: guitar %08x %s %s\n", D.g[i].id, D.m[D.g[i].model].name, D.g[i].finish); n++; }
        printf("charsel: %d stories in English\n", n); }
    return &D;
}

/* ------------------------------------------------------------------ owned, chosen */
static int rocker_idx(const char *name) { for (int i = 0; i < D.nr; i++) if (!strcasecmp(D.r[i].name, name)) return i; return -1; }
static int guitar_idx(uint32_t id) { for (int i = 0; i < D.ng; i++) if (D.g[i].id == id) return i; return -1; }
static int owns_rocker(int i) { char k[48]; snprintf(k, sizeof k, "roqueiro:%s", D.r[i].name); return !D.r[i].secret || store_owns(k); }
static int owns_guitar(int i) { char k[32]; snprintf(k, sizeof k, "guitarra:%08x", D.g[i].id); return D.g[i].deflt || store_owns(k); }
static int owns_model(int m) { for (int i = 0; i < D.m[m].n; i++) if (owns_guitar(D.m[m].first + i)) return 1; return 0; }
static int model_buy(int m) { int best = -1; for (int i = 0; i < D.m[m].n; i++) { int g = D.m[m].first + i; if (D.g[g].cat == CS_GUITARS && (best < 0 || D.g[g].price < D.g[best].price)) best = g; } return best; }   /* the finish the store sells the guitar as */
static int usable_rocker(int i) { return i >= 0 && D.r[i].hasPak && owns_rocker(i); }
static int cur_rocker(void) { charsel_data(); int i = rocker_idx(SET.rocker[0] ? SET.rocker : "Axel"); if (!usable_rocker(i)) i = rocker_idx("Axel"); return i; }
static int cur_outfit(int ri) { for (int i = 0; i < D.no; i++) if (D.o[i].id == SET.outfit && D.o[i].rocker == ri && D.o[i].hasPak) return i; for (int i = 0; i < D.no; i++) if (D.o[i].rocker == ri && D.o[i].outfit == 1 && D.o[i].style == 1) return i; return -1; }
static int cur_guitar(void) { int g = SET.guitar ? guitar_idx(SET.guitar) : -1; return g >= 0 && owns_guitar(g) ? g : -1; }   /* -1: the rocker's own */
const char *charsel_rocker(void) { int i = cur_rocker(); return i >= 0 ? D.r[i].name : SET.rocker[0] ? SET.rocker : "Axel"; }
void charsel_paks(const char *rocker, char *body, char *anims, char *guitar, size_t n) {
    body[0] = anims[0] = guitar[0] = 0; charsel_data(); int ri = rocker_idx(rocker ? rocker : charsel_rocker()), o = ri >= 0 ? cur_outfit(ri) : -1, g = cur_guitar();
    if (o >= 0 && D.o[o].hasPak && strcasecmp(D.o[o].pak, D.r[ri].pak)) { snprintf(body, n, "%s", D.o[o].pak); snprintf(anims, n, "%s", D.o[o].anims); }
    if (g >= 0 && D.g[g].hasPak) snprintf(guitar, n, "%s", D.g[g].pak);
}

/* ------------------------------------------------------------------ the photos (each character pak's .img, read alone) */
static int read_photo(const char *base, fb_t *out) {
    char fn[96]; snprintf(fn, sizeof fn, "%s.pak.xen", base); const char *p = app_data_path("pak", fn); FILE *f = p ? fopen(p, "rb") : NULL; if (!f) return -1;
    uint32_t KI = gh3_qbkey(".img"), KL = gh3_qbkey(".last"), KL2 = gh3_qbkey("last"); long pos = 0, at = -1; uint32_t off = 0, len = 0; uint8_t h[32]; int r = -1;
    for (int n = 0; n < 8192; n++) { if (fseek(f, pos, SEEK_SET) || fread(h, 1, 32, f) != 32) break; uint32_t type = be32(h); if (!type || type == KL || type == KL2) break;
        if (type == KI && (at < 0 || be32(h + 16) == 0xc9c796cbu)) { at = pos; off = be32(h + 4); len = be32(h + 8); if (be32(h + 16) == 0xc9c796cbu) break; }   /* every character's photo has that name */
        pos += 32 + ((be32(h + 28) & 0x20) ? 160 : 0); }
    if (at >= 0 && len > 0x28 && len < (8u << 20)) { uint8_t *b = malloc(len); if (b && !fseek(f, at + off, SEEK_SET) && fread(b, 1, len, f) == len) { size_t o = be32(b + 0x1c); if (!o) o = 0x28; if (o < len) r = ga_decode_dds(b + o, len - o, out); } free(b); }
    fclose(f); return r;
}
gfx_sprite *charsel_photo(const char *rocker) {
    static struct { char pak[48]; gfx_sprite *s; unsigned used; } C[3]; static unsigned tick;
    charsel_data(); int ri = rocker_idx(rocker ? rocker : charsel_rocker()); if (ri < 0) return NULL;
    char body[48], an[48], g[48]; charsel_paks(D.r[ri].name, body, an, g, sizeof body); const char *pak = rocker && strcasecmp(rocker, charsel_rocker()) ? D.r[ri].pak : *body ? body : D.r[ri].pak;
    for (int i = 0; i < 3; i++) if (C[i].pak[0] && !strcasecmp(C[i].pak, pak)) { C[i].used = ++tick; return C[i].s; }
    int lo = 0; for (int i = 1; i < 3; i++) if (C[i].used < C[lo].used) lo = i;
    gfx_free_sprite(C[lo].s); C[lo].s = NULL; snprintf(C[lo].pak, sizeof C[lo].pak, "%s", pak); C[lo].used = ++tick;
    fb_t f; if (read_photo(pak, &f) == 0) { C[lo].s = gfx_bake(&f, 1); fb_free(&f); } return C[lo].s;
}

/* ------------------------------------------------------------------ the store's shelves */
static int shelf(int cat, int i) {   /* the i-th item of a shelf: a rocker or a guitar index */
    int n = 0; if (cat == CS_ROCKERS) { for (int r = 0; r < D.nr; r++) if (D.r[r].secret && n++ == i) return r; return -1; }
    for (int g = 0; g < D.ng; g++) if (D.g[g].cat == cat && n++ == i) return g; return -1;
}
int charsel_store_n(int cat) { charsel_data(); int n = 0; if (cat == CS_ROCKERS) { for (int r = 0; r < D.nr; r++) n += D.r[r].secret; } else for (int g = 0; g < D.ng; g++) n += D.g[g].cat == cat; return n; }
void charsel_store_item(int cat, int i, const char **title, const char **sub, int *price, int *have, const char **info, const char **why, char *key, size_t cap) {
    static char t[96], s[96], w[160]; int k = shelf(cat, i); *title = *sub = ""; *price = 0; *have = 0; *info = NULL; *why = NULL; if (key) *key = 0; if (k < 0) return;
    if (cat == CS_ROCKERS) { const cs_rocker *r = &D.r[k]; *title = r->full; *sub = r->hasPak ? "roqueiro secreto" : "falta o arquivo do personagem"; *price = r->price; *have = owns_rocker(k); *info = r->blurb; if (key) snprintf(key, cap, "roqueiro:%s", r->name); if (!r->hasPak) *why = "Esta cópia do jogo não tem o arquivo deste roqueiro."; return; }
    const cs_guitar *g = &D.g[k]; const cs_model *m = &D.m[g->model];
    char ml[80]; maker_line(g, ml, sizeof ml);
    if (cat == CS_GUITARS) { snprintf(t, sizeof t, "%s", m->name); snprintf(s, sizeof s, "%s • %s", ml, g->finish); }
    else { snprintf(t, sizeof t, "%s %s", m->name, g->finish); snprintf(s, sizeof s, "acabamento • %s", ml); }
    *title = t; *sub = s; *price = g->price; *have = owns_guitar(k); *info = g->blurb; if (key) snprintf(key, cap, "guitarra:%08x", g->id);
    if (cat == CS_FINISHES && !owns_model(g->model)) { snprintf(w, sizeof w, "Compre a guitarra %s primeiro (Guitarras).", m->name); *why = w; }
}
int charsel_store_find(int cat, const char *key) { char k[64]; for (int i = 0, n = charsel_store_n(cat); i < n; i++) { const char *a, *b, *c, *d; int p, h; charsel_store_item(cat, i, &a, &b, &p, &h, &c, &d, k, sizeof k); if (!strcmp(k, key)) return i; } return -1; }

/* ------------------------------------------------------------------ the hub and its lists */
enum { M_HUB, M_ROCKER, M_OUTFIT, M_GUITAR, M_FINISH };
enum { H_START, H_ROCKER, H_OUTFIT, H_GUITAR, H_N };
static int mode, hubRow, cur[5], model;
static void note(double now, const char *m) { snprintf(A.note, sizeof A.note, "%s", m); A.noteAt = now; }
static void keep(void) { if (!demo) settings_save(); }
static int list_n(void) {
    int ri = cur_rocker(), n = 0; switch (mode) { case M_ROCKER: return D.nr; case M_GUITAR: return D.nm + 1; case M_FINISH: return model >= 0 && model < D.nm ? D.m[model].n : 0;
        case M_OUTFIT: for (int i = 0; i < D.no; i++) n += D.o[i].rocker == ri; return n; default: return H_N; }
}
static int outfit_at(int k) { int ri = cur_rocker(); for (int i = 0, n = 0; i < D.no; i++) if (D.o[i].rocker == ri && n++ == k) return i; return -1; }
static void guitar_words(int g, int ri, char *out, size_t n) {   /* "SG Worn Red", or the rocker's own */
    if (g < 0) { int own = ri >= 0 ? guitar_idx(D.r[ri].inst) : -1; if (own >= 0) snprintf(out, n, "%s %s (a dele)", D.m[D.g[own].model].name, D.g[own].finish); else snprintf(out, n, "A guitarra dele"); return; }
    snprintf(out, n, "%s %s", D.m[D.g[g].model].name, D.g[g].finish);
}
void charsel_view(cs_view *v) {
    memset(v, 0, sizeof *v); charsel_data(); int ri = cur_rocker(), show = mode == M_ROCKER && cur[M_ROCKER] >= 0 && cur[M_ROCKER] < D.nr ? cur[M_ROCKER] : ri, gi = cur_guitar(); char b[96];
    static const char *T[5] = { "SEU ROQUEIRO", "ROQUEIROS", "VISUAL", "GUITARRAS", "ACABAMENTO" }; v->title = T[mode]; v->hub = mode == M_HUB;
    snprintf(v->cash, sizeof v->cash, "%s", store_cash_str());
    if (show >= 0) { const cs_rocker *r = &D.r[show]; snprintf(v->name, sizeof v->name, "%s", r->full); v->photo = r->hasPak ? charsel_photo(r->name) : NULL; v->blurb = r->blurb;
        int o = show == ri ? cur_outfit(ri) : -1; if (o < 0) for (int i = 0; i < D.no && o < 0; i++) if (D.o[i].rocker == show) o = i;
        if (o >= 0) snprintf(v->outfit, sizeof v->outfit, "%s - %s", D.o[o].oname, D.o[o].sname); guitar_words(show == ri ? gi : -1, show, v->guitar, sizeof v->guitar); }
    int n = list_n(); v->n = n > 72 ? 72 : n; v->sel = mode == M_HUB ? hubRow : cur[mode];
    for (int i = 0; i < v->n; i++) { cs_row *w = &v->rows[i];
        switch (mode) {
        case M_HUB: { static const char *L[H_N] = { "COMEÇAR", "TROCAR ROQUEIRO", "TROCAR VISUAL", "TROCAR GUITARRA" }; snprintf(w->label, sizeof w->label, "%s", L[i]); break; }
        case M_ROCKER: { const cs_rocker *r = &D.r[i]; snprintf(w->label, sizeof w->label, "%s", r->full); w->chosen = i == ri; w->off = !r->hasPak; w->locked = !owns_rocker(i);
            if (w->off) snprintf(w->tag, sizeof w->tag, "sem arquivo"); else if (w->locked) money(r->price, w->tag, sizeof w->tag); else if (w->chosen) snprintf(w->tag, sizeof w->tag, "✓"); break; }
        case M_OUTFIT: { int o = outfit_at(i); if (o < 0) break; snprintf(w->label, sizeof w->label, "%s", D.o[o].oname); snprintf(w->sub, sizeof w->sub, "%s", D.o[o].sname); w->off = !D.o[o].hasPak; w->chosen = o == cur_outfit(ri);
            if (w->off) snprintf(w->tag, sizeof w->tag, "não instalado"); else if (w->chosen) snprintf(w->tag, sizeof w->tag, "✓"); break; }
        case M_GUITAR: if (!i) { snprintf(w->label, sizeof w->label, "A GUITARRA DELE"); guitar_words(-1, ri, b, sizeof b); char *p = strstr(b, " (a dele)"); if (p) *p = 0; snprintf(w->sub, sizeof w->sub, "%s", b); w->chosen = gi < 0; if (w->chosen) snprintf(w->tag, sizeof w->tag, "✓"); }
            else { const cs_model *m = &D.m[i - 1]; const cs_guitar *g = &D.g[m->first]; int buy = model_buy(i - 1); snprintf(w->label, sizeof w->label, "%s", m->name);
                maker_line(g, b, sizeof b); snprintf(w->sub, sizeof w->sub, "%s - %d %s", b, m->n, m->n == 1 ? "acabamento" : "acabamentos");
                w->locked = !owns_model(i - 1); w->chosen = gi >= 0 && D.g[gi].model == i - 1; if (w->locked) { if (buy >= 0) money(D.g[buy].price, w->tag, sizeof w->tag); else snprintf(w->tag, sizeof w->tag, "fora da loja"); } else if (w->chosen) snprintf(w->tag, sizeof w->tag, "✓"); }
            break;
        case M_FINISH: { int g = D.m[model].first + i; snprintf(w->label, sizeof w->label, "%s", D.g[g].finish); maker_line(&D.g[g], w->sub, sizeof w->sub);
            w->locked = !owns_guitar(g); w->chosen = g == gi; if (w->locked) { if (D.g[g].cat >= 0) money(D.g[g].price, w->tag, sizeof w->tag); else snprintf(w->tag, sizeof w->tag, "fora da loja"); } else if (w->chosen) snprintf(w->tag, sizeof w->tag, "✓"); break; }
        } }
    /* the line under the list: what A does on the row chosen */
    int s = v->sel; const cs_row *w = s >= 0 && s < v->n ? &v->rows[s] : NULL;
    if (mode == M_HUB) snprintf(v->info, sizeof v->info, "%s", hubRow == H_START ? "Pronto para tocar: vamos à lista de músicas." : hubRow == H_ROCKER ? "Escolha quem sobe ao palco." : hubRow == H_OUTFIT ? "A roupa e as cores do roqueiro." : "A guitarra e o acabamento dela.");
    else if (w && w->off) snprintf(v->info, sizeof v->info, "%s", mode == M_ROCKER ? "Esta cópia do jogo não tem o arquivo deste roqueiro." : "Esta cópia do jogo só traz o visual de sempre de cada roqueiro.");
    else if (w && w->locked && w->tag[0] == '$') snprintf(v->info, sizeof v->info, "Bloqueado: compre na Loja por %s (A).", w->tag);
    else if (w && w->locked) snprintf(v->info, sizeof v->info, "A loja não vende este.");
    else if (mode == M_GUITAR || mode == M_FINISH) { int g = mode == M_FINISH ? D.m[model].first + s : s > 0 ? D.m[s - 1].first : (ri >= 0 ? guitar_idx(D.r[ri].inst) : -1);
        if (g >= 0 && !D.g[g].hasPak) snprintf(v->info, sizeof v->info, "Sem o modelo 3D desta guitarra nesta cópia: no palco 3D o roqueiro toca a dele.");
        else snprintf(v->info, sizeof v->info, "%s", mode == M_GUITAR && s > 0 ? "A: ver os acabamentos." : "A: escolher."); }
    else if (w && w->chosen) snprintf(v->info, sizeof v->info, "É o escolhido.");
    else snprintf(v->info, sizeof v->info, "A: escolher.");
    if (mode == M_GUITAR || mode == M_FINISH) { int g = mode == M_FINISH ? D.m[model].first + s : s > 0 ? D.m[s - 1].first : -1; v->blurb = g >= 0 ? D.g[g].blurb : NULL; }
    static const char *B1[3] = { "A", "B", "Y" }, *T1[3] = { "Escolher", "Voltar", "Loja" }, *T2[2] = { "Escolher", "Voltar" };
    v->nh = mode == M_HUB ? 3 : 2; for (int i = 0; i < v->nh; i++) { v->bt[i] = B1[i]; v->tx[i] = mode == M_HUB ? T1[i] : T2[i]; }
}

int charsel_open(double now) {
    if (!charsel_data()->ok) return 0; mode = M_HUB; hubRow = H_START; A.screen = SC_CHARSEL; (void)now;
    if (SET.rocker[0] && cur_rocker() >= 0 && strcasecmp(SET.rocker, D.r[cur_rocker()].name)) snprintf(SET.rocker, sizeof SET.rocker, "%s", D.r[cur_rocker()].name);   /* one no longer usable: back to Axel */
    return 1;
}
static void to_store(int cat, const char *key, double now) { store_open_at(cat + 2, key, SC_CHARSEL); (void)now; }
static void open_list(int m) {
    int ri = cur_rocker(), gi = cur_guitar(); mode = m; cur[m] = 0;
    if (m == M_ROCKER) cur[m] = ri < 0 ? 0 : ri; else if (m == M_OUTFIT) { int o = cur_outfit(ri); for (int k = 0; outfit_at(k) >= 0; k++) if (outfit_at(k) == o) cur[m] = k; }
    else if (m == M_GUITAR) cur[m] = gi < 0 ? 0 : D.g[gi].model + 1; else if (m == M_FINISH) { cur[m] = 0; if (gi >= 0 && D.g[gi].model == model) cur[m] = gi - D.m[model].first; else for (int i = 0; i < D.m[model].n; i++) if (owns_guitar(D.m[model].first + i)) { cur[m] = i; break; } }
}
void charsel_event(int in, double now) {
    int n = list_n(), *s = mode == M_HUB ? &hubRow : &cur[mode]; char key[64];
    if (in == IN_UP || in == IN_DOWN) { if (n) { *s = (*s + n + (in == IN_UP ? -1 : 1)) % n; audio_sfx(SFX_MOVE); } return; }
    if (in == IN_Y) { to_store(CS_ROCKERS, "", now); return; }
    if (in == IN_BACK) { audio_sfx(SFX_BACK); if (mode == M_HUB) screens_enter(SC_MAIN, now); else if (mode == M_FINISH) { mode = M_GUITAR; } else mode = M_HUB; return; }
    if (in != IN_OK) return; int ri = cur_rocker();
    switch (mode) {
    case M_HUB: if (hubRow == H_START) { keep(); audio_sfx(SFX_OK); audio_sfx(SFX_WHOOSH); screens_mode_go(now); } else { audio_sfx(SFX_OK); open_list(hubRow == H_ROCKER ? M_ROCKER : hubRow == H_OUTFIT ? M_OUTFIT : M_GUITAR); } return;
    case M_ROCKER: { const cs_rocker *r = &D.r[*s]; if (!r->hasPak) { audio_sfx(SFX_WARN); note(now, "Esta cópia do jogo não tem o arquivo deste roqueiro."); return; }
        if (!owns_rocker(*s)) { snprintf(key, sizeof key, "roqueiro:%s", r->name); audio_sfx(SFX_OK); to_store(CS_ROCKERS, key, now); return; }
        snprintf(SET.rocker, sizeof SET.rocker, "%s", r->name); SET.outfit = 0; keep(); audio_sfx(SFX_OK); mode = M_HUB; return; }
    case M_OUTFIT: { int o = outfit_at(*s); if (o < 0) return; if (!D.o[o].hasPak) { audio_sfx(SFX_WARN); note(now, "Esta cópia do jogo só traz o visual de sempre de cada roqueiro."); return; }
        SET.outfit = D.o[o].outfit == 1 && D.o[o].style == 1 ? 0 : D.o[o].id; keep(); audio_sfx(SFX_OK); mode = M_HUB; return; }
    case M_GUITAR: if (*s == 0) { SET.guitar = 0; keep(); audio_sfx(SFX_OK); mode = M_HUB; return; }
        model = *s - 1; if (!owns_model(model)) { int b = model_buy(model); if (b < 0) { audio_sfx(SFX_WARN); note(now, "A loja não vende esta guitarra."); return; } snprintf(key, sizeof key, "guitarra:%08x", D.g[b].id); audio_sfx(SFX_OK); to_store(CS_GUITARS, key, now); return; }
        audio_sfx(SFX_OK); open_list(M_FINISH); return;
    case M_FINISH: { int g = D.m[model].first + *s; if (!owns_guitar(g)) { if (D.g[g].cat < 0) { audio_sfx(SFX_WARN); note(now, "A loja não vende este acabamento."); return; } snprintf(key, sizeof key, "guitarra:%08x", D.g[g].id); audio_sfx(SFX_OK); to_store(D.g[g].cat, key, now); return; }
        SET.guitar = D.g[g].id; keep(); audio_sfx(SFX_OK); mode = M_HUB; (void)ri; return; }
    }
}

/* ------------------------------------------------------------------ drawing (the plain look, when the game's art is missing) */
void charsel_draw(double now) {
    cs_view v; charsel_view(&v); if (SET.theme == 2 && orig_ready() && orig_charsel_draw(now, &v)) return;
    render_backdrop(now, SET.stageFx); int W = A.W, H = A.H; char b[160]; snprintf(b, sizeof b, "Seu dinheiro: %s", v.cash); ui_header(W, v.title, b);
    if (v.photo) gfx_blit(v.photo, 0, 0, gfx_sprite_w(v.photo), gfx_sprite_h(v.photo), 20, 70, 250, 250, RGB(255, 255, 255), 1); else ui_panel(20, 70, 250, 250, UI_PANEL_DARK);
    gfx_text_sh(20, 330, 2, v.name, C_INK, C_SHADOW); gfx_text(20, 356, 1, v.outfit, C_MUTED); gfx_text(20, 372, 1, v.guitar, C_MUTED);
    int x = 290, y0 = 72, rowH = 24, vis = 13, top = v.sel - vis + 1 < 0 ? 0 : v.sel - vis + 1;
    for (int i = top, y = y0; i < v.n && i < top + vis; i++, y += rowH) { const cs_row *r = &v.rows[i]; int on = i == v.sel; if (on) ui_panel(x - 6, y - 3, W - x - 8, rowH, UI_PANEL_AMBER);
        gfx_text_sh(x, y + 1, 2, r->label, r->off ? C_MUTED : on ? C_INK : RGB(210, 204, 196), C_SHADOW); if (r->tag[0]) gfx_text_sh_right(W - 22, y + 4, 1, r->tag, r->locked ? C_AMP : C_GOOD, C_SHADOW); }
    gfx_text_box(290, H - 70, W - 300, 1, v.info, C_MUTED); ui_hints(H - 24, W, v.bt, v.tx, v.nh);
}

/* ------------------------------------------------------------------ --screens */
void charsel_screens(void (*shot)(const char *name)) {
    if (!charsel_data()->ok) { printf("charsel: no rockers / guitars in qb.pak\n"); return; }
    int was = A.screen; settings_t keepSet = SET; demo = 1;
#define SHOT(nm) do { gfx_begin(); screens_draw(0); gfx_end(); shot(nm); } while (0)
    charsel_open(0); SHOT("charsel_hub");
    open_list(M_ROCKER); cur[M_ROCKER] = rocker_idx("Slash") >= 0 ? rocker_idx("Slash") : 0; SHOT("charsel_rocker_locked");
    cur[M_ROCKER] = rocker_idx("Judy") >= 0 ? rocker_idx("Judy") : 0; charsel_event(IN_OK, 0); SHOT("charsel_hub_judy");
    hubRow = H_OUTFIT; charsel_event(IN_OK, 0); SHOT("charsel_outfit"); charsel_event(IN_BACK, 0);
    hubRow = H_GUITAR; charsel_event(IN_OK, 0); SHOT("charsel_guitar");
    cur[M_GUITAR] = 1; for (int m = 0; m < D.nm; m++) if (!strcmp(D.m[m].name, "X-Plorer")) cur[M_GUITAR] = m + 1; charsel_event(IN_OK, 0); SHOT("charsel_finish");
    for (int i = 0; i < D.m[model].n; i++) if (!owns_guitar(D.m[model].first + i)) { cur[M_FINISH] = i; break; } SHOT("charsel_finish_locked");
    charsel_event(IN_OK, 0); if (A.screen == SC_STORE) { SHOT("store_finishes"); store_event(IN_LEFT, 0); SHOT("store_guitars"); store_event(IN_LEFT, 0); SHOT("store_rockers"); store_event(IN_BACK, 0); }
    printf("charsel: %d rockers, %d guitars (%d finishes); %s with %s\n", D.nr, D.nm, D.ng, charsel_rocker(), cur_guitar() >= 0 ? D.g[cur_guitar()].finish : "the rocker's own guitar");
#undef SHOT
    SET = keepSet; demo = 0; mode = M_HUB; A.screen = was;
}
