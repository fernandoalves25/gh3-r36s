#include "game.h"
#include "battle.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

void (*gh3_sfx_hook)(const char *name) = 0;
void (*gh3_mute_hook)(int mute) = 0;
static void sfx(gh3_run *r, const char *n) { if (!r->silent && gh3_sfx_hook) gh3_sfx_hook(n); }
static void mute(gh3_run *r, int m) { if (!r->silent && gh3_mute_hook) gh3_mute_hook(m); }

#define INPUT_SLOP 4.0            /* ms of slack for how coarsely the controller is reported */
#define GAME_FRAME (1000.0 / 60)
#define STRUM_LOOK (GAME_FRAME * 0.75)
#define MISS_HOLD (STRUM_LOOK + 6)
#define WHAMMY_KEEP 85.0
static const double HYPER[6] = { 1, 0.88, 0.83, 0.78, 0.73, 0.68 };

int game_mult(const gh3_run *r) { int m = 1 + r->streak / 10; if (m > 4) m = 4; return m * (r->spOn ? 2 : 1); }
int game_stars(double score, double base) { /* the game's rule: average multiplier over the base score; a finished song is never under 3 stars */
    double avg = base > 0 ? score / base : 0;
    return avg >= 2.8 ? 5 : avg >= 2.0 ? 4 : 3;
}

