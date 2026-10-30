/* Movie playback. A worker thread owns the pl_mpeg decoder: it keeps a few pictures decoded ahead in a small queue
 * and the mixer's movie ring about 0.4 s full. The main thread runs the clock (what has been heard of the movie's
 * sound, followed smoothly; the wall clock when there is no sound) and shows the newest picture that is due, through
 * an IYUV streaming texture on the GPU or converted to RGB into the software framebuffer. */
#include "movie.h"
#include "audio.h"
#include "gfx.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"
#pragma GCC diagnostic pop

#define NQ 4                       /* the picture on screen and up to three decoded ahead */
#define AUDIO_AHEAD (44100 * 2 / 5)
typedef struct { double t; uint8_t *y, *u, *v; } slot_t;
static struct {
    plm_t *plm; SDL_Thread *th; SDL_atomic_t head, tail, quit, done, audEnded, audStarted;
    slot_t q[NQ]; int w, h, ys, yh, cs, ch;   /* picture size; plane strides and heights (whole macroblocks) */
    int hasAudio; double audBase, fps;
    int started, shown, hasFrame; double T, lastNow;
    SDL_Texture *tex; int texYuv; uint32_t *rgb; int dx, dy, dw, dh; int open;
} M;

static inline uint32_t clamp8(int v) { return v < 0 ? 0 : v > 255 ? 255 : (uint32_t)v; }
/* BT.601, video range, scaled (nearest) to ow x oh */
static void yuv_to_rgb(const slot_t *q, uint32_t *out, int ow, int oh) {
    for (int y = 0; y < oh; y++) { int sy = oh == M.h ? y : y * M.h / oh; const uint8_t *Y = q->y + (size_t)sy * M.ys, *U = q->u + (size_t)(sy >> 1) * M.cs, *V = q->v + (size_t)(sy >> 1) * M.cs; uint32_t *o = out + (size_t)y * ow;
        for (int x = 0; x < ow; x++) { int sx = ow == M.w ? x : x * M.w / ow, c = (Y[sx] - 16) * 298, d = U[sx >> 1] - 128, e = V[sx >> 1] - 128;
            o[x] = 0xFF000000u | clamp8((c + 409 * e + 128) >> 8) << 16 | clamp8((c - 100 * d - 208 * e + 128) >> 8) << 8 | clamp8((c + 516 * d + 128) >> 8); } }
}
static void sizes(int w, int h, int W, int H) {
    M.w = w; M.h = h; M.ys = (w + 15) & ~15; M.yh = (h + 15) & ~15; M.cs = M.ys / 2; M.ch = M.yh / 2;
    double s = (double)W / w < (double)H / h ? (double)W / w : (double)H / h; M.dw = (int)(w * s + 0.5) & ~1; M.dh = (int)(h * s + 0.5) & ~1; M.dx = (W - M.dw) / 2; M.dy = (H - M.dh) / 2;
    for (int i = 0; i < NQ; i++) { M.q[i].y = malloc((size_t)M.ys * M.yh); M.q[i].u = malloc((size_t)M.cs * M.ch); M.q[i].v = malloc((size_t)M.cs * M.ch); }
}
static void copy_plane(uint8_t *dst, int ds, int dh, const plm_plane_t *p) { int w = (int)p->width < ds ? (int)p->width : ds, h = (int)p->height < dh ? (int)p->height : dh; for (int y = 0; y < h; y++) memcpy(dst + (size_t)y * ds, p->data + (size_t)y * p->width, w); }
static void copy_frame(slot_t *q, const plm_frame_t *f) { q->t = f->time; copy_plane(q->y, M.ys, M.yh, &f->y); copy_plane(q->u, M.cs, M.ch, &f->cb); copy_plane(q->v, M.cs, M.ch, &f->cr); }

static int worker(void *ud) {
    (void)ud; int vidEnd = 0, audEnd = !M.hasAudio;
    while (!SDL_AtomicGet(&M.quit) && !(vidEnd && audEnd)) { int did = 0;
        if (!audEnd && audio_movie_queued() < AUDIO_AHEAD) { plm_samples_t *s = plm_decode_audio(M.plm);
            if (!s) { audEnd = 1; SDL_AtomicSet(&M.audEnded, 1); }
            else { if (!SDL_AtomicGet(&M.audStarted)) { M.audBase = s->time; SDL_AtomicSet(&M.audStarted, 1); }
                int off = 0; while (off < (int)s->count && !SDL_AtomicGet(&M.quit)) { off += audio_movie_push(s->interleaved + off * 2, (int)s->count - off); if (off < (int)s->count) SDL_Delay(5); } }
            did = 1; }
        if (!vidEnd && SDL_AtomicGet(&M.head) - SDL_AtomicGet(&M.tail) < NQ) { plm_frame_t *f = plm_decode_video(M.plm);
            if (!f) vidEnd = 1; else { copy_frame(&M.q[SDL_AtomicGet(&M.head) % NQ], f); SDL_AtomicIncRef(&M.head); }
            did = 1; }
        if (!did) SDL_Delay(2);
    }
    SDL_AtomicSet(&M.done, 1); return 0;
}

