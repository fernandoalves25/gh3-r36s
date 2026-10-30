/* Practice mode: the game's speeds and the run of sections chosen. */
#ifndef PRACTICE_H
#define PRACTICE_H
#include "gh3fmt.h"
/* the game's own speeds (qb.pak, script menu_choose_practice_speed_set_speed: full, slow, slower, slowest set
 * current_speedfactor to 1, 0.8, 0.666667, 0.5), full speed first */
#define PRACTICE_SPEEDS 4
extern const double PRACTICE_SPEED[PRACTICE_SPEEDS];
extern const char *PRACTICE_SPEED_NAME[PRACTICE_SPEEDS];
/* the stretch from the start of section `first` (1-based; 0 = from the beginning) to the end of section `last`
 * (0 = to the end of the song, at songEnd ms) */
void practice_range(const gh3_songdata *sd, int first, int last, double songEnd, double *from, double *to);
/* where the sound starts and how long the count-in is (both in song ms), as the browser port does: up to 3 s of the
 * song before the section (scaled by the speed), and a lead of silence so the wait is at least 2.4 s of real time */
void practice_lead(double from, double speed, double *audioFrom, double *leadSongMs);
/* --stretchtest --song NAME: the loaded song through the mixer at each speed (length and CPU) */
void practice_bench(double durationMs);
#endif
