/* Menu building blocks in the same pixel style: bevelled panels, button hints, option rows, bars. */
#ifndef UI_H
#define UI_H
#include "gfx.h"
enum { UI_PANEL_DARK = 0, UI_PANEL_AMBER = 1, UI_PANEL_SP = 2, UI_PANEL_FLAT = 3 };
void ui_panel(float x, float y, float w, float h, int style);
void ui_hint(int x, int y, const char *button, const char *text);          /* "(A) Escolher": a round button badge and a label; returns nothing, 1x font */
int ui_hint_w(const char *button, const char *text);
void ui_hints(int y, int W, const char *const *buttons, const char *const *texts, int n); /* a row of hints centred in W */
void ui_option(int x, int y, int w, const char *label, const char *value, int selected, int arrows); /* label left, "< value >" right */
void ui_bar(float x, float y, float w, float h, float frac, uint32_t color);
void ui_header(int W, const char *title, const char *subtitle);              /* the pixel title band at the top of a menu */
void ui_title(int x, int y, int scale, const char *s, uint32_t c, uint32_t edge, int depth);   /* gfx_text_title, or the game's own title font in the Original theme */
void ui_dim(int W, int H, int alpha);
#endif
