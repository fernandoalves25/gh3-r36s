/* A boss battle on screen, the browser port's layout (two highways 225 either side of the middle, the meters with
 * their cards in the top corners: battle_hud_2d_elements) fitted to 640x480: the player's highway on the left, the
 * boss's on the right, both narrowed; a tug-of-war bar between the meters; the attacks as they land (alerts, bolts,
 * the broken strings over the frets, the whammy count, the Death Drain). The cards are baked per visual theme:
 * chunky pixel art enlarged, or smooth shapes in HD. Also the battle's own screen: the intro card and the banners. */
#include "battle.h"
#include "app.h"
#include "render.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define LANE_Q 0.55f
static gfx_sprite *icons[THEME_N][PU_KINDS + 1];   /* [PU_KINDS]: an empty slot */
static const uint32_t CARD_BG[PU_KINDS + 1] = { RGB(0xb0, 0x6a, 0x10), RGB(0x1f, 0x8a, 0x3e), RGB(0x2a, 0x5c, 0xb8), RGB(0x7a, 0x3a, 0xa8), RGB(0x16, 0x8a, 0x8a), RGB(0xa8, 0x26, 0x22), RGB(0xc8, 0x6a, 0x16), RGB(0x30, 0x10, 0x14), RGB(0x2a, 0x25, 0x36) };

