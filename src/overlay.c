/* Overlay movies with transparency. The file is an ordinary MPEG-1 program stream (pl_mpeg) whose picture is twice as
 * tall as what is shown: the colour premultiplied by its alpha on top, the alpha as luma below (16..235 = 0..255).
 * One decoder keeps both halves in step and costs about 1.3x a 640x360 cutscene at the 448x336 tools/convert_overlays.py
 * writes (zlib'd RGBA frames would be 10x the size and slower to inflate).
 *   GPU: one IYUV texture, drawn twice with custom blend modes: the alpha half (its chroma set to grey) as
 *        dst = dst * (1 - a), then the colour half added, dst = dst + c. That is premultiplied "over", with no
 *        conversion on the CPU. A renderer without custom blend modes gets the frame converted to straight-alpha ARGB.
 *   Software: both halves read per pixel into the framebuffer.
 * A worker thread decodes a few frames ahead (overlay_sync: on the caller's thread, for headless checks). */
#include "overlay.h"
#include "app.h"
#include "gfx.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pl_mpeg.h"

#define NQ 3
typedef struct { double t; uint8_t *y, *u, *v; } oslot_t;
static struct {
    plm_t *plm; SDL_Thread *th; SDL_atomic_t head, tail, quit, done;
    oslot_t q[NQ]; int w, h, hh, ys, cs, ch;          /* picture size, the shown half's height, plane strides, chroma height */
    double at, fps, last; int shown, open, ended;
    SDL_Texture *tex; int mode; uint32_t *argb;       /* mode 1: IYUV + custom blends, 2: straight ARGB texture, 0: software */
    SDL_BlendMode mul, add;
} O;
static int syncMode;
void overlay_sync(int on) { syncMode = on; }

static void copy_plane(uint8_t *dst, int ds, int dh, const plm_plane_t *p) { int w = (int)p->width < ds ? (int)p->width : ds, h = (int)p->height < dh ? (int)p->height : dh; for (int y = 0; y < h; y++) memcpy(dst + (size_t)y * ds, p->data + (size_t)y * p->width, w); }
static int decode_one(void) {   /* one frame into the queue: 0 at the end */
    plm_frame_t *f = plm_decode_video(O.plm); if (!f) return 0;
    oslot_t *q = &O.q[SDL_AtomicGet(&O.head) % NQ]; q->t = f->time;
    copy_plane(q->y, O.ys, O.h, &f->y); copy_plane(q->u, O.cs, O.ch, &f->cb); copy_plane(q->v, O.cs, O.ch, &f->cr);
    size_t c0 = (size_t)(O.hh / 2) * O.cs, cn = (size_t)O.ch * O.cs - c0; memset(q->u + c0, 128, cn); memset(q->v + c0, 128, cn);   /* the alpha half: grey, exactly */
    O.last = f->time; SDL_AtomicIncRef(&O.head); return 1;
}
static int worker(void *ud) {
    (void)ud;
    while (!SDL_AtomicGet(&O.quit)) {
        if (SDL_AtomicGet(&O.head) - SDL_AtomicGet(&O.tail) < NQ) { if (!decode_one()) break; } else SDL_Delay(2); }
    SDL_AtomicSet(&O.done, 1); return 0;
}

void overlay_close(void) {
    if (!O.open) return;
    SDL_AtomicSet(&O.quit, 1); if (O.th) SDL_WaitThread(O.th, NULL);
    if (O.plm) plm_destroy(O.plm);
    for (int i = 0; i < NQ; i++) { free(O.q[i].y); free(O.q[i].u); free(O.q[i].v); }
    if (O.tex) SDL_DestroyTexture(O.tex); free(O.argb);
    memset(&O, 0, sizeof O);
}
int overlay_play(const char *name, double now) {
    overlay_close(); char nm[96]; snprintf(nm, sizeof nm, "%s.alpha.mpg", name); const char *path = app_data_path("movies/r36s", nm); if (!path) return -1;
    O.plm = plm_create_with_filename(path); if (!O.plm) return -1;
    int w = plm_get_width(O.plm), h = plm_get_height(O.plm);
    if (!plm_has_headers(O.plm) || w <= 0 || h < 32 || h % 4) { plm_destroy(O.plm); O.plm = NULL; return -1; }
    O.open = 1; plm_set_loop(O.plm, 0); plm_set_audio_enabled(O.plm, 0);
    O.w = w; O.h = (h + 15) & ~15; O.hh = h / 2; O.ys = (w + 15) & ~15; O.cs = O.ys / 2; O.ch = O.h / 2; O.at = now; O.shown = -1;
    O.fps = plm_get_framerate(O.plm); if (O.fps <= 0) O.fps = 29.97;
    for (int i = 0; i < NQ; i++) { O.q[i].y = malloc((size_t)O.ys * O.h); O.q[i].u = malloc((size_t)O.cs * O.ch); O.q[i].v = malloc((size_t)O.cs * O.ch); }
    SDL_Renderer *R = gfx_renderer();
    if (R) {
        SDL_SetYUVConversionMode(SDL_YUV_CONVERSION_BT601);   /* (automatic would pick BT.709 for a picture this tall; the cutscenes are BT.601 too) */
        O.mul = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE_MINUS_SRC_COLOR, SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD);
        O.add = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
        O.tex = SDL_CreateTexture(R, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, O.ys, O.h);
        if (O.tex && !SDL_SetTextureBlendMode(O.tex, O.mul) && !SDL_SetTextureBlendMode(O.tex, O.add)) O.mode = 1;
        else { if (O.tex) SDL_DestroyTexture(O.tex); O.tex = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, O.w, O.hh);
            if (O.tex) { SDL_SetTextureBlendMode(O.tex, SDL_BLENDMODE_BLEND); O.argb = malloc((size_t)O.w * O.hh * 4); O.mode = 2; } } }
    SDL_Log("overlay %s: %dx%d (shown %dx%d) %.2f fps, %s", path, w, h, w, O.hh, O.fps, O.mode == 1 ? "IYUV texture, two blends" : O.mode == 2 ? "ARGB texture" : "software");
    if (!syncMode) { O.th = SDL_CreateThread(worker, "overlay", NULL); if (!O.th) { overlay_close(); return -1; } }
    return 0;
}

