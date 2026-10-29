#include "career.h"
#include "settings.h"
#include <string.h>

int career_required(int diff) { static const int req[4] = { 2, 3, 4, 4 }; return req[diff < 0 ? 0 : diff > 3 ? 3 : diff]; }
static int completed(const gh3_song *s, int diff) { return records_get(s->name, diff, 0) != NULL || records_get(s->name, diff, 1) != NULL; }
void career_eval(const gh3_library *lib, int diff, int unlockAll, tierstate_t *out) {
    int prevDone = 1;
    for (int t = 0; t < lib->ntiers; t++) {
        const gh3_tier *tr = &lib->tiers[t]; tierstate_t *s = &out[t]; memset(s, 0, sizeof *s);
        if (!tr->career) { s->open = 1; s->done = 1; continue; }
        s->required = career_required(diff);
        for (int k = 0; k < tr->nsongs; k++) { int si = tr->songIdx[k]; if (si == tr->bossIdx || si == tr->encoreIdx) continue; s->regTotal++; if (completed(&lib->songs[si], diff)) s->regDone++; }
        if (s->required > s->regTotal) s->required = s->regTotal;
        s->open = unlockAll || prevDone;
        int enough = s->regDone >= s->required;
        s->bossDone = tr->bossIdx >= 0 && completed(&lib->songs[tr->bossIdx], diff);
        s->encoreDone = tr->encoreIdx >= 0 && completed(&lib->songs[tr->encoreIdx], diff);
        s->bossOpen = tr->bossIdx >= 0 && (unlockAll || (s->open && enough));
        s->encoreOpen = tr->encoreIdx >= 0 && (unlockAll || (s->open && enough && (tr->bossIdx < 0 || s->bossDone)));
        s->done = tr->encoreIdx >= 0 ? s->encoreDone : tr->bossIdx >= 0 ? s->bossDone : enough;
        prevDone = s->done;
    }
}
int career_song_lock(const gh3_library *lib, const tierstate_t *ts, int t, int si) {
    const gh3_tier *tr = &lib->tiers[t]; const tierstate_t *s = &ts[t];
    if (!tr->career) return LOCK_OPEN;
    if (!s->open) return LOCK_TIER;
    if (si == tr->bossIdx) return s->bossOpen ? LOCK_OPEN : LOCK_BOSS;
    if (si == tr->encoreIdx) return s->encoreOpen ? LOCK_OPEN : LOCK_ENCORE;
    return LOCK_OPEN;
}
int career_tiers_done(const gh3_library *lib, const tierstate_t *ts) { int n = 0; for (int t = 0; t < lib->ntiers; t++) if (lib->tiers[t].career && ts[t].done) n++; return n; }
