#include "practice.h"
#include "audio.h"
#include "sfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

const double PRACTICE_SPEED[PRACTICE_SPEEDS] = { 1, 0.8, 2.0 / 3, 0.5 };
const char *PRACTICE_SPEED_NAME[PRACTICE_SPEEDS] = { "Normal", "Devagar", "Mais devagar", "Bem devagar" };

void practice_range(const gh3_songdata *sd, int first, int last, double songEnd, double *from, double *to) {
    int n = sd->nmarkers; *from = first > 0 && first <= n ? sd->markers[first - 1].t : 0;
    *to = last > 0 && last < n ? sd->markers[last].t : songEnd;   /* section k ends where section k + 1 begins */
    if (*to <= *from) *to = songEnd;
}
void practice_lead(double from, double speed, double *audioFrom, double *leadSongMs) {
    double pre = fmin(from, 3000 * speed); *audioFrom = from - pre; *leadSongMs = fmax(500, 2400 - pre / speed) * speed;
}
static double now_ms(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6; }
void practice_bench(double durationMs) {
    enum { BLK = 512 }; static float buf[BLK * 2]; double base = 0;
    printf("the song through the mixer (three MP3 streams, %.1f s of song):\n", durationMs / 1000);
    for (int k = 0; k <= PRACTICE_SPEEDS; k++) { int bend = k == PRACTICE_SPEEDS; double sp = PRACTICE_SPEED[bend ? PRACTICE_SPEEDS - 1 : k];   /* the last run: slowest with the whammy held down */
        audio_set_speed(sp); audio_play(0, 0); audio_set_bend(bend ? 1 : 0);
        int64_t frames = (int64_t)((durationMs / sp / 1000 + 1.5) * 44100), last = 0, o = 0, over, hard, full = 0; double t0 = now_ms(), sq = 0, lost; audio_limit_stats(NULL, NULL, NULL);
        while (o < frames) { audio_render(buf, BLK); for (int i = 0; i < BLK; i++) { if (fabsf(buf[2 * i]) > 1e-4f || fabsf(buf[2 * i + 1]) > 1e-4f) last = o + i; if (!k) { sq += (double)buf[2 * i] * buf[2 * i] + (double)buf[2 * i + 1] * buf[2 * i + 1]; full += (fabsf(buf[2 * i]) >= 0.999f) + (fabsf(buf[2 * i + 1]) >= 0.999f); } } o += BLK; }
        double ms = now_ms() - t0, heard = (last + 1) / 44100.0, perSec = ms / heard; audio_set_bend(0); audio_stop(); audio_limit_stats(&over, &hard, &lost);
        if (k == 0) printf("  the whole song at full volume: %lld samples over full scale (a hard clip's), now %lld at full scale; %lld bent by the soft limit; rms %.4f hard-clipped, %.4f now\n",
            (long long)hard, (long long)full, (long long)over, sqrt((sq + lost) / (2.0 * o)), sqrt(sq / (2.0 * o)));
        if (k == 0) base = perSec;   /* full speed: decoding (and mixing) alone, which slows with the song: x speed per second heard */
        printf("  %-12s x%.3f%s: sound lasts %.2f s (want %.2f), %.2f ms of CPU per second heard, of which the stretch ~%.2f\n",
            PRACTICE_SPEED_NAME[bend ? PRACTICE_SPEEDS - 1 : k], sp, bend ? " + whammy" : "", heard, durationMs / sp / 1000, perSec, k ? perSec - base * sp : 0); }
    audio_set_speed(1);
}

/* ---- the judging: script 0xd8eeddfa (run when the speed is set) calls SetSlomo current_speedfactor, so the game's clock
 * and the song run at the speed together, and puts player_status.check_time_early / _late = the globals check_time_early
 * / _late * current_speedfactor (the song's setup, 0x2324fe5a, does the same after the cheats set the globals). So at
 * half speed the windows are 58 / 50 song ms: as long in real time as at full speed. hammer_time_*, strum_before_forming
 * and form_before_strumming are globals no script scales: the same song ms, longer in real time. */
const gh3_tuning *practice_tuning(const gh3_tuning *base, double speed) {
    static gh3_tuning t; t = *base; if (speed > 0 && speed < 1) { t.early *= speed; t.late *= speed; } return &t;
}

