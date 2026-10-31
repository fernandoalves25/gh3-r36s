/* Languages (lang.h): the tables of every group of screens hashed by their Portuguese key at lang_set. */
#include "lang.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* one table per group of screens (src/lang/*.c); each group's file defines its own */
extern const lang_entry LANG_MENUS[], LANG_SONG[], LANG_STORE[], LANG_TUTORIAL[], LANG_PLAY[];
static const lang_entry *const TABLES[] = { LANG_MENUS, LANG_SONG, LANG_STORE, LANG_TUTORIAL, LANG_PLAY };
#define NTABLES ((int)(sizeof TABLES / sizeof *TABLES))

static int cur = LANG_PT;
static const lang_entry **slot; static unsigned cap;   /* open addressing on the key's hash */
static unsigned hash(const char *s) { unsigned h = 2166136261u; while (*s) h = (h ^ (unsigned char)*s++) * 16777619u; return h; }
static void build(void) {
    if (slot) return; unsigned n = 0; for (int t = 0; t < NTABLES; t++) for (const lang_entry *e = TABLES[t]; e->pt; e++) n++;
    cap = 64; while (cap < n * 2) cap <<= 1; slot = calloc(cap, sizeof *slot); if (!slot) return;
    for (int t = 0; t < NTABLES; t++) for (const lang_entry *e = TABLES[t]; e->pt; e++) { unsigned i = hash(e->pt) & (cap - 1);
        while (slot[i] && strcmp(slot[i]->pt, e->pt)) i = (i + 1) & (cap - 1); if (!slot[i]) slot[i] = e; }   /* a key given twice: the first table's */
}
static const lang_entry *find(const char *pt) {
    if (!slot) return NULL; unsigned i = hash(pt) & (cap - 1); while (slot[i]) { if (!strcmp(slot[i]->pt, pt)) return slot[i]; i = (i + 1) & (cap - 1); } return NULL;
}
const char *T(const char *pt) {
    if (!pt || cur == LANG_PT || !*pt) return pt; const lang_entry *e = find(pt); const char *s = e ? (cur == LANG_EN ? e->en : e->es) : NULL;
    if (s && *s) return s;
    static int logOn = -1; if (logOn < 0) logOn = getenv("GH3_LANG_LOG") != NULL;
    if (logOn) { static unsigned seen[4096]; unsigned h = hash(pt), k = h % 4096; if (seen[k] != h + 1) { seen[k] = h + 1; SDL_Log("lang: no %s for \"%s\"", lang_code(cur), pt); } }
    return pt;
}
void lang_set(int lang) { cur = lang < 0 || lang >= LANG_N ? LANG_PT : lang; build(); }
int lang_get(void) { return cur; }
const char *lang_name(int lang) { static const char *N[LANG_N] = { "Português", "English", "Español" }; return N[lang < 0 || lang >= LANG_N ? 0 : lang]; }
const char *lang_code(int lang) { static const char *C[LANG_N] = { "pt", "en", "es" }; return C[lang < 0 || lang >= LANG_N ? 0 : lang]; }
int lang_from_code(const char *code) { for (int l = 0; l < LANG_N; l++) if (code && !strncmp(code, lang_code(l), 2)) return l; return LANG_PT; }
static int convs(const char *s, char *out, int n) {   /* a format's conversions, in order ("%d%s"), to check the translations keep them */
    int k = 0; for (; *s && k < n - 2; s++) if (*s == '%') { s++; if (*s == '%') continue; while (*s && strchr("0123456789.-+ #*l", *s)) s++; if (*s) { out[k++] = '%'; out[k++] = *s; } else break; }
    out[k] = 0; return k;
}
int lang_check(void) {
    build(); int bad = 0, rows = 0;
    for (int t = 0; t < NTABLES; t++) for (const lang_entry *e = TABLES[t]; e->pt; e++) { rows++; char a[64], b[64], c[64]; convs(e->pt, a, 64);
        if (!e->en || !*e->en || !e->es || !*e->es) { printf("lang: empty column for \"%s\"\n", e->pt); bad++; continue; }
        convs(e->en, b, 64); convs(e->es, c, 64); if (strcmp(a, b) || strcmp(a, c)) { printf("lang: conversions differ for \"%s\" (%s / %s / %s)\n", e->pt, a, b, c); bad++; }
        if (find(e->pt) != e) { printf("lang: \"%s\" given twice (table %d)\n", e->pt, t); bad++; } }
    printf("lang: %d rows in %d tables, %d problems\n", rows, NTABLES, bad); return bad;
}