void movie_close(void) {
    if (!M.open) return;
    SDL_AtomicSet(&M.quit, 1); if (M.th) SDL_WaitThread(M.th, NULL);
    audio_movie_run(0); audio_movie_start();
    if (M.plm) plm_destroy(M.plm);
    for (int i = 0; i < NQ; i++) { free(M.q[i].y); free(M.q[i].u); free(M.q[i].v); }
    if (M.tex) SDL_DestroyTexture(M.tex); free(M.rgb);
    memset(&M, 0, sizeof M);
}
int movie_open(const char *path, int W, int H) {
    movie_close(); memset(&M, 0, sizeof M);
    M.plm = plm_create_with_filename(path); if (!M.plm) return -1;
    if (!plm_has_headers(M.plm) || plm_get_width(M.plm) <= 0 || plm_get_height(M.plm) <= 0) { plm_destroy(M.plm); M.plm = NULL; return -1; }
    M.open = 1; plm_set_loop(M.plm, 0);
    M.hasAudio = plm_get_num_audio_streams(M.plm) > 0 && audio_has_device() && plm_get_samplerate(M.plm) == 44100;   /* the mixer does not resample */
    plm_set_audio_enabled(M.plm, M.hasAudio);
    M.fps = plm_get_framerate(M.plm); if (M.fps <= 0) M.fps = 30;
    sizes(plm_get_width(M.plm), plm_get_height(M.plm), W, H);
    SDL_Renderer *R = gfx_renderer();
    if (R) { SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
        M.tex = SDL_CreateTexture(R, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, M.w, M.h); M.texYuv = M.tex != NULL;
        if (!M.tex) { M.tex = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, M.w, M.h); M.rgb = malloc((size_t)M.w * M.h * 4); } }
    else M.rgb = malloc((size_t)M.dw * M.dh * 4);
    SDL_Log("movie %s: %dx%d %.2f fps, %s, %s", path, M.w, M.h, M.fps, M.hasAudio ? "sound" : "no sound", M.texYuv ? "IYUV texture" : M.tex ? "RGB texture" : "software");
    audio_movie_start(); M.shown = -1;
    M.th = SDL_CreateThread(worker, "movie", NULL); if (!M.th) { movie_close(); return -1; }
    return 0;
}
int movie_has_audio(void) { return M.hasAudio; }
static void show(const slot_t *q) {
    if (M.texYuv) SDL_UpdateYUVTexture(M.tex, NULL, q->y, M.ys, q->u, M.cs, q->v, M.cs);
    else if (M.tex) { yuv_to_rgb(q, M.rgb, M.w, M.h); SDL_UpdateTexture(M.tex, NULL, M.rgb, M.w * 4); }
    else yuv_to_rgb(q, M.rgb, M.dw, M.dh);
    M.hasFrame = 1;
}
int movie_tick(double now) {
    if (!M.open) return 0;
    int head = SDL_AtomicGet(&M.head), tail = SDL_AtomicGet(&M.tail), done = SDL_AtomicGet(&M.done);
    int audioLeft = M.hasAudio && !(SDL_AtomicGet(&M.audEnded) && audio_movie_queued() == 0);
    if (!M.started) {   /* the clock starts with a picture decoded and some sound queued */
        if (done && head == tail) return 0;
        if (head == tail || (M.hasAudio && !SDL_AtomicGet(&M.audEnded) && audio_movie_queued() < 4410)) return 1;
        M.started = 1; M.lastNow = now; M.T = M.hasAudio && SDL_AtomicGet(&M.audStarted) ? M.audBase : M.q[tail % NQ].t; audio_movie_run(1);
    }
    double T = M.T + (now - M.lastNow) / 1000;
    if (audioLeft && SDL_AtomicGet(&M.audStarted)) { double a = audio_movie_ms();   /* follow the sound: gently, snapping only on a big gap */
        if (a >= 0) { double raw = M.audBase + a / 1000, err = raw - T; T = fabs(err) > 0.12 ? raw : T + err * 0.05; } }
    if (T < M.T) T = M.T; M.T = T; M.lastNow = now;
    while (head - tail >= 2 && M.q[(tail + 1) % NQ].t <= T) tail++;
    SDL_AtomicSet(&M.tail, tail);
    if (head - tail >= 1 && tail != M.shown) { show(&M.q[tail % NQ]); M.shown = tail; }
    if (done && head - tail <= 1 && T >= M.q[tail % NQ].t + 1 / M.fps && !audioLeft) return 0;
    return 1;
}
void movie_draw(int W, int H) {
    gfx_rect(0, 0, (float)W, (float)H, RGB(0, 0, 0));
    if (!M.hasFrame) return;
    if (M.tex) { SDL_Rect d = { M.dx, M.dy, M.dw, M.dh }; SDL_RenderCopy(gfx_renderer(), M.tex, NULL, &d); return; }
    fb_t *fb = gfx_soft_fb(); int x0 = M.dx < 0 ? 0 : M.dx, w = M.dw - (x0 - M.dx); if (x0 + w > fb->w) w = fb->w - x0; if (w <= 0) return;
    for (int y = 0; y < M.dh; y++) { int py = M.dy + y; if (py < 0 || py >= fb->h) continue; memcpy(fb->px + (size_t)py * fb->w + x0, M.rgb + (size_t)y * M.dw + (x0 - M.dx), (size_t)w * 4); }
}

