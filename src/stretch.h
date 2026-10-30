/* Practice below full speed: the song is played slower at the same pitch (as the game does: it slows the sound and
 * corrects the pitch). A streaming WSOLA time-stretch: grains of 44 ms laid out every 22 ms of output, each taken
 * from where it best continues the one before (a correlation search of +-11 ms), the same as the browser port's
 * stretchSound but one frame at a time inside the mixer. ST_CH channels go through together (one search for all, so
 * they stay in step): the mixer sends the player's track and the rest of the band as two stereo pairs. */
#ifndef STRETCH_H
#define STRETCH_H
#include <stdint.h>
#define ST_CH 4
#define ST_H 970                  /* round(44100 * 0.022): output hop, half a grain */
#define ST_N (2 * ST_H)           /* grain */
#define ST_TOL 485                /* the search reaches this far either side (11 ms) */
#define ST_RING 8192
typedef void (*stretch_src)(void *ud, float *frame);   /* writes the next input frame (ST_CH samples; zeros past the end) */
typedef struct {
    double speed; int64_t k, have, nat; int outPos;
    float ring[ST_CH][ST_RING], mono[ST_RING], acc[ST_CH][ST_N];
} stretch_t;
void stretch_init(stretch_t *st, double speed);        /* speed < 1: output 1 / speed as long; output sample o is input o * speed */
void stretch_frame(stretch_t *st, float *out, stretch_src src, void *ud);   /* one output frame */
int stretch_selftest(void);                            /* --stretchtest: tones through each practice speed; 0 when all pass */
#endif
