/* Prints the QB sections of qb.pak, and its scripts decompiled roughly (names, numbers, strings, the structs passed
 * to calls). Keys are matched against the words of a names file (one per line) to show names. How the tutorials and
 * the cheats were read (the pak of the PC game has no file names: use "text" or "script" to find things).
 *   gcc -D_GNU_SOURCE -Isrc -o build/qbgrep tools/qbgrep.c -lm
 *   Q="DATA/PAK/qb.pak.xen DATA/PAK/qb.pab.xen names.txt"
 *   ./build/qbgrep $Q text hyperspeed     sections holding that text (the cheat list: guitar_hero_cheats)
 *   ./build/qbgrep $Q key 8f62b310        sections whose name (or hex key) contains that
 *   ./build/qbgrep $Q entry 444           every section of the 444th file of the pak
 *   ./build/qbgrep $Q script tutorial     scripts whose bytes or name contain the word
 *   ./build/qbgrep $Q script n444         every script of a file;  script k0cd9ac19: scripts using that key
 *   ./build/qbgrep $Q files               every file of the pak */
#include "../src/gh3fmt.c"   /* the static QB value readers, for the structs inside scripts */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
static uint8_t *rd(const char *p, size_t *n) { FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(1); } fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET); uint8_t *b = malloc(*n); if (fread(b, 1, *n, f) != *n) exit(1); fclose(f); return b; }
static char **names; static uint32_t *keys; static int nnames;
static void load_names(const char *p) { FILE *f = fopen(p, "r"); if (!f) return; char l[256]; int cap = 1024; names = malloc(cap * sizeof *names); keys = malloc(cap * 4);
    while (fgets(l, sizeof l, f)) { l[strcspn(l, "\r\n ")] = 0; if (!*l) continue; if (nnames == cap) { cap *= 2; names = realloc(names, cap * sizeof *names); keys = realloc(keys, cap * 4); } names[nnames] = strdup(l); keys[nnames++] = gh3_qbkey(l); } fclose(f); }
