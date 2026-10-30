/* The song setup in the game's look: GH3's difficulty poster (1p_difficulty_poster, create_select_difficulty_menu) with
 * the song on top, the four difficulties where the game lists them and the other rows on a taped note. */
#ifndef ORIG_SETUP_H
#define ORIG_SETUP_H
#include "original.h"   /* orig_ready() for the hook */
/* rows: setup_rows() of screens.c (its SR_* values), n of them, A.setupRow the chosen one. 1 when drawn (with the
 * encore over it), 0 when the poster or its fonts are missing (the screen then draws as before) */
int orig_setup_draw(double now, const int *rows, int n);
#endif
