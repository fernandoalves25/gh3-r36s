/* The game's cutscenes: MPEG-1 video + MP2 sound in an MPEG program stream (DATA/MOVIES/R36S/<name>.mpg, made by
 * tools/convert_movies.sh), decoded with pl_mpeg on a worker thread and shown letterboxed over the whole screen. */
#ifndef MOVIE_H
#define MOVIE_H
int movie_open(const char *path, int W, int H);   /* 0 when it plays; one movie at a time */
void movie_close(void);
int movie_tick(double nowMs);                     /* moves the movie to now: 1 while it plays, 0 at its end */
void movie_draw(int W, int H);                    /* the current picture, black bars around it */
int movie_has_audio(void);
int movie_selftest(const char *path, int W, int H);   /* --movietest: decodes all of it, prints the cost, leaves the middle frame drawn (software) */
#endif