/* ---- the cards: a coloured tile with the attack's sign, drawn on a 16-unit grid scaled by u */
static void card_art(fb_t *f, int kind, float u) {
    uint32_t bg = CARD_BG[kind], ink = RGB(255, 246, 220), dk = RGB(12, 8, 16);
    fb_rect(f, (int)u, 0, (int)(14 * u), (int)(16 * u), dk); fb_rect(f, 0, (int)u, (int)(16 * u), (int)(14 * u), dk);
    fb_rect(f, (int)(2 * u), (int)u, (int)(12 * u), (int)(14 * u), bg); fb_rect(f, (int)u, (int)(2 * u), (int)(14 * u), (int)(12 * u), bg);
    fb_rect(f, (int)(2 * u), (int)u, (int)(12 * u), (int)u, fb_alpha(RGB(255, 255, 255), 0.35f));
    if (kind == PU_KINDS) { fb_rect(f, (int)(2 * u), (int)(2 * u), (int)(12 * u), (int)(12 * u), RGB(0x1a, 0x16, 0x22)); fb_ellipse(f, 8 * u, 8 * u, 2 * u, 2 * u, RGB(0x3a, 0x34, 0x48)); return; }
    float P[24];
    switch (kind) {
    case PU_LIGHTNING: { float b[] = { 9, 2, 4, 9, 7.5f, 9, 6, 14, 12, 6.5f, 8.5f, 6.5f }; for (int i = 0; i < 12; i++) P[i] = b[i] * u; fb_poly(f, P, 6, RGB(0xff, 0xe0, 0x40)); break; }
    case PU_DIFFUP: for (int k = 0; k < 2; k++) { float y = (k ? 9 : 4) * u; float b[] = { 8 * u, y, 12.5f * u, y + 4 * u, 3.5f * u, y + 4 * u }; fb_poly(f, b, 3, ink); } break;
    case PU_DOUBLE: fb_ellipse(f, 5.5f * u, 9 * u, 3 * u, 2 * u, RGB(0x31, 0xc4, 0x66)); fb_ellipse(f, 10.5f * u, 9 * u, 3 * u, 2 * u, RGB(0xea, 0x4b, 0x40)); fb_ellipse(f, 5.5f * u, 8.5f * u, 1.4f * u, 0.9f * u, ink); fb_ellipse(f, 10.5f * u, 8.5f * u, 1.4f * u, 0.9f * u, ink); fb_rect(f, (int)(5 * u), (int)(4 * u), (int)(6 * u), (int)u, ink); break;
    case PU_STEAL: { fb_rect(f, (int)(7 * u), (int)(3 * u), (int)(6 * u), (int)(8 * u), ink); fb_rect(f, (int)(8 * u), (int)(4 * u), (int)(4 * u), (int)(6 * u), bg); float b[] = { 2 * u, 11 * u, 6 * u, 8 * u, 6 * u, 14 * u }; fb_poly(f, b, 3, RGB(0xff, 0xe0, 0x40)); fb_rect(f, (int)(5 * u), (int)(10 * u), (int)(6 * u), (int)(2 * u), RGB(0xff, 0xe0, 0x40)); break; }
    case PU_LEFTY: { float a[] = { 2 * u, 6 * u, 6 * u, 3 * u, 6 * u, 9 * u }, b[] = { 14 * u, 10 * u, 10 * u, 7 * u, 10 * u, 13 * u }; fb_poly(f, a, 3, ink); fb_poly(f, b, 3, ink); fb_rect(f, (int)(6 * u), (int)(5 * u), (int)(7 * u), (int)(2 * u), ink); fb_rect(f, (int)(3 * u), (int)(9 * u), (int)(7 * u), (int)(2 * u), ink); break; }
    case PU_STRING: fb_line(f, 3 * u, 3 * u, 7 * u, 7 * u, 1.2f * u, ink); fb_line(f, 9 * u, 9 * u, 13 * u, 13 * u, 1.2f * u, ink); fb_line(f, 7 * u, 7 * u, 6 * u, 10 * u, 1.0f * u, RGB(0xff, 0xe0, 0x40)); fb_line(f, 9 * u, 9 * u, 10.5f * u, 6 * u, 1.0f * u, RGB(0xff, 0xe0, 0x40)); break;
    case PU_WHAMMY: fb_line(f, 4 * u, 13 * u, 9 * u, 6 * u, 1.4f * u, ink); fb_line(f, 9 * u, 6 * u, 12 * u, 4 * u, 1.4f * u, ink); fb_ellipse(f, 12.5f * u, 3.5f * u, 1.8f * u, 1.8f * u, RGB(0xff, 0xe0, 0x40)); fb_ellipse(f, 4 * u, 13 * u, 2 * u, 1.4f * u, dk); break;
    case PU_DEATH: fb_ellipse(f, 8 * u, 7 * u, 5 * u, 4.6f * u, ink); fb_rect(f, (int)(5.5f * u), (int)(10 * u), (int)(5 * u), (int)(3 * u), ink); fb_ellipse(f, 6 * u, 7 * u, 1.5f * u, 1.6f * u, RGB(0xc0, 0x10, 0x10)); fb_ellipse(f, 10 * u, 7 * u, 1.5f * u, 1.6f * u, RGB(0xc0, 0x10, 0x10));
        for (int i = 0; i < 3; i++) fb_rect(f, (int)((6.4f + i * 1.4f) * u), (int)(11 * u), (int)(0.6f * u + 1), (int)(2 * u), dk); break;
    }
}
static void bake_icons(void) {
    if (icons[0][0]) return; int px = art_px();
    for (int k = 0; k <= PU_KINDS; k++) { fb_t f; fb_init(&f, 16, 16); card_art(&f, k, 1); icons[0][k] = gfx_bake_up(&f, px, 0); fb_free(&f);   /* pixel art: 16 art px, enlarged */
        fb_init(&f, 64, 64); card_art(&f, k, 4); icons[1][k] = gfx_bake(&f, 1); fb_free(&f); }                                   /* HD: smooth, filtered */
}
static int theme(void) { return SET.theme >= 0 && SET.theme < THEME_N ? SET.theme : 0; }
static void card(int pi, float cx, float cy, float d, float alpha) { int k = pi < 0 ? PU_KINDS : battle_pu_kind(pi); if (k < 0) k = PU_LIGHTNING; gfx_sprite *s = icons[theme()][k]; gfx_blit(s, 0, 0, gfx_sprite_w(s), gfx_sprite_h(s), cx - d / 2, cy - d / 2, d, d, RGB(255, 255, 255), alpha); }

