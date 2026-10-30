/* Boss battles: the rules (see battle.h), the boss's side and the battle's own screen. A port of the browser port's
 * battleStart / battlePick / battleAward / battleFire / battleHit / battleTick / battleOver. */
#include "battle.h"
#include "app.h"
#include "audio.h"
#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <math.h>

battle_t BTL;
static int introSkip, legendShown, verbose, nAttacks, nAwards;
static const char *PU_PT[PU_KINDS] = { "Amp Sobrecarregado", "Dificuldade Maior", "Notas Dobradas", "Roubo de Poder", "Inversão Canhota", "Corda Arrebentada", "Ataque de Alavanca", "Dreno Mortal" };
static const gh3_powerup *pu(int i) { return i >= 0 && i < BTL.BT->npu ? &BTL.BT->pu[i] : NULL; }
const char *battle_pu_title(int i) { const gh3_powerup *p = pu(i); if (!p) return ""; if (p->id == PU_LEFTY && SET.lefty) return "Inversão Destra"; return p->id >= 0 ? PU_PT[p->id] : p->title; }
int battle_pu_kind(int i) { const gh3_powerup *p = pu(i); return p ? p->id : -1; }
static double frand(void) { return rand() / (RAND_MAX + 1.0); }
static bside_t *other(bside_t *s) { return s->side ? &BTL.me : &BTL.cpu; }
static int lower_bound(const gh3_run *r, double t) { int lo = 0, hi = r->nnotes; while (lo < hi) { int m = (lo + hi) >> 1; if (r->notes[m].t < t) lo = m + 1; else hi = m; } return lo; }
static void sfx(int k) { if (!BTL.me.r->silent) audio_sfx(k); }
static void alert(const char *text, int side, int small, double perf, uint32_t col) {
    if (BTL.nalerts == 8) { memmove(BTL.alerts, BTL.alerts + 1, 7 * sizeof BTL.alerts[0]); BTL.nalerts = 7; }
    snprintf(BTL.alerts[BTL.nalerts].text, sizeof BTL.alerts[0].text, "%s", text); BTL.alerts[BTL.nalerts].side = side; BTL.alerts[BTL.nalerts].small = small; BTL.alerts[BTL.nalerts].at = perf; BTL.alerts[BTL.nalerts].col = col; BTL.nalerts++;
}
static void fx_over(bside_t *s) { (void)s; sfx(SFX_ATK_OVER); }   /* an attack is over (GH_SFX_BattleMode_Attack_Over) */

/* ---- cards (bossbattle_select) */
static int pick(bside_t *s) {
    const gh3_battledata *BT = BTL.BT; bside_t *o = other(s); int top = s->ncards ? s->cards[s->ncards - 1] : -1, ok[32], nok = 0; double tot = 0;
    for (int i = 0; i < BT->npu && nok < 32; i++) { int mine = 0; for (int k = 0; k < BTL.boss->npowerups; k++) if (BTL.boss->powerups[k] == BT->pu[i].key) mine = 1;
        if (mine && !(o->diff == 3 && BT->pu[i].id == PU_DIFFUP)) ok[nok++] = i; }   /* Difficulty Up is left out against an expert */
    if (!nok) return -1;
    for (int k = 0; k < nok; k++) tot += ok[k] == s->last || ok[k] == top ? BT->pu[ok[k]].weightLow : BT->pu[ok[k]].weight;
    if (tot <= 0) return ok[rand() % nok];
    double x = floor(frand() * tot); for (int k = 0; k < nok; k++) { x -= ok[k] == s->last || ok[k] == top ? BT->pu[ok[k]].weightLow : BT->pu[ok[k]].weight; if (x < 0) return ok[k]; }
    return ok[nok - 1];
}
void battle_award(gh3_run *r, gh3_note *n, double perf) {   /* a phrase of battle gems completed: a card, up to three (a fourth takes the newest one's place) */
    bside_t *s = r->battle; if (!s || BTL.over) return;
    int i = pick(s); if (i < 0) return; s->last = i;
    if (s->ncards >= BTL.max) s->ncards--; s->cards[s->ncards++] = i; sfx(SFX_BATTLE_AWARD);
    int f = 0; while (f < 4 && !(n->mask & (1 << f))) f++;
    if (BTL.nflies == 4) { memmove(BTL.flies, BTL.flies + 1, 3 * sizeof BTL.flies[0]); BTL.nflies = 3; }
    BTL.flies[BTL.nflies].side = s->side; BTL.flies[BTL.nflies].card = i; BTL.flies[BTL.nflies].at = perf; BTL.flies[BTL.nflies].x = (float)f; BTL.flies[BTL.nflies].y = 0; BTL.nflies++;
    alert(battle_pu_title(i), s->side, 1, perf, RGB(255, 233, 168)); nAwards++;
    if (s->cpu || r->autoPlay) s->fireAt = r->now + 900 + frand() * 2200;   /* the boss (and autoplay) use a card a moment after getting it */
}

