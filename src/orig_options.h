/* The Options screens in the game's own look (the "Original" theme): the options poster with its strips and the floating
 * monster, the Audio poster with its knobs and the pointing hand, the Controller poster on the paper, the Graphics poster,
 * the calibration paper and the bindings on the car poster (menu_options.qb, menu_audio_settings.qb,
 * menu_controller_settings.qb, winport_menu_graphics.qb, winport_menu_song_skew.qb, winport_menu_bind_buttons.qb, as the
 * browser port rebuilt them), laid out for 640x480. screens.c keeps the input and the state; these only draw.
 * Each returns 1 when it drew the screen, 0 when the art is missing (the caller then draws its own). */
#ifndef ORIG_OPTIONS_H
#define ORIG_OPTIONS_H
#include <stddef.h>
/* the options menu (A.optCat < 0) and a category's page. The tables are screens.c's: category names and help, each
 * category's items (-1 ends them; categories 0 Audio, 2 Controller, 3 Video have pages), item names and help, an item's
 * value as shown ("70%", "Ligada", "Não", "+0 ms"; "" for an action) and whether a category is closed (opened from the pause) */
int orig_options_draw(double now, const char *const *catLabel, const char *const *catHelp, const int (*catItems)[7], int ncat,
    const char *const *optLabel, const char *const *optHelp, void (*value)(int item, char *out, size_t n), int (*off)(int cat));
int orig_calib_draw(double now);       /* Calibrar atraso: the lane with the clicks, the explanation on the game's paper */
int orig_controls_draw(double now);    /* the bindings on the controller poster, the live test under them */
#endif
