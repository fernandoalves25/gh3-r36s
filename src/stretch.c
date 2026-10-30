#include "stretch.h"
#include "practice.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

/* The ring holds input frames by index q = input frame + PAD: the PAD frames before the song are silence, so the
 * first grains (which reach a little before the start) read zeros without a check. */
#define PAD ST_N
#define M (ST_RING - 1)
static float win[ST_N]; static int winOk;
void stretch_init(stretch_t *st, double speed) {
    memset(st, 0, sizeof *st); st->speed = speed; st->have = PAD; st->outPos = ST_H;
    if (!winOk) { for (int i = 0; i < ST_N; i++) win[i] = 0.5f - 0.5f * cosf(2 * (float)M_PI * (i + 0.5f) / ST_N); winOk = 1; }   /* two at half a grain apart sum to 1 */
}
static void fill(stretch_t *st, int64_t upto, stretch_src src, void *ud) {
    float f[ST_CH];
    while (st->have < upto) { src(ud, f); int q = (int)(st->have & M); float m = 0; for (int c = 0; c < ST_CH; c++) { st->ring[c][q] = f[c]; m += f[c]; } st->mono[q] = m; st->have++; }
}
/* how well the piece at b continues the natural one at a (normalized correlation, every `step` frames over a hop) */
static float match(const float *mono, int64_t a, int64_t b, int step) {
    float x = 0, e = 1e-9f; for (int i = 0; i < ST_H; i += step) { float v = mono[(b + i) & M]; x += mono[(a + i) & M] * v; e += v * v; }
    return x / sqrtf(e);
}
static void hop(stretch_t *st, stretch_src src, void *ud) {
    /* grain k is heard from output k*H for a whole grain; its middle (output k*H + H) should be input (k*H + H) * speed */
    int64_t want = llround((double)(st->k * ST_H + ST_H) * st->speed) - ST_H + PAD, start = want;
    fill(st, want + ST_TOL + ST_N + 8, src, ud);
    if (st->k > 0) { /* coarse: every 4th place, every 8th frame of the hop; then the best one's neighbours, every 2nd frame */
        /* (ties, as in silence, keep the place the clock wants: a drift there would put the next notes late) */
        int bd = 0; float best = match(st->mono, st->nat, want, 8) + 1e-6f;
        for (int d = -ST_TOL; d <= ST_TOL; d += 4) { if (!d || want + d < 0) continue; float s = match(st->mono, st->nat, want + d, 8); if (s > best) { best = s; bd = d; } }
        int c0 = bd; best = match(st->mono, st->nat, want + c0, 2) + 1e-6f;
        for (int d = c0 - 3; d <= c0 + 3; d++) { if (d == c0 || d < -ST_TOL || d > ST_TOL || want + d < 0) continue; float s = match(st->mono, st->nat, want + d, 2); if (s > best) { best = s; bd = d; } }
        start = want + bd; }
    for (int c = 0; c < ST_CH; c++) { float *a = st->acc[c]; const float *r = st->ring[c]; for (int i = 0; i < ST_N; i++) a[i] += r[(start + i) & M] * win[i]; }
    st->nat = start + ST_H; st->k++; st->outPos = 0;
}
void stretch_frame(stretch_t *st, float *out, stretch_src src, void *ud) {
    if (st->outPos >= ST_H) { /* the first half is out: the second half moves up and the next grain is laid over it */
        for (int c = 0; c < ST_CH; c++) { memmove(st->acc[c], st->acc[c] + ST_H, ST_H * sizeof(float)); memset(st->acc[c] + ST_H, 0, ST_H * sizeof(float)); }
        hop(st, src, ud); }
    for (int c = 0; c < ST_CH; c++) out[c] = st->acc[c][st->outPos];
    st->outPos++;
}

/* ---- self test: tones of known pitch through each practice speed. The output must be 1 / speed as long, keep each
 * tone's pitch (zero crossings over the steady middle) and its loudness (no dips from pieces out of phase). */
