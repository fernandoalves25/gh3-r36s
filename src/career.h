/* Career progress, as the game does it: a tier opens when the one before is finished; its regular songs are all
 * available at once; after enough of them are completed (2 on Easy, 3 on Medium, 4 on Hard and Expert) the
 * boss battle (where there is one) and then the encore open; the encore (or the last battle) finishes the tier.
 * Progress is per difficulty and read from the records table (a song counts once completed on any part). */
#ifndef CAREER_H
#define CAREER_H
#include "gh3fmt.h"
enum { LOCK_OPEN = 0, LOCK_TIER, LOCK_ENCORE, LOCK_BOSS };
typedef struct { int open; int done; int regDone, regTotal, required; int bossOpen, bossDone, encoreOpen, encoreDone; } tierstate_t;
int career_required(int diff);
void career_eval(const gh3_library *lib, int diff, int unlockAll, tierstate_t *out);   /* out has lib->ntiers entries */
int career_song_lock(const gh3_library *lib, const tierstate_t *ts, int tier, int songIdx);  /* LOCK_* for a song of that tier */
int career_tiers_done(const gh3_library *lib, const tierstate_t *ts);
#endif
