/* Player settings (kept in gh3.cfg next to the port) and the records table (gh3-recordes.cfg). */
#ifndef SETTINGS_H
#define SETTINGS_H
#include <stddef.h>
enum { ACT_FRET0, ACT_FRET1, ACT_FRET2, ACT_FRET3, ACT_FRET4, ACT_STRUM_UP, ACT_STRUM_DOWN, ACT_STAR, ACT_WHAMMY, ACT_PAUSE, ACT_N };
enum { BIND_NONE = 0, BIND_BUTTON = 1, BIND_AXIS = 2, BIND_STICK = 3 };
/* axis: dir -1 / +1 is the side that counts, 0 = either way; stick: id 0 left / 1 right, the deflection in any direction */
typedef struct { int type, id, dir; } bind_t;
#define BIND_SLOTS 3
#define SETTINGS_VERSION 3   /* 2: whammy on both sticks, offset = lag compensation; 3: frets on the shoulders as in the GH3 PS2 manual */
#define THEME_N 3   /* 2 Original (the game's own 2D art, original.c) is the game's look; 1 HD draws what the Original art lacks
                     * (or all of it when ZONES/global is missing); 0 pixel art only for headless checks (--theme 0) */
typedef struct {
    int tap;            /* 1 = notes play when their frets are held (no strumming) */
    int lefty;          /* mirror the highway */
    int hyper;          /* highway speed 0..5 */
    int noFail;         /* the rock meter cannot end the song */
    int stageFx;        /* crowd, lights and beams */
    int showFps;
    int preview;        /* song previews on the set list */
    int volMusic, volGuitar, volSfx;   /* 0..100: the band (and the menu music), the part being played, the effects */
    int menuMusic;      /* the game's menu music plays in the menus */
    int offsetMs;       /* audio lag compensation: the song clock used for notes and judging is the heard clock minus this */
    int bgDim;          /* 0..100: how dark the stage behind the highway is */
    int unlockAll;      /* every song open whatever the career says */
    int easyExpert, precision;   /* cheats of the game: hit windows doubled / halved (cheats.c) */
    int cheatCodes;     /* bit per cheat (the game's order) whose code was played on the cheats screen */
    int theme;          /* 0 pixel art, 1 HD (smooth), 2 Original (the game's art) */
    int introMovies;    /* the logos and the game's intro at start-up */
    int careerIntro;    /* the career's opening movie was shown (kept by Restaurar padrões) */
    int stage3d;        /* the game's venue in 3D behind the highway (only the gh3-3d build draws it; the 2D build ignores it) */
    bind_t binds[ACT_N][BIND_SLOTS];
} settings_t;
extern settings_t SET;
void settings_defaults(void);
const char *settings_home(void);                     /* GH3_HOME or "." */
int settings_load(void);
int settings_save(void);

typedef struct { char key[96]; long score; int pct, streak, stars; int plays; int career; } record_t;   /* career: completed in Career mode */
const record_t *records_get(const char *song, int diff, int part);
int records_put(const char *song, int diff, int part, long score, int pct, int streak, int stars, int career);   /* 1 when it is a new best score */
int records_load(void);
int records_save(void);
int records_count_stars(void);                       /* all stars earned, for the set list header */
#endif