/* ---- an attack landing on side v (from side a) at song time t */
static void addwin(bside_t *v, double x, double y, double pct, int noAi) {
    if (v->nwins == 16) { memmove(v->wins, v->wins + 1, 15 * sizeof v->wins[0]); v->nwins = 15; }
    v->wins[v->nwins].a = x; v->wins[v->nwins].b = y; v->nwins++;
    if (v->cpu && !noAi) { double P = fmax(0, fmin(100, pct)) / 100; gh3_run *r = v->r;   /* ApplyBossBattleGemMisses: that share of his notes in the stretch are missed */
        for (int i = 0; i < r->nnotes; i++) { gh3_note *n = &r->notes[i]; if (n->t >= x && n->t < y && !n->hit && !n->missed && frand() < P) n->bflags |= BN_BOTMISS; } }
}
static void late_edges(gh3_run *r, int i0, int i1) { double split = r->tn->split > 0 ? r->tn->split : 0.5;
    for (int i = i0 < 0 ? 0 : i0; i < i1 && i < r->nnotes; i++) { gh3_note *n = &r->notes[i]; double gap = i + 1 < r->nnotes ? (r->notes[i + 1].t - n->t) * split : 1e18; n->lateW = fmin(r->tn->late, gap); n->hamLateW = fmin(r->tn->hamLate, gap); } }
