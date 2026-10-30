/* Readers for the Guitar Hero III (PC) data files, ported from the browser port's
 * "GH3 PC file readers": PAK/PAB directories, QB sections, song charts and FSB3 audio banks. */
#ifndef GH3FMT_H
#define GH3FMT_H
#include <stdint.h>
#include <stddef.h>

uint32_t gh3_qbkey(const char *s);

/* ---- PAK directory ---- */
typedef struct { uint32_t type, len, pakKey, fullKey, nameKey, flags; const uint8_t *data; char filename[160]; } gh3_pakent;
int gh3_pak_parse(const uint8_t *pak, size_t pakLen, const uint8_t *pab, size_t pabLen, gh3_pakent **out);

/* ---- QB values ---- */
enum { QB_NONE = 0, QB_INT = 1, QB_FLOAT = 2, QB_STR = 3, QB_VEC2 = 5, QB_VEC3 = 6, QB_SCRIPT = 7, QB_STRUCT = 0x0A, QB_ARRAY = 0x0C, QB_KEY = 0x0D };
typedef struct qbv qbv;
typedef struct { uint32_t id; int kind; qbv *v; } qbitem;
struct qbv {
    int kind;
    int32_t i; float f[3]; uint32_t key; char *s;
    qbitem *items; int nitems;      /* struct */
    qbv **arr; int narr; int arrKind; /* array */
};
typedef struct { uint32_t id; int kind; qbv *v; } qbsection;
int gh3_qb_parse(const uint8_t *buf, size_t n, qbsection **out);
void qb_free(qbv *v);
void gh3_qb_free_sections(qbsection *s, int n);
const qbv *qb_get(const qbv *st, const char *name);
const qbv *qb_getk(const qbv *st, uint32_t key);
double qb_num(const qbv *v, double def);
const char *qb_str(const qbv *v);
int qb_keyof(const qbv *v, uint32_t *key);

/* ---- song list + tuning (qb.pak + qb.pab) ---- */
typedef struct { uint32_t key; char name[64], title[128], artist[128], year[16]; float hmScale, bandVol, guitarVol; int hasRhythm; uint32_t bossKey; int isBoss; } gh3_song;
/* a set of the song list. tab: 0 career tier, 1 bonus, 2 downloads, 3 the rest. In a career tier the songs are in
 * the game's order; encoreIdx / bossIdx are song indices (or -1) and defaultUnlocked the songs open from the start. */
typedef struct { char title[96]; int *songIdx; int nsongs; int tab; int career, hasEncore, defaultUnlocked, encoreIdx, bossIdx; } gh3_tier;
typedef struct { double lookahead, good, bad, star; } gh3_diff;
typedef struct { double hmScale, early, late, hamEarly, hamLate, strumBefore, formBefore, susShorten, split, spDrain; gh3_diff diffs[4]; } gh3_tuning;
typedef struct { uint32_t id; char *s; } gh3_text;
typedef struct { gh3_song *songs; int nsongs; gh3_tier *tiers; int ntiers; gh3_tuning tuning; gh3_text *texts; int ntexts; } gh3_library;
int gh3_library_read(const uint8_t *pak, size_t pakLen, const uint8_t *pab, size_t pabLen, gh3_library *lib);
const char *gh3_library_text(const gh3_library *lib, uint32_t key);   /* a string section of the main script pak (section names and the like) */
void gh3_default_tuning(gh3_tuning *t);

/* ---- chart data (<name>_song.pak) ---- */
extern const char *GH3_PART_INFIX[4];  /* "", "_rhythm", "_guitarcoop", "_rhythmcoop" */
extern const char *GH3_PART_NAME[4];
extern const char *GH3_DIFF_NAME[4];   /* easy, medium, hard, expert */
typedef struct { int32_t *raw; int nraw; int (*star)[2]; int nstar; } gh3_partdiff;
typedef struct { int t; uint32_t key; char text[64]; } gh3_marker;   /* text when the chart names the section itself, else key into the main pak's strings */
typedef struct {
    char name[64];
    int32_t *fretbars; int nfret;
    int (*timesig)[3]; int nsig;
    gh3_marker *markers; int nmarkers;
    gh3_partdiff parts[4][4];
} gh3_songdata;
int gh3_songpak_read(const uint8_t *pak, size_t len, const char *name, gh3_songdata *sd);
void gh3_songdata_free(gh3_songdata *sd);

typedef struct { int t, len, mask, n, hopo, sus, star; int hit, missed, dropped; double lateW, hamLateW, beat; } gh3_note;
typedef struct { int t, end, total, hit, failed, done; } gh3_phrase;
typedef struct { int t, strong; } gh3_bar;
typedef struct { gh3_note *notes; int nnotes; gh3_phrase *phrases; int nphrases; gh3_bar *bars; int nbars; const int32_t *fretbars; int nfret; double base; int end; } gh3_chart;
int gh3_chart_build(const gh3_songdata *sd, int part, int diff, double hmScale, double shorten, gh3_chart *out);
void gh3_chart_free(gh3_chart *c);
double gh3_beatpos(const int32_t *fb, int nfb, double t);

/* ---- FSB3 bank of MP3 streams (XOR + bit-reverse scrambled) ---- */
enum { FSB_GUITAR = 0, FSB_RHYTHM, FSB_SONG, FSB_PREVIEW, FSB_COOP_GUITAR, FSB_COOP_RHYTHM, FSB_COOP_SONG, FSB_ROLES };
typedef struct { char name[32]; uint32_t samples, freq, channels, offset, size; int role, done; } gh3_fsbstream;
typedef struct { uint8_t *bytes; size_t n; gh3_fsbstream *streams; int nstreams; int plain; } gh3_fsb;
int gh3_fsb_read(uint8_t *bytes, size_t n, const uint8_t *dat, size_t datLen, const char *songName, gh3_fsb *out);
const uint8_t *gh3_fsb_extract(gh3_fsb *f, int role, size_t *size);
int gh3_fsb_has(const gh3_fsb *f, int role);
void gh3_fsb_descramble(uint8_t *b, size_t n, size_t at);   /* bytes read from offset `at` of a scrambled bank */

#endif