/* ---- the drum kit: practice_notemapping = [{ midinote 60..69 scr = SoundEvent params = { Event } }]. Each event's
 * <event>_struct of global_sound_events = { default_priority 50 default_buss NoRepeatFor 0.1 instancemanagement stop_oldest
 * Instancelimit 1 }, and its container plays the one file at vol=100. The buss (0x03c71b4d) holds the kit's ten events
 * and nothing else: vol=-3 in Default_BussSet, a child of User_Sfx in BussTree (the effects' volume, not the band's) */
static int kit[128]; static int kitN = -1;   /* note -> sfx event index (-1: none) */
static double kitBussDb = -3; static int kitLimit = 1; static double kitNoRepeat = 0.1;   /* as read (tests print them) */
static uint32_t crc[256];
static uint32_t qbkey_add(uint32_t c, const char *more) {   /* the key of the name `more` longer (as GH3.qbkeyAdd): "<event>_container" */
    if (!crc[1]) for (uint32_t i = 0; i < 256; i++) { uint32_t x = i; for (int k = 0; k < 8; k++) x = (x & 1) ? (x >> 1) ^ 0xEDB88320u : (x >> 1); crc[i] = x; }
    for (; *more; more++) { unsigned ch = (unsigned char)*more; if (ch >= 'A' && ch <= 'Z') ch += 32; c = (c >> 8) ^ crc[(c ^ ch) & 0xFF]; }
    return c;
}
static const qbv *find_section(const uint8_t *pak, size_t pl, const uint8_t *pab, size_t bl, uint32_t key, qbsection **keep, int *keepN) {   /* parses only the .qb file holding it */
    gh3_pakent *e = NULL; int n = gh3_pak_parse(pak, pl, pab, bl, &e); uint32_t KQB = gh3_qbkey(".qb"); const uint8_t kb[4] = { key >> 24, key >> 16, key >> 8, key }; const qbv *out = NULL; *keep = NULL; *keepN = 0;
    for (int i = 0; i < n && !out; i++) { if (e[i].type != KQB || !e[i].data || e[i].len < 32) continue;
        const uint8_t *d = e[i].data; int has = 0; for (uint32_t p = 0; p + 4 <= e[i].len && !has; p++) has = d[p] == kb[0] && !memcmp(d + p, kb, 4); if (!has) continue;
        qbsection *s; int ns = gh3_qb_parse(d, e[i].len, &s); if (ns < 0) continue;
        for (int k = 0; k < ns; k++) if (s[k].id == key) out = s[k].v;
        if (out) { *keep = s; *keepN = ns; } else gh3_qb_free_sections(s, ns); }
    if (n > 0) free(e); return out;
}
int practice_init(const uint8_t *qbpak, size_t pl, const uint8_t *qbpab, size_t bl) {
    for (int i = 0; i < 128; i++) kit[i] = -1; kitN = 0; if (!qbpak) return 0;
    qbsection *s; int ns; const qbv *a = find_section(qbpak, pl, qbpab, bl, gh3_qbkey("practice_notemapping"), &s, &ns);
    uint32_t evk[128] = { 0 };
    if (a && a->kind == QB_ARRAY) for (int i = 0; i < a->narr; i++) { const qbv *it = a->arr[i]; int note = (int)qb_num(qb_get(it, "midinote"), -1); uint32_t ev;
        if (note < 0 || note > 127 || !qb_keyof(qb_get(qb_get(it, "params"), "event"), &ev)) continue;
        int idx = sfx_event_find(qbkey_add(ev, "_container")); if (idx >= 0) { kit[note] = idx; evk[note] = ev; kitN++; } }
    if (s) gh3_qb_free_sections(s, ns);
    qbsection *rs = NULL; int nrs = 0; uint32_t buss[128] = { 0 }; int lim[128]; double nor[128];   /* the events' rules (one .qb holds them all) */
    for (int n = 0; n < 128; n++) { lim[n] = kitLimit; nor[n] = kitNoRepeat; if (kit[n] < 0) continue; uint32_t sk = qbkey_add(evk[n], "_struct"); const qbv *st = NULL;
        if (!rs) st = find_section(qbpak, pl, qbpab, bl, sk, &rs, &nrs); else for (int k = 0; k < nrs && !st; k++) if (rs[k].id == sk) st = rs[k].v;
        if (st) { qb_keyof(qb_get(st, "default_buss"), &buss[n]); lim[n] = (int)qb_num(qb_get(st, "Instancelimit"), 1); nor[n] = qb_num(qb_get(st, "NoRepeatFor"), 0); } }
    if (rs) gh3_qb_free_sections(rs, nrs);
    const qbv *bs = find_section(qbpak, pl, qbpab, bl, gh3_qbkey("Default_BussSet"), &s, &ns);   /* each buss's vol (dB) */
    for (int n = 0; n < 128; n++) if (kit[n] >= 0) { double db = buss[n] && bs ? qb_num(qb_get(qb_getk(bs, buss[n]), "vol"), kitBussDb) : kitBussDb;
        sfx_event_rules(kit[n], lim[n], nor[n], db); kitBussDb = db; kitLimit = lim[n]; kitNoRepeat = nor[n]; }
    if (s) gh3_qb_free_sections(s, ns);
    return kitN;
}
int practice_kit_note(int note) { return note >= 0 && note < 128 ? kit[note] : -1; }