/* ---- where things are */
static float lane_cx(int side) { return A.W * (side ? 0.7f : 0.3f); }
static float hash01(unsigned x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return (x & 0xFFFF) / 65535.0f; }
static int in_wins(const bwin_t *w, int n, double t) { for (int i = 0; i < n; i++) if (t >= w[i].a && t < w[i].b) return 1; return 0; }
static void lane_set(const bside_t *s, double perf) {   /* battleLaneFx: Amp Overload shakes the highway, the whammy attack wobbles it */
    double t = s->r->now; memset(&RL, 0, sizeof RL); RL.on = 1; RL.cx = lane_cx(s->side); RL.q = LANE_Q;
    if (t < s->shake) { unsigned k = (unsigned)(perf / 40); RL.dx = (hash01(k * 2 + s->side) - 0.5f) * 7; RL.dy = (hash01(k * 2 + 7 + s->side) - 0.5f) * 4; RL.flick = s->flick > 0 ? s->flick : 2; }
    if (s->whammy > 0) RL.dx += sinf((float)perf / 45) * 3;
    RL.flip = in_wins(s->lefty, s->nlefty, t);
}
static void slot_xy(int side, int j, float *x, float *y, float *d) {   /* the three card slots under each meter, the top card nearest the middle */
    static const float D[3] = { 30, 22, 16 }; float c = 130 - 8 - D[0] / 2; for (int k = 1; k <= j; k++) c -= D[k - 1] / 2 + 6 + D[k] / 2;
    *d = D[j]; *x = side ? A.W - c : c; *y = 104;
}

/* ---- over a highway: broken strings, the whammy count, the Death Drain */
static void lane_top(const bside_t *s, double perf) {
    float hw = render_lane_hw(); double t = s->r->now; char b[32];
    for (int f = 0; f < 5; f++) if (s->broken[f]) { float x, y, sc; render_lane_xy(f, 0, &x, &y, &sc);
        gfx_rect(x - hw * 0.17f, y - hw * 0.07f, hw * 0.34f, hw * 0.14f, RGBA(120, 0, 0, 150)); gfx_line(x - 8, y - 15, x + 6, y + 3, 2, RGB(255, 106, 80));
        snprintf(b, sizeof b, "%d", s->broken[f]); gfx_text_sh_center((int)x, (int)(y - 32), 2, b, RGB(255, 217, 200), C_SHADOW); }
    if (s->whammy > 0) { float x, y, sc; render_lane_xy(2, 0, &x, &y, &sc); float a = 0.75f + 0.25f * sinf((float)perf / 90); snprintf(b, sizeof b, "Alavanca!  %d", s->whammy); gfx_text_sh_center((int)RL.cx, (int)(y - 60), 2, b, fb_alpha(RGB(255, 230, 128), a), C_SHADOW); }
    if (s->death && t >= s->deathFrom && t < s->deathUntil) { float k = 0.5f + 0.5f * sinf((float)perf / 120); gfx_rect(RL.cx - hw * 1.1f, A.H * 0.12f, hw * 2.2f, A.H * 0.88f, RGBA(200, 20, 20, (int)(60 + 50 * k))); }
    if (in_wins(s->wins, s->nwins, t) || s->mask) { float k = 0.5f + 0.5f * sinf((float)perf / 160); gfx_rect(RL.cx - hw * 1.08f, A.H * 0.84f + 6, hw * 2.16f, 3, fb_alpha(C_BAD, 0.4f + 0.4f * k)); }   /* under attack: the meters move */
}