/* one pixel of the shown picture: premultiplied colour and alpha, 0..255 */
static inline int clamp8(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
static inline void pixel(const oslot_t *q, int x, int y, int *r, int *g, int *b, int *a) {
    *a = clamp8(((q->y[(size_t)(y + O.hh) * O.ys + x] - 16) * 298 + 128) >> 8);
    int c = (q->y[(size_t)y * O.ys + x] - 16) * 298, d = q->u[(size_t)(y >> 1) * O.cs + (x >> 1)] - 128, e = q->v[(size_t)(y >> 1) * O.cs + (x >> 1)] - 128;
    *r = clamp8((c + 409 * e + 128) >> 8); *g = clamp8((c - 100 * d - 208 * e + 128) >> 8); *b = clamp8((c + 516 * d + 128) >> 8);
    if (*r > *a) *r = *a; if (*g > *a) *g = *a; if (*b > *a) *b = *a;   /* premultiplied: never brighter than its alpha (the codec's noise) */
}
static void upload(const oslot_t *q) {
    if (O.mode == 1) { SDL_UpdateYUVTexture(O.tex, NULL, q->y, O.ys, q->u, O.cs, q->v, O.cs); return; }
    if (O.mode != 2) return;
    for (int y = 0; y < O.hh; y++) for (int x = 0; x < O.w; x++) { int r, g, b, a; pixel(q, x, y, &r, &g, &b, &a);
        O.argb[(size_t)y * O.w + x] = a ? (uint32_t)a << 24 | (uint32_t)(r * 255 / a) << 16 | (uint32_t)(g * 255 / a) << 8 | (uint32_t)(b * 255 / a) : 0; }
    SDL_UpdateTexture(O.tex, NULL, O.argb, O.w * 4);
}
int overlay_draw(double now, int W, int H) {
    if (!O.open || O.ended) return 0;
    double T = (now - O.at) / 1000;
    int head, tail = SDL_AtomicGet(&O.tail), done;
    for (;;) { head = SDL_AtomicGet(&O.head); done = SDL_AtomicGet(&O.done);
        while (head - tail >= 2 && O.q[(tail + 1) % NQ].t <= T) tail++;
        SDL_AtomicSet(&O.tail, tail);
        if (!syncMode || done || head - tail >= 2) break;   /* headless: decoded on until the frame after the one due */
        if (!decode_one()) SDL_AtomicSet(&O.done, 1); }
    if (head == tail) { if (done) O.ended = 1; return !O.ended; }   /* not decoded yet (or nothing at all) */
    const oslot_t *q = &O.q[tail % NQ];
    if (done && head - tail <= 1 && T >= q->t + 1 / O.fps) { O.ended = 1; return 0; }   /* its last frame has had its time */
    if (q->t > T + 0.5 / O.fps) return 1;   /* the first frame is not due yet */
    if (tail != O.shown) { upload(q); O.shown = tail; }
    /* cover the screen, as the game's 1280x720 stage does */
    double s = (double)W / O.w > (double)H / O.hh ? (double)W / O.w : (double)H / O.hh; int dw = (int)(O.w * s + 0.5), dh = (int)(O.hh * s + 0.5), dx = (W - dw) / 2, dy = (H - dh) / 2;
    SDL_Renderer *R = gfx_renderer();
    if (O.mode == 1) { SDL_Rect sa = { 0, O.hh, O.w, O.hh }, sc = { 0, 0, O.w, O.hh }, d = { dx, dy, dw, dh };
        SDL_SetTextureBlendMode(O.tex, O.mul); SDL_RenderCopy(R, O.tex, &sa, &d); SDL_SetTextureBlendMode(O.tex, O.add); SDL_RenderCopy(R, O.tex, &sc, &d); return 1; }
    if (O.mode == 2) { SDL_Rect d = { dx, dy, dw, dh }; SDL_RenderCopy(R, O.tex, NULL, &d); return 1; }
    fb_t *fb = gfx_soft_fb();
    for (int py = dy < 0 ? 0 : dy; py < dy + dh && py < fb->h; py++) { int sy = (int)((py - dy) / s); if (sy >= O.hh) sy = O.hh - 1; uint32_t *o = fb->px + (size_t)py * fb->w;
        for (int px = dx < 0 ? 0 : dx; px < dx + dw && px < fb->w; px++) { int sx = (int)((px - dx) / s), r, g, b, a; if (sx >= O.w) sx = O.w - 1;
            pixel(q, sx, sy, &r, &g, &b, &a); if (a < 3) continue; uint32_t c = o[px]; int k = 255 - a;
            o[px] = 0xFF000000u | (uint32_t)(r + (((c >> 16) & 255) * k + 127) / 255) << 16 | (uint32_t)(g + (((c >> 8) & 255) * k + 127) / 255) << 8 | (uint32_t)(b + ((c & 255) * k + 127) / 255); } }
    return 1;
}
