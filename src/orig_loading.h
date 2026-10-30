/* Between the song's setup and the song: the game's loading screen (loading_flying_static, the picture that carries
 * the word LOADING, with one of the game's loading_screen_tips in its text font, the song and the venue, a spinner).
 * main.c shows it for at least LOADING_MIN_MS and does the blocking load (chart, audio, the 3D venue) only once a frame
 * of it is on screen. And at the song's start, in the game's look, the intro card (the game's intro_song_info_text,
 * intro_artist_info_text and the artist_text line between them, with current_intro's fade and hold times). */
#ifndef ORIG_LOADING_H
#define ORIG_LOADING_H
#include "render.h"
enum { SC_LOADING = 47 };                 /* the loading screen (kept apart from app.h's list, like SC_STORE) */
#define LOADING_MIN_MS 1100
void loading_start(double now);           /* for A.selSong / A.selTier: picks a tip; the caller sets A.screen = SC_LOADING */
int loading_phase(double now);            /* 0: keep showing it; 1: load now (a frame of it has been shown); 2: loaded and shown long enough, start the song */
void loading_loaded(void);                /* the blocking load is done */
void loading_draw(double now);            /* screens_draw's SC_LOADING */
void orig_intro_card(const hud_info *hi, double songT, int W, int H);   /* render_game's HUD in the game's look: the intro card and the section's name */
#endif
