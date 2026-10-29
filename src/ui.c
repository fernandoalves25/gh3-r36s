#include "ui.h"
#include "render.h"
#include <string.h>
#include <stdio.h>

void ui_panel(float x, float y, float w, float h, int style) {
    uint32_t fill = style == UI_PANEL_AMBER ? RGBA(70, 48, 20, 230) : style == UI_PANEL_SP ? RGBA(20, 60, 80, 230) : style == UI_PANEL_FLAT ? RGBA(20, 17, 27, 200) : RGBA(27, 24, 35, 225);
    uint32_t hi = style == UI_PANEL_AMBER ? RGB(233, 176, 76) : style == UI_PANEL_SP ? RGB(98, 228, 255) : RGB(90, 80, 112), lo = RGB(8, 6, 12);
    int p = 2;
    gfx_rect(x + p, y, w - 2 * p, h, fill); gfx_rect(x, y + p, p, h - 2 * p, fill); gfx_rect(x + w - p, y + p, p, h - 2 * p, fill);   /* cut corners */
    if (style == UI_PANEL_FLAT) return;
    gfx_rect(x + p, y, w - 2 * p, p, hi); gfx_rect(x, y + p, p, h - 2 * p, hi);                 /* light top and left */
    gfx_rect(x + p, y + h - p, w - 2 * p, p, lo); gfx_rect(x + w - p, y + p, p, h - 2 * p, lo); /* dark bottom and right */
    gfx_rect(x + p, y + p, p, p, hi); gfx_rect(x + w - 2 * p, y + h - 2 * p, p, p, lo);
}
int ui_hint_w(const char *button, const char *text) { return 18 + gfx_text_w(button, 1) + 6 + gfx_text_w(text, 1); }
void ui_hint(int x, int y, const char *button, const char *text) {
    int bw = gfx_text_w(button, 1) + 12; gfx_rect(x + 1, y - 3, bw - 2, 17, C_INK); gfx_rect(x, y - 2, bw, 15, C_INK); gfx_rect(x + 1, y - 2, bw - 2, 15, C_INK);
    gfx_text(x + 6, y, 1, button, C_STAGE); gfx_text(x + bw + 6, y, 1, text, C_MUTED);
}
void ui_hints(int y, int W, const char *const *buttons, const char *const *texts, int n) {
    int total = 0; for (int i = 0; i < n; i++) total += ui_hint_w(buttons[i], texts[i]) + 16; total -= 16;
    int x = W / 2 - total / 2; if (x < 8) x = 8;
    for (int i = 0; i < n; i++) { ui_hint(x, y, buttons[i], texts[i]); x += ui_hint_w(buttons[i], texts[i]) + 16; }
}
void ui_option(int x, int y, int w, const char *label, const char *value, int selected, int arrows) {
    if (selected) { ui_panel(x - 8, y - 8, w + 16, 34, UI_PANEL_AMBER); gfx_sprite_at(art_get(ART_PICK), x - 2, y - 1, 0.5f, RGB(255, 255, 255), 1); }
    gfx_text_sh(x + 14, y, 2, label, selected ? C_INK : C_MUTED, C_SHADOW);
    if (value) { char b[120]; if (arrows && selected) snprintf(b, sizeof b, "← %s →", value); else snprintf(b, sizeof b, "%s", value); gfx_text_sh_right(x + w - 6, y, 2, b, selected ? C_AMP : C_INK, C_SHADOW); }
}
void ui_bar(float x, float y, float w, float h, float frac, uint32_t color) {
    if (frac < 0) frac = 0; if (frac > 1) frac = 1;
    gfx_rect(x, y, w, h, C_SHADOW); gfx_rect(x + 1, y + 1, w - 2, h - 2, C_PANEL2); gfx_rect(x + 1, y + 1, (w - 2) * frac, h - 2, color); gfx_rect(x + 1, y + 1, (w - 2) * frac, 1, fb_alpha(RGB(255, 255, 255), 0.35f));
}
void ui_header(int W, const char *title, const char *subtitle) {
    gfx_rect(0, 0, W, 56, RGBA(10, 8, 16, 170)); gfx_rect(0, 56, W, 2, C_AMP); gfx_rect(0, 58, W, 1, C_AMP_DARK);
    gfx_text_title(16, 12, 3, title, C_AMP, RGB(70, 30, 10), 3);
    if (subtitle) gfx_text_sh_right(W - 16, 22, 1, subtitle, C_MUTED, C_SHADOW);
}
void ui_dim(int W, int H, int alpha) { gfx_rect(0, 0, W, H, RGBA(10, 8, 16, alpha)); }
