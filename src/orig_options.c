/* The Options screens with the game's own posters (see orig_options.h). Places come from the browser port's rebuild of
 * the game's scripts (a 1280x720 stage), brought to 640x480 at 2/3 and moved where the small screen needs the room for
 * the help text and the button pills; the posters are drawn straight (the game tilts them a degree or two). */
#include "orig_options.h"
#include "original.h"
#include "gamearts.h"
#include "app.h"
#include "settings.h"
#include "render.h"
#include "input.h"
#include "audio.h"
#include "art.h"
#include "ui.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *F_TITLE = "fontgrid_title_a1", *F_HEAD = "fontgrid_text_a6", *F_TEXT = "fontgrid_text_a4", *F_NOTE = "fontgrid_text_a5", *F_SMALL = "fontgrid_text_a11";
#define WHITE RGB(255, 255, 255)
static void img(const char *name, float x, float y, float w, float h, uint32_t tint, float a) { gfx_sprite *s = ga_img(name); if (s) gfx_blit(s, 0, 0, gfx_sprite_w(s), gfx_sprite_h(s), x, y, w, h, tint, a); }
static void tape(const char *name, float x, float y, float w, float h) { img(name, x + 3, y + 3, w, h, RGB(0, 0, 0), 0.5f); img(name, x, y, w, h, WHITE, 1); }   /* a strip of tape and its shadow */
static float wrap(const ga_font *f, float x, float y, float w, float size, float lh, const char *s, uint32_t c) {   /* word-wrapped in the game's font; the height used */
    char line[300] = "", word[120], t[300]; float y0 = y;
    while (*s) { int k = 0; while (*s == ' ') s++; while (*s && *s != ' ' && k < 119) word[k++] = *s++; word[k] = 0; if (!k) break;
        snprintf(t, sizeof t, "%s%s%s", line, line[0] ? " " : "", word);
        if (line[0] && ga_text_w(f, t, size) > w) { ga_text(f, x, y, size, line, c); y += lh; snprintf(line, sizeof line, "%s", word); } else snprintf(line, sizeof line, "%s", t); }
    if (line[0]) { ga_text(f, x, y, size, line, c); y += lh; } return y - y0;
}
static int tick_of(const char *v) {   /* an on/off value: 1 on, 0 off, -1 something else */
    static const char *ON[4] = { "Sim", "Ligado", "Ligada", "Ligados" }, *OFF[4] = { "Não", "Desligado", "Desligada", "Desligados" };
    for (int i = 0; i < 4; i++) { if (!strcmp(v, ON[i])) return 1; if (!strcmp(v, OFF[i])) return 0; } return -1;
}
static void back(double now) {   /* what is behind: the paused song, dimmed, when the options were opened from the pause; the venue wall otherwise */
    if (A.screen == SC_OPTIONS && A.optionsFrom == SC_PAUSE) { hud_info hi = { A.S.song->title, A.S.song->artist, A.S.section, 0, SET.lefty, SET.stageFx, SET.bgDim, 0, SET.noFail, NULL }; render_game(&A.S.run, now, &hi); ui_dim(A.W, A.H, 200); }
    else render_backdrop(now, SET.stageFx);
}
static void help_panel(float x, float y, float w, float h, const char *s) {   /* h < 0: as tall as the text, its bottom at y */
    if (h < 0) { h = gfx_text_box((int)x + 10, 0, (int)w - 20, 1, s, RGBA(0, 0, 0, 0)) + 14; y -= h; } ui_panel(x, y, w, h, UI_PANEL_FLAT); gfx_text_box((int)x + 10, (int)y + 7, (int)w - 20, 1, s, C_INK); }

