/* The set list as GH3 draws it (menu_setlist.qb), with the game's own pages, pictures and handwriting: see orig_setlist.c. */
#ifndef ORIG_SETLIST_H
#define ORIG_SETLIST_H
#include "original.h"
int orig_setlist_draw(double now);   /* 1 when drawn; 0 when the art is missing (the screen draws its own list then) */
#endif