/* ---- the HUD: both meters with their cards, the tug-of-war between them */
static void meter(const bside_t *s, double perf) {
    gfx_sprite *m = art_get(ART_METER); int mw = gfx_sprite_w(m), mh = gfx_sprite_h(m); int px = art_px(); float pw = 122, ph = 120, x0 = s->side ? A.W - 8 - pw : 8, y0 = 6;
    ui_panel(x0, y0, pw, ph, s->side ? UI_PANEL_DARK : UI_PANEL_FLAT);
    float mx = x0 + (pw - mw) / 2, my = y0 + 8; gfx_sprite_at(m, mx, my, 1, RGB(255, 255, 255), 1);
    double h = s->r->health / 2; if (h < 0) h = 0; if (h > 1) h = 1;   /* the battle's meters run 0..2 */
    float ang = (float)M_PI * (float)(1 - h), ncx = mx + mw / 2.0f, ncy = my + mh - 2 * px;
    gfx_line(ncx, ncy, ncx + cosf(ang) * (mw * 0.42f), ncy - sinf(ang) * (mw * 0.42f), 2.0f * px, C_INK); gfx_rect(ncx - 2 * px, ncy - 2 * px, 4 * px, 4 * px, C_AMP);
    if (h < 0.25) { float k = 0.5f + 0.5f * sinf((float)perf / 70); gfx_rect(x0, y0, pw, ph, fb_alpha(C_BAD, 0.1f + 0.15f * k)); }
    const char *nm = s->cpu ? BTL.boss->name : "Você"; gfx_text_sh_center((int)(x0 + pw / 2), (int)(my + mh + 2), 1, nm, s->cpu ? RGB(255, 200, 180) : RGB(255, 240, 215), C_SHADOW);
    for (int j = 0; j < 3; j++) { float x, y, d; slot_xy(s->side, j, &x, &y, &d); int c = j < s->ncards ? s->cards[s->ncards - 1 - j] : -1;
        int flying = 0; for (int i = 0; i < BTL.nflies; i++) if (BTL.flies[i].side == s->side && perf - BTL.flies[i].at < 300 && j == 0) flying = 1;
        card(flying ? -1 : c, x, y, d, c >= 0 || j == 0 ? 1 : 0.6f); }
}
static void tug(double perf) {   /* who is ahead: the two meters against each other */
    int W = A.W; float bw = 140, x = W / 2.0f - bw / 2, y = 26; double a = BTL.me.r->health, b = BTL.cpu.r->health; float k = a + b > 0 ? (float)(a / (a + b)) : 0.5f;
    gfx_text_title(W / 2 - gfx_text_w("VS", 2) / 2, 6, 2, "VS", C_AMP, C_SHADOW, 2);
    gfx_rect(x - 2, y - 2, bw + 4, 14, C_SHADOW); gfx_rect(x, y, bw * k, 10, C_AMP); gfx_rect(x + bw * k, y, bw * (1 - k), 10, C_BAD); gfx_rect(x, y, bw, 2, fb_alpha(RGB(255, 255, 255), 0.3f));
    gfx_rect(x + bw * k - 2, y - 4, 4, 18, C_INK);
    if (A.S.section[0]) gfx_text_sh_center(W / 2, y + 16, 1, A.S.section, C_MUTED, C_SHADOW);
}
static void bolts_alerts_flies(double perf) {
    for (int j = 0; j < BTL.nbolts; j++) { float age = (float)((perf - BTL.bolts[j].at) / 260); if (age < 0 || age >= 1) continue;   /* attack_bolt: a red bolt from the attacker's meter to the other highway */
        int a = BTL.bolts[j].from; float x0 = a ? A.W - 69.0f : 69.0f, y0 = 70, x1 = lane_cx(!a), y1 = A.H * 0.72f; unsigned seed = (unsigned)(perf / 40) + j * 31;
        float px = x0, py = y0; for (int i = 1; i <= 8; i++) { float k = i / 8.0f, nx = x0 + (x1 - x0) * k + (i < 8 ? (hash01(seed + i) - 0.5f) * 26 : 0), ny = y0 + (y1 - y0) * k;
            gfx_line(px, py, nx, ny, 4, fb_alpha(RGB(255, 70, 50), 1 - age * age)); gfx_line(px, py, nx, ny, 1.5f, fb_alpha(RGB(255, 230, 200), 1 - age * age)); px = nx; py = ny; } }
    for (int j = 0; j < BTL.nalerts; j++) { double age = perf - BTL.alerts[j].at, life = BTL.alerts[j].small ? 1400 : 1900; if (age < 0 || age >= life) continue;
        float al = age > life - 300 ? (float)((life - age) / 300) : 1; const char *tx = BTL.alerts[j].text; int x = (int)lane_cx(BTL.alerts[j].side);
        if (BTL.alerts[j].small) gfx_text_sh_center(x, (int)(A.H * 0.40f - age / 40), 2, tx, fb_alpha(BTL.alerts[j].col, al), C_SHADOW);
        else { int sc = age < 160 ? 3 : 2, w = gfx_text_w(tx, sc); if (w > A.W * 0.44f) sc = 2, w = gfx_text_w(tx, 2); gfx_text_title(x - w / 2, (int)(A.H * 0.58f), sc, tx, fb_alpha(BTL.alerts[j].col, al), C_SHADOW, 2); } }
    for (int j = 0; j < BTL.nflies; j++) { float age = (float)((perf - BTL.flies[j].at) / 300); if (age < 0 || age >= 1) continue;   /* a card won flies up to its place */
        int s = BTL.flies[j].side; float fx, fy, sc, tx, ty, d; RL.cx = lane_cx(s); RL.q = LANE_Q; RL.dx = RL.dy = 0; RL.flip = 0; RL.on = 1; render_lane_xy((int)BTL.flies[j].x, 0, &fx, &fy, &sc); RL.on = 0;
        slot_xy(s, 0, &tx, &ty, &d); float e = 1 - (1 - age) * (1 - age); card(BTL.flies[j].card, fx + (tx - fx) * e, fy + (ty - fy) * e, 32 - 2 * e, 1); }
}
void render_battle(double perf, int countdown) {
    if (!BTL.on) return; bake_icons(); gh3_run *r = BTL.me.r; int W = A.W, H = A.H;
    float pulse = r->hasLastBeat ? 1 - (float)fmod(r->lastBeat, 1.0) : 0;
    render_stage(perf, pulse, (float)fmin(1, r->streak / 40.0), SET.stageFx);
    if (SET.bgDim > 0) gfx_rect(0, 0, W, H, RGBA(8, 6, 12, (int)(SET.bgDim * 1.8)));
    for (int k = 1; k >= 0; k--) { const bside_t *s = k ? &BTL.cpu : &BTL.me; lane_set(s, perf); render_game(s->r, perf, NULL); lane_top(s, perf); }
    memset(&RL, 0, sizeof RL);
    meter(&BTL.me, perf); meter(&BTL.cpu, perf); tug(perf);
    bolts_alerts_flies(perf);
    char b[96];
    if (BTL.me.ncards && !r->autoPlay && !BTL.over) { char keys[80]; snprintf(keys, sizeof keys, "%s", A.starKeys[0] ? A.starKeys : "Select"); snprintf(b, sizeof b, "ATACAR: %s", keys); float k = 0.6f + 0.4f * sinf((float)perf / 110);
        int tw = gfx_text_w(b, 1) + 16; ui_panel(W / 2 - tw / 2, H - 28, tw, 20, UI_PANEL_AMBER); gfx_text_sh_center(W / 2, H - 23, 1, b, fb_alpha(C_INK, k), C_SHADOW); }
    if (countdown && r->now < 0) { int n3 = (int)ceil(-r->now / 1000); snprintf(b, sizeof b, "%d", n3); float k = (float)fmod(-r->now, 1000) / 1000; int sc = 8 + (int)(3 * k);
        gfx_text_title(W / 2 - gfx_text_w(b, sc) / 2, H / 2 - 50, sc, b, C_INK, C_SHADOW, 3); snprintf(b, sizeof b, "Derrote %s!", BTL.boss->name); gfx_text_sh_center(W / 2, H / 2 + 50, 2, b, C_AMP, C_SHADOW); }
    if (SET.showFps && A.fps > 0) { snprintf(b, sizeof b, "%d fps", A.fps); gfx_text(W / 2 - 20, H - 44, 1, b, C_MUTED); }
}

