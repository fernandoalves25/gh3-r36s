/* The "Original" theme: GH3's own gems, fret buttons, flames, highway, HUD boxes, menu backgrounds, frames, pills and
 * fonts, placed as the game's scripts place them (the layout the browser port rebuilt), scaled to 640x480. Every
 * picture comes through gamearts.c on first use; nothing is read until this theme is chosen, and a theme change lets
 * it all go. Where the game had a 3D object (the stage, the rotating needle) the other themes' drawing stays. */
#include "original.h"
#include "gamearts.h"
#include "app.h"
#include "settings.h"
#include "ui.h"
#include "gh3fmt.h"
#include "store.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int tried, loaded;
static const char *COLOR[5] = { "Green", "Red", "Yellow", "Blue", "Orange" };
static const char *F_TITLE = "fontgrid_title_a1", *F_HEAD = "fontgrid_text_a6", *F_BUTTONS = "buttons_x360", *F_SCORE = "fontgrid_numeral_a9";
#define C_PAPER RGB(215, 160, 110)      /* the main menu's words */
#define C_FRAME RGB(160, 34, 22)        /* the pause frame's red */
int orig_ready(void) {
    if (!tried) { tried = 1; const char *pk = app_data_path("zones", "global.pak.xen"), *pb = app_data_path("zones", "global.pab.xen");
        if (pk && pb && ga_open(pk, pb) == 0) SDL_Log("tema Original: arte do jogo em %s", pk); else SDL_Log("tema Original: sem ZONES/global.pak.xen + global.pab.xen, fica o desenho do tema HD"); }
    if (ga_ok()) loaded = 1; return ga_ok();
}
/* ---- helpers ---- */
static void black_to_alpha(fb_t *f) { /* sheets meant to be added to the screen (flames on black): the black becomes see-through */
    for (int i = 0, n = f->w * f->h; i < n; i++) { uint32_t c = f->px[i]; int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255, a = r > g ? (r > b ? r : b) : (g > b ? g : b);
        if (!a) { f->px[i] = 0; continue; } f->px[i] = RGBA(r * 255 / a, g * 255 / a, b * 255 / a, a); }
}
static void fb_rotate(const fb_t *src, float deg, float scale, fb_t *out) { /* src turned by deg (clockwise on screen) and scaled, into a tight image (bilinear) */
    float a = deg * (float)M_PI / 180, ca = cosf(a), sa = sinf(a), hw = src->w * scale / 2, hh = src->h * scale / 2;
    float bw = fabsf(hw * ca) + fabsf(hh * sa), bh = fabsf(hw * sa) + fabsf(hh * ca); int W = (int)ceilf(bw * 2) + 2, H = (int)ceilf(bh * 2) + 2; fb_init(out, W, H);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { float dx = x + 0.5f - W / 2.0f, dy = y + 0.5f - H / 2.0f, sx = (dx * ca + dy * sa) / scale + src->w / 2.0f - 0.5f, sy = (-dx * sa + dy * ca) / scale + src->h / 2.0f - 0.5f;
        int x0 = (int)floorf(sx), y0 = (int)floorf(sy); float fx = sx - x0, fy = sy - y0; if (x0 < -1 || y0 < -1 || x0 >= src->w || y0 >= src->h) continue;
        uint32_t acc[4] = { 0, 0, 0, 0 }; float wsum = 0;
        for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) { int px = x0 + i, py = y0 + j; if (px < 0 || py < 0 || px >= src->w || py >= src->h) continue; float w = (i ? fx : 1 - fx) * (j ? fy : 1 - fy); uint32_t c = src->px[(size_t)py * src->w + px]; wsum += w;
            acc[0] += (uint32_t)(((c >> 24) & 255) * w); acc[1] += (uint32_t)(((c >> 16) & 255) * w * ((c >> 24) & 255) / 255); acc[2] += (uint32_t)(((c >> 8) & 255) * w * ((c >> 24) & 255) / 255); acc[3] += (uint32_t)((c & 255) * w * ((c >> 24) & 255) / 255); }
        if (wsum <= 0 || !acc[0]) continue; uint32_t al = acc[0]; out->px[(size_t)y * W + x] = RGBA(acc[1] * 255 / al, acc[2] * 255 / al, acc[3] * 255 / al, al > 255 ? 255 : al); }
}
static gfx_sprite *bake(fb_t *f, int linear) { gfx_sprite *s = gfx_bake(f, linear); fb_free(f); return s; }
static void black_to_alpha(fb_t *f);
typedef struct { uint32_t key; int fx, fy, add; gfx_sprite *s; } flip_t; static flip_t flips[24]; static int nflips;
static gfx_sprite *variant(const char *name, int fx, int fy, int add) { /* a picture mirrored left to right (fx) or upside down (fy), or its black made see-through (add: lights on black), baked once */
    uint32_t k = gh3_qbkey(name); for (int i = 0; i < nflips; i++) if (flips[i].key == k && flips[i].fx == fx && flips[i].fy == fy && flips[i].add == add) return flips[i].s;
    if (nflips == 24) return NULL; fb_t f, o; if (ga_img_fb(name, &f)) return NULL; fb_init(&o, f.w, f.h);
    for (int y = 0; y < f.h; y++) for (int x = 0; x < f.w; x++) o.px[(size_t)y * f.w + x] = f.px[(size_t)(fy ? f.h - 1 - y : y) * f.w + (fx ? f.w - 1 - x : x)];
    fb_free(&f); if (add) black_to_alpha(&o); flips[nflips].key = k; flips[nflips].fx = fx; flips[nflips].fy = fy; flips[nflips].add = add; return flips[nflips++].s = bake(&o, 1);
}
static void blit_fit(gfx_sprite *s, float x, float y, float w, float h, uint32_t tint, float alpha) { if (s) gfx_blit(s, 0, 0, gfx_sprite_w(s), gfx_sprite_h(s), x, y, w, h, tint, alpha); }
static void blit_cover(gfx_sprite *s, int W, int H, uint32_t tint, float alpha) { /* the picture scaled to fill the screen, centred, the excess cut */
    if (!s) return; int sw = gfx_sprite_w(s), sh = gfx_sprite_h(s); float k = fmaxf((float)W / sw, (float)H / sh), dw = sw * k, dh = sh * k;
    int cx = (int)((dw - W) / 2 / k), cy = (int)((dh - H) / 2 / k); gfx_blit(s, cx, cy, sw - 2 * cx, sh - 2 * cy, 0, 0, (float)W, (float)H, tint, alpha);
}
static void tile(gfx_sprite *s, float x, float y, float w, float h, float ts, float alpha) { /* the picture repeated over the rectangle, ts px a tile, cut at the edges */
    if (!s) return; int sw = gfx_sprite_w(s), sh = gfx_sprite_h(s);
    for (float ty = y; ty < y + h; ty += ts) for (float tx = x; tx < x + w; tx += ts) { float cw = fminf(ts, x + w - tx), ch = fminf(ts, y + h - ty); gfx_blit(s, 0, 0, (int)(sw * cw / ts), (int)(sh * ch / ts), tx, ty, cw, ch, RGB(255, 255, 255), alpha); }
}

