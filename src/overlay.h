/* The game's see-through overlay movies (Player1_wins, Player2_wins, Satan-Battle_WIN/LOSS, Golden_Guitar,
 * Fret_Flames) drawn over the game: DATA/MOVIES/R36S/<name>.alpha.mpg, made by tools/convert_overlays.py from the
 * browser port's animated WebP. One MPEG-1 picture holds the colour (premultiplied) above its alpha (as luma); the two
 * halves are composited over what is already drawn, covering the screen like the game's 1280x720 stage. Silent, at
 * the movie's own rate from the moment it is opened, then gone. */
#ifndef OVERLAY_H
#define OVERLAY_H
int overlay_play(const char *name, double now);   /* 0 when <name>.alpha.mpg is there and starts at now */
void overlay_close(void);
int overlay_draw(double now, int W, int H);       /* the frame due at now, over the picture: 1 while it plays */
void overlay_sync(int on);                        /* headless: frames decoded on the caller's thread, exactly the one due */
#endif