/* ================================================================== the options menu (create_options_menu) */
/* the poster's twelve paper strips (rows of the 512x512 picture, x 62..208); the categories take the first ones */
static const short STRIP[12][2] = { { 36, 65 }, { 77, 96 }, { 109, 137 }, { 151, 170 }, { 183, 202 }, { 214, 244 }, { 254, 282 }, { 292, 319 }, { 334, 352 }, { 366, 396 }, { 408, 426 }, { 438, 457 } };
static int menu_draw(double now, const char *const *catLabel, const char *const *catHelp, int ncat, int (*off)(int)) {
    const ga_font *note = ga_font_get(F_NOTE), *big = ga_font_get(F_SMALL); if (!ga_img("options_main_poster") || !note) return 0;
    int W = A.W, H = A.H; const float PX = 120, PY = 2, S = 400.0f / 512; float t = (float)now / 1000;
    img("options_main_poster", PX, PY, 512 * S, 512 * S, WHITE, 1);
    for (int i = 0; i < ncat && i < 12; i++) { float sx = PX + 62 * S, sw = 147 * S, sy = PY + STRIP[i][0] * S, sh = (STRIP[i][1] - STRIP[i][0] + 1) * S, cy = sy + sh / 2; int on = i == A.optRow, closed = off && off(i);
        if (on) img("options_main_highlight", sx - 4, sy - 3, sw + 8, sh + 6, RGB(50, 0, 0), 1);   /* the dark bar over the chosen strip */
        float size = fminf(sh * 0.95f, 19); ga_text_fit(note, sx + sw / 2, cy - size * 0.52f, size, sw - 10, catLabel[i], closed ? RGB(128, 128, 128) : on ? RGB(220, 220, 220) : RGB(0, 0, 0), 1); }
    { const ga_font *tf = big ? big : note; ga_text_fit(tf, PX + 331 * S - 3, PY + 40 * S + 3, 40, 160, "OPÇÕES", RGB(160, 130, 105), 1); ga_text_fit(tf, PX + 331 * S, PY + 40 * S, 40, 160, "OPÇÕES", RGB(50, 0, 0), 1); }
    { float u = t - floorf(t), dy = u < 0.5f ? 20 - 40 * (1 - (1 - u / 0.5f) * (1 - u / 0.5f)) : -20 + 40 * ((u - 0.5f) / 0.5f) * ((u - 0.5f) / 0.5f);   /* anim_float: up and down, again and again */
      img("options_main_monster", PX + 337 * S - 85, PY + 239 * S - 170 + dy * 0.65f, 170, 340, WHITE, 1); }
    tape("tape_h_04", PX - 22, PY - 4, 86, 43); tape("tape_h_03", PX + 40, PY + 372, 86, 43); tape("tape_v_02", PX + 392, PY + 210, 40, 80);
    help_panel(14, H - 74, W - 28, 44, off && off(A.optRow) ? "Não dá para abrir durante a música." : catHelp[A.optRow]);
    static const char *bt[2] = { "A", "B" }, *tx[2] = { "Abrir", "Voltar" }; ui_hints(H - 24, W, bt, tx, 2);
    return 1;
}