void game_new(gh3_run *r, gh3_chart *chart, const gh3_tuning *tn, int diff, const gh3_runopts *o) {
    memset(r, 0, sizeof *r);
    r->chart = chart; r->notes = chart->notes; r->nnotes = chart->nnotes; r->tn = tn; r->d = tn->diffs[diff < 0 ? 0 : diff > 3 ? 3 : diff];
    double split = tn->split > 0 ? tn->split : 0.5;
    for (int i = 0; i < r->nnotes; i++) { gh3_note *n = &r->notes[i]; n->hit = n->missed = n->dropped = 0;
        double gap = i + 1 < r->nnotes ? (r->notes[i + 1].t - n->t) * split : 1e18;
        n->lateW = fmin(tn->late, gap); n->hamLateW = fmin(tn->hamLate, gap); }
    for (int i = 0; i < chart->nphrases; i++) { chart->phrases[i].hit = 0; chart->phrases[i].failed = 0; chart->phrases[i].done = 0; }
    r->health = 1; r->hamUntil = -1e9; r->spFlash = r->missFlash = r->spAt = r->spOffAt = r->spReadyAt = r->streakAt = -1e9; r->whamMoveAt = -1e9;
    for (int f = 0; f < 5; f++) r->flash[f] = -1e9;
    r->autoPlay = o->autoPlay; r->noStrum = !o->autoPlay && o->noStrum; r->noFail = o->noFail || o->autoPlay; r->silent = o->silent; r->bass = o->bass;
    r->endMs = o->endMs; r->botRelease = -1;
    double speed = o->speed > 0 ? o->speed : 100;
    r->lookahead = r->d.lookahead * 1000 * HYPER[o->hyper >= 0 && o->hyper < 6 ? o->hyper : 0] * 100 / speed;
}
static int topFret(int held) { for (int b = 16; b; b >>= 1) if (held & b) return b; return 0; }
static int matches(int held, const gh3_note *n) { return n->n == 1 ? topFret(held) == n->mask : held == n->mask; }
static int addStar(gh3_run *r, double amount, double perf) {
    if (r->spOn || r->battle) return 0;   /* a battle has power-ups instead */ double was = r->sp; r->sp = fmin(1, was + amount);
    if (was < 0.5 && r->sp >= 0.5) { r->spReadyAt = perf; if (!r->autoPlay) sfx(r, "ready"); }
    return 1;
}
static void streakLost(gh3_run *r) { int m = 1 + r->streak / 10; if (m > 4) m = 4; if (m > 1) { char nm[8] = "lose0"; nm[4] = (char)('0' + m); sfx(r, nm); } r->streak = 0; }
static gh3_note *nextNote(gh3_run *r) { while (r->idx < r->nnotes && (r->notes[r->idx].hit || r->notes[r->idx].missed)) r->idx++; return r->idx < r->nnotes ? &r->notes[r->idx] : NULL; }
static void hitNote(gh3_run *r, gh3_note *n, double t, int hammered, double perf, int skipped) {
    (void)t;
    n->hit = 1; r->lastHitMask = n->mask; r->hitSerial++; r->streak++; if (r->streak > r->best) r->best = r->streak; r->hits++;
    if (r->streak == 50 || (r->streak > 0 && r->streak % 100 == 0)) { r->streakAt = perf; r->streakN = r->streak; sfx(r, r->streak == 50 ? "streak50" : "streak100"); }
    r->score += 50.0 * n->n * game_mult(r);
    r->health = fmin(2, r->health + (r->spOn && r->d.star ? r->d.star : r->d.good));
    if (n->sus) { r->sus = n; n->beat = gh3_beatpos(r->chart->fretbars, r->chart->nfret, n->t); } else if (r->sus && r->sus != n) r->sus = NULL;
    if (n->star >= 0) { gh3_phrase *p = &r->chart->phrases[n->star]; p->hit++; if (!p->failed && !p->done && p->hit >= p->total) { p->done = 1; if (r->battle) battle_award(r, n, perf); else if (addStar(r, 0.25, perf)) { r->spFlash = perf; sfx(r, "awarded"); } } }
    if (r->battle) battle_judge(r, n, 1);
    r->chain = skipped ? 0 : n->mask; r->hasPending = 0;
    r->nhist = 0;
    if (hammered) {
        gh3_note *nx = NULL; for (int i = r->idx; i < r->nnotes; i++) { gh3_note *e = &r->notes[i]; if (e != n && !e->hit && !e->missed && e->t > n->t) { nx = e; break; } }
        r->hamShape = n->mask; r->hamUntil = fmin(n->t + n->lateW, nx ? nx->t - r->tn->early : 1e18);
    }
    for (int f = 0; f < 5; f++) if (n->mask & (1 << f)) r->flash[f] = perf;
    mute(r, 0);
}
/* GH3's guitar_events (qb.pak): the exe sends missed_note (bum_note = 1 when a strum on the wrong frets missed it) and
 * unnecessary_note (a strum with no note to play). GuitarEvent_UnnecessaryNote always calls Guitar_Wrong_Note_Sound_Logic
 * (Single_Player_Bad_Note_Guitar / _Bass) and sets guitar_volume = 0; GuitarEvent_MissedNote calls it only for a bum_note
 * and sets guitar_volume = 0 too, so a note left to pass is a silent miss that mutes the guitar. Both: CrowdDecrease. */
