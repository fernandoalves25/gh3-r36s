/* Lists what a GH3 PAK holds: entries grouped by type with their total size and a few names.
 *   cc -O2 -Isrc tools/paklist.c src/gh3fmt.c -o build/paklist -lm && build/paklist DATA/ZONES/global [-v]
 * The argument is the path without .pak.xen; a .pab.xen beside it is used when present. */
#include "gh3fmt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *slurp(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb"); if (!f) { *n = 0; return NULL; } fseek(f, 0, SEEK_END); long L = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(L > 0 ? L : 1); *n = fread(b, 1, L, f); fclose(f); return b;
}
typedef struct { uint32_t type; int n; long long bytes; char names[3][160]; } group;
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: paklist PATH-without-.pak.xen [-v]\n"); return 1; }
    int verbose = argc > 2 && !strcmp(argv[2], "-v"); char p[600]; size_t pl, bl;
    snprintf(p, sizeof p, "%s.pak.xen", argv[1]); uint8_t *pak = slurp(p, &pl); if (!pak) { perror(p); return 1; }
    snprintf(p, sizeof p, "%s.pab.xen", argv[1]); uint8_t *pab = slurp(p, &bl);
    gh3_pakent *ents; int n = gh3_pak_parse(pak, pl, pab, bl, &ents); if (n < 0) { fprintf(stderr, "not a pak\n"); return 1; }
    group *g = calloc(n + 1, sizeof *g); int ng = 0;
    for (int i = 0; i < n; i++) { gh3_pakent *e = &ents[i]; group *k = NULL; for (int j = 0; j < ng; j++) if (g[j].type == e->type) k = &g[j]; if (!k) { k = &g[ng++]; k->type = e->type; }
        if (k->n < 3) snprintf(k->names[k->n], 160, "%s", e->filename[0] ? e->filename : "?"); k->n++; k->bytes += e->len;
        if (verbose) printf("%08x %9u %s\n", e->type, e->len, e->filename); }
    printf("%d entries\n", n);
    for (int j = 0; j < ng; j++) { printf("type %08x: %4d entries, %8.1f MB", g[j].type, g[j].n, g[j].bytes / 1048576.0); for (int k = 0; k < g[j].n && k < 3; k++) printf("%s %s", k ? "," : "  e.g.", g[j].names[k]); printf("\n"); }
    return 0;
}
