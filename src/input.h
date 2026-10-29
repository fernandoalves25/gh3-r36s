/* Controller bindings: actions -> buttons or axis directions (two slots each), presets, names in Portuguese,
 * the event translation used while playing and the capture used by the controls screen. The keyboard keeps
 * fixed keys (1-5 / A S D F G frets, arrows strum, space star power, W whammy, Esc pause). */
#ifndef INPUT_H
#define INPUT_H
#include <SDL.h>
#include "settings.h"
#define PRESET_N 3
const char *input_preset_name(int preset);
void input_apply_preset(int preset);
const char *input_action_name(int action);
void input_bind_name(const bind_t *b, char *out, size_t n);
typedef struct { int action, pressed; float value; } inev_t;
int input_translate(const SDL_Event *ev, inev_t *out, int max);  /* play-time events -> actions (edge-triggered for axes) */
int input_capture(const SDL_Event *ev, bind_t *out);             /* 1 when a button press or an axis push was seen */
void input_reset_axes(void);
#endif