/* ---- SC_BATTLE */
static void draw_intro(double now) {
    int W = A.W, H = A.H; const gh3_song *s = &A.lib->songs[BTL.songIdx]; const gh3_boss *b = gh3_library_boss(A.lib, BTL.songIdx); double T = (now - BTL.phaseAt) / 1000; char t[160], keys[80];
    bake_icons(); render_backdrop(now, SET.stageFx); ui_dim(W, H, 60);
    float k = T < 0.35 ? (float)(T / 0.35) : 1; int y0 = 40 - (int)(30 * (1 - k));
    gfx_text_title(W / 2 - gfx_text_w("BATALHA", 6) / 2, y0, 6, "BATALHA", C_AMP, C_SHADOW, 4);
    snprintf(t, sizeof t, "vs %s", b ? b->name : s->artist); int sc = gfx_text_w(t, 4) > W - 40 ? 3 : 4; gfx_text_title(W / 2 - gfx_text_w(t, sc) / 2, y0 + 84, sc, t, C_INK, C_SHADOW, 3);
    gfx_text_sh_center(W / 2, y0 + 140, 1, s->title, C_MUTED, C_SHADOW);
    input_action_buttons(ACT_STAR, keys, sizeof keys);
    const char *L[3]; char l2[160], l3[120]; snprintf(l2, sizeof l2, "Lance o ataque do topo com o botão de star power (%s).", keys[0] ? keys : "Select"); snprintf(l3, sizeof l3, "Faça %s falhar antes do fim da música!", b ? b->name : "o chefe");
    L[0] = "Complete as notas de batalha para ganhar um ataque (até 3)."; L[1] = l2; L[2] = l3;
    ui_panel(30, 214, W - 60, 150, UI_PANEL_DARK);
    static const int IC[3] = { PU_DIFFUP, PU_LIGHTNING, PU_DEATH };
    for (int i = 0; i < 3; i++) { float a = (float)fmax(0, fmin(1, (T - 0.3 - 0.25 * i) / 0.3)); if (a <= 0) continue; int y = 228 + i * 44; gfx_sprite *ic = icons[theme()][IC[i]];
        gfx_blit(ic, 0, 0, gfx_sprite_w(ic), gfx_sprite_h(ic), 45, y - 5, 28, 28, RGB(255, 255, 255), a); gfx_text_box(84, y + 3, W - 130, 1, L[i], fb_alpha(C_INK, a)); }
    if (BTL.phaseMs > 0) ui_bar(30, 376, W - 60, 6, (float)((now - BTL.phaseAt) / BTL.phaseMs), C_AMP);
    static const char *bt[2] = { "A", "B" }, *tx[2] = { "Começar", "Voltar" }; ui_hints(H - 24, W, bt, tx, 2);
}
static void draw_end(double now) {
    int W = A.W, H = A.H; double age = now - BTL.phaseAt; char sub[120] = "";
    if (BTL.on) render_battle(now, 0); else { gh3_run *r = &A.S.run; render_game(r, now, NULL); }
    ui_dim(W, H, (int)fmin(150, age / 4));
    const char *tx = BTL.banner; int sc = 5; while (sc > 2 && gfx_text_w(tx, sc) > W - 30) sc--;
    int pop = age < 220 ? (int)((1 - age / 220) * 3) : 0, s2 = sc + pop; int jx = (int)(sinf((float)age / 90) * 3), jy = (int)(cosf((float)age / 70) * 2);
    uint32_t col = BTL.endKind == 0 ? C_BAD : BTL.endKind == 3 ? RGB(255, 215, 90) : C_INK;
    gfx_text_title(W / 2 - gfx_text_w(tx, s2) / 2 + jx, H / 2 - 40 - pop * 6 + jy, s2, tx, col, C_SHADOW, 4);
    const char *bn = BTL.boss ? BTL.boss->name : "Lou";
    if (BTL.endKind == 1) snprintf(sub, sizeof sub, "%s falhou: a batalha é sua!", bn); else if (BTL.endKind == 2) snprintf(sub, sizeof sub, "Toque o resto da música até o fim!");
    else if (BTL.endKind == 3) snprintf(sub, sizeof sub, "O diabo foi derrotado!"); else snprintf(sub, sizeof sub, "Você não derrotou %s. Tente de novo!", bn);
    if (age > 500) gfx_text_sh_center(W / 2, H / 2 + 40, 2, sub, C_AMP, C_SHADOW);
    if (age > 1200) { static const char *bt[1] = { "A" }, *t2[1] = { "Continuar" }; ui_hints(H - 24, W, bt, t2, 1); }
}
void battle_screen_draw(double now) { if (BTL.phase == BPH_INTRO) draw_intro(now); else draw_end(now); }
void battle_result_lines(int cx, int y) {
    gh3_run *r = BTL.me.r; char b[120]; const char *bn = BTL.boss->name; int played = 0; for (int i = 0; i < r->nnotes; i++) played += r->notes[i].hit || r->notes[i].missed;
    if (BTL.won) snprintf(b, sizeof b, "Você venceu %s!", bn); else snprintf(b, sizeof b, "%s venceu a batalha", bn);
    gfx_text_sh_center(cx, y, 2, b, BTL.won ? C_GOOD : C_BAD, C_SHADOW);
    snprintf(b, sizeof b, "Ataques: %d seus, %d de %s • notas tocadas: %d de %d", BTL.me.fired, BTL.cpu.fired, bn, r->hits, played); gfx_text_center(cx, y + 28, 1, b, C_MUTED);
}