typedef struct { const float *in; int64_t n, pos; } tsrc;
static void tone_src(void *ud, float *f) { tsrc *t = ud; for (int c = 0; c < ST_CH; c++) f[c] = t->pos < t->n ? t->in[t->pos * ST_CH + c] : 0; t->pos++; }
static double now_ms(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6; }
enum { SR = 44100 };
static float *run(const float *in, int64_t n, double sp, int64_t *on, double *ms) {
    static stretch_t st; *on = (int64_t)ceil(n / sp) + ST_N * 2; float *out = malloc(sizeof(float) * *on * ST_CH); tsrc ts = { in, n, 0 };
    stretch_init(&st, sp); double t0 = now_ms(); for (int64_t o = 0; o < *on; o++) stretch_frame(&st, out + o * ST_CH, tone_src, &ts); *ms = now_ms() - t0; return out;
}
static int tone_check(const char *what, const float *out, int64_t on, int c, double hz, double tol) {   /* pitch by zero crossings and loudness in 10 ms blocks, over the steady middle */
    int64_t a = (int64_t)(on * 0.2), b = (int64_t)(on * 0.7); int cross = 0, sgn = out[a * ST_CH + c] >= 0; int64_t first = -1, lastX = -1; float lo = 1e9f, hi = 0;
    for (int64_t o = a + 1; o < b; o++) { int s2 = out[o * ST_CH + c] >= 0; if (s2 != sgn) { if (first < 0) first = o; else cross++; lastX = o; sgn = s2; } }
    for (int64_t o = a; o + 441 < b; o += 441) { double e = 0; for (int i = 0; i < 441; i++) { double v = out[(o + i) * ST_CH + c]; e += v * v; } float rms = (float)sqrt(e / 441); if (rms < lo) lo = rms; if (rms > hi) hi = rms; }
    double f = lastX > first ? cross / 2.0 / ((lastX - first) / (double)SR) : 0; int pok = fabs(f / hz - 1) < tol, lok = lo > hi * 0.85f;
    printf("     %-26s %6.1f Hz: heard %8.2f Hz (%+.2f%%, %+.1f cents) %s, loudness %.3f..%.3f %s\n", what, hz, f, (f / hz - 1) * 100, 1200 * log2(f / hz), pok ? "ok" : "WRONG", lo, hi, lok ? "steady" : "DIPS");
    return !pok + !lok;
}
int stretch_selftest(void) {
    const int64_t n = 4 * SR; const double mix[ST_CH] = { 440, 660, 1000, 220 }; float *in = malloc(sizeof(float) * n * ST_CH); int fails = 0;
    printf("stretch self test (4 s of input at each speed):\n");
    for (int k = 1; k < PRACTICE_SPEEDS; k++) { double sp = PRACTICE_SPEED[k], ms; int64_t on; float *out;
        printf("  %s, x%.3f:\n", PRACTICE_SPEED_NAME[k], sp);
        /* one tone on every channel: 440 Hz, then 110 Hz with its third harmonic (a low string) */
        for (int t = 0; t < 2; t++) { double hz = t ? 110 : 440; for (int64_t i = 0; i < n; i++) { double ph = 2 * M_PI * hz * i / SR; for (int c = 0; c < ST_CH; c++) in[i * ST_CH + c] = (float)(t ? 0.4 * (sin(ph) + 0.3 * sin(3 * ph)) : 0.5 * sin(ph)); }
            out = run(in, n, sp, &on, &ms); int64_t last = 0; for (int64_t o = 0; o < on; o++) if (fabsf(out[o * ST_CH]) > 0.05f) last = o;
            double dur = (last + 1) / (double)SR, wantDur = n / sp / SR; int ok = fabs(dur - wantDur) < 0.045;   /* the edges are smeared by about a grain */
            if (!t) printf("     length %.3f s (want %.3f) %s; %.2f ms of CPU per second of output\n", dur, wantDur, ok ? "ok" : "WRONG", ms / (on / (double)SR));
            fails += !ok + tone_check(t ? "tone with harmonic" : "tone", out, on, 0, hz, 0.001); free(out); }
        /* four different tones together: one search serves all channels, so each is a little off its own best place */
        for (int64_t i = 0; i < n; i++) for (int c = 0; c < ST_CH; c++) in[i * ST_CH + c] = (float)(0.5 * sin(2 * M_PI * mix[c] * i / SR));
        out = run(in, n, sp, &on, &ms); for (int c = 0; c < ST_CH; c++) fails += tone_check("4 tones at once, one is", out, on, c, mix[c], 0.005); free(out);
        /* timing: 10 ms clicks every 400 ms of input must be heard at (input time) / speed, which is what the song clock says */
        memset(in, 0, sizeof(float) * n * ST_CH); for (int64_t q = 200 * SR / 1000; q + 441 < n; q += 400 * SR / 1000) for (int i = 0; i < 441; i++) for (int c = 0; c < ST_CH; c++) in[(q + i) * ST_CH + c] = (float)(0.8 * sin(2 * M_PI * 2000 * i / SR));
        out = run(in, n, sp, &on, &ms); double sum = 0, worst = 0, osum = 0, oworst = 0; int cnt = 0;
        for (int64_t q = 200 * SR / 1000; q + 441 < n; q += 400 * SR / 1000) { double c0 = (q + 220) / sp, e = 0, et = 0; int64_t w = (int64_t)(0.15 * SR), onset = -1;   /* the click's energy centroid and its onset, in song ms */
            for (int64_t o = (int64_t)c0 - w; o < (int64_t)c0 + w; o++) { if (o < 0 || o >= on) continue; double v = out[o * ST_CH]; e += v * v; et += v * v * o; if (onset < 0 && fabs(v) > 0.4) onset = o; }
            if (e <= 0 || onset < 0) continue; double err = (et / e - c0) * sp / SR * 1000, oerr = (onset - q / sp) * sp / SR * 1000; sum += err; osum += oerr; cnt++; if (fabs(err) > fabs(worst)) worst = err; if (fabs(oerr) > fabs(oworst)) oworst = oerr; }
        int tok = cnt > 5 && fabs(sum / cnt) < 8 && fabs(worst) < 15 && fabs(osum / cnt) < 8 && fabs(oworst) < 15;   /* the hit windows are about +-100 ms */
        printf("     clicks: %d, heard on average %+.2f ms of song from their place (worst %+.2f), onsets %+.2f (worst %+.2f) %s\n", cnt, cnt ? sum / cnt : 0, worst, cnt ? osum / cnt : 0, oworst, tok ? "ok" : "WRONG"); fails += !tok; free(out); }
    free(in); printf(fails ? "stretch self test: %d FAILED\n" : "stretch self test: all passed\n", fails); return fails;
}