static void missNote(gh3_run *r, gh3_note *n, double perf, int bum) {
    n->missed = 1; streakLost(r); r->chain = 0; r->misses++; r->health += r->d.bad; r->missFlash = perf;
    if (n->star >= 0) r->chart->phrases[n->star].failed = 1;
    r->sus = NULL; mute(r, 1); if (bum) sfx(r, r->bass ? "badBass" : "bad");
    if (r->battle) battle_judge(r, n, 0);
}
static void overstrum(gh3_run *r, double perf) {
    streakLost(r); r->chain = 0; r->overstrums++; r->health += r->d.bad; r->missFlash = perf;
    if (r->sus) { r->sus->dropped = 1; r->sus = NULL; }
    mute(r, 1); sfx(r, r->bass ? "badBass" : "bad");
}
static void wrongStrum(gh3_run *r, double perf) {   /* the strum's strum_before_forming lapsed without the frets: the note it was at is missed, with the clank */
    r->hasPending = 0; gh3_note *n = r->pendNote >= 0 && r->pendNote < r->nnotes ? &r->notes[r->pendNote] : NULL;
    if (n && !n->hit && !n->missed) { if (r->sus) r->sus->dropped = 1; missNote(r, n, perf, 1); } else overstrum(r, perf);
}
static void playAt(gh3_run *r, int i, double t, int hammered, double perf) {
    int skipped = 0; for (int k = r->idx; k < i; k++) { gh3_note *e = &r->notes[k]; if (!e->hit && !e->missed) { missNote(r, e, perf, 0); skipped++; } }
    hitNote(r, &r->notes[i], t, hammered, perf, skipped > 0);
}
static int tryHammer(gh3_run *r, double t, double perf) {
    int top = r->chain ? topFret(r->held) : 0; if (!top || top == r->chain) return 0;
    for (int i = r->idx; i < r->nnotes; i++) { gh3_note *n = &r->notes[i];
        if (n->t - r->tn->early > t) break;
        if (n->hit || n->missed || t > n->t + n->lateW) continue;
        if (n->hopo && n->mask == top && t >= n->t - r->tn->hamEarly && t < n->t + n->hamLateW) { if (top < r->chain) r->pulls++; else r->hammers++; playAt(r, i, t, 1, perf); return 1; } }
    return 0;
}
static int firstInReach(gh3_run *r, double t) { for (int i = r->idx; i < r->nnotes; i++) { gh3_note *n = &r->notes[i]; if (n->t - r->tn->early > t) break; if (!n->hit && !n->missed && t <= n->t + n->lateW) return i; } return -1; }
static void strumAt(gh3_run *r, double t, double perf);
static void onFretChange(gh3_run *r, double t, double perf, int pressed) {
    if (r->autoPlay || r->finished) return;
    if (r->hasPending) {
        if (t - r->pending > r->tn->strumBefore + INPUT_SLOP) wrongStrum(r, perf);
        else if (r->held) for (int i = r->idx; i < r->nnotes; i++) { gh3_note *n = &r->notes[i];
            if (n->t - r->tn->early > t) break;
            if (n->hit || n->missed || t > n->t + n->lateW) continue;
            if (matches(r->held, n)) { playAt(r, i, t, 0, perf); return; } }
    }
    if (r->noStrum && pressed) { strumAt(r, t, perf); return; }   /* the controller's play (Toque livre): a fret button pressed is the strum */
    tryHammer(r, t, perf);
}
void game_set_frets(gh3_run *r, int held, double t, double perf) {
    if (r->battle) held = battle_frets(r, held);   /* a broken string (or the whammy attack) takes frets out of play */
    if (held == r->held) return;
    if (r->nhist == 16) { memmove(r->hist, r->hist + 1, 15 * sizeof r->hist[0]); r->nhist = 15; }
    r->hist[r->nhist].held = r->held; r->hist[r->nhist].until = t; r->nhist++;
    int pressed = held & ~r->held; r->held = held; onFretChange(r, t, perf, pressed);
}
void game_strum(gh3_run *r, double t, double perf) { if (!r->autoPlay && !r->finished && !r->noStrum) strumAt(r, t, perf); }
static void strumAt(gh3_run *r, double t, double perf) {
    int hamOpen = t < r->hamUntil;
    int shapes[18]; int ns = 0; shapes[ns++] = r->held; double lim = t - r->tn->formBefore - INPUT_SLOP;
    for (int i = r->nhist - 1; i >= 0 && r->hist[i].until >= lim; i--) shapes[ns++] = r->hist[i].held;
    int inReach = 0, hit = -1;
    for (int i = r->idx; i < r->nnotes && hit < 0; i++) { gh3_note *n = &r->notes[i];
        if (n->t - r->tn->early > t) break;
        if (n->hit || n->missed || t > n->t + n->lateW) continue;
        inReach = 1;
        for (int k = 0; k < ns; k++) { int s = shapes[k]; if (s && !(hamOpen && s == r->hamShape) && matches(s, n)) { hit = i; break; } } }
    if (!inReach) { if (hamOpen && (r->held & r->hamShape)) return; if (r->hasPending) wrongStrum(r, perf); overstrum(r, perf); return; }   /* unnecessary_note */
    if (r->hasPending) { if (hit == r->pendNote) { r->hasPending = 0; overstrum(r, perf); } else wrongStrum(r, perf); }   /* a second strum inside the first one's window: the first was on the wrong frets */
    if (hit >= 0) { playAt(r, hit, t, 0, perf); return; }
    if (!hamOpen) { r->pending = t; r->hasPending = 1; r->pendNote = firstInReach(r, t); }   /* the frets may still come (strum_before_forming), else a bum_note miss */
}
static void starOn(gh3_run *r, double perf) { r->spOn = 1; r->spAt = perf; sfx(r, "deployed"); }
static void starOff(gh3_run *r, double perf) { r->sp = 0; r->spOn = 0; r->spOffAt = perf; sfx(r, "staroff"); }
void game_star(gh3_run *r, double perf) { if (r->battle) { battle_fire(r, perf); return; } if (!r->spOn && r->sp >= 0.5 && !r->finished) starOn(r, perf); }
void game_whammy(gh3_run *r, double v, double perf) { if (v < 0) v = 0; if (v > 1) v = 1; if (fabs(v - r->whammy) > 0.02) r->whamMoveAt = perf; r->whammy = v; }
void game_step(gh3_run *r, double t, double perf) {
    r->now = t;
    if (r->autoPlay) {
        if (r->botRelease >= 0 && t >= r->botRelease) { r->held = 0; r->botRelease = -1; }
        while (r->botIdx < r->nnotes && r->notes[r->botIdx].t <= t) { gh3_note *n = &r->notes[r->botIdx++]; if (n->hit || n->missed) continue; if ((n->bflags & BN_BOTMISS) || (r->battle && ((bside_t *)r->battle)->cpu && (n->mask & ((bside_t *)r->battle)->mask))) { missNote(r, n, perf, 0); continue; } /* (a boss under attack misses some, and those on a fret out of play) */ r->held = n->mask; r->botRelease = n->t + fmax(90, n->sus); hitNote(r, n, t, 0, perf, 0); }
        if (!r->spOn && r->sp >= 0.5) starOn(r, perf);
    } else if (r->hasPending && t - r->pending > r->tn->strumBefore + INPUT_SLOP) wrongStrum(r, perf);
    for (gh3_note *n = nextNote(r); n && t > n->t + n->lateW + (r->autoPlay ? 0 : MISS_HOLD); n = nextNote(r)) { int bum = r->hasPending && n == &r->notes[r->pendNote]; if (bum) r->hasPending = 0; missNote(r, n, perf, bum); }   /* passed unplayed: silent, unless it was strummed at on the wrong frets */
    if (!r->autoPlay) tryHammer(r, t, perf);
    double bp = gh3_beatpos(r->chart->fretbars, r->chart->nfret, t); double db = r->hasLastBeat ? fmax(0, bp - r->lastBeat) : 0; r->lastBeat = bp; r->hasLastBeat = 1;
    if (r->sus) { gh3_note *s = r->sus; double endT = s->t + s->sus, upTo = fmin(t, endT);
        if (upTo > s->t) { double b = gh3_beatpos(r->chart->fretbars, r->chart->nfret, upTo); if (b > s->beat) { r->score += 25.0 * s->n * game_mult(r) * (b - s->beat); if (s->star >= 0 && !r->chart->phrases[s->star].failed && perf - r->whamMoveAt < WHAMMY_KEEP) addStar(r, (b - s->beat) / 30, perf); s->beat = b; } }
        if (t >= endT) r->sus = NULL; else if (!r->autoPlay && (r->held & s->mask) != s->mask) { s->dropped = 1; r->sus = NULL; } }
    if (r->spOn) { r->sp -= db * (r->tn->spDrain / 400); if (r->sp <= 0) starOff(r, perf); }
    if (r->health <= 0) { r->health = 0; if (!r->noFail) r->failed = 1; }
    if (t >= r->endMs) r->finished = 1;
}
