/* Boss battles (Tom Morello, Slash, Lou), ported from the browser port's battle code, which follows the game's own
 * scripts (bossbattle_*, battlemode_*) and numbers (battlemode_powerups, Boss_*_Props, battle_<diff>_drain_offset,
 * health_change_*_boss_*):
 *   * Instead of star power the player gets power-ups: completing a phrase of battle gems (_starbattlemode) draws a
 *     card from the boss's own list (weighted, the last one drawn and the one on top less likely), up to three.
 *     The star power button fires the top card at the boss. The boss plays the song's rhythm chart, with its own
 *     battle gems, and fires his cards back a moment after he gets them.
 *   * An attack lasts its drain time (longer on the harder difficulties). While it lasts the boss misses a share of
 *     his notes and each note he plays or misses moves his meter; the player's meter moves the same way while under
 *     attack. Outside an attack neither meter moves.
 *   * The boss fails when his meter runs out: the player wins. If he lasts, the song's last cue
 *     (boss_battle_begin_deathlick) sets a Death Drain on the player. Against Lou, once he fails the rest of the song
 *     is played to finish him ("Agora acabe com ele!").
 * The battle has its own screen (SC_BATTLE) for the boss's intro card and the end banners. */
#ifndef BATTLE_H
#define BATTLE_H
#include "game.h"

typedef struct { double a, b; } bwin_t;
typedef struct {
    gh3_run *r; int diff, cpu, side;            /* side 0: the player (left), 1: the boss (right) */
    int cards[8], ncards, last, fired; double fireAt;
    int raw, mask, wUp;                          /* raw: the frets really held; mask: those taken out of play */
    gh3_chart up; int hasUp;                     /* the next difficulty's notes, for Difficulty Up */
    double shake; int flick; bwin_t wins[16]; int nwins; bwin_t lefty[8]; int nlefty;
    int broken[5], bseq[5], seq; double bNext; int whammy; double wNext;   /* broken: presses left to mend a fret */
    int death; double deathFrom, deathUntil;
    bwin_t fx[8]; int nfx;                       /* attacks still sounding (their end; the broken string's is open) */
} bside_t;
enum { BPH_NONE, BPH_INTRO, BPH_END };
typedef struct {
    int on, won, over, shown, devil, songIdx; const gh3_boss *boss; const gh3_battledata *BT; int max;
    bside_t me, cpu; gh3_run cpuRun; gh3_chart cpuChart;
    int cue, hasLastT; double lastT, finishAt;
    struct { char text[48]; int side, small; double at; uint32_t col; } alerts[8]; int nalerts;
    struct { int from; double at; } bolts[6]; int nbolts;
    struct { int side, card; double at; float x, y; } flies[4]; int nflies;
    /* SC_BATTLE: the intro card and the end banners */
    int phase, endKind, finishing; double phaseAt, phaseMs; char banner[64];
    int finishNext;                              /* > 0: the next start plays the devil's last part from there */
    uint8_t *clip[2]; size_t clipN[2]; double clipMs;
} battle_t;
extern battle_t BTL;

/* game.c */
int battle_frets(gh3_run *r, int held);           /* the held frets less those out of play; pressing a broken one mends it */
void battle_judge(gh3_run *r, gh3_note *n, int hit);
void battle_award(gh3_run *r, gh3_note *n, double perf);
void battle_fire(gh3_run *r, double perf);        /* the star power button: the top card goes at the other side */
/* main.c */
int battle_song(int songIdx);                     /* 1 when a song is played as a battle (Career and Quickplay, lead guitar) */
int battle_intro_start(int songIdx, double now);  /* the boss's intro (SC_BATTLE) before the song; 0 when there is none to show */
int battle_start(int songIdx, double *fromMs, double *leadMs);   /* after the session is loaded: 1 when it is a battle (or the devil's last part) */
void battle_tick(double perf);                    /* once a frame, after the player's run has moved on */
int battle_end(double now);                       /* the run is over: 1 while the battle's banner is to be shown first (SC_BATTLE) */
void battle_frame(double now);                    /* SC_BATTLE timing: the intro and the banners end by themselves */
void battle_input(int in, double now);            /* SC_BATTLE: A skips the intro, B goes back */
void battle_free(void);
void battle_dump(const gh3_library *lib);
void battle_mark_misses(gh3_run *r, int pct);     /* tests: the autoplay misses that share of the notes */
void battle_headless(int frames, int every, int miss, void (*save)(const char *name));   /* --frames on a boss song */
const char *battle_pu_title(int pu);               /* a power-up's name in Portuguese */
int battle_pu_kind(int pu);                        /* PU_* (or -1) */
/* render_battle.c */
void render_battle(double perf, int countdown);   /* both highways, the meters and cards, the attacks */
void battle_screen_draw(double now);              /* SC_BATTLE */
void battle_result_lines(int cx, int y);          /* the results screen of a battle */
#endif