static void swap_up(bside_t *v, double a, double b) {   /* Difficulty Up: the notes that come up in the stretch are the next difficulty's */
    gh3_run *r = v->r; gh3_chart *c = r->chart; int i0 = lower_bound(r, a), i1 = lower_bound(r, b); if (i0 < r->idx) return;
    int k0 = 0, k1; while (k0 < v->up.nnotes && v->up.notes[k0].t < a) k0++; k1 = k0; while (k1 < v->up.nnotes && v->up.notes[k1].t < b) k1++;
    int add = k1 - k0, tail = r->nnotes - i1;   /* the buffer has room for every note of both charts (battle_start) */
    memmove(&r->notes[i0 + add], &r->notes[i1], tail * sizeof *r->notes);
    for (int k = 0; k < add; k++) { gh3_note *n = &r->notes[i0 + k]; *n = v->up.notes[k0 + k]; n->hit = n->missed = n->dropped = 0; n->bflags = 0; n->star = -1;
        for (int p = 0; p < c->nphrases; p++) if (n->t >= c->phrases[p].t && n->t < c->phrases[p].end) { n->star = p; break; } }
    r->nnotes = c->nnotes = i0 + add + tail;
    if (r->botIdx > i0) r->botIdx = i0;
    for (int p = 0; p < c->nphrases; p++) { gh3_phrase *ph = &c->phrases[p]; if (ph->done || ph->failed || ph->end <= a || ph->t >= b) continue; int cnt = 0; for (int i = 0; i < r->nnotes; i++) if (r->notes[i].star == p) cnt++; ph->total = cnt; }
    late_edges(r, i0 - 1, i0 + add + 1);
}
static void doubles(bside_t *v, double a, double b) {   /* Double Notes: each single note in the stretch gets the fret beside it */
    gh3_run *r = v->r; for (int i = 0; i < r->nnotes; i++) { gh3_note *n = &r->notes[i]; if (n->t < a || n->t >= b || n->n != 1 || n->hit || n->missed) continue;
        int f = 0; while (f < 4 && !(n->mask & (1 << f))) f++; n->mask |= 1 << (f < 4 ? f + 1 : f - 1); n->n = 2; n->bflags |= BN_DOUBLE; }
}
static void addfx(bside_t *v, double kind, double until) { if (v->nfx == 8) { memmove(v->fx, v->fx + 1, 7 * sizeof v->fx[0]); v->nfx = 7; } v->fx[v->nfx].a = kind; v->fx[v->nfx].b = until; v->nfx++; }
static void hit(bside_t *v, int pi, double t, double perf, bside_t *a, double drainMs) {
    const gh3_battledata *BT = BTL.BT; const gh3_powerup *p = pu(pi); static const gh3_powerup DEATH = { 0, PU_DEATH, "", "", 0, 0, 5000, 0, 0, 0, { 0 }, { 0 }, { 0 } };
    if (!p) p = &DEATH; int d = v->diff, id = p->id; double drain = drainMs > 0 ? drainMs : p->drain + BT->offset[d], from = t + v->r->lookahead, to = from + drain;
    const char *label = pi >= 0 ? battle_pu_title(pi) : PU_PT[PU_DEATH];
    if (id == PU_DIFFUP && (d == 3 || !v->hasUp)) { id = PU_DOUBLE; label = PU_PT[PU_DOUBLE]; }   /* battle_up_difficulty: against an expert it is Double Notes */
    double miss = BTL.boss->miss[d];
    if (id == PU_LIGHTNING) { v->shake = fmax(v->shake, t) + drain; v->flick = p->flicker[d] > 0 ? (int)p->flicker[d] : 2; addwin(v, t, v->shake, miss, p->noAi); }
    else if (id == PU_DIFFUP) { swap_up(v, from, to); addwin(v, from, to, miss, p->noAi); }
    else if (id == PU_DOUBLE) { doubles(v, from, to); addwin(v, from, to, miss, p->noAi); }
    else if (id == PU_LEFTY) { for (int i = 0; i < v->r->nnotes; i++) if (v->r->notes[i].t >= from && v->r->notes[i].t < to) v->r->notes[i].bflags ^= BN_FLIP;
        if (v->nlefty == 8) { memmove(v->lefty, v->lefty + 1, 7 * sizeof v->lefty[0]); v->nlefty = 7; } v->lefty[v->nlefty].a = from - 500; v->lefty[v->nlefty].b = to - 500; v->nlefty++; addwin(v, from, to, miss, p->noAi); }
    else if (id == PU_STRING) {
        int hi = d == 0 ? 3 : d == 1 ? 4 : 5, fr[5], nf = 0; for (int f = 0; f < hi; f++) if (!v->broken[f]) fr[nf++] = f; if (!nf) return;
        int f = fr[rand() % nf], rep = p->repair[d] > 0 ? (int)p->repair[d] : 8; v->broken[f] = rep; v->bseq[f] = ++v->seq;
        if (v->cpu) { double each = BTL.boss->stringSpeed[d] > 0 ? BTL.boss->stringSpeed[d] : 800; v->bNext = t + each; addwin(v, t, t + fmin(60000, rep * each), BTL.boss->stringMiss[d], p->noAi); } }
    else if (id == PU_WHAMMY) { int rep = p->repair[d] > 0 ? (int)p->repair[d] : 4; if (rep > v->whammy) v->whammy = rep; if (v->cpu) v->wNext = t + (BTL.boss->whammySpeed[d] > 0 ? BTL.boss->whammySpeed[d] : 500); }
    else if (id == PU_STEAL) { if (v->ncards && a) { a->cards[a->ncards++] = v->cards[--v->ncards]; if (a->ncards > BTL.max) { memmove(a->cards, a->cards + 1, (a->ncards - 1) * sizeof a->cards[0]); a->ncards--; } } }
    else if (id == PU_DEATH) { v->death = 1; v->deathFrom = t; v->deathUntil = t + drain; }
    sfx(id >= 0 && id < PU_KINDS ? SFX_ATK_LIGHTNING + id : SFX_STAR_ON);   /* GH_SFX_BattleMode_<attack> */
    if (id == PU_LIGHTNING || id == PU_DIFFUP || id == PU_DOUBLE || id == PU_LEFTY) addfx(v, 0, t + drain); else if (id == PU_STRING) addfx(v, 1, 1e18);   /* GH_Guitar_Battle_DSP_Effects: what is heard on the victim's guitar while it lasts */
    alert(label, v->side, 0, perf, id == PU_DEATH ? RGB(255, 90, 60) : RGB(255, 255, 255)); nAttacks++;
    if (verbose) printf("  %6.0f ms  %-20s on %s (meters %.2f / %.2f)\n", t, label, v->cpu ? BTL.boss->name : "the player", BTL.me.r->health, BTL.cpu.r->health);
    if (p->bolt || id == PU_DEATH) { if (BTL.nbolts == 6) { memmove(BTL.bolts, BTL.bolts + 1, 5 * sizeof BTL.bolts[0]); BTL.nbolts = 5; } BTL.bolts[BTL.nbolts].from = a ? a->side : !v->side; BTL.bolts[BTL.nbolts].at = perf; BTL.nbolts++; }
}
static int fire(bside_t *s, double perf) {
    if (BTL.over || !s->ncards || s->r->finished) return 0;
    int i = s->cards[--s->ncards]; s->fired++; hit(other(s), i, s->r->now, perf, s, 0); return 1;
}
void battle_fire(gh3_run *r, double perf) { if (r->battle && BTL.on) fire(r->battle, perf); }

