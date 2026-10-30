/* The game's narrated tutorials (Training > Tutorials): the lesson list and the lessons themselves. */
#ifndef TUTORIAL_H
#define TUTORIAL_H
#include <SDL.h>
void tutorial_open(double now);                          /* the lesson list (SC_TUTORIAL) */
void tutorial_frame(double now);                         /* runs the lesson (or the list) one frame and draws it */
void tutorial_event(int in, double now);                 /* menu input: the list and the lesson's pause */
int tutorial_raw_event(const SDL_Event *ev, double now); /* frets, strum, star power, pause while a lesson runs (1 = used) */
int tutorial_busy(void);                                 /* a lesson is running (no menu music then) */
int tutorial_headless(const char *id, int frames, int every, void (*save)(const char *name));   /* --tutorial 1b: the bot plays a lesson */
#endif