/* ================================================================== gameplay */
static gfx_sprite *hitSheet, *hitBlue, *holdSheet;
static gfx_sprite *sheet_add(const char *mat, gfx_sprite **slot) { if (!*slot) { fb_t f; if (ga_mat_fb(mat, &f) == 0) { black_to_alpha(&f); *slot = bake(&f, 1); } } return *slot; }
int orig_bake_back(fb_t *f, orig_proj proj, float vy, float sy, float persp, float zNear, int sp) {
    uint32_t key; fb_t tex, side; if (ga_tex_biggest(512, 1024, &key) || ga_tex_fb(key, &tex)) return -1; int hasSide = ga_mat_fb("sys_sidebar2D", &side) == 0;
    float s, ax, ay, bx, by, cx, cy, dx, dy; proj(-1, 1, &ax, &ay, &s); proj(1, 1, &bx, &by, &s); proj(1, zNear, &cx, &cy, &s); proj(-1, zNear, &dx, &dy, &s);
    const float TILING = 1.5f; float railW = (cx - dx) * 0.02f;   /* the rail: a thin strip along each edge, 35 % of it outside */
    for (int y = (int)ay; y <= (int)cy && y < f->h; y++) {
        float kk = (y + 0.5f - vy) / (sy - vy); if (kk <= 0.001f) continue; float z = (1 / kk - 1) / persp, k = (y - ay) / (cy - ay);
        float lx = ax + (dx - ax) * k, rx = bx + (cx - bx) * k, v = z * TILING; v -= floorf(v); int ty = (int)((1 - v) * tex.h); if (ty >= tex.h) ty = tex.h - 1; if (ty < 0) ty = 0;
        float dark = z > 0.75f ? 1 - (z - 0.75f) / 0.25f : 1; if (dark < 0) dark = 0;
        for (int x = (int)lx; x < (int)rx && x < f->w; x++) { if (x < 0) continue; float u = (x + 0.5f - lx) / (rx - lx); int tx = (int)(u * tex.w); if (tx >= tex.w) tx = tex.w - 1; uint32_t c = tex.px[(size_t)ty * tex.w + tx];
            int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255; if (sp) { r = r * 64 / 255; }   /* star power: the board goes blue (the game multiplies it by (64, 255, 255)) */
            f->px[(size_t)y * f->w + x] = RGBA(r, g, b, (int)(255 * dark)); }
        if (hasSide) for (int side2 = 0; side2 < 2; side2++) { float ex = side2 ? rx : lx, x0 = ex - railW * (side2 ? 0.35f : 0.65f); float vv = (y - ay) / (cy - ay);
            for (int x = (int)floorf(x0); x < (int)ceilf(x0 + railW); x++) { if (x < 0 || x >= f->w) continue; float u = (x + 0.5f - x0) / railW; if (u < 0 || u >= 1) continue; if (side2) u = 1 - u;
                uint32_t c = side.px[(size_t)(int)(vv * (side.h - 1)) * side.w + (int)(u * side.w)]; uint32_t a = (c >> 24) * (uint32_t)(255 * dark) / 255; if (!a) continue;
                if (sp) c = (c & 0xFF00FFFF) | ((uint32_t)(((c >> 16) & 255) * 64 / 255) << 16);
                uint32_t *d = &f->px[(size_t)y * f->w + x], da = *d >> 24, ia = 255 - a;   /* over the board */
                *d = RGBA((((c >> 16) & 255) * a + ((*d >> 16) & 255) * ia) / 255, (((c >> 8) & 255) * a + ((*d >> 8) & 255) * ia) / 255, ((c & 255) * a + (*d & 255) * ia) / 255, a + da * ia / 255); } } }
    fb_free(&tex); if (hasSide) fb_free(&side); return 0;
}
void orig_gem(float x, float y, float s, float hw, int fret, int kind, int sp, int missed, float alpha, double perf) {
    char nm[64]; float w = hw * 0.4f * s, h = w / 2; uint32_t tint = missed ? RGB(110, 110, 120) : RGB(255, 255, 255);
    if (kind == 2 && !sp) { snprintf(nm, sizeof nm, "sys_Star2D_%s", COLOR[fret]); gfx_sprite *sh = ga_mat(nm); if (sh) { int fw = gfx_sprite_w(sh) / 4, fh = gfx_sprite_h(sh) / 4, fr = (int)(perf / 45) % 16;
        gfx_blit(sh, (fr % 4) * fw, (fr / 4) * fh, fw, fh, x - w * 0.56f, y - h * 1.12f * 0.6f, w * 1.12f, h * 1.12f, tint, alpha); return; } }
    if (sp) snprintf(nm, sizeof nm, kind == 1 ? "sys_Gem2D_StarPower_hammer" : "sys_Gem2D_StarPower"); else snprintf(nm, sizeof nm, kind == 1 ? "sys_Gem2D_%s_hammer" : "sys_Gem2D_%s", COLOR[fret]);
    gfx_sprite *g = ga_mat(nm); if (!g && kind == 1) { snprintf(nm, sizeof nm, "sys_Gem2D_%s", COLOR[fret]); g = ga_mat(nm); }
    blit_fit(g, x - w / 2, y - h * 0.6f, w, h, tint, alpha);
}
void orig_target(float x, float y, float hw, int fret, int held, int popped) {
    char nm[64]; float bw = hw * 0.42f, bh = bw / 2, lift = popped ? bh * 0.4f : 0; gfx_sprite *mid, *head, *lip, *neck;
    if (held && !popped) { snprintf(nm, sizeof nm, "sys_NowBar_Button01_%s_Down", COLOR[fret]); blit_fit(ga_mat(nm), x - bw / 2, y - bh * 0.6f, bw, bh, RGB(255, 255, 255), 1); return; }
    snprintf(nm, sizeof nm, "sys_NowBar_Button01_%s_Mid2", COLOR[fret]); mid = ga_mat(nm); snprintf(nm, sizeof nm, "sys_NowBar_Button01_%s_Lip", COLOR[fret]); lip = ga_mat(nm);
    snprintf(nm, sizeof nm, popped ? "sys_NowBar_Head_%sL" : "sys_NowBar_Head_%s", COLOR[fret]); head = ga_mat(nm); if (!head) { snprintf(nm, sizeof nm, "sys_NowBar_Head_%s", COLOR[fret]); head = ga_mat(nm); }
    blit_fit(mid, x - bw / 2, y - bh * 0.6f, bw, bh, RGB(255, 255, 255), 1);
    if (lift > 0 && (neck = ga_mat("sys_NowBar_Neck01"))) blit_fit(neck, x - bw * 0.17f, y - lift - bh * 0.05f, bw * 0.34f, lift + bh * 0.08f, RGB(255, 255, 255), 1);
    blit_fit(head, x - bw / 2, y - lift - bh * 0.6f, bw, bh, RGB(255, 255, 255), 1);
    blit_fit(lip, x - bw / 2, y - bh * 0.6f, bw, bh, RGB(255, 255, 255), 1);
}
static const int FLAME_ORDER[10] = { 9, 10, 11, 10, 7, 6, 5, 4, 2, 1 };   /* frames of the hit sheet: biggest first, then dying down */
void orig_flame(float x, float y, float hw, float age, int sp) {
    gfx_sprite *sh = sp ? sheet_add("sys_note_hit_blue_01", &hitBlue) : NULL; if (!sh) sh = sheet_add("sys_note_hit01", &hitSheet); if (!sh || age < 0 || age >= 1) return;
    int fr = FLAME_ORDER[(int)(age * 10) > 9 ? 9 : (int)(age * 10)], fw = gfx_sprite_w(sh) / 4, fh = gfx_sprite_h(sh) / 4; float bw = hw * 0.42f, w = bw * 1.15f, h = w * fh / fw;
    gfx_blit(sh, (fr % 4) * fw, (fr / 4) * fh, fw, fh, x - w / 2, y - h * 0.86f, w, h, RGB(255, 255, 255), 1);
}
void orig_hold(float x, float y, float hw, double perf) {
    gfx_sprite *sh = sheet_add("sys_whammy_hold01", &holdSheet); if (!sh) return; int fr = (int)(perf / 40) % 16, fw = gfx_sprite_w(sh) / 4, fh = gfx_sprite_h(sh) / 4; float w = hw * 0.42f * 0.9f;
    gfx_blit(sh, (fr % 4) * fw, (fr / 4) * fh, fw, fh, x - w / 2, y - w * 0.8f, w, w, RGB(255, 255, 255), 1);
}
void orig_fretbar(float x0, float x1, float y, float s, int strong, float alpha) {
    gfx_sprite *b = ga_mat(strong ? "sys_fretbar_large" : "sys_fretbar_medium"); if (!b) return; float h = gfx_sprite_h(b) * 0.16f * s * (strong ? 1.5f : 1); if (h < 1.5f) h = 1.5f;   /* the browser port's 0.48 on a 1280-wide stage, and these are plain white bars: kept thin and soft */
    blit_fit(b, x0, y - h / 2, x1 - x0, h, RGB(255, 255, 255), alpha * (strong ? 0.6f : 0.4f));
}
static gfx_sprite *tail_tex(int fret, int sp, int dead) { char nm[48]; if (dead) return ga_mat("sys_Whammy2D_Dead"); if (sp) { gfx_sprite *t = ga_mat("sys_Whammy2D_StarPower"); if (t) return t; } snprintf(nm, sizeof nm, "sys_Whammy2D_%s", COLOR[fret]); return ga_mat(nm); }
void orig_tail_slice(int fret, int sp, int dead, float cx, float yTop, float yBot, float w) { gfx_sprite *t = tail_tex(fret, sp, dead); if (!t) return; int th = gfx_sprite_h(t); gfx_blit(t, 0, (int)(th * 0.7f), gfx_sprite_w(t), 1, cx - w / 2, yTop, w, yBot - yTop + 0.6f, RGB(255, 255, 255), dead ? 0.7f : 1); }
void orig_tail_cap(int fret, int sp, int dead, float cx, float y, float w) { gfx_sprite *t = tail_tex(fret, sp, dead); if (!t) return; int th = gfx_sprite_h(t); gfx_blit(t, 0, 0, gfx_sprite_w(t), (int)(th * 0.55f), cx - w / 2, y - w * 0.5f, w, w * 0.55f, RGB(255, 255, 255), dead ? 0.7f : 1); }

