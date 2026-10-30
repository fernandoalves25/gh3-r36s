/* Run state and note judging, a port of the browser port's rules (which follow GH3.exe). */
#ifndef GAME_H
#define GAME_H
#include "gh3fmt.h"

typedef struct {
    gh3_chart *chart; gh3_note *notes; int nnotes;
    const gh3_tuning *tn; gh3_diff d;
    int idx, held; double score; int streak, best, hits, misses, overstrums; double sp; int spOn; double health;
    gh3_note *sus; int hasPending, pendNote; double pending; int chain, hamShape; double hamUntil;   /* pending: a strum on the wrong frets, pendNote the note it was at */
    struct { int held; double until; } hist[16]; int nhist;
    double flash[5], spFlash, missFlash, spAt, spOffAt, spReadyAt, streakAt; int streakN;
    double lastBeat; int hasLastBeat;
    int autoPlay, noStrum, noFail, silent, bass; double endMs; int failed, finished;
    double lookahead, now;
    double whammy, whamMoveAt;
    int botIdx; double botRelease;
    int lastHitMask, hitSerial;
    void *battle;                    /* a boss battle's side of this run (battle.h), NULL otherwise */
    int hammers, pulls;                  /* notes played without strumming, up the neck / down it (the tutorials count them) */
} gh3_run;

typedef struct { int autoPlay, noStrum, noFail, silent, bass; double endMs; double speed; int hyper; } gh3_runopts;

extern void (*gh3_sfx_hook)(const char *name);
extern void (*gh3_mute_hook)(int mute);

void game_new(gh3_run *r, gh3_chart *chart, const gh3_tuning *tn, int diff, const gh3_runopts *o);
void game_set_frets(gh3_run *r, int held, double t, double perf);
void game_strum(gh3_run *r, double t, double perf);
void game_star(gh3_run *r, double perf);
void game_whammy(gh3_run *r, double v, double perf);
void game_step(gh3_run *r, double t, double perf);
int game_mult(const gh3_run *r);
int game_stars(double score, double base);
#endif