/* ---- the song's drum cues: <song>_drums_notes = [[time ms, midinote, length] ...] */
static int (*cue)[2]; static int ncue;
int practice_drums_load(const uint8_t *songPak, size_t len, const char *name) {
    free(cue); cue = NULL; ncue = 0; if (kitN <= 0 || !songPak) return 0;
    for (int i = 0; i < 128; i++) if (kit[i] >= 0) sfx_event_prepare(kit[i]);   /* (the loading screen: decoded before the mixer sets them off) */
    char nm[96]; snprintf(nm, sizeof nm, "%s_drums_notes", name); qbsection *s; int ns; const qbv *a = find_section(songPak, len, NULL, 0, gh3_qbkey(nm), &s, &ns);
    if (a && a->kind == QB_ARRAY && a->narr > 0) { cue = malloc((size_t)a->narr * sizeof *cue);
        for (int i = 0; i < a->narr; i++) { const qbv *e = a->arr[i]; if (!e || e->kind != QB_ARRAY || e->narr < 2) continue; int t = (int)qb_num(e->arr[0], -1), n = (int)qb_num(e->arr[1], -1);
            if (t < 0 || practice_kit_note(n) < 0) continue; cue[ncue][0] = t; cue[ncue][1] = n; ncue++; }
        for (int i = 1; i < ncue; i++) { int c0 = cue[i][0], c1 = cue[i][1], j = i; while (j > 0 && cue[j - 1][0] > c0) { cue[j][0] = cue[j - 1][0]; cue[j][1] = cue[j - 1][1]; j--; } cue[j][0] = c0; cue[j][1] = c1; } }
    if (s) gh3_qb_free_sections(s, ns);
    return ncue;
}
int practice_drums_start(double from, double to, double speed) {
    if (!(speed > 0 && speed < 0.999) || kitN <= 0) { audio_practice_drums(NULL, -1); return -1; }   /* full speed, or no kit (no sfx bank): the band */
    int (*h)[2] = malloc((size_t)(ncue ? ncue : 1) * sizeof *h), n = 0;
    for (int i = 0; i < ncue; i++) if (cue[i][0] >= from && cue[i][0] < to) { h[n][0] = cue[i][0]; h[n][1] = kit[cue[i][1]]; n++; }
    audio_practice_drums((const int (*)[2])h, n); free(h); return n;
}
int practice_drum_cue(int i, int *t, int *note) { if (i < 0 || i >= ncue) return 0; *t = cue[i][0]; *note = cue[i][1]; return 1; }

/* ---- --stretchtest --song NAME: the kit, and the section [at, to) at `sp` through the mixer: the kit alone (the
 * player's track muted), the band stretched instead (what is no longer heard), everything (clipping); then at full
 * speed the band at 100% and at the practice level, the volumes applied halfway through (they must not undo it) */
