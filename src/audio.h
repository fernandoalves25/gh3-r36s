/* Song playback: up to three MP3 streams (the player's track, the band, a track kept apart)
 * decoded with minimp3 inside the SDL audio callback, and a song clock derived from it.
 * Also a handful of synthesized sound effects (menus, bad notes, star power, fanfares) and song previews. */
#ifndef AUDIO_H
#define AUDIO_H
#include <stdint.h>
#include <stddef.h>
enum { BUS_PLAYER = 0, BUS_BAND = 1, BUS_OTHER = 2, BUS_N = 3 };
enum { SFX_MOVE, SFX_OK, SFX_BACK, SFX_BAD, SFX_BAD_BASS, SFX_READY, SFX_STAR_ON, SFX_STAR_OFF, SFX_STREAK, SFX_DING, SFX_FANFARE, SFX_TICK, SFX_GO, SFX_FAIL, SFX_LOSE, SFX_N };
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
double audio_duration_ms(void);                   /* best estimate from the stream headers */
void audio_fade_out(double sec);
void audio_selftest(void);
void audio_set_bend(double amount);               /* whammy bend of the player track, 0..1 (only while a sustain is held) */
void audio_metronome(double periodMs, double leadMs); /* calibration: clicks at song time 0, period, 2 period... (audio_song_ms is their clock) */
/* effects */
void audio_sfx(int kind);
void audio_set_sfx_gain(double gain);
/* previews: one looping stream, faded in and out, on the player bus (the song clock is not used meanwhile) */
void audio_preview(const uint8_t *mp3, size_t size, double levelDb);
void audio_preview_stop(double fadeSec);
int audio_preview_on(void);
#endif
