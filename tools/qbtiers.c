/* Prints the career tier structs of qb.pak (keys matched against candidate names) to see the unlock rules.
 *   gcc -Isrc -o build/qbtiers tools/qbtiers.c src/gh3fmt.c && ./build/qbtiers DATA/PAK/qb.pak.xen DATA/PAK/qb.pab.xen */
#include "gh3fmt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t *rd(const char *p, size_t *n) { FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(1); } fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET); uint8_t *b = malloc(*n); fread(b, 1, *n, f); fclose(f); return b; }
static const char *CAND[] = { "title", "songs", "unlock_count", "unlockcount", "level", "encore", "boss", "unlock", "num_songs", "songs_to_unlock", "unlock_songs", "song_count", "encore_count", "boss_battle", "unlock_level", "career_unlock", "progression", "tier", "tier_number", "num_tracks", "unlock_count_easy", "unlock_count_medium", "unlock_count_hard", "unlock_count_expert", "min_songs", "songs_needed", "encore_songs", "encore_song", "boss_song", "unlockable", "unlock_all", "max_songs", "venue", "bonus", "unlock_text", "career", "unlock_rule", "completion", "required", "needed", "index", NULL };
static const char *name_of(uint32_t k) { static char b[16]; for (int i = 0; CAND[i]; i++) if (gh3_qbkey(CAND[i]) == k) return CAND[i]; snprintf(b, sizeof b, "0x%08x", k); return b; }
static void pv(const qbv *v, int depth) {
    if (!v) { printf("(null)"); return; }
    switch (v->kind) { case QB_INT: printf("%d", v->i); break; case QB_FLOAT: printf("%g", v->f[0]); break; case QB_STR: case 0x1B: printf("\"%s\"", v->s ? v->s : ""); break; case 0x04: case 0x1C: printf("w\"%s\"", v->s ? v->s : "");
        break; case QB_KEY: case 0x1A: case 0x09: printf("key %s", name_of(v->key)); break;
        case QB_ARRAY: printf("["); for (int i = 0; i < v->narr && i < 12; i++) { if (i) printf(", "); pv(v->arr[i], depth + 1); } if (v->narr > 12) printf(", ... (%d)", v->narr); printf("]"); break;
        case QB_STRUCT: printf("{"); for (int i = 0; i < v->nitems; i++) { if (i) printf(", "); printf("%s=", name_of(v->items[i].id)); pv(v->items[i].v, depth + 1); } printf("}"); break;
        default: printf("kind%d", v->kind); }
}
int main(int argc, char **argv) {
    size_t pl, bl; uint8_t *pak = rd(argv[1], &pl), *pab = rd(argv[2], &bl);
    gh3_pakent *ents; int n = gh3_pak_parse(pak, pl, pab, bl, &ents);
    const char *want[] = { "gh3_general_songs", "gh3_bonus_songs", "gh3_download_songs", "gh3_singleplayer_songs", "gh3_career_songs", "gh3_coop_songs", "gh3_quickplay_songs", NULL };
    for (int e = 0; e < n; e++) { if (ents[e].type != gh3_qbkey(".qb")) continue; qbsection *secs; int ns = gh3_qb_parse(ents[e].data, ents[e].len, &secs);
        for (int s = 0; s < ns; s++) for (int w = 0; want[w]; w++) if (secs[s].id == gh3_qbkey(want[w])) { printf("== %s (%s)\n", want[w], ents[e].filename); pv(secs[s].v, 0); printf("\n"); }
        gh3_qb_free_sections(secs, ns); }
    return 0;
}