/* ---- the HUD: the crowd meter with its six star power tubes fanned above, the score box with the multiplier tube
 * seen through its ring and the streak lights, as the game's layout table places them (256 px boxes, here at K) ---- */
#define HUD_K 0.47f
static gfx_sprite *tube[6], *tubeGlow[6], *tubeGlowB[6]; static float tubeX[6], tubeY[6], tubeW[6], tubeH[6]; static int tubesBaked;
static void bake_tubes(void) {
    static const float ROT[6] = { -47.5f, -33, -18.5f, 0, 21, 42 }; tubesBaked = 1; fb_t t, g, gb; int ok = ga_img_fb("hud_rock_tube", &t) == 0, okg = ga_img_fb("hud_rock_tube_glow_full", &g) == 0, okb = ga_img_fb("hud_rock_tube_glow_full_b", &gb) == 0;
    for (int i = 0; i < 6; i++) { int big = i >= 3; float bs = big ? 1 : 0.7f, py = big ? -170 : -160, a = ROT[i] * (float)M_PI / 180; fb_t r;
        tubeX[i] = (128 - py * sinf(a)) * HUD_K; tubeY[i] = (128 + py * cosf(a)) * HUD_K;   /* where the tube's middle lands once turned about the meter's centre */
        if (ok) { fb_rotate(&t, ROT[i], bs * HUD_K, &r); tubeW[i] = (float)r.w; tubeH[i] = (float)r.h; tube[i] = bake(&r, 1); }
        if (okg) { fb_rotate(&g, ROT[i], bs * HUD_K, &r); tubeGlow[i] = bake(&r, 1); } if (okb) { fb_rotate(&gb, ROT[i], bs * HUD_K, &r); tubeGlowB[i] = bake(&r, 1); } }
    if (ok) fb_free(&t); if (okg) fb_free(&g); if (okb) fb_free(&gb);
}
static void img_at(const char *name, float bx, float by, float x, float y, float alpha) { gfx_sprite *s = ga_img(name); if (s) blit_fit(s, bx + x * HUD_K, by + y * HUD_K, gfx_sprite_w(s) * HUD_K, gfx_sprite_h(s) * HUD_K, RGB(255, 255, 255), alpha); }
static void score_digits(float bx, float by, long score) { /* the running score in the game's numeral font, right-aligned in the box's window (SCORE_AT of the browser port) */
    const ga_font *f = ga_font_get(F_SCORE); int sx, sy, sw, sh; float gw, gh, yo; if (!f || ga_glyph(f, '0', &sx, &sy, &sw, &sh, &gw, &gh, &yo) || gh <= 0) return;
    char b[24]; snprintf(b, sizeof b, "%ld", score < 0 ? 0 : score); int n = (int)strlen(b); float cell = fminf(30, 188.0f / n), k = 35 / gh * fminf(1, cell / 30 + 0.08f), x = 219 - n * cell;
    for (int i = 0; i < n; i++, x += cell) { if (ga_glyph(f, b[i], &sx, &sy, &sw, &sh, &gw, &gh, &yo) || sw <= 0) continue; float w = fminf(cell, gw * k);
        gfx_blit(ga_font_sheet(f), sx, sy, sw, sh, bx + (x + (cell - w) / 2) * HUD_K, by + (71 + (yo - gh) * k) * HUD_K, w * HUD_K, gh * k * HUD_K, RGB(255, 255, 255), 1); }
}
void orig_hud(const gh3_run *r, const hud_info *hi, double perf, int W, int H, int mult, int sp, float pulse) {
    char b[80]; float box = 256 * HUD_K, rx = W - 6 - box, ry = H - 4 - box, sx = 6, sy = H - 4 - box; int ready = !sp && r->sp >= 0.5 && !r->autoPlay;
    if (!tubesBaked) bake_tubes();
    /* crowd meter */
    for (int i = 0; i < 6; i++) { float fill = (float)fmin(1, fmax(0, r->sp * 6 - i)); if (tube[i]) gfx_sprite_at(tube[i], rx + tubeX[i] - tubeW[i] / 2, ry + tubeY[i] - tubeH[i] / 2, 1, RGB(255, 255, 255), 1);
        gfx_sprite *gl = (sp || ready) && tubeGlowB[i] ? tubeGlowB[i] : tubeGlow[i]; if (fill > 0 && gl) gfx_sprite_at(gl, rx + tubeX[i] - tubeW[i] / 2, ry + tubeY[i] - tubeH[i] / 2, 1, RGB(255, 255, 255), fill * (ready ? 0.65f + 0.35f * sinf((float)perf / (58 + i * 17) + i * 1.9f) : 1)); }
    double h = r->health > 1 ? 1 : r->health < 0 ? 0 : r->health; int zone = h < 0.33 ? 0 : h < 0.665 ? 1 : 2;
    img_at(zone == 0 ? "hud_rock_bg_red" : zone == 1 ? "hud_rock_bg_yellow" : "hud_rock_bg_green", rx, ry, 0, 0, 1); img_at("hud_rock_lights_all", rx, ry, 0, 0, 1);
    float la = 0.75f + 0.25f * sinf((float)perf / (zone == 0 ? 70 : 240)); if (zone == 0) img_at("hud_rock_lights_red", rx, ry, 0, 0, la); else if (zone == 1) img_at("hud_rock_lights_yellow", rx, ry, 64, 0, la); else img_at("hud_rock_lights_green", rx, ry, 128, 0, la);
    { float ang = (float)(2 * h - 1) * 0.92f, ncx = rx + 132 * HUD_K, ncy = ry + 165 * HUD_K, L = 128 * 0.8f * HUD_K;   /* the needle turns about a point 80 % down it */
        gfx_line(ncx, ncy, ncx + sinf(ang) * L, ncy - cosf(ang) * L, 3, RGB(0x1a, 0x1a, 0x1e)); gfx_line(ncx, ncy, ncx + sinf(ang) * L, ncy - cosf(ang) * L, 1.5f, RGB(0xd8, 0xdc, 0xe4)); }
    img_at("hud_rock_body", rx, ry, 0, 0, 1);
    if (hi->noFail) gfx_text_right(W - 10, (int)ry - 12, 1, "SEM FALHAR", C_MUTED);
    if (h < 0.33 && !hi->noFail) { float k = 0.5f + 0.5f * sinf((float)perf / 90); gfx_rect(0, 0, 6, (float)H, fb_alpha(C_BAD, 0.35f * k)); gfx_rect((float)W - 6, 0, 6, (float)H, fb_alpha(C_BAD, 0.35f * k)); }
    /* score box */
    int base = mult < 1 ? 1 : mult > 4 ? 4 : mult, c = base >= 4 ? 10 : r->streak % 10; snprintf(b, sizeof b, sp ? "hud_score_nixie_%db" : "hud_score_nixie_%da", sp ? base * 2 : base);
    img_at(b, sx, sy, 70, 90, 1); img_at("hud_score_body", sx, sy, 0, 0, 1);
    for (int i = 0; i < 5; i++) img_at(c >= 2 * (i + 1) ? "hud_score_light_2" : c == 2 * i + 1 ? "hud_score_light_1" : "hud_score_light_0", sx, sy, 0, 200 - 30 * i, 1);
    { float age = (float)(perf - r->spAt); gfx_sprite *fl = ga_img("hud_score_flash"); if (fl && age >= 0 && age < 300) { float up = fminf(1, age / 200), down = fmaxf(0, (age - 200) / 100), sz = 64 * (1 + 4 * (up - down)) * HUD_K; gfx_sprite_draw(fl, sx + 128 * HUD_K, sy + 128 * HUD_K, sz / gfx_sprite_w(fl), RGB(255, 255, 255), up - down); } }
    score_digits(sx, sy, (long)r->score);
    snprintf(b, sizeof b, "%d seguidas", r->streak); gfx_text_sh((int)(sx + box) + 6, H - 16, 1, b, r->streak >= 10 ? C_AMP : C_MUTED, C_SHADOW);
    if (ready) { snprintf(b, sizeof b, "STAR POWER: %s", hi->starKeys && *hi->starKeys ? hi->starKeys : "Select"); float k = 0.6f + 0.4f * sinf((float)perf / 110); int tw = gfx_text_w(b, 1) + 16;
        ui_panel((float)W - 10 - tw, ry - 36 - 70, (float)tw, 20, UI_PANEL_SP); gfx_text_sh_right(W - 18, (int)ry - 36 - 65, 1, b, fb_alpha(C_INK, k), C_SHADOW); }
    (void)pulse;
}

