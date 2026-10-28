/* Song playback: up to three MP3 streams (the player's track, the band, a track kept apart)
 * decoded with minimp3 inside the SDL audio callback, and a song clock derived from it. */
#ifndef AUDIO_H
#define AUDIO_H
#include <stdint.h>
#include <stddef.h>
enum { BUS_PLAYER = 0, BUS_BAND = 1, BUS_OTHER = 2, BUS_N = 3 };
int audio_open(void);
void audio_close(void);
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
#endif
