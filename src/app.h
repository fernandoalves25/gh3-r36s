/* What the screens and the main loop share. */
#ifndef APP_H
#define APP_H
#include <SDL.h>
#include "gh3fmt.h"
#include "game.h"
#include "settings.h"
#include "career.h"
enum { SC_TITLE, SC_MAIN, SC_TRAINING, SC_SETLIST, SC_SETUP, SC_OPTIONS, SC_CONTROLS, SC_CALIB, SC_TOPROCK, SC_PLAY, SC_PAUSE, SC_RESULTS, SC_ERROR };
/* the main menu's ways to play, as in the game: Career moves the career, Quickplay plays any song the career has
 * opened (scores kept, career untouched), Practice plays a song from a chosen section (nothing saved) */
enum { MODE_CAREER, MODE_QUICK, MODE_PRACTICE };
enum { IN_NONE = 0, IN_UP, IN_DOWN, IN_LEFT, IN_RIGHT, IN_OK, IN_BACK, IN_X, IN_Y, IN_L, IN_R, IN_SELECT };
typedef struct { int kind, idx, tier; } row_t;            /* kind 0 = set header, 1 = song */
typedef struct {
    gh3_songdata sd; gh3_chart chart; gh3_run run; uint8_t *pakBytes, *fsbBytes, *datBytes; gh3_fsb fsb; double durationMs; int loaded;
    const gh3_song *song; char section[64]; int part, diff;
} session_t;
typedef struct {
    int W, H; gh3_library *lib; int libOk;
    row_t *rows; int nrows, cursor, scroll;
    int screen, setupRow, pauseRow, optRow, optCat, ctlRow, ctlSlot, capturing, resRow, preset, optionsFrom, confirmQuit;
    int mode, mainRow, trainRow, topDiff, topScroll, practiceSec; double titleAt, noteAt; char note[120];   /* note: a short line shown over a menu */
    unsigned char *quickOpen;                /* per song: opened by the career on some difficulty (Quickplay, Practice) */
    int selSong, selDiff, selPart, selTier;
    char errMsg[300];
    session_t S;
    tierstate_t *tiers;                      /* career state for selDiff */
    double resumeAt;                         /* > 0: the countdown after a pause runs until this time */
    int newRecord, resultStars, resultPct; long resultScore; double resultsAt; int starsSounded;
    char unlockMsg[120]; double unlockAt; int unlockPending;   /* a message waiting to be shown on the set list */
    int fps, autoPlay;
    int testHeld, testStrum; double testStrumAt; float testWhammy; int testStar;
    double calDiff[24]; int calN; double calLastDiff, calLastTapAt; int calFrom;   /* calibration taps (ms after the click) */
    char starKeys[80]; double capAt;
} app_t;
extern app_t A;
/* services of main.c */
void app_begin_song(void);
int app_prepare_song(int songIdx);     /* loads the chart for the setup screen (no audio); 0 when fine */
int app_song_lock(int tier, int songIdx);   /* LOCK_* for the current mode */
void app_guitar_lick(void);            /* one of the game's guitar licks (the title screen's Start), then the menu music again */
/* clips of STREAMS/streamall.fsb.xen by their bank name ("encore_build_dive.mp3", "tom_intro_front_speakers.mp3",
 * "en_tutorial_1a_01_god.mp3"…): load returns the descrambled MP3 (malloc'd, NULL when missing); play puts it on the
 * menu music channel once, so it is heard on the menu screens only (it is cut on the others, and when it ends the menu
 * music comes back). Over a song, mix the loaded clip in some other way. */
uint8_t *app_stream_load(const char *name, size_t *size);
int app_play_stream(const char *name, double levelDb);
void app_session_free(void);
void app_quit(void);
void app_preview(int songIdx);         /* the preview of a song is wanted (-1: none) */
void app_resume_play(void);            /* leaves the pause with the countdown */
void app_apply_volumes(void);
void app_calib_start(int from);
void app_calib_stop(void);
int app_calib_result(void);          /* the median of the taps, ms */
void app_refresh_career(int announce);
const char *app_section_at(double t);
/* screens.c */
void screens_build_rows(void);
void screens_draw(double now);
void screens_event(int in, double now);
int screens_raw_event(const SDL_Event *ev, double now);   /* 1 when the controls screen used the event */
const char *diff_name_pt(int d);
#endif