/* ---- frets out of play */
static int block(const bside_t *s) { if (s->whammy > 0) return 31; int m = 0; for (int f = 0; f < 5; f++) if (s->broken[f]) m |= 1 << f; return m; }
static void mend(bside_t *s, int f) { if (s->broken[f] <= 1) { s->broken[f] = 0; fx_over(s); } else s->broken[f]--; }
int battle_frets(gh3_run *r, int held) {
    bside_t *s = r->battle; int rise = held & ~s->raw; s->raw = held;
    for (int f = 0; f < 5; f++) if (s->broken[f] && (rise & (1 << f))) mend(s, f);
    s->mask = block(s); return held & ~s->mask;
}
static int under(const bside_t *s, double t) {
    for (int i = 0; i < s->nwins; i++) if (t >= s->wins[i].a && t < s->wins[i].b) return 1;
    return (!s->cpu && s->mask) || (s->death && t >= s->deathFrom);
}
void battle_judge(gh3_run *r, gh3_note *n, int hit) {   /* a note played or missed: the meters move only while under attack */
    bside_t *s = r->battle; if (!s || !BTL.on || BTL.over || BTL.won) return;
    if (s->cpu && !BTL.me.r->silent) audio_mute(BUS_OTHER, !hit);   /* the boss's part is the rhythm track */
    if (!under(s, n->t)) return;
    int d = s->diff; double dh = s->cpu ? (hit ? BTL.boss->gain[d] : -BTL.boss->loss[d]) / 100 : hit ? BTL.BT->good[d] : BTL.BT->bad[d];
    r->health = fmax(0, fmin(2, r->health + dh));
    if (s->cpu && r->health <= 0) { BTL.won = 1; BTL.me.r->finished = 1; }   /* the boss failed there and then: the player has won */
}

/* ---- set up */
static int near_diff(const gh3_songdata *sd, int part, int want) {
    if (sd->parts[part][want].raw) return want;
    for (int dist = 1; dist < 4; dist++) for (int sg = -1; sg <= 1; sg += 2) { int d = want + sg * dist; if (d >= 0 && d < 4 && sd->parts[part][d].raw) return d; }
    return -1;
}
static int battle_chart(const gh3_songdata *sd, int part, int diff, gh3_chart *out) {   /* the chart with the battle gems' phrases in place of the star power ones */
    gh3_songdata c = *sd; c.parts[part][diff].star = c.parts[part][diff].battle; c.parts[part][diff].nstar = c.parts[part][diff].nbattle;
    const gh3_song *s = A.S.song; return gh3_chart_build(&c, part, diff, s->hmScale > 0 ? s->hmScale : A.lib->tuning.hmScale, A.lib->tuning.susShorten, out);
}
static void side_init(bside_t *s, gh3_run *r, int diff, int cpu) {
    memset(s, 0, offsetof(bside_t, up)); s->r = r; s->diff = diff; s->cpu = cpu; s->side = cpu; s->last = -1; s->shake = -1e9;
    r->battle = s; r->d.good = r->d.bad = r->d.star = 0;   /* the meters move by the battle's own rules */
}
static void roomy(gh3_chart *c, int more) { c->notes = realloc(c->notes, (c->nnotes + more + 1) * sizeof *c->notes); }
int battle_song(int si) {
    const gh3_library *lib = A.lib; return A.mode != MODE_PRACTICE && si >= 0 && si < lib->nsongs && lib->songs[si].isBoss && gh3_library_boss(lib, si) && A.selPart == 0;
}
void battle_free(void) {
    if (BTL.me.r && BTL.me.r->battle == &BTL.me) BTL.me.r->battle = NULL;
    if (BTL.me.hasUp) gh3_chart_free(&BTL.me.up); if (BTL.cpu.hasUp) gh3_chart_free(&BTL.cpu.up); gh3_chart_free(&BTL.cpuChart);
    memset(&BTL, 0, offsetof(battle_t, phase));
}
int battle_start(int si, double *fromMs, double *leadMs) {
    battle_free(); session_t *S = &A.S; gh3_run *r = &S->run;
    int fin = BTL.finishNext > 0 ? BTL.finishNext : BTL.finishing > 0 && A.screen == SC_PAUSE ? BTL.finishing : 0;   /* (a restart from the pause plays the last part again) */
    BTL.finishNext = 0; BTL.finishing = 0; legendShown = 0;
    if (fin > 0) {   /* the devil failed: the rest of the song is played to finish him, a normal song from there */
        int i = 0; while (i < r->nnotes && r->notes[i].t < fin - 1) i++; if (i >= r->nnotes) return 0;
        memmove(r->notes, r->notes + i, (r->nnotes - i) * sizeof *r->notes); r->nnotes -= i; S->chart.nnotes = r->nnotes;
        BTL.finishing = fin; *fromMs = fmax(0, fin - 1500.0); *leadMs = 1200; return 1; }
    if (!battle_song(si) || S->part != 0) return 0;
    const gh3_library *lib = A.lib; int diff = S->diff, bdiff = near_diff(&S->sd, 1, diff); if (bdiff < 0) return 0;
    if (battle_chart(&S->sd, 1, bdiff, &BTL.cpuChart)) { gh3_chart_free(&BTL.cpuChart); return 0; }
    gh3_chart pc; if (battle_chart(&S->sd, 0, diff, &pc)) { gh3_chart_free(&pc); gh3_chart_free(&BTL.cpuChart); return 0; }
    gh3_chart_free(&S->chart); S->chart = pc;
    gh3_runopts o; memset(&o, 0, sizeof o); o.autoPlay = r->autoPlay; o.noStrum = r->noStrum; o.noFail = r->noFail; o.silent = r->silent; o.bass = r->bass; o.endMs = r->endMs; o.speed = 100; o.hyper = SET.hyper;
    double L = r->lookahead; game_new(r, &S->chart, &lib->tuning, diff, &o); r->lookahead = L;
    gh3_runopts oc = o; oc.autoPlay = 1; oc.silent = 1; oc.noFail = 1; game_new(&BTL.cpuRun, &BTL.cpuChart, &lib->tuning, bdiff, &oc);
    BTL.on = 1; BTL.songIdx = si; BTL.boss = gh3_library_boss(lib, si); BTL.BT = &lib->battle; BTL.max = lib->battle.max > 0 && lib->battle.max <= 8 ? lib->battle.max : 3; BTL.devil = !strcmp(S->song->name, "bossdevil");
    side_init(&BTL.me, r, diff, 0); side_init(&BTL.cpu, &BTL.cpuRun, bdiff, 1);
    if (diff < 3 && S->sd.parts[0][diff + 1].raw && !battle_chart(&S->sd, 0, diff + 1, &BTL.me.up)) BTL.me.hasUp = 1;   /* Difficulty Up plays the harder chart's notes for a while */
    if (bdiff < 3 && S->sd.parts[1][bdiff + 1].raw && !battle_chart(&S->sd, 1, bdiff + 1, &BTL.cpu.up)) BTL.cpu.hasUp = 1;
    roomy(&S->chart, BTL.me.hasUp ? BTL.me.up.nnotes : 0); r->notes = S->chart.notes; roomy(&BTL.cpuChart, BTL.cpu.hasUp ? BTL.cpu.up.nnotes : 0); BTL.cpuRun.notes = BTL.cpuChart.notes;
    if (BTL.devil && S->sd.nmarkers > 37) {   /* Lou: once he fails, the rest is played to finish him (start_devil_finish, from the 38th marker) */
        BTL.finishAt = S->sd.markers[37].t; int n = 0; while (n < r->nnotes && r->notes[n].t < BTL.finishAt) n++; r->nnotes = S->chart.nnotes = n; if (r->endMs > BTL.finishAt) r->endMs = BTL.finishAt; }
    *fromMs = 0; (void)leadMs; return 1;
}
void battle_mark_misses(gh3_run *r, int pct) { for (int i = 0; i < r->nnotes; i++) if (rand() % 100 < pct) r->notes[i].bflags |= BN_BOTMISS; }

