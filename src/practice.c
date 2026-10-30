#include "practice.h"
#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
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
        int64_t frames = (int64_t)((durationMs / sp / 1000 + 1.5) * 44100), last = 0, o = 0; double t0 = now_ms();
        while (o < frames) { audio_render(buf, BLK); for (int i = 0; i < BLK; i++) if (fabsf(buf[2 * i]) > 1e-4f || fabsf(buf[2 * i + 1]) > 1e-4f) last = o + i; o += BLK; }
        double ms = now_ms() - t0, heard = (last + 1) / 44100.0, perSec = ms / heard; audio_set_bend(0); audio_stop();
        if (k == 0) base = perSec;   /* full speed: decoding (and mixing) alone, which slows with the song: x speed per second heard */
        printf("  %-12s x%.3f%s: sound lasts %.2f s (want %.2f), %.2f ms of CPU per second heard, of which the stretch ~%.2f\n",
            PRACTICE_SPEED_NAME[bend ? PRACTICE_SPEEDS - 1 : k], sp, bend ? " + whammy" : "", heard, durationMs / sp / 1000, perSec, k ? perSec - base * sp : 0); }
    audio_set_speed(1);
}
