/* Lists the streams of a GH3 FSB3 bank (.fsb.xen) and the qbkeys its .dat maps to them.
 *   cc -O2 -Isrc tools/fsblist.c src/gh3fmt.c -o build/fsblist && build/fsblist DATA/STREAMS/streamall [guess...]
 * Each guess is hashed with the qbkey CRC and printed next to the stream it names. */
#include "gh3fmt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *slurp(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long L = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(L > 0 ? L : 1); *n = fread(b, 1, L, f) ; fclose(f); return b;
}
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: fsblist BANK [guess...]\n"); return 1; }
    char p[600]; size_t n, dn = 0;
    snprintf(p, sizeof p, "%s.fsb.xen", argv[1]); uint8_t *b = slurp(p, &n); if (!b) { perror(p); return 1; }
    snprintf(p, sizeof p, "%s.dat.xen", argv[1]); uint8_t *dat = slurp(p, &dn);
    gh3_fsb f; if (gh3_fsb_read(b, n, NULL, 0, NULL, &f)) { fprintf(stderr, "not an FSB3 bank\n"); return 1; }
    uint32_t nd = dat && dn >= 8 ? be32(dat) : 0;
    for (int i = 0; i < f.nstreams; i++) {
        const gh3_fsbstream *s = &f.streams[i]; uint32_t key = 0;
        for (uint32_t k = 0; k < nd && 8 + (size_t)k * 20 + 8 <= dn; k++) if (be32(dat + 12 + k * 20) == (uint32_t)i) key = be32(dat + 8 + k * 20);
        const char *hit = "";
        for (int g = 2; g < argc; g++) if (gh3_qbkey(argv[g]) == key) hit = argv[g];
        printf("%4d %-30s %7.1fs %5u Hz %u ch %9u bytes key %08x %s\n", i, s->name, s->freq ? (double)s->samples / s->freq : 0, s->freq, s->channels, s->size, key, hit);
    }
    return 0;
}