/* ---- once a frame */
void battle_tick(double perf) {
    if (!BTL.on || BTL.over) return;
    bside_t *me = &BTL.me, *cpu = &BTL.cpu; double t = me->r->now, dts = BTL.hasLastT ? fmax(0, t - BTL.lastT) : 0; BTL.lastT = t; BTL.hasLastT = 1;
    game_step(cpu->r, t, perf);
    { const gh3_songdata *sd = &A.S.sd; uint32_t K = gh3_qbkey("boss_battle_begin_deathlick"); int dl = -1; for (int i = 0; i < BTL.BT->npu; i++) if (BTL.BT->pu[i].id == PU_DEATH) dl = i;
      while (BTL.cue < sd->ncues && sd->cues[BTL.cue].t <= t) { const gh3_cue *c = &sd->cues[BTL.cue++]; if (c->scr == K) hit(me, dl, c->t, perf, cpu, 5000); } }   /* the song's last cue: his Death Drain on the player */
    for (int k = 0; k < 2; k++) { bside_t *s = k ? cpu : me; int aut = s->cpu || s->r->autoPlay; const gh3_boss *b = BTL.boss; int d = s->diff;
        if (aut && t >= s->bNext) { int f = -1; for (int g = 0; g < 5; g++) if (s->broken[g] && (f < 0 || s->bseq[g] < s->bseq[f])) f = g; if (f >= 0) { mend(s, f); s->bNext = t + (s->cpu ? (b->stringSpeed[d] > 0 ? b->stringSpeed[d] : 800) : 220); } }
        if (aut && s->whammy > 0 && t >= s->wNext) { s->whammy--; s->wNext = t + (s->cpu ? (b->whammySpeed[d] > 0 ? b->whammySpeed[d] : 500) : 300); if (!s->whammy) fx_over(s); }
        if (s->death && t >= s->deathFrom) { if (t < s->deathUntil) s->r->health = fmax(0, s->r->health - 2 * dts / fmax(1, s->deathUntil - s->deathFrom)); else if (s->r->health > 0) s->death = 0; }
        if (!s->cpu && !s->r->silent) audio_sfx_loop(SFX_HEARTBEAT, s->death && t >= s->deathFrom && t < s->deathUntil && !BTL.over);   /* GH3_Battlemode_Heartbeat while the player is drained */
        if (aut && s->ncards && s->fireAt > 0 && t >= s->fireAt) { fire(s, perf); s->fireAt = s->ncards ? t + 1500 + frand() * 2500 : 0; }
        { int n = 0; for (int i = 0; i < s->nwins; i++) if (s->wins[i].b > t) s->wins[n++] = s->wins[i]; s->nwins = n; }
        { int anyBroken = 0; for (int f = 0; f < 5; f++) if (s->broken[f]) anyBroken = 1; for (int i = 0; i < s->nfx; i++) if (s->fx[i].a == 1 && !anyBroken) s->fx[i].b = t;   /* a mended string ends its sound */
          int was = s->nfx, n = 0; for (int i = 0; i < s->nfx; i++) if (s->fx[i].b > t) s->fx[n++] = s->fx[i]; s->nfx = n; if (n < was && !n && !anyBroken && !s->whammy) fx_over(s); }
        if (!aut && s->whammy > 0) { double w = s->r->whammy; if (w >= 0.5 && !s->wUp) { s->wUp = 1; if (!--s->whammy) fx_over(s); } else if (w < 0.25) s->wUp = 0; }
        if (!s->cpu) { int m = block(s); if (m != s->mask) { s->mask = m; if (!aut) game_set_frets(s->r, s->raw, t, perf); } } }
    if (cpu->r->health <= 0 && !BTL.won) { cpu->r->health = 0; BTL.won = 1; me->r->finished = 1; }
}

