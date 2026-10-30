/* The career's money, the Guitar Hero store (Loja) and the encore chant, as in GH3 (see store.c). */
#ifndef STORE_H
#define STORE_H
#include "gh3fmt.h"
enum { SC_STORE = 40 };                 /* the store's screen (kept apart from app.h's list) */
enum { LOCK_BUY = 10, LOCK_FINALE };    /* a bonus song not bought yet / the one the game opens when a career is finished */
/* money */
void store_load(const gh3_library *lib);                 /* gh3-carreira.cfg; the first time, the pay of the career already played */
void store_refresh(const gh3_library *lib);              /* what is owned (bought, or played before) and whether a career is finished */
long store_cash(void);
const char *store_cash_str(void);                        /* "$1.234" */
int store_cash_for(const gh3_library *lib, int diff, int newStars, int oldStars);   /* the game's table: what a result pays */
int store_result(const gh3_song *s, int diff, int stars, int pays);   /* a song's result: pays and saves when `pays` (Career); 0 otherwise */
int store_last_gain(void);                               /* what the last result paid */
int store_song_lock(const gh3_library *lib, int tier, int songIdx);   /* LOCK_BUY / LOCK_FINALE for a bonus song not open, else 0 */
const char *store_lock_text(int songIdx, int career);   /* the set list's note on such a song ("$500 • Loja (Y)") */
int store_selftest(const gh3_library *lib);              /* --moneytest: the pay rules on a scratch state; 0 when all pass */
void store_demo(long cash);                              /* --screens: a pretend balance; nothing is saved from then on */
/* the store screen (from the career set list, Y) */
void store_open(double now);
void store_draw(double now);
void store_event(int in, double now);
void store_draw_result(int W, int y, double T);          /* "+$X" on the results screen (Career) */
/* the encore: the crowd chants first (the venue's encore_build_* clip of streamall), then the song starts */
void encore_begin(double now);                           /* the setup screen's Tocar: the chant first when it is the tier's encore in Career */
int encore_event(int in, double now);                    /* 1 when the chant screen used the input */
void encore_draw(double now);                            /* over the setup screen; starts the song when the chant is over */
int store_encore_offer(void);                            /* on the results: the tier's encore to send the player to (or -1) */
#endif