enum { HMAX = 8192 };
static int hitN; static double hitSong[HMAX]; static int64_t hitFrame[HMAX];
static void drum_hook(int i, double songMs, int64_t frame) { (void)i; if (hitN < HMAX) { hitSong[hitN] = songMs; hitFrame[hitN] = frame; hitN++; } }
typedef struct { double sq, peak, half[2][2], lost; int64_t n, clip, over, hard; float *env; int nenv, kitV, fxV; } pass_t;   /* env: the peak of each ms; half: sq and n of each half; the most voices sounding at once; over / hard / lost: audio_limit_stats */
static double hard_rms(const pass_t *P) { return P->n ? sqrt((P->sq + P->lost) / P->n) : 0; }   /* what a hard clip would have given */
static double rms(const double *h) { return h[1] > 0 ? sqrt(h[0] / h[1]) : 0; }
/* band: the play's band level (audio_set_band_level); mid: called halfway through (the pause's volumes applied) */
static void pass(double at, double to, double sp, double lead, int drums, int player, double band, void (*mid)(void), pass_t *P) {
    enum { BLK = 441 }; static float buf[BLK * 2]; memset(P, 0, sizeof *P);
    int64_t lf = (int64_t)(lead / sp / 1000.0 * 44100), frames = lf + (int64_t)((to - at) / sp / 1000.0 * 44100), midAt = lf + (frames - lf) / 2;
    P->nenv = (int)(frames / BLK) + 1; P->env = calloc(P->nenv, sizeof *P->env);
    if (drums) practice_drums_start(at, to, sp); else audio_practice_drums(NULL, -1);
    audio_set_speed(sp); audio_set_band_level(band); hitN = 0; audio_play(at, lead); audio_mute(BUS_PLAYER, !player); audio_limit_stats(NULL, NULL, NULL);
    for (int64_t o = 0, b = 0; o < frames; o += BLK, b++) { if (mid && o <= midAt && o + BLK > midAt) mid(); audio_render(buf, BLK); float pk = 0;
        { int64_t ov, cl; double lo; audio_limit_stats(&ov, &cl, &lo); P->over += ov; P->hard += cl; if (o >= lf + 8820) P->lost += lo; }
        for (int i = 0; i < BLK * 2; i++) { float v = fabsf(buf[i]); if (v > pk) pk = v; if (v >= 0.999f) P->clip++; if (o + i / 2 >= lf + 8820) { double q = (double)buf[i] * buf[i]; P->sq += q; P->n++; int h = o + i / 2 >= midAt + 8820 ? 1 : o + i / 2 < midAt ? 0 : -1; if (h >= 0) { P->half[h][0] += q; P->half[h][1]++; } } }   /* (rms after the first 0.2 s: the player's track fading out) */
        if (pk > P->peak) P->peak = pk; P->env[b] = pk; int kv = sfx_voices_on(1), fv = sfx_voices_on(0); if (kv > P->kitV) P->kitV = kv; if (fv > P->fxV) P->fxV = fv; }
    audio_stop(); audio_mute(BUS_PLAYER, 0);
}
int practice_drumtest(double at, double to, double sp, double lead, void (*volumes)(void)) {
    int bad = 0, notes = 0;
    printf("practice drum kit (practice_notemapping -> global_sound_events):\n");
    for (int k = 0; k < 128; k++) if (kit[k] >= 0) { int c = 0; for (int i = 0; i < ncue; i++) c += cue[i][1] == k; notes++; printf("   note %d: event #%d, %d cues in the song\n", k, kit[k], c); sfx_event_show(kit[k]); }
    if (!notes) { puts("   no kit (no ZONES/global_sfx or no practice_notemapping): the band is stretched instead"); return 1; }
    int want = 0; double cueT[HMAX]; for (int i = 0; i < ncue && want < HMAX; i++) if (cue[i][0] >= at && cue[i][0] < to) cueT[want++] = cue[i][0];
    printf("   (every drum: its own buss at %+.1f dB under the effects' volume, Instancelimit %d stop_oldest, NoRepeatFor %.2f s)\n", kitBussDb, kitLimit, kitNoRepeat);
    audio_drum_hook = drum_hook; pass_t kitP, bandP, allP;
    pass(at, to, sp, lead, 1, 0, 1, NULL, &kitP); int fired = hitN, late = 0, early = 0; double worst = 0;
    for (int i = 0; i < fired && i < want; i++) { double d = hitSong[i] - cueT[i]; if (d < 0) early++; if (d > sp / 44.1 + 1e-6) late++; if (fabs(d) > worst) worst = fabs(d); }
    int lf = (int)(lead / sp / 1000.0 * 44100), audible = 0; double rise = 0;   /* each hit heard: the ms after it louder than the 5 ms before it */
    for (int i = 0; i < fired; i++) { int b = (int)((hitFrame[i] + lf) / 441); if (b < 6 || b + 12 >= kitP.nenv) continue; float pre = 0, post = 0; for (int j = b - 6; j < b - 1; j++) if (kitP.env[j] > pre) pre = kitP.env[j]; for (int j = b; j < b + 12; j++) if (kitP.env[j] > post) post = kitP.env[j]; if (post > pre * 1.2f + 0.005f) audible++; rise += post - pre; }
    printf("section %.0f..%.0f ms at x%.3f: %d drum cues, %d set off by the mixer (%d early, %d over a frame late, worst %.4f song ms off its cue); %d of them heard as a new hit\n", at, to, sp, want, fired, early, late, worst, audible);
    for (int i = 0; i < fired && i < 8; i++) printf("   hit %d: cue %.0f ms, set off at song %.3f ms = %.1f ms of real time after the music starts (want %.1f)\n", i, cueT[i], hitSong[i], hitFrame[i] / 44.1, (cueT[i] - at) / sp);
    pass_t oldP, fullP, unitP; pass(at, to, sp, lead, 0, 0, 1, NULL, &bandP); pass(at, to, sp, lead, 1, 1, 1, NULL, &allP); pass(at, to, sp, lead, 0, 1, 1, NULL, &oldP);
    pass(at, to, 1, lead, 0, 0, 1, NULL, &unitP); pass(at, to, 1, lead, 0, 0, PRACTICE_BAND_FULL, volumes, &fullP);   /* full speed: the band at 100%, then at the practice level with the volumes applied halfway (as the pause's options do) */
    audio_drum_hook = NULL; audio_practice_drums(NULL, -1); audio_set_speed(1);
    double kr = kitP.n ? sqrt(kitP.sq / kitP.n) : 0, br = bandP.n ? sqrt(bandP.sq / bandP.n) : 0, ar = allP.n ? sqrt(allP.sq / allP.n) : 0, orr = oldP.n ? sqrt(oldP.sq / oldP.n) : 0, fr = fullP.n ? sqrt(fullP.sq / fullP.n) : 0;
    printf("levels: the kit alone rms %.3f peak %.2f (%lld samples clipped); the band it replaces (stretched, at 100%%) rms %.3f peak %.2f (%lld clipped): kit %+.1f dB against it\n",
        kr, kitP.peak, (long long)kitP.clip, br, bandP.peak, (long long)bandP.clip, kr > 0 && br > 0 ? 20 * log10(kr / br) : 0);
    printf("        the band of full-speed practice (x%.3f) rms %.3f peak %.2f: kit %+.1f dB against it\n", PRACTICE_BAND_FULL, fr, fullP.peak, kr > 0 && fr > 0 ? 20 * log10(kr / fr) : 0);
    printf("        kit + player's track rms %.3f peak %.2f, %lld of %lld samples clipped; band + player's track (before) rms %.3f, %lld clipped\n", ar, allP.peak, (long long)allP.clip, (long long)allP.n, orr, (long long)oldP.clip);
    const pass_t *LP[4] = { &kitP, &allP, &oldP, &fullP }; const char *LN[4] = { "the kit alone", "kit + player's track", "band + player's track (stretched)", "full-speed practice band" };
    printf("the output's soft limit (over %.2f, -1 dBFS):\n", 0.89);
    for (int i = 0; i < 4; i++) { const pass_t *p = LP[i]; printf("   %-34s %6lld samples over full scale (a hard clip's), now %lld at full scale; %6lld bent; rms %.4f hard-clipped, %.4f now (%+.2f dB)\n", LN[i], (long long)p->hard, (long long)p->clip, (long long)p->over, hard_rms(p), p->n ? sqrt(p->sq / p->n) : 0, hard_rms(p) > 0 && p->sq > 0 ? 20 * log10(sqrt(p->sq / p->n) / hard_rms(p)) : 0); if (p->clip) bad++; }
    double h0 = rms(unitP.half[0]) > 0 ? rms(fullP.half[0]) / rms(unitP.half[0]) : 0, h1 = rms(unitP.half[1]) > 0 ? rms(fullP.half[1]) / rms(unitP.half[1]) : 0;
    printf("full-speed practice band: x%.3f of the band at 100%% before the volumes are applied, x%.3f after (want x%.3f both)\n", h0, h1, PRACTICE_BAND_FULL);
    printf("voices: the kit used at most %d of its own 20, the effects' voices at most %d while it played\n", kitP.kitV, kitP.fxV);
    free(kitP.env); free(bandP.env); free(allP.env); free(oldP.env); free(fullP.env); free(unitP.env);
    if (fired != want || early || late) bad++; if (want && audible < want / 2) bad++;
    if (fabs(h0 - PRACTICE_BAND_FULL) > 0.01 || fabs(h1 - PRACTICE_BAND_FULL) > 0.01) bad++; if (kitP.fxV) bad++;
    printf("drum test: %s\n", bad ? "FAIL" : "ok"); return bad;
}