/* ---- the battle's screen: the boss's intro, then the end banners */
static int mp3_info(const uint8_t *b, size_t n, int *hz, double *ms) {   /* the first frame's sampling rate, and the length at its bit rate */
    static const int SR[3] = { 44100, 48000, 32000 }, BR1[16] = { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0 }, BR2[16] = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 };
    for (size_t i = 0; i + 4 < n && i < 65536; i++) { if (b[i] != 0xFF || (b[i + 1] & 0xE0) != 0xE0) continue;
        int ver = (b[i + 1] >> 3) & 3, layer = (b[i + 1] >> 1) & 3, bri = b[i + 2] >> 4, sri = (b[i + 2] >> 2) & 3;
        if (ver == 1 || layer != 1 || !bri || bri == 15 || sri == 3) continue;
        *hz = SR[sri] >> (ver == 3 ? 0 : ver == 2 ? 1 : 2); *ms = (n - i) * 8.0 / (ver == 3 ? BR1 : BR2)[bri]; return 0; }
    return -1;
}
static void clip_name(int si, int back, char *out, size_t cap) { const char *nm = A.lib->songs[si].name; if (!strncmp(nm, "boss", 4)) nm += 4; if (!strcmp(nm, "devil")) nm = "lou"; snprintf(out, cap, "%s_intro_%s_speakers.mp3", nm, back ? "back" : "front"); }
static void clips_free(void) { for (int k = 0; k < 2; k++) { free(BTL.clip[k]); BTL.clip[k] = NULL; BTL.clipN[k] = 0; } }
int battle_intro_start(int si, double now) {
    if (introSkip) { introSkip = 0; return 0; }
    if (BTL.finishNext > 0 || !battle_song(si) || A.screen == SC_PAUSE) return 0;   /* (a restart from the pause goes straight in) */
    clips_free(); BTL.clipMs = 0; app_session_free();   /* the clips play on the song's buses: front and back speakers, mixed */
    for (int k = 0; k < 2; k++) { char nm[96]; clip_name(si, k, nm, sizeof nm); int hz = 0; double ms = 0; BTL.clip[k] = app_stream_load(nm, &BTL.clipN[k]);
        if (BTL.clip[k] && mp3_info(BTL.clip[k], BTL.clipN[k], &hz, &ms)) { free(BTL.clip[k]); BTL.clip[k] = NULL; BTL.clipN[k] = 0; }   /* (48 kHz: the mixer reads it on at its rate) */
        if (BTL.clip[k]) { audio_set_stream(k ? BUS_OTHER : BUS_BAND, BTL.clip[k], BTL.clipN[k], k ? -4 : 0); if (ms > BTL.clipMs) BTL.clipMs = ms; } }
    if (BTL.clipMs > 0) { app_apply_volumes(); audio_play(0, 0); }
    A.screen = SC_BATTLE; BTL.phase = BPH_INTRO; BTL.phaseAt = now; BTL.phaseMs = BTL.clipMs > 0 ? fmin(40000, BTL.clipMs + 300) : 3000; BTL.songIdx = si;
    return 1;
}
static void intro_done(void) { audio_clear(); BTL.phase = BPH_NONE; introSkip = 1; app_begin_song(); introSkip = 0; clips_free(); }
int battle_end(double now) {
    gh3_run *r = &A.S.run;
    if (!BTL.on) {   /* the devil's last part played through: the golden guitar */
        if (BTL.finishing > 0 && !r->failed && !legendShown) { legendShown = 1; snprintf(BTL.banner, sizeof BTL.banner, "Você é uma lenda do rock!"); BTL.endKind = 3; BTL.phaseMs = 4400; /* TODO movie: Golden_Guitar */ audio_fade_out(0.5); audio_sfx(SFX_DEVIL_DIE); A.screen = SC_BATTLE; BTL.phase = BPH_END; BTL.phaseAt = now; return 1; }
        return 0; }
    if (BTL.shown) return 0;
    if (!BTL.won) r->failed = 1;   /* a battle is won only by making the boss fail */
    BTL.over = 1; audio_fade_out(0.3);
    if (BTL.won && BTL.finishAt > 0) { snprintf(BTL.banner, sizeof BTL.banner, "Agora acabe com ele!"); BTL.endKind = 2; BTL.phaseMs = 2800; /* TODO movie: Satan-Battle_WIN */ }
    else if (BTL.won) { snprintf(BTL.banner, sizeof BTL.banner, "Você arrasou!"); BTL.endKind = 1; BTL.phaseMs = 4300; /* TODO movie: Player1_wins */ audio_sfx(SFX_WIN_P1); }
    else { snprintf(BTL.banner, sizeof BTL.banner, "%s arrasou!", BTL.boss->name); BTL.endKind = 0; BTL.phaseMs = 4300; /* TODO movie: Satan-Battle_LOSS (Lou), Player2_wins */ audio_sfx(SFX_WIN_P2); }
    A.screen = SC_BATTLE; BTL.phase = BPH_END; BTL.phaseAt = now; return 1;
}
static void results_count(void) { gh3_run *r = &A.S.run; int n = r->nnotes; while (n > 0 && !r->notes[n - 1].hit && !r->notes[n - 1].missed) n--; r->nnotes = n; }   /* the results count the notes the battle got to */
static void end_done(double now) {
    BTL.phase = BPH_NONE;
    if (BTL.endKind == 2) { BTL.finishNext = (int)BTL.finishAt; app_begin_song(); return; }   /* now finish him: the rest of the song */
    if (BTL.on) results_count(); BTL.shown = 1; A.screen = SC_PLAY; app_finish_song(now);
}
void battle_frame(double now) {
    if (A.screen != SC_BATTLE) return;
    if (now - BTL.phaseAt < BTL.phaseMs) return;
    if (BTL.phase == BPH_INTRO) intro_done(); else if (BTL.phase == BPH_END) end_done(now);
}
void battle_input(int in, double now) {
    if (BTL.phase == BPH_INTRO) { if (in == IN_OK && now - BTL.phaseAt > 250) { audio_sfx(SFX_OK); intro_done(); }
        else if (in == IN_BACK) { audio_clear(); clips_free(); BTL.phase = BPH_NONE; audio_sfx(SFX_BACK); if (app_prepare_song(BTL.songIdx) == 0) A.screen = SC_SETUP; } }
    else if (BTL.phase == BPH_END && in == IN_OK && now - BTL.phaseAt > 1200) end_done(now);
}

