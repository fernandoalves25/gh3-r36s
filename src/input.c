#include "input.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static const char *ACTNAME[ACT_N] = { "Traste verde", "Traste vermelho", "Traste amarelo", "Traste azul", "Traste laranja", "Palhetada para cima", "Palhetada para baixo", "Star Power", "Alavanca (whammy)", "Pausa" };
const char *input_action_name(int a) { return a >= 0 && a < ACT_N ? ACTNAME[a] : "?"; }
/* The R36S buttons are named by their labels (A right, B bottom, X top, Y left), as PortMaster maps them.
 * Profile 0 is the PS2 game's DualShock layout moved to those positions: green R2, red Circle (right = A),
 * yellow Triangle (top = X), blue Cross (bottom = B), orange Square (left = Y); strum on the D-pad, star power
 * on Select and on L2 (the PS2 "tilt" button), whammy on the analog sticks. */
static const char *PRESET[PRESET_N] = { "PS2 (R2 A X B Y)", "Em linha (X Y B A R1)", "Invertido (R1 A B Y X)", "Gatilhos (L1 X Y A R1)" };
const char *input_preset_name(int p) { return p >= 0 && p < PRESET_N ? PRESET[p] : "?"; }

static bind_t B(int id) { bind_t b = { BIND_BUTTON, id, 0 }; return b; }
static bind_t AX(int id, int dir) { bind_t b = { BIND_AXIS, id, dir }; return b; }
static bind_t ST(int id) { bind_t b = { BIND_STICK, id, 0 }; return b; }
void input_apply_preset(int p) {
    enum { R2 = 100 };   /* a fret on the right trigger axis */
    static const int frets[PRESET_N][5] = {
        { R2, SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_Y },
        { SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER },
        { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_X },
        { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER } };
    if (p < 0 || p >= PRESET_N) p = 0;
    memset(SET.binds, 0, sizeof SET.binds);
    for (int f = 0; f < 5; f++) SET.binds[ACT_FRET0 + f][0] = frets[p][f] == R2 ? AX(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 1) : B(frets[p][f]);
    if (p == 0) SET.binds[ACT_FRET0][1] = B(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);   /* R1 next to R2 also plays green */
    SET.binds[ACT_STRUM_UP][0] = B(SDL_CONTROLLER_BUTTON_DPAD_UP);
    SET.binds[ACT_STRUM_DOWN][0] = B(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    SET.binds[ACT_STAR][0] = B(SDL_CONTROLLER_BUTTON_BACK);
    if (p == 3) SET.binds[ACT_STAR][1] = AX(SDL_CONTROLLER_AXIS_TRIGGERLEFT, 1);
    else { SET.binds[ACT_STAR][1] = AX(SDL_CONTROLLER_AXIS_TRIGGERLEFT, 1); SET.binds[ACT_STAR][2] = B(SDL_CONTROLLER_BUTTON_LEFTSHOULDER); }
    SET.binds[ACT_WHAMMY][0] = ST(0); SET.binds[ACT_WHAMMY][1] = ST(1);
    if (p != 0) SET.binds[ACT_WHAMMY][2] = AX(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 1);
    SET.binds[ACT_PAUSE][0] = B(SDL_CONTROLLER_BUTTON_START);
}
void input_bind_name(const bind_t *b, char *out, size_t n) {
    if (b->type == BIND_BUTTON) {
        static const char *names[] = { "A", "B", "X", "Y", "Select", "Guide", "Start", "L3", "R3", "L1", "R1", "↑ direcional", "↓ direcional", "← direcional", "→ direcional" };
        if (b->id >= 0 && b->id < (int)(sizeof names / sizeof names[0])) snprintf(out, n, "%s", names[b->id]); else snprintf(out, n, "Botão %d", b->id);
    } else if (b->type == BIND_STICK) snprintf(out, n, "analógico %s", b->id ? "direito" : "esquerdo");
    else if (b->type == BIND_AXIS) {
        if (b->id == SDL_CONTROLLER_AXIS_TRIGGERLEFT) snprintf(out, n, "L2");
        else if (b->id == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) snprintf(out, n, "R2");
        else if (b->id == SDL_CONTROLLER_AXIS_LEFTY || b->id == SDL_CONTROLLER_AXIS_RIGHTY) { const char *side = b->id == SDL_CONTROLLER_AXIS_LEFTY ? "E" : "D"; snprintf(out, n, b->dir < 0 ? "analógico %s ↑" : b->dir > 0 ? "analógico %s ↓" : "analógico %s ↕", side); }
        else if (b->id == SDL_CONTROLLER_AXIS_LEFTX || b->id == SDL_CONTROLLER_AXIS_RIGHTX) { const char *side = b->id == SDL_CONTROLLER_AXIS_LEFTX ? "E" : "D"; snprintf(out, n, b->dir < 0 ? "analógico %s ←" : b->dir > 0 ? "analógico %s →" : "analógico %s ↔", side); }
        else snprintf(out, n, "eixo %d", b->id);
    } else snprintf(out, n, "–");
}
void input_action_buttons(int a, char *out, size_t n) {   /* "Select / L2" */
    out[0] = 0; for (int s = 0; s < BIND_SLOTS; s++) { if (!SET.binds[a][s].type) continue; char b[40]; input_bind_name(&SET.binds[a][s], b, sizeof b); size_t L = strlen(out); snprintf(out + L, n - L, "%s%s", L ? " / " : "", b); }
}

/* ---- state: axis values, button states, edge detection ---- */
static int axVal[SDL_CONTROLLER_AXIS_MAX];
static int axState[SDL_CONTROLLER_AXIS_MAX][2];   /* [axis][0 = negative side, 1 = positive side] */
static int stickOn[2];
static float whamLast = -1;
void input_reset_axes(void) { memset(axState, 0, sizeof axState); memset(stickOn, 0, sizeof stickOn); whamLast = -1; }
#define AX_ON 14000
#define AX_OFF 7000
#define STICK_DEAD 0.20f
static float stick_mag(int s) { int x = axVal[s ? SDL_CONTROLLER_AXIS_RIGHTX : SDL_CONTROLLER_AXIS_LEFTX], y = axVal[s ? SDL_CONTROLLER_AXIS_RIGHTY : SDL_CONTROLLER_AXIS_LEFTY];
    float m = sqrtf((float)x * x + (float)y * y) / 32767.0f; m = (m - STICK_DEAD) / (1 - STICK_DEAD); return m < 0 ? 0 : m > 1 ? 1 : m; }
static float bind_value(const bind_t *b) {
    if (b->type == BIND_STICK) return stick_mag(b->id);
    if (b->type == BIND_AXIS) { int v = axVal[b->id]; float f = b->dir == 0 ? fabsf((float)v) / 32767.0f : b->dir > 0 ? (v > 0 ? v / 32767.0f : 0) : (v < 0 ? -v / 32767.0f : 0); return f > 1 ? 1 : f; }
    return 0;
}
static int key_action(SDL_Keycode k, int *act) {
    switch (k) {
    case SDLK_1: case SDLK_a: *act = ACT_FRET0; return 1; case SDLK_2: case SDLK_s: *act = ACT_FRET1; return 1; case SDLK_3: case SDLK_d: *act = ACT_FRET2; return 1;
    case SDLK_4: case SDLK_f: *act = ACT_FRET3; return 1; case SDLK_5: case SDLK_g: *act = ACT_FRET4; return 1;
    case SDLK_UP: *act = ACT_STRUM_UP; return 1; case SDLK_DOWN: *act = ACT_STRUM_DOWN; return 1; case SDLK_SPACE: *act = ACT_STAR; return 1; case SDLK_w: *act = ACT_WHAMMY; return 1;
    case SDLK_ESCAPE: case SDLK_RETURN: *act = ACT_PAUSE; return 1; }
    return 0;
}
int input_translate(const SDL_Event *ev, inev_t *out, int max) {
    int n = 0, act;
    if (ev->type == SDL_KEYDOWN || ev->type == SDL_KEYUP) { if (ev->key.repeat || !key_action(ev->key.keysym.sym, &act) || n >= max) return 0; out[n].action = act; out[n].pressed = ev->type == SDL_KEYDOWN; out[n].value = out[n].pressed ? 1.0f : 0; return 1; }
    if (ev->type == SDL_CONTROLLERBUTTONDOWN || ev->type == SDL_CONTROLLERBUTTONUP) {
        for (int a = 0; a < ACT_N && n < max; a++) for (int s = 0; s < BIND_SLOTS; s++) { const bind_t *b = &SET.binds[a][s]; if (b->type == BIND_BUTTON && b->id == ev->cbutton.button) {
            out[n].action = a; out[n].pressed = ev->type == SDL_CONTROLLERBUTTONDOWN; out[n].value = out[n].pressed ? 1.0f : 0; n++; break; } }
        return n;
    }
    if (ev->type == SDL_CONTROLLERAXISMOTION) {
        int ax = ev->caxis.axis, v = ev->caxis.value; if (ax < 0 || ax >= SDL_CONTROLLER_AXIS_MAX) return 0; axVal[ax] = v;
        int side[2] = { v < -AX_ON ? 1 : v > -AX_OFF ? 0 : axState[ax][0], v > AX_ON ? 1 : v < AX_OFF ? 0 : axState[ax][1] };
        int changed[2] = { side[0] != axState[ax][0], side[1] != axState[ax][1] }; axState[ax][0] = side[0]; axState[ax][1] = side[1];
        int stick = ax == SDL_CONTROLLER_AXIS_LEFTX || ax == SDL_CONTROLLER_AXIS_LEFTY ? 0 : ax == SDL_CONTROLLER_AXIS_RIGHTX || ax == SDL_CONTROLLER_AXIS_RIGHTY ? 1 : -1;
        int stOn = stick >= 0 && stick_mag(stick) > 0.5f, stChanged = stick >= 0 && stOn != stickOn[stick]; if (stick >= 0) stickOn[stick] = stOn;
        for (int a = 0; a < ACT_N && n < max; a++) {
            if (a == ACT_WHAMMY) { int touches = 0; float val = 0;   /* the whammy is the deepest of all its bindings */
                for (int s = 0; s < BIND_SLOTS; s++) { const bind_t *b = &SET.binds[a][s]; if ((b->type == BIND_AXIS && b->id == ax) || (b->type == BIND_STICK && b->id == stick)) touches = 1; float bv = bind_value(b); if (bv > val) val = bv; }
                if (touches && fabsf(val - whamLast) > 0.004f) { whamLast = val; out[n].action = a; out[n].pressed = val > 0.02f; out[n].value = val; n++; }
                continue; }
            for (int s = 0; s < BIND_SLOTS; s++) { const bind_t *b = &SET.binds[a][s];
                if (b->type == BIND_STICK && b->id == stick && stChanged) { out[n].action = a; out[n].pressed = stOn; out[n].value = stOn; n++; break; }
                if (b->type != BIND_AXIS || b->id != ax) continue;
                int sd = b->dir < 0 ? 0 : 1; if (b->dir == 0) sd = v < 0 ? 0 : 1; if (!changed[sd]) continue;
                out[n].action = a; out[n].pressed = side[sd]; out[n].value = side[sd] ? 1.0f : 0; n++; break; }
        }
        return n;
    }
    return 0;
}
int input_capture(const SDL_Event *ev, int action, bind_t *out) {
    if (ev->type == SDL_CONTROLLERBUTTONDOWN) { *out = B(ev->cbutton.button); return 1; }
    if (ev->type == SDL_CONTROLLERAXISMOTION) { int v = ev->caxis.value, ax = ev->caxis.axis; if (ax < 0 || ax >= SDL_CONTROLLER_AXIS_MAX) return 0; axVal[ax] = v;
        if (v > -AX_ON && v < AX_ON) { if (v > -AX_OFF && v < AX_OFF) axState[ax][0] = axState[ax][1] = 0; return 0; }
        int sd = v < 0 ? 0 : 1; if (axState[ax][sd]) return 0; axState[ax][sd] = 1;
        int trigger = ax == SDL_CONTROLLER_AXIS_TRIGGERLEFT || ax == SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
        if (action == ACT_WHAMMY && !trigger) { *out = ST(ax == SDL_CONTROLLER_AXIS_LEFTX || ax == SDL_CONTROLLER_AXIS_LEFTY ? 0 : 1); return 1; }   /* a stick for the whammy: the whole stick */
        *out = AX(ax, trigger ? 1 : v < 0 ? -1 : 1); return 1; }
    return 0;
}
