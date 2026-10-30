/* The game's cheats (Options > Cheats): the list, the codes played on the frets and what each one does. */
#ifndef CHEATS_H
#define CHEATS_H
#include <SDL.h>
#include "gh3fmt.h"
void cheats_open(void);                          /* from the Options categories; back returns there */
void cheats_draw(double now);
void cheats_event(int in, double now);
int cheats_raw_event(const SDL_Event *ev, double now);   /* 1 while a code is being played (the frets are not menu keys) */
const gh3_tuning *cheats_tuning(const gh3_tuning *base, int mode);   /* the hit windows of a run: Easy Expert / Precision Mode */
int cheats_records_ok(int mode);                 /* 0 when a cheat makes the run not count (Easy Expert, as No Fail) */
#endif