/* ================================================================== menus */
void orig_backdrop(double perf, int W, int H) { int screen = A.screen;
    float t = (float)perf / 1000;
    if (screen == SC_MAIN || screen == SC_TITLE) {   /* create_main_menu_backdrop: the hall, its lamps breathing, the eyes in the dark blinking in their own time */
        blit_cover(ga_img("menu_main_bg"), W, H, RGB(255, 255, 255), 1);
        { gfx_sprite *g = ga_img("menu_main_bg_2"); float a = 0.5f + 0.5f * sinf(t * 1.6f); if (g) blit_fit(g, W * 0.26f, 0, (float)H, (float)H, RGB(255, 255, 255), a); }
        gfx_sprite *il = ga_img("menu_main_illustrations"); if (!il) return; int sw = gfx_sprite_w(il), sh = gfx_sprite_h(il); float k = fmaxf((float)W / sw, (float)H / sh); int cx = (int)((sw * k - W) / 2 / k), cy = (int)((sh * k - H) / 2 / k), from = (int)(0.44f * (sw - 2 * cx));
        gfx_blit(il, cx + from, cy, sw - 2 * cx - from, sh - 2 * cy, from * k, 0, W - from * k, (float)H, RGB(255, 255, 255), 1);   /* the guitarist and the right side only: the picture's own logo would sit under the menu */
        /* the eyes: patches of the illustration (128 x 64 on the game's 1280 x 720 stage) drawn back over their own spot, fading in and out; only those on the part shown */
        static const char *EYES[5] = { "menu_main_eyes_bl", "menu_main_eyes_br", "menu_main_eyes_c", "menu_main_eyes_tl", "menu_main_eyes_tr" }; static const float AT[5][3] = { { 93, 676, 1 }, { 1176, 659, 1 }, { 406, 398, 1.5f }, { 271, 215, 1.7f }, { 995, 71, 1 } };
        for (int i = 0; i < 5; i++) { float u = t / AT[i][2] + i * 0.7f, p = u - floorf(u), a = p < 0.5f ? 1 - p * 2 : (p - 0.5f) * 2; a = a * a * (3 - 2 * a);
            float ex = (AT[i][0] * sw / 1280 - cx) * k, ey = (AT[i][1] * sh / 720 - cy) * k, ew = 128.0f * sw / 1280 * k, eh = 64.0f * sh / 720 * k; if (ex - ew / 2 < from * k) continue;
            blit_fit(ga_img(EYES[i]), ex - ew / 2, ey - eh / 2, ew, eh, RGB(255, 255, 255), a); }
        return; }
    if (screen == SC_SETLIST || screen == SC_TOPROCK || screen == SC_STORE) {   /* the set list's wooden table */
        blit_cover(ga_img("setlist_bg_head_676"), W, H, RGB(180, 180, 180), 1); gfx_rect(0, 0, (float)W, (float)H, RGBA(10, 6, 4, 70)); return; }
    blit_cover(ga_img("menu_venue_bg"), W, H, RGB(255, 255, 255), 1);   /* the venue poster wall behind every other menu */
    blit_fit(ga_img("light_overlay_3spots"), 0, 0, (float)W, (float)H, RGB(105, 56, 7), 0.63f);
    gfx_rect(0, 0, (float)W, (float)H, RGBA(0, 0, 0, 105));   /* darker than the game's: the menus' small text sits straight on it */
}
int orig_header(int W, const char *title, const char *subtitle) {
    const ga_font *f = ga_font_get(F_TITLE); gfx_sprite *banner = ga_img("menu_pause_frame_banner"); if (!f || !banner) return -1;
    float bw = 340, bh = 76; blit_fit(banner, W / 2.0f - bw / 2, -6, bw, bh, RGB(255, 255, 255), 1);
    ga_text_fit(f, W / 2.0f, 10, 34, 260, title, RGB(0, 0, 0), 1); ga_text_fit(f, W / 2.0f - 2, 8, 34, 260, title, RGB(245, 226, 190), 1);
    if (subtitle) gfx_text_sh_right(W - 16, 22, 1, subtitle, C_INK, C_SHADOW);
    return 0;
}
int orig_panel(float x, float y, float w, float h, int style) {
    gfx_sprite *tl = ga_img("menu_pause_bg_tile"), *fr1 = ga_img("menu_pause_frame_1"), *fr2 = ga_img("menu_pause_frame_2"), *hl = ga_img("dialog_menu_hilight"); if (!tl) return -1;
    if (style == UI_PANEL_AMBER || style == UI_PANEL_SP) {   /* a chosen row: the game's dark red bar with its flourish */
        uint32_t fill = style == UI_PANEL_SP ? RGBA(18, 55, 80, 225) : RGBA(112, 24, 16, 225), edge = style == UI_PANEL_SP ? RGB(98, 228, 255) : RGB(214, 92, 50);
        gfx_rect(x, y, w, h, fill); gfx_rect(x, y, w, 1, edge); gfx_rect(x, y + h - 1, w, 1, edge);
        if (hl) blit_fit(hl, x + 2, y + h / 2 - h * 0.3f, h * 1.2f, h * 0.6f, style == UI_PANEL_SP ? RGB(150, 238, 255) : RGB(255, 200, 130), 0.9f); return 0; }
    tile(tl, x, y, w, h, 64, style == UI_PANEL_FLAT ? 0.86f : 0.96f);
    if (style == UI_PANEL_FLAT || h < 90 || w > 2.6f * h || !fr1) { gfx_frame(x, y, w, h, 2, style == UI_PANEL_FLAT ? RGBA(160, 34, 22, 170) : C_FRAME); return 0; }
    /* a window (create_pause_menu_frame): the jagged frame's corner piece drawn four times, mirrored into each corner and
     * stretched so the pieces meet in the middle, the flourishes behind them, a dark copy as the shadow */
    float pw = w / 2 + w * 0.10f, ph = h / 2 + h * 0.12f, cx = x + w / 2, cy = y + h / 2; (void)fr2;
    for (int c = 0; c < 4; c++) { int fx = c & 1, fy = c >> 1; float px = fx ? cx : cx - pw, py = fy ? cy : cy - ph;
        gfx_sprite *fl = variant("menu_pause_frame_2", fx, fy, 0); if (fl) blit_fit(fl, fx ? cx : cx - pw * 0.95f, fy ? cy : cy - ph * 1.3f, pw * 0.95f, ph * 1.3f, RGB(255, 255, 255), 1);
        gfx_sprite *p = variant("menu_pause_frame_1", fx, fy, 0); if (!p) continue;
        blit_fit(p, px - 2 + (fx ? 6 : 0), py - 1 + (fy ? 5 : 0), pw, ph, RGB(0, 0, 0), 0.7f); blit_fit(p, px, py, pw, ph, RGB(255, 255, 255), 1); }
    return 0;
}
static int button_glyph(const char *b) { /* the game's button font: Xbox pictures by number */
    if (!strcmp(b, "A")) return 16; if (!strcmp(b, "B")) return 17; if (!strcmp(b, "Y")) return 18; if (!strcmp(b, "X")) return 19; if (!strcmp(b, "↑↓") || !strcmp(b, "← →")) return 23; if (!strcmp(b, "Start")) return 1; return -1;
}
int orig_hints(int y, int W, const char *const *buttons, const char *const *texts, int n) {
    const ga_font *f = ga_font_get(F_TITLE), *bf = ga_font_get(F_BUTTONS); gfx_sprite *body = ga_img("helper_pill_body"), *end = ga_img("helper_pill_end"); if (!f || !body || !end || n <= 0) return -1;
    const float TS = 15, PH = 22, END = 11, GAP = 14; float max = 0, gwid[8]; int glyph[8];
    for (int i = 0; i < n && i < 8; i++) { glyph[i] = bf ? button_glyph(buttons[i]) : -1; gwid[i] = 22; int sx, sy, sw, sh; float gw, gh, yo;
        if (glyph[i] >= 0 && ga_glyph(bf, 0xE000 + glyph[i], &sx, &sy, &sw, &sh, &gw, &gh, &yo) == 0 && gh > 0) gwid[i] = fminf(44, gw * 22 / gh);   /* the button's picture, 22 px tall (the strum bar is wide) */
        float w = ga_text_w(f, texts[i], TS) + (glyph[i] >= 0 ? gwid[i] + 4 : ga_text_w(f, buttons[i], TS) + 10); if (w > max) max = w; }
    float total = (max + END * 2) * n + GAP * (n - 1); if (total > W - 16) max = ((W - 16) - GAP * (n - 1)) / n - END * 2;
    float px = W / 2.0f - total / 2, py = y - 4; uint32_t pill = RGB(180, 180, 180);
    for (int i = 0; i < n && i < 8; i++) {
        blit_fit(body, px, py, max, PH, pill, 1); blit_fit(variant("helper_pill_end", 1, 0, 0), px - END, py, END, PH, pill, 1); blit_fit(end, px + max, py, END, PH, pill, 1);
        float tx = px + (max - (ga_text_w(f, texts[i], TS) + (glyph[i] >= 0 ? gwid[i] + 4 : ga_text_w(f, buttons[i], TS) + 10))) / 2;
        if (glyph[i] >= 0) { int sx, sy, sw, sh; float gw, gh, yo; if (ga_glyph(bf, 0xE000 + glyph[i], &sx, &sy, &sw, &sh, &gw, &gh, &yo) == 0) { float k = fminf(22 / gh, gwid[i] / gw); gfx_blit(ga_font_sheet(bf), sx, sy, sw, sh, tx - 2, py + PH / 2 - gh * k / 2, gw * k, gh * k, RGB(255, 255, 255), 1); } tx += gwid[i] + 4; }
        else { ga_text(f, tx, py + 3, TS, buttons[i], RGB(120, 20, 10)); tx += ga_text_w(f, buttons[i], TS) + 10; }
        ga_text(f, tx, py + 3, TS, texts[i], RGB(0, 0, 0)); px += max + END * 2 + GAP; }
    return 0;
}
int orig_title(int x, int y, int scale, const char *s, uint32_t c, uint32_t edge) {
    const ga_font *f = ga_font_get(F_TITLE); if (!f) return -1; float size = 9.6f * scale, cx = x + gfx_text_w(s, scale) / 2.0f, w = ga_text_w(f, s, size);
    float d = scale > 2 ? 2 : 1; ga_text(f, cx - w / 2 + d, y - size * 0.1f + d, size, s, (edge & 0xFFFFFF) | (c & 0xFF000000)); ga_text(f, cx - w / 2, y - size * 0.1f, size, s, c); return 0;
}
void orig_tick(void) {
    if (SET.theme == 2 || !loaded) return; loaded = 0; tubesBaked = 0;
    for (int i = 0; i < 6; i++) { gfx_free_sprite(tube[i]); gfx_free_sprite(tubeGlow[i]); gfx_free_sprite(tubeGlowB[i]); tube[i] = tubeGlow[i] = tubeGlowB[i] = NULL; }
    gfx_free_sprite(hitSheet); gfx_free_sprite(hitBlue); gfx_free_sprite(holdSheet); hitSheet = hitBlue = holdSheet = NULL;
    for (int i = 0; i < nflips; i++) gfx_free_sprite(flips[i].s); nflips = 0; ga_release();
}
