/* Song playback: up to three MP3 streams (the player's track, the band, a track kept apart)
 * decoded with minimp3 inside the SDL audio callback, and a song clock derived from it.
 * Also a handful of synthesized sound effects (menus, bad notes, star power, fanfares) and song previews. */
#ifndef AUDIO_H
#define AUDIO_H
#include <stdint.h>
#include <stddef.h>
enum { BUS_PLAYER = 0, BUS_BAND = 1, BUS_OTHER = 2, BUS_N = 3 };
/* the effects: the game's own sounds through sfx.c, the synthesized ones when the bank is missing. SFX_LOSE is the
 * 2x multiplier lost, LOSE3 / LOSE4 the others; the ATK_ kinds are in PU_* order (battle.h) */
enum { SFX_MOVE, SFX_OK, SFX_BACK, SFX_BAD, SFX_BAD_BASS, SFX_READY, SFX_STAR_ON, SFX_STAR_OFF, SFX_STREAK, SFX_DING, SFX_FANFARE, SFX_TICK, SFX_GO, SFX_FAIL, SFX_LOSE,
    SFX_LOSE3, SFX_LOSE4, SFX_STREAK100, SFX_AWARDED, SFX_ROCK, SFX_CASH, SFX_WHOOSH, SFX_INTRO, SFX_HIGHWAY, SFX_WIN_P1, SFX_WIN_P2,
    SFX_ATK_LIGHTNING, SFX_ATK_DIFFUP, SFX_ATK_DOUBLE, SFX_ATK_STEAL, SFX_ATK_LEFTY, SFX_ATK_STRING, SFX_ATK_WHAMMY, SFX_ATK_DEATH, SFX_ATK_OVER, SFX_BATTLE_AWARD, SFX_SUDDEN_DEATH, SFX_DEVIL_DIE, SFX_HEARTBEAT, SFX_WARN, SFX_N };
int audio_open(void);
void audio_close(void);
int audio_has_device(void);
void audio_set_stream(int bus, const uint8_t *mp3, size_t size, double levelDb);
void audio_clear(void);
void audio_play(double fromMs, double leadMs);   /* starts the clock at -leadMs, streams at fromMs */
void audio_stop(void);
void audio_pause(int paused);
int audio_running(void);
double audio_song_ms(void);                       /* song position heard right now (ms), smoothed */
void audio_mute(int bus, int mute);
void audio_set_master(double gain);
void audio_set_bus_gain(int bus, double gain);   /* the player track (guitar) and the band have their own volumes */
double audio_duration_ms(void);                   /* best estimate from the stream headers */
void audio_fade_out(double sec);
void audio_selftest(void);
void audio_set_bend(double amount);               /* whammy bend of the player track, 0..1 (only while a sustain is held) */
void audio_metronome(double periodMs, double leadMs); /* calibration: clicks at song time 0, period, 2 period... (audio_song_ms is their clock) */
/* practice: the next audio_play runs the song at this speed (< 1: slower at the same pitch; only that play). The clock
 * stays in song ms, running at audio_speed() of real time; audio_play's lead is in song ms too. */
void audio_set_speed(double speed);
double audio_speed(void);
void audio_render(float *out, int frames);        /* tests: mixes stereo frames without a device */
/* effects */
void audio_sfx(int kind);
void audio_sfx_loop(int kind, int on);            /* a looping effect (the battle heartbeat) on or off */
void audio_set_sfx_gain(double gain);
void audio_lock(void);                            /* the device lock, for sfx.c's voices (no-ops without a device) */
void audio_unlock(void);
/* previews and the menu music: one stream, faded in and out, on the player bus at the band volume (the song clock is
 * not used meanwhile); a looping preview repeats, a menu track stops at its end and audio_preview_on() turns 0 */
void audio_preview(const uint8_t *mp3, size_t size, double levelDb, int looping);
void audio_preview_stop(double fadeSec);
int audio_preview_on(void);
/* movie sound: 44.1 kHz stereo float PCM pushed by the movie player into a ring that the callback mixes at the band
 * volume, apart from the song streams; its clock is what has been heard of it */
void audio_movie_start(void);                          /* empties the ring, not playing yet */
int audio_movie_push(const float *lr, int frames);     /* frames taken (as many as fit) */
int audio_movie_queued(void);                          /* frames waiting in the ring */
void audio_movie_run(int on);                          /* 0: silent and the ring stays put */
double audio_movie_ms(void);                           /* ms of the pushed sound heard so far (-1: no device) */
/* a spoken clip (the tutorial narration) mixed over the song, the previews and the effects, playing on while the song
 * is paused; one at a time: a new clip (or NULL) replaces the old, whose bytes the caller may free once this returns */
void audio_voice(const uint8_t *mp3, size_t size, double levelDb);
int audio_voice_on(void);
#endif