int movie_selftest(const char *path, int W, int H) {
    movie_close(); memset(&M, 0, sizeof M);
    plm_t *p = plm_create_with_filename(path); if (!p || !plm_has_headers(p)) { printf("%s: not an MPEG-1 program stream\n", path); if (p) plm_destroy(p); return -1; }
    int w = plm_get_width(p), h = plm_get_height(p), na = plm_get_num_audio_streams(p), sr = na ? plm_get_samplerate(p) : 0; double fps = plm_get_framerate(p), dur = plm_get_duration(p);
    printf("%s: %dx%d, %.3f fps, %.2f s, %d audio stream(s)%s\n", path, w, h, fps, dur, na, na ? "" : " (silent)");
    if (na) printf("  audio %d Hz%s\n", sr, sr == 44100 ? "" : " (the mixer runs at 44100: this movie would play silent)");
    sizes(w, h, W, H); M.rgb = malloc((size_t)M.dw * M.dh * 4);
    plm_set_audio_enabled(p, 0); int frames = 0, grabbed = 0; double worst = 0, last = 0; Uint64 f = SDL_GetPerformanceFrequency(), t0 = SDL_GetPerformanceCounter(), tp = t0; plm_frame_t *fr;
    while ((fr = plm_decode_video(p))) { Uint64 tn = SDL_GetPerformanceCounter(); double ms = (double)(tn - tp) * 1000 / f; if (ms > worst) worst = ms; frames++; last = fr->time;
        if (!grabbed && fr->time >= dur / 2) { copy_frame(&M.q[0], fr); grabbed = 1; } tp = SDL_GetPerformanceCounter(); }
    double vms = (double)(SDL_GetPerformanceCounter() - t0) * 1000 / f; plm_destroy(p);
    printf("  video: %d frames (last at %.2f s), %.2f ms/frame (worst %.1f), %.0fx realtime\n", frames, last, frames ? vms / frames : 0, worst, frames && vms > 0 ? last * 1000 / vms : 0);
    if (na) { p = plm_create_with_filename(path); plm_set_video_enabled(p, 0); plm_samples_t *s; long n = 0; float peak = 0; t0 = SDL_GetPerformanceCounter();
        while ((s = plm_decode_audio(p))) { n += s->count; for (unsigned i = 0; i < s->count * 2; i++) { float v = fabsf(s->interleaved[i]); if (v > peak) peak = v; } }
        double ams = (double)(SDL_GetPerformanceCounter() - t0) * 1000 / f; plm_destroy(p);
        printf("  audio: %ld samples per channel (%.2f s), peak %.2f, decoded in %.0f ms (%.0fx realtime)\n", n, sr ? (double)n / sr : 0, peak, ams, ams > 0 && sr ? n * 1000.0 / sr / ams : 0); }
    if (grabbed) { t0 = SDL_GetPerformanceCounter(); for (int i = 0; i < 10; i++) yuv_to_rgb(&M.q[0], M.rgb, M.dw, M.dh); double cms = (double)(SDL_GetPerformanceCounter() - t0) * 1000 / f / 10;
        printf("  software YUV->RGB to %dx%d: %.2f ms/frame\n", M.dw, M.dh, cms); M.hasFrame = 1; movie_draw(W, H); }
    for (int i = 0; i < NQ; i++) { free(M.q[i].y); free(M.q[i].u); free(M.q[i].v); } free(M.rgb); memset(&M, 0, sizeof M);
    return frames > 0 ? 0 : -1;
}