static const char *nm(uint32_t k) { static char b[4][16]; static int r; for (int i = 0; i < nnames; i++) if (keys[i] == k) return names[i]; r = (r + 1) & 3; snprintf(b[r], 16, "0x%08x", k); return b[r]; }
static void pv(const qbv *v, int d) {
    if (!v) { printf("null"); return; }
    switch (v->kind) { case QB_INT: printf("%d", v->i); break; case QB_FLOAT: printf("%g", v->f[0]); break; case QB_STR: printf("\"%s\"", v->s ? v->s : ""); break;
    case QB_VEC2: printf("(%g,%g)", v->f[0], v->f[1]); break; case QB_VEC3: printf("(%g,%g,%g)", v->f[0], v->f[1], v->f[2]); break;
    case QB_KEY: printf("$%s", nm(v->key)); break; case QB_SCRIPT: printf("<script>"); break;
    case QB_ARRAY: printf("["); for (int i = 0; i < v->narr; i++) { if (i) printf(", "); pv(v->arr[i], d + 1); } printf("]"); break;
    case QB_STRUCT: printf("{\n"); for (int i = 0; i < v->nitems; i++) { printf("%*s%s = ", (d + 1) * 2, "", v->items[i].id ? nm(v->items[i].id) : "_"); pv(v->items[i].v, d + 1); printf("\n"); } printf("%*s}", d * 2, ""); break;
    default: printf("kind%d:%08x", v->i, v->key); }
}
static int has_text(const qbv *v, const char *w) {
    if (!v) return 0; if (v->kind == QB_STR && v->s && strcasestr(v->s, w)) return 1;
    for (int i = 0; i < v->nitems; i++) if (has_text(v->items[i].v, w)) return 1;
    for (int i = 0; i < v->narr; i++) if (has_text(v->arr[i], w)) return 1; return 0;
}
/* GH3 script bodies: [crc][uncompressed size][compressed size] then LZSS (when the sizes differ) */
static int lzss(const uint8_t *in, int inN, uint8_t *out, int outN) {
    uint8_t win[4096]; memset(win, 0, sizeof win); int wp = 4078, i = 0, o = 0; unsigned flags = 0;
    while (i < inN && o < outN) { flags >>= 1; if (!(flags & 0x100)) { flags = in[i++] | 0xFF00; if (i >= inN) break; }
        if (flags & 1) { uint8_t c = in[i++]; out[o++] = c; win[wp] = c; wp = (wp + 1) & 4095; }
        else { if (i + 1 >= inN) break; int a = in[i] | ((in[i + 1] & 0xF0) << 4), len = (in[i + 1] & 0x0F) + 3; i += 2; for (int k = 0; k < len && o < outN; k++) { uint8_t c = win[(a + k) & 4095]; out[o++] = c; win[wp] = c; wp = (wp + 1) & 4095; } } }
    return o;
}
static void pv1(const qbv *v) {   /* one line */
    if (!v) { printf("null "); return; }
    switch (v->kind) { case QB_INT: printf("%d ", v->i); break; case QB_FLOAT: printf("%gf ", v->f[0]); break; case QB_STR: printf("\"%s\" ", v->s ? v->s : ""); break;
    case QB_VEC2: printf("(%g,%g) ", v->f[0], v->f[1]); break; case QB_VEC3: printf("(%g,%g,%g) ", v->f[0], v->f[1], v->f[2]); break; case QB_KEY: printf("%s ", nm(v->key)); break;
    case QB_ARRAY: printf("[ "); for (int i = 0; i < v->narr; i++) pv1(v->arr[i]); printf("] "); break;
    case QB_STRUCT: printf("{ "); for (int i = 0; i < v->nitems; i++) { if (v->items[i].id) printf("%s=", nm(v->items[i].id)); pv1(v->items[i].v); } printf("} "); break;
    default: printf("kind%d:%s ", v->i, nm(v->key)); }
}
static uint32_t le32x(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
/* the script bytecode, printed roughly as source: names, numbers, strings, structure words */
static void tok(const uint8_t *b, int n) {
    static const char *SYM[0x50] = { [1] = "\n", [3] = "{", [4] = "}", [5] = "[", [6] = "]", [7] = "=", [8] = ".", [9] = ",", [0xA] = "-", [0xB] = "+", [0xC] = "/", [0xD] = "*", [0xE] = "(", [0xF] = ")",
        [0x10] = "==?", [0x11] = "==", [0x12] = "<", [0x13] = "<=", [0x14] = ">", [0x15] = ">=", [0x20] = "begin", [0x21] = "repeat", [0x22] = "break", [0x23] = "script", [0x24] = "endscript", [0x25] = "if", [0x26] = "else", [0x27] = "elseif", [0x28] = "endif", [0x29] = "return",
        [0x2C] = "<...>", [0x2D] = "<", [0x32] = "||", [0x33] = "&&", [0x39] = "not", [0x3C] = "switch", [0x3D] = "endswitch", [0x3E] = "case", [0x3F] = "default", [0x42] = ":" };
    int i = 0;
    while (i < n) { int t = b[i++];
        switch (t) {
        case 0x16: if (i + 4 > n) return; printf("%s ", nm(le32x(b + i))); i += 4; break;
        case 0x17: case 0x18: if (i + 4 > n) return; printf("%d ", (int)le32x(b + i)); i += 4; break;
        case 0x1A: { if (i + 4 > n) return; uint32_t u = le32x(b + i); float f; memcpy(&f, &u, 4); printf("%gf ", f); i += 4; break; }
        case 0x1B: case 0x1C: { if (i + 4 > n) return; int l = (int)le32x(b + i); i += 4; if (l < 0 || i + l > n) return; printf("\"%.*s\" ", l > 0 ? l - 1 : 0, b + i); i += l; break; }
        case 0x1E: { float f[3]; memcpy(f, b + i, 12); printf("(%g,%g,%g) ", f[0], f[1], f[2]); i += 12; break; }
        case 0x1F: { float f[2]; memcpy(f, b + i, 8); printf("(%g,%g) ", f[0], f[1]); i += 8; break; }
        case 0x2E: i += 4; break;
        case 0x2F: case 0x37: case 0x40: case 0x41: { int c = (int)le32x(b + i); i += 4; if (c < 0 || c > 1000) return; printf("random(%d) ", c); i += c * 2 + c * 4; break; }
        case 0x47: case 0x48: case 0x49: if (t == 0x47) printf("if "); else if (t == 0x48) printf("else "); i += 2; break;
        case 0x4A: { int l = b[i] | b[i + 1] << 8; i += 2; i = (i + 3) & ~3; if (i + l > n) return; qbctx c = { b + i, (size_t)l, 0, 0, 0 }; qbv *v = structc(&c, 0); pv1(v); qb_free(v); i += l; break; }
        case 0x4C: { int l = (int)le32x(b + i); i += 4; if (l < 0 || i + l > n) return; printf("w\""); for (int k = 0; k + 1 < l; k += 2) { int ch = b[i + k] << 8 | b[i + k + 1]; if (ch) putchar(ch < 128 ? ch : '?'); } printf("\" "); i += l; break; }
        case 0x2B: { printf("[name %s=", nm(le32x(b + i))); i += 4; while (i < n && b[i]) putchar(b[i++]); i++; printf("] "); break; }
        case 0x00: break;
        default: if (t < 0x50 && SYM[t]) { if (t == 1) printf("\n"); else printf("%s ", SYM[t]); } else printf("?%02x ", t); }
    }
}
int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: qbgrep pak pab names mode [word]\n"); return 1; }
    size_t pl, bl; uint8_t *pak = rd(argv[1], &pl), *pab = rd(argv[2], &bl); load_names(argv[3]); const char *mode = argv[4], *w = argc > 5 ? argv[5] : "";
    gh3_pakent *e; int n = gh3_pak_parse(pak, pl, pab, bl, &e); uint32_t KQB = gh3_qbkey(".qb");
    for (int i = 0; i < n; i++) {
        if (!strcmp(mode, "files")) { printf("%08x %6u %s\n", e[i].type, e[i].len, e[i].filename[0] ? e[i].filename : nm(e[i].fullKey)); continue; }
        if (e[i].type != KQB || !e[i].data) continue;
        int fileHit = !strcmp(mode, "entry") ? i == atoi(w) : strcasestr(e[i].filename, w) != NULL;
        if (!strcmp(mode, "script")) { const uint8_t *b = e[i].data; size_t L = e[i].len;
            for (size_t p = 28; p + 20 <= L; p += 4) { if (be32(b + p) != 0x00200700) continue; uint32_t id = be32(b + p + 4), raw = be32(b + p + 12); if (raw + 12 > L || raw < p) continue;
                uint32_t usz = be32(b + raw + 4), csz = be32(b + raw + 8); if (usz > (1u << 22) || raw + 12 + csz > L) continue; uint8_t *out = malloc(usz + 1); int got;
                if (usz == csz) { memcpy(out, b + raw + 12, usz); got = usz; } else got = lzss(b + raw + 12, csz, out, usz);
                int hit = w[0] == 'n' ? i == atoi(w + 1) : strcasestr(nm(id), w) != NULL; if (w[0] == 'n') goto shown;
                if (w[0] == 'k') { uint32_t want = (uint32_t)strtoul(w + 1, NULL, 16); uint8_t kb[4] = { want, want >> 8, want >> 16, want >> 24 }; hit = memmem(out, got, kb, 4) != NULL; goto shown; } for (int k = 0; !hit && k + (int)strlen(w) <= got; k++) if (!strncasecmp((char *)out + k, w, strlen(w))) hit = 1;
                shown: if (hit) { printf("== script %s (%d bytes, entry %d)\n", nm(id), got, i); tok(out, got); printf("\n"); } free(out); }
            continue; }
        qbsection *s; int ns = gh3_qb_parse(e[i].data, e[i].len, &s); if (ns < 0) { if (fileHit) printf("##### %s: parse failed\n", e[i].filename); continue; }
        int printedFile = 0;
        for (int k = 0; k < ns; k++) { int hit = !strcmp(mode, "file") || !strcmp(mode, "entry") ? fileHit : !strcmp(mode, "key") ? strcasestr(nm(s[k].id), w) != NULL : has_text(s[k].v, w); if (!hit) continue;
            if (!printedFile) { printf("##### %s\n", e[i].filename); printedFile = 1; }
            printf("%s = ", nm(s[k].id)); pv(s[k].v, 0); printf("\n"); }
        gh3_qb_free_sections(s, ns); }
    return 0;
}