/* --frames on a boss song: the battle played by autoplay (miss: the share of the player's notes it misses, and the
 * player can fail), frames saved every `every` and shortly after each attack, then the end banner */
void battle_headless(int frames, int every, int miss, void (*save)(const char *name)) {
    gh3_run *r = &A.S.run; if (miss > 0) { battle_mark_misses(r, miss); r->noFail = 0; }
    printf("battle vs %s: player %s (%d notes), boss rhythm %s (%d notes)%s\n", BTL.boss->name, GH3_DIFF_NAME[BTL.me.diff], r->nnotes, GH3_DIFF_NAME[BTL.cpu.diff], BTL.cpuRun.nnotes, BTL.finishAt > 0 ? ", finished from the 38th marker" : "");
    { uint32_t K = gh3_qbkey("boss_battle_begin_deathlick"); for (int i = 0; i < A.S.sd.ncues; i++) if (A.S.sd.cues[i].scr == K) printf("  Death Drain cue at %d ms (song end %.0f ms)\n", A.S.sd.cues[i].t, r->endMs); }
    verbose = 1; double t = -2000, perf = 0, step = 1000.0 / 60; int saved = 0, seen = 0, seenA = 0, grab = -1, grabA = -1, f = 0; char nm[48];
    for (; f < frames; f++, t += step, perf += step) {
        game_step(r, t, perf); battle_tick(perf); snprintf(A.S.section, sizeof A.S.section, "%s", app_section_at(t));
        if (nAttacks != seen) { seen = nAttacks; if (grab < 0) grab = f + 20; }
        if (nAwards != seenA) { seenA = nAwards; if (grabA < 0) grabA = f + 25; }   /* a card just won, in its slot */
        if (f % every == 0 || f == grab || f == grabA) { gfx_begin(); render_battle(perf, 1); gfx_end(); snprintf(nm, sizeof nm, "%s_%05d", f == grab ? "attack" : f == grabA ? "card" : "frame", f); save(nm); saved++; if (f == grab) grab = -1; if (f == grabA) grabA = -1; }
        if (r->finished || r->failed) break;
    }
    verbose = 0;
    printf("battle over at %.0f ms after %d frames (%d saved): %s; meters %.2f / %.2f; attacks %d by the player, %d by %s; hits %d, misses %d; boss hits %d, misses %d\n", t, f, saved, BTL.won ? "WON" : r->failed ? "LOST (the player failed)" : "LOST (the boss lasted)",
        r->health, BTL.cpuRun.health, BTL.me.fired, BTL.cpu.fired, BTL.boss->name, r->hits, r->misses, BTL.cpuRun.hits, BTL.cpuRun.misses);
    if (battle_end(perf)) for (int k = 0; k < 2; k++) { double at = perf + (k ? 2000 : 150); gfx_begin(); battle_screen_draw(at); gfx_end(); snprintf(nm, sizeof nm, "end_%d", k); save(nm); }
    printf("banner: %s%s\n", BTL.banner, BTL.endKind == 2 ? " (then the rest of the song)" : "");
    if (BTL.endKind != 2) { BTL.phase = BPH_NONE; results_count(); BTL.shown = 1; A.screen = SC_PLAY; app_finish_song(perf); A.resultsAt = perf - 3000; gfx_begin(); screens_draw(perf); gfx_end(); save("results"); }
}
void battle_dump(const gh3_library *lib) {
    const gh3_battledata *B = &lib->battle;
    printf("battle: %d power-ups, %d bosses, max %d cards\n", B->npu, B->nbosses, B->max);
    for (int d = 0; d < 4; d++) printf("  %-6s drain offset %.0f good %.4f bad %.4f\n", GH3_DIFF_NAME[d], B->offset[d], B->good[d], B->bad[d]);
    for (int i = 0; i < B->npu; i++) { const gh3_powerup *p = &B->pu[i]; printf("  power-up %-20s id %d weight %.0f/%.0f drain %.0f bolt %d noAi %d imm %d repair %.0f/%.0f/%.0f/%.0f flicker %.0f/%.0f/%.0f/%.0f\n", p->title, p->id, p->weight, p->weightLow, p->drain, p->bolt, p->noAi, p->immediate, p->repair[0], p->repair[1], p->repair[2], p->repair[3], p->flicker[0], p->flicker[1], p->flicker[2], p->flicker[3]); }
    for (int i = 0; i < B->nbosses; i++) { const gh3_boss *b = &B->bosses[i]; printf("  boss %s (%s): %d power-ups:", b->name, lib->songs[b->songIdx].name, b->npowerups);
        for (int k = 0; k < b->npowerups; k++) for (int j = 0; j < B->npu; j++) if (B->pu[j].key == b->powerups[k]) printf(" %s", B->pu[j].title);
        printf("\n    gain %.1f/%.1f/%.1f/%.1f loss %.1f/%.1f/%.1f/%.1f miss %.0f/%.0f/%.0f/%.0f whammy %.0f string %.0f stringMiss %.0f (expert)\n", b->gain[0], b->gain[1], b->gain[2], b->gain[3], b->loss[0], b->loss[1], b->loss[2], b->loss[3], b->miss[0], b->miss[1], b->miss[2], b->miss[3], b->whammySpeed[3], b->stringSpeed[3], b->stringMiss[3]);
        for (int k = 0; k < 2; k++) { char nm[96]; clip_name(b->songIdx, k, nm, sizeof nm); size_t n; uint8_t *c = app_stream_load(nm, &n); int hz = 0; double ms = 0; if (c) mp3_info(c, n, &hz, &ms); printf("    %s: %s", nm, c ? "" : "not in streamall\n"); if (c) printf("%zu bytes, %d Hz, about %.1f s\n", n, hz, ms / 1000); free(c); } }
}
