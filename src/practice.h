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
/* the judging below full speed, as the game's scripts: check_time_early / _late times the speed (song ms); a copy */
const gh3_tuning *practice_tuning(const gh3_tuning *base, double speed);
/* below full speed GH3 plays no band (set_band_volume vol=0) and maps the song's drum cues through practice_notemapping
 * (script 0xee015cbd: SECTION=drums), whose notes 60..69 are SoundEvents of the drum kit in global_sfx (on a buss of
 * their own under User_Sfx: the effects' volume). At full speed the band plays at set_band_volume vol=7 (-4.9 dB:
 * User_Band, which the game sets back to the player's band_volume when the practice ends); the crowd is off in practice
 * either way (CROWD vol=-100). */
#define PRACTICE_BAND_FULL 0.569                   /* -4.9 dB (menu_audio_settings_get_buss_volume 7) */
int practice_init(const uint8_t *qbpak, size_t pl, const uint8_t *qbpab, size_t bl);   /* after sfx_init: the kit; how many notes have a sound */
int practice_kit_note(int midinote);               /* the sfx event a drum cue note plays, -1 for none */
int practice_drums_load(const uint8_t *songPak, size_t len, const char *name);   /* the song's drum cues the kit plays (the kit decoded); their count */
int practice_drums_start(double from, double to, double speed);   /* before audio_play: the hits in [from, to) at that speed (-1: the band instead) */
int practice_drum_cue(int i, int *t, int *note);   /* tests: the loaded cues */
int practice_drumtest(double at, double to, double speed, double leadSongMs, void (*volumes)(void));   /* --stretchtest --song: the kit through the mixer (volumes: app_apply_volumes); failures */
/* --stretchtest --song NAME: the loaded song through the mixer at each speed (length and CPU) */
void practice_bench(double durationMs);
#endif