/* ================================================================== AUDIO (create_audio_settings_menu) */
static gfx_sprite *knobLine[11];
static gfx_sprite *knob_line(int step) {   /* the knob's line turned step x 18 degrees clockwise from pointing left, about its right end, which sits in the middle of a square (drawn over the knob, as big as it) */
    if (step < 0) step = 0; if (step > 10) step = 10; if (knobLine[step]) return knobLine[step];
    fb_t s, o; if (ga_img_fb("options_audio_knob_line", &s)) return NULL; int C = 4 * s.w; fb_init(&o, C, C); float a = step * 18 * (float)M_PI / 180, ca = cosf(a), sa = sinf(a);
    for (int y = 0; y < C; y++) for (int x = 0; x < C; x++) { float dx = (x + 0.5f - C / 2.0f) / 2, dy = (y + 0.5f - C / 2.0f) / 2, ux = dx * ca + dy * sa, uy = -dx * sa + dy * ca; int sx = (int)floorf(s.w + ux), sy = (int)floorf(s.h / 2.0f + uy);
        o.px[(size_t)y * C + x] = sx >= 0 && sy >= 0 && sx < s.w && sy < s.h ? s.px[(size_t)sy * s.w + sx] : 0; }
    fb_free(&s); knobLine[step] = gfx_bake(&o, 1); fb_free(&o); return knobLine[step];
}
static void short_label(const char *l, char *out, size_t n) {   /* "Volume da banda" -> "BANDA", the game's knob names; others as they are */
    static const char *PRE[3] = { "Volume da ", "Volume do ", "Volume dos " }; for (int i = 2; i >= 0; i--) if (!strncmp(l, PRE[i], strlen(PRE[i]))) { l += strlen(PRE[i]); snprintf(out, n, "%s", l); for (char *c = out; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32; return; }
    snprintf(out, n, "%s", l);
}
static int ptrSel = -1, ptrFrom; static double ptrAt, ptrSeen;
static int audio_draw(double now, int cat, const int *items, int n, const char *const *optLabel, const char *const *optHelp, void (*value)(int, char *, size_t)) {
    const ga_font *title = ga_font_get(F_TITLE), *note = ga_font_get(F_NOTE); if (!ga_img("options_audio_poster") || !title || !note) return 0;
    int W = A.W, H = A.H; const float PX = 176, S = 456.0f / 512, CX = 262, ROW = 56, TOP = 18; const uint32_t GOLD = RGB(230, 190, 70), GREY = RGB(150, 150, 150); char v[48], l[64]; (void)cat;
    img("options_audio_poster", PX, 0, 512 * S, 512 * S, WHITE, 1);
    tape("tape_h_01", PX + 4, -14, 96, 48); tape("tape_v_01", W - 54, H - 150, 48, 96);
    float rowc[8];
    for (int k = 0; k < n && k < 8; k++) { int i = items[k], on = k == A.optRow; float y = TOP + k * ROW; uint32_t c = on ? GOLD : GREY; value(i, v, sizeof v); short_label(optLabel[i], l, sizeof l);
        ga_text_fit(title, CX + 2, y, 15, 94, l, c, 0);
        int len = (int)strlen(v), knob = len > 1 && v[len - 1] == '%';
        if (knob) { int pct = 0; sscanf(v, "%d", &pct); float kx = CX + 2, ky = y + 17, ks = 36; rowc[k] = ky + ks / 2;   /* the knob, its line turned with the volume (0 at the left, full at the right) */
            img("options_audio_knob", kx, ky, ks, ks, GREY, 1); gfx_sprite *ln = knob_line((pct + 5) / 10); if (ln) gfx_blit(ln, 0, 0, gfx_sprite_w(ln), gfx_sprite_h(ln), kx, ky, ks, ks, c, 1);
            ga_text_sh(note, kx + ks + 6, ky + 9, 17, v, c, RGB(0, 0, 0)); continue; }
        rowc[k] = y + 27; int tk = tick_of(v);
        if (tk >= 0) { img(tk ? "data_settings_checkmark" : "data_settings_xmark", CX, y + 15, 26, 26, WHITE, on ? 1 : 0.75f); ga_text_sh(note, CX + 30, y + 20, 15, v, c, RGB(0, 0, 0)); }
        else ga_text_sh(note, CX + 2, y + 19, 17, v, c, RGB(0, 0, 0)); }
    img("options_audio_bellystrings", PX + 308 * S, 81 * S, 56 * S, 242 * S, WHITE, 1);
    { int sel = A.optRow < n ? A.optRow : 0; if (ptrSel < 0 || now - ptrSeen > 250) { ptrSel = ptrFrom = sel; ptrAt = -1e9; } ptrSeen = now; if (sel != ptrSel) { ptrFrom = ptrSel; ptrSel = sel; ptrAt = now; }   /* aom_move_pointer: the hand goes to the chosen row, dipping to half its size on the way */
      float k = (float)fmin(1, (now - ptrAt) / 200), ax = CX - 4, ay = rowc[ptrFrom < n ? ptrFrom : 0], bx = ax, by = rowc[sel], mx = bx - 40, my = by + (sel < ptrFrom ? -40 : 40), px = bx, py = by, sc = 1;
      if (k < 0.5f) { float q = k / 0.5f; px = ax + (mx - ax) * q; py = ay + (my - ay) * q; sc = 1 - 0.5f * q; } else if (k < 1) { float q = (k - 0.5f) / 0.5f; px = mx + (bx - mx) * q; py = my + (by - my) * q; sc = 0.5f + 0.5f * q; }
      float pw = 112 * sc, ph = 56 * sc; img("options_audio_pointer_v2", px - pw * 0.96f, py - ph * 0.31f, pw, ph, WHITE, 1); }   /* the finger's tip at (0.96, 0.31) of the picture */
    ga_text_fit(title, 12 + 1, 16 + 1, 30, 150, "ÁUDIO", RGB(0, 0, 0), 0); ga_text_fit(title, 12, 16, 30, 150, "ÁUDIO", RGB(245, 226, 190), 0); gfx_text_sh(14, 54, 1, "salvas ao sair", C_MUTED, C_SHADOW);
    help_panel(6, 440, 146, -1, optHelp[items[A.optRow]]);
    static const char *bt[3] = { "← →", "A", "B" }, *tx[3] = { "Mudar", "Abrir", "Voltar" }; ui_hints(H - 24, W, bt, tx, 3);
    return 1;
}

/* ================================================================== CONTROLLER (create_controller_settings_menu) */
static int ctrl_draw(const int *items, int n, const char *const *optLabel, const char *const *optHelp, void (*value)(int, char *, size_t)) {
    const ga_font *head = ga_font_get(F_HEAD); if (!ga_img("options_controller_poster") || !ga_img("options_controller_poster2") || !head) return 0;
    int W = A.W, H = A.H; const uint32_t FOC = RGB(190, 185, 165), UNF = RGB(60, 45, 30); char v[48], b[120];
    #define CX_(x) (((x) - 607) * 2.0f / 3 + 320)
    #define CY_(y) ((y) * 2.0f / 3)
    const float LX = CX_(638), RY = CY_(173), RP = 34 * 2.0f / 3, SZ = 21;
    img("options_controller_poster2", CX_(525), CY_(130), 368, 177, WHITE, 1);   /* the paper the words are on */
    tape("tape_h_04", CX_(760), CY_(106), 93, 43);
    for (int k = 0; k < n && k < 2; k++) { value(items[k], v, sizeof v); if (tick_of(v) >= 0) img("options_controller_checkbg", CX_(945) + 3 * k, CY_(190) + 30 * k - 2, 60, 30, WHITE, 1); }
    img("options_controller_highlight", CX_(620), RY + A.optRow * RP + SZ * 0.45f - 19, 307, 38, WHITE, 1);
    img("options_controller_poster", CX_(135), CY_(30), 427, 413, WHITE, 1);   /* the castle, over the paper's left side */
    for (int k = 0; k < n && k < 6; k++) { int i = items[k], on = k == A.optRow; float y = RY + k * RP; value(i, v, sizeof v); int tk = tick_of(v);
        if (tk >= 0 || !v[0]) snprintf(b, sizeof b, "%s%s", optLabel[i], tk >= 0 ? ":" : ""); else snprintf(b, sizeof b, "%s: %s", optLabel[i], v);
        ga_text_fit(head, LX, y, SZ, tk >= 0 ? 210 : 280, b, on ? FOC : UNF, 0);
        if (tk >= 0) img(tk ? "options_controller_check" : "options_controller_x", CX_(975) - 15 - 3 * k, y - 7, 30, 30, WHITE, 1); }
    ga_text_fit(head, CX_(135) + 213, CY_(30) + 352, 19, 100, "Controle", UNF, 1); ga_text_fit(head, CX_(135) + 213, CY_(30) + 174, 14, 80, "Opções", UNF, 1);
    gfx_text_box((int)LX - 6, (int)(RY + n * RP + 10), (int)(CX_(1080) - LX), 1, optHelp[items[A.optRow]], UNF);   /* the help on the paper, under the words */
    tape("tape_v_01", CX_(250), CY_(360), 53, 95);
    static const char *bt[3] = { "← →", "A", "B" }, *tx[3] = { "Mudar", "Abrir", "Voltar" }; ui_hints(H - 24, W, bt, tx, 3);
    #undef CX_
    #undef CY_
    return 1;
}

/* ================================================================== GRAPHICS (winport_create_gfx_settings_menu): the blue poster, a list on it */
static int gfx_draw(int cat, const char *const *catLabel, const int *items, int n, const char *const *optLabel, const char *const *optHelp, void (*value)(int, char *, size_t)) {
    const ga_font *text = ga_font_get(F_TEXT), *note = ga_font_get(F_NOTE); if (!ga_img("graphics_options_poster_part1") || !text || !note) return 0;
    int W = A.W, H = A.H; const float X0 = 120, X1 = 520, TOP = 152, ROW = 27, SZ = 19; char v[48], b[64];
    static const char *PART[3] = { "graphics_options_poster_part1", "graphics_options_poster_part2", "graphics_options_poster_part3" };
    for (int p = 0; p < 3; p++) img(PART[p], 47, -50, 546, 580, WHITE, 1);
    tape("tape_v_01", W - 58, 196, 44, 88); tape("tape_h_02", 18, 316, 85, 43);
    { float y = TOP + A.optRow * ROW; img("graphics_options_highlight", X0 - 17, y + SZ * 0.5f - 20, 433, 40, WHITE, 1); }
    { char t[48]; snprintf(t, sizeof t, "%s", catLabel[cat]); ga_text_fit(note, 451, 96, 46, 170, t, RGB(50, 0, 0), 1); }   /* the title, over the vein, under the words */
    for (int k = 0; k < n; k++) { int i = items[k], on = k == A.optRow; float y = TOP + k * ROW; value(i, v, sizeof v);
        ga_text(text, X0 - 1, y + 1, SZ, optLabel[i], RGB(0, 0, 0)); ga_text(text, X0, y, SZ, optLabel[i], WHITE);
        if (v[0]) { snprintf(b, sizeof b, "%s", v); ga_text_right(text, X1 - 1, y + 1, SZ, b, RGB(0, 0, 0)); ga_text_right(text, X1, y, SZ, b, on ? RGB(230, 171, 60) : WHITE); } }
    { float y = TOP + n * ROW + 12; gfx_text_box(102, (int)y + 1, 290, 1, optHelp[items[A.optRow]], C_SHADOW); y += gfx_text_box(101, (int)y, 290, 1, optHelp[items[A.optRow]], C_INK) + 6; gfx_text_sh(101, (int)y, 1, "salvas ao sair", C_MUTED, C_SHADOW); }
    static const char *bt[3] = { "← →", "A", "B" }, *tx[3] = { "Mudar", "Abrir", "Voltar" }; ui_hints(H - 24, W, bt, tx, 3);
    return 1;
}

int orig_options_draw(double now, const char *const *catLabel, const char *const *catHelp, const int (*catItems)[7], int ncat,
    const char *const *optLabel, const char *const *optHelp, void (*value)(int, char *, size_t), int (*off)(int)) {
    if (A.optCat < 0) { if (!ga_img("options_main_poster")) return 0; back(now); return menu_draw(now, catLabel, catHelp, ncat, off); }
    int cat = A.optCat, n = 0; const int *items = catItems[cat]; while (n < 7 && items[n] >= 0) n++; if (!n) return 0;
    if (A.optRow >= n) A.optRow = n - 1;   /* a row past the category (one that lost an item), as draw_options does */
    const char *need = cat == 0 ? "options_audio_poster" : cat == 2 ? "options_controller_poster" : "graphics_options_poster_part1"; if (!ga_img(need)) return 0;
    back(now);
    if (cat == 0) return audio_draw(now, cat, items, n, optLabel, optHelp, value);
    if (cat == 2) return ctrl_draw(items, n, optLabel, optHelp, value);
    return gfx_draw(cat, catLabel, items, n, optLabel, optHelp, value);
}

/* ================================================================== CALIBRATE LAG (winport_create_calibrate_lag_menu): the explanation on the game's paper, beside the lane */
int orig_calib_draw(double now) {
    const ga_font *title = ga_font_get(F_TITLE), *text = ga_font_get(F_TEXT); if (!ga_img("options_calibrate_paper") || !text || !title) return 0;
    int W = A.W, H = A.H; char b[120]; double raw = audio_song_ms(); int med = A.calN >= 4 ? app_calib_result() : SET.offsetMs; const uint32_t INK = RGB(60, 40, 115), OFF = RGB(40, 100, 165), ON = RGB(230, 230, 230);
    render_calib(raw - med, 500, now, A.calN, A.calLastDiff, A.calLastTapAt);
    img("options_calibrate_paper", -5, -14, 240, 296, WHITE, 1);   /* the sheet hangs from the top left, its torn edge at the bottom */
    tape("tape_v_02", 196, 196, 28, 56);
    const float X = 12, WD = 168;
    ga_text_fit(title, X, 8, 21, WD, "CALIBRAR ATRASO", INK, 0);
    float y = 36 + wrap(text, X, 36, WD, 14, 15, "Palhete (direcional para cima ou para baixo) junto com cada clique, de preferência ouvindo e sem olhar a pista. Depois de uns 10 toques o atraso medido fica estável.", INK);
    snprintf(b, sizeof b, "Atual: %+d ms", SET.offsetMs); ga_text(text, X, y + 6, 16, b, INK);
    img("options_calibrate_highlight", X - 6, y + 28, WD + 10, 30, OFF, 1);
    if (A.calN >= 4) snprintf(b, sizeof b, "Medido: %+d ms", med); else snprintf(b, sizeof b, "Medido: —"); ga_text_fit(text, X + 2, y + 34, 18, WD - 8, b, ON, 0);
    snprintf(b, sizeof b, "%d %s", A.calN, A.calN == 1 ? "toque" : "toques"); ga_text(text, X, y + 64, 16, b, INK);
    if (A.calN > 0 && now - A.calLastTapAt < 600) { snprintf(b, sizeof b, "%+d ms", (int)A.calLastDiff); gfx_text_sh_center(W / 2, H - 70, 2, b, fabs(A.calLastDiff) < 35 ? C_GOOD : C_AMP, C_SHADOW); }
    static const char *bt[3] = { "↑↓", "A", "B" }, *tx[3] = { "Palhetar no clique", "Salvar", "Cancelar" }; ui_hints(H - 24, W, bt, tx, 3);
    return 1;
}

/* ================================================================== CONTROLS (winport_create_bind_buttons): the bindings on the car poster */
int orig_controls_draw(double now) {
    const ga_font *note = ga_font_get(F_NOTE); if (!ga_img("controller_config_poster") || !note) return 0;
    int W = A.W, H = A.H, rows = ACT_N + 2; char b[80]; const uint32_t LIME = RGB(202, 243, 113), HI = RGB(64, 184, 246), DIM = RGB(120, 150, 70), SH = RGB(0, 0, 0);
    render_backdrop(now, SET.stageFx);
    img("controller_config_poster", 64, -50, 512, 580, WHITE, 1);
    tape("tape_v_01", 92, -10, 48, 96); tape("tape_h_02", 500, 400, 88, 43);
    ga_text_fit(note, 447 - 1, 44 + 2, 38, 200, "CONTROLES", SH, 1); ga_text_fit(note, 447, 44, 38, 200, "CONTROLES", LIME, 1);
    const float LX = 94, SX = 222, SW = 112, TOP = 106, ROW = 21, SZ = 17;
    gfx_text_sh((int)SX, (int)TOP - 16, 1, "1º", LIME, C_SHADOW); gfx_text_sh((int)(SX + SW), (int)TOP - 16, 1, "2º", LIME, C_SHADOW); gfx_text_sh((int)(SX + 2 * SW), (int)TOP - 16, 1, "3º", LIME, C_SHADOW); gfx_text_sh_right(548, (int)TOP - 16, 1, "← → coluna", LIME, C_SHADOW);
    { float y = TOP + A.ctlRow * ROW; img("controller_config_highlight", 76, y + SZ * 0.5f - 14, 482, 28, WHITE, 1); }
    for (int r = 0; r < rows; r++) { float y = TOP + r * ROW; int on = r == A.ctlRow; uint32_t c = on ? WHITE : LIME;
        if (r == 0) { gfx_text_sh((int)LX, (int)y + 3, 1, "Perfil", c, C_SHADOW); gfx_text_sh_right(548, (int)y + 3, 1, input_preset_name(A.preset), on ? WHITE : HI, C_SHADOW); continue; }
        if (r == rows - 1) { gfx_text_sh_center(318, (int)y + 3, 1, "Restaurar o perfil escolhido", c, C_SHADOW); continue; }
        int a = r - 1; if (a < 5) orig_gem(LX - 12, y + SZ * 0.55f, 1, 45, a, 0, 0, 0, 1, now);
        gfx_text_sh((int)LX, (int)y + 3, 1, input_action_name(a), c, C_SHADOW);
        for (int s = 0; s < BIND_SLOTS; s++) { float x = SX + s * SW; int hot = on && A.ctlSlot == s, bound = SET.binds[a][s].type != 0; input_bind_name(&SET.binds[a][s], b, sizeof b);
            if (hot) gfx_rect(x - 5, y - 2, SW - 6, ROW - 1, RGBA(0, 0, 0, 120));
            gfx_text_sh((int)x, (int)y + 3, 1, b, hot ? HI : on ? WHITE : bound ? LIME : DIM, C_SHADOW); } }
    /* the live test: the five fret buttons, the strum, star power and the whammy */
    float ty = TOP + rows * ROW + 8; gfx_rect(84, ty - 4, 468, 70, RGBA(0, 0, 0, 110)); gfx_text_sh(94, (int)ty, 1, "Teste: aperte os botões", LIME, C_SHADOW);
    for (int f = 0; f < 5; f++) orig_target(108 + f * 40, ty + 40, 92, f, A.testHeld & (1 << f), 0);
    float sa = (float)(now - A.testStrumAt); int strumOn = sa < 150; gfx_text_sh(300, (int)ty + 24, 2, A.testStrum < 0 ? "↑" : "↓", strumOn ? WHITE : DIM, C_SHADOW); gfx_text_sh(318, (int)ty + 28, 1, "palhetada", strumOn ? WHITE : DIM, C_SHADOW);
    gfx_sprite_at(art_get(ART_BOLT), 382, ty + 22, 0.7f, A.testStar ? C_SP : DIM, 1); gfx_text_sh(398, (int)ty + 28, 1, "star power", A.testStar ? C_SP : DIM, C_SHADOW);
    gfx_text_sh(472, (int)ty + 18, 1, "alavanca", LIME, C_SHADOW); ui_bar(472, ty + 32, 72, 8, A.testWhammy, LIME);
    if (A.capturing) { ui_dim(W, H, 170); ui_panel(W / 2 - 200, H / 2 - 50, 400, 100, UI_PANEL_SP); snprintf(b, sizeof b, "%s", input_action_name(A.ctlRow - 1)); gfx_text_sh_center(W / 2, H / 2 - 36, 2, b, C_INK, C_SHADOW);
        gfx_text_center(W / 2, H / 2 - 6, 1, "Aperte um botão ou mova um analógico", C_SP); char cb[60]; snprintf(cb, sizeof cb, "(sem apertar nada, cancela em %d s)", (int)ceil((A.capAt + 5000 - now) / 1000)); gfx_text_center(W / 2, H / 2 + 14, 1, cb, C_MUTED);
        if (now - A.capAt > 5000) A.capturing = 0; }   /* the capture gives up, as draw_controls does */
    static const char *bt[3] = { "A", "X", "B" }, *tx[3] = { "Trocar", "Apagar", "Voltar" }; ui_hints(H - 22, W, bt, tx, 3);
    return 1;
}
