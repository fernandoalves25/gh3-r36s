/* The game's own sound effects (ZONES/global_sfx.pak.xen played by qb.pak's sound-event table) and the crowd.
 * audio_sfx(kind) goes through sfx_play first; the synthesized voices of audio.c are the fallback without the bank. */
#ifndef SFX_H
#define SFX_H
#include <stdint.h>
#include <stddef.h>
#include "game.h"
/* qb.pak + qb.pab (the table global_sound_events is read now, the bytes are not kept) and the sfx pak, which is kept
 * (sfx owns `bank`). Returns 0 when both the table and the bank were read; the mixer stays silent otherwise. */
int sfx_init(const uint8_t *qbpak, size_t pl, const uint8_t *qbpab, size_t bl, uint8_t *bank, size_t bankLen);
int sfx_ready(void);
int sfx_play(int kind);                               /* the game's sound for an SFX_* kind; 0 when there is none */
void sfx_prepare(int kind);                           /* decodes a kind's files now (for a sfx_play from the audio callback) */
void sfx_loop(int kind, int on);                      /* a looping kind (the battle heartbeat) on or off */
void sfx_mix(float *l, float *r, float gain, int songOn);   /* one frame, from the audio callback; beds hold while !songOn */
/* the crowd of a song: `zone` is the tier's venue ("z_dive", "" for none), the beds start at the lead-in and the
 * Song_Intro_Kick comes when the music does (leadMs later); tick once a frame with the run; end with the result */
void sfx_song_prepare(const char *zone);            /* the loading screen: decodes the venue's crowd and every sound a song can play, so none is decoded mid-song */
void sfx_song_begin(const char *zone, double leadMs);
void sfx_song_tick(const gh3_run *r, double songMs);
void sfx_song_end(int failed);
void sfx_song_stop(void);                             /* everything of the song off at once (audio_clear) */
/* sound events by their container key (practice.c: the drum kit of practice_notemapping) */
int sfx_event_find(uint32_t containerKey);            /* its index in the table, -1 when it has none (or no bank) */
int sfx_event_prepare(int idx);                       /* decodes its files now; 0 when none decodes */
void sfx_event_fire(int idx, float level);            /* one of its files, now: from the audio callback (or under audio_lock) */
void sfx_event_show(int idx);                         /* tests: its files (name, rate, length, peak, rms, vol, pitch) */
int sfx_selftest(void);                               /* --sfxtest: lists and decodes every mapped sound; failures */
#endif
