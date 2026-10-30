#include "audio.h"
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#include "minimp3.h"
#include "stretch.h"
#include <SDL.h>
#include <string.h>
#include <math.h>

typedef struct {
    mp3dec_t dec; const uint8_t *data; size_t size, pos; int on, ended;
    short pcm[MINIMP3_MAX_SAMPLES_PER_FRAME]; int pcmLen, pcmPos, channels, hz;
    float level, gain, target; double durationMs;
    int64_t produced; /* frames handed to the mixer since play */
} stream_t;

static SDL_AudioDeviceID dev; static SDL_AudioSpec spec; static stream_t S[BUS_N];
static float master = 1.0f, fadeStep = 0, fadeGain = 1, sfxGain = 0.8f, busGain[BUS_N] = { 1, 1, 1 };
static int running = 0, paused = 0, preview = 0, loop = 0;
static int64_t mixed = 0;          /* frames mixed since play (negative start = lead-in) */
static double fromMs = 0, leadMs = 0;
static Uint64 cbTicks = 0; static int64_t cbMixed = 0; static SDL_SpinLock lock = 0;
static double est = 0, estAt = 0; static int estOn = 0; static Uint32 playTicks = 0;
static int64_t metroFrames = 0;    /* > 0: the calibration metronome clicks every metroFrames of song time */
/* practice below full speed: the song clock runs at `rate` of real time and the song buses go through the stretcher
 * (pitch kept): the player's track as one stereo pair (its miss mute, volume and whammy are applied after, in real
 * time), the band and the other track mixed as the second pair */
static double rate = 1, rateNext = 1; static int stretching = 0; static stretch_t ST;

/* ---- the whammy bend: the player track goes through a two-tap delay-line pitch shifter (two read heads drifting
 * through a 40 ms window, crossfaded with triangle windows that always sum to 1). The bend is a level 0..1 set by the
 * game while a sustain is held; a full bend lowers the pitch by BEND_SEMIS, like the whammy of the PS2 game. */
#define BEND_SEMIS 1.5f
#define PS_N 4096
#define PS_W 1764.0f
static float psL[PS_N], psR[PS_N], psPhase = 0, bendTarget = 0, bendCur = 0, bendMix = 0, bendRatio = 1; static int psW = 0;
static inline float ps_read(const float *buf, float d) { float pos = (float)psW - d; int i = (int)floorf(pos); float fr = pos - i; float a = buf[i & (PS_N - 1)], b = buf[(i + 1) & (PS_N - 1)]; return a + (b - a) * fr; }
static inline void ps_process(float *l, float *r) {
    psL[psW & (PS_N - 1)] = *l; psR[psW & (PS_N - 1)] = *r;
    bendMix += ((bendCur > 0.01f ? 1.0f : 0.0f) - bendMix) * 0.002f;
    if (bendMix > 0.001f) {
        psPhase += (1 - bendRatio) / PS_W; if (psPhase >= 1) psPhase -= 1; if (psPhase < 0) psPhase += 1;
        float p2 = psPhase + 0.5f; if (p2 >= 1) p2 -= 1;
        float g1 = 1 - fabsf(2 * psPhase - 1), g2 = 1 - g1, d1 = psPhase * PS_W + 2, d2 = p2 * PS_W + 2;
        float wl = g1 * ps_read(psL, d1) + g2 * ps_read(psL, d2), wr = g1 * ps_read(psR, d1) + g2 * ps_read(psR, d2);
        *l += (wl - *l) * bendMix; *r += (wr - *r) * bendMix;
    } else psPhase = 0;
    psW++;
}
void audio_set_bend(double amount) { bendTarget = (float)(amount < 0 ? 0 : amount > 1 ? 1 : amount); }

/* ---- synthesized effects: a few voices, each a small score of (frequency, length, wave) steps ---- */
#define VOICES 6
typedef struct { int kind, on; int t; float lp; unsigned noise; } voice_t;
static voice_t voices[VOICES];
typedef struct { float hz, ms; int wave; } step_t;          /* wave 0 square, 1 triangle, 2 sine, 3 noise, 4 sweep (to next step's hz) */
#define STEPS 6
static const step_t SCORE[SFX_N][STEPS] = {
    [SFX_MOVE] = { {1100, 28, 0} },
    [SFX_OK] = { {880, 45, 0}, {1320, 70, 0} },
    [SFX_BACK] = { {660, 45, 0}, {440, 70, 0} },
    [SFX_BAD] = { {0, 160, 3} },
    [SFX_BAD_BASS] = { {0, 220, 3} },
    [SFX_READY] = { {523, 60, 1}, {659, 60, 1}, {784, 60, 1}, {1047, 140, 1} },
    [SFX_STAR_ON] = { {300, 320, 4}, {1800, 120, 2} },
    [SFX_STAR_OFF] = { {1200, 260, 4}, {200, 80, 2} },
    [SFX_STREAK] = { {1760, 90, 2}, {2349, 220, 2} },
    [SFX_DING] = { {1568, 240, 2} },
    [SFX_FANFARE] = { {523, 90, 0}, {659, 90, 0}, {784, 90, 0}, {1047, 260, 0} },
    [SFX_TICK] = { {880, 35, 0} },
    [SFX_GO] = { {1320, 180, 0} },
    [SFX_FAIL] = { {392, 180, 0}, {311, 180, 0}, {262, 360, 0} },
    [SFX_LOSE] = { {440, 60, 0}, {330, 90, 0} },
};
static float voice_sample(voice_t *v) {
    const step_t *sc = SCORE[v->kind]; float t = v->t / 44100.0f * 1000; int i = 0; float start = 0;
    while (i < STEPS && sc[i].ms > 0 && t >= start + sc[i].ms) { start += sc[i].ms; i++; }
    if (i >= STEPS || sc[i].ms <= 0) { v->on = 0; return 0; }
    float k = (t - start) / sc[i].ms, total = 0; for (int j = 0; j < STEPS && sc[j].ms > 0; j++) total += sc[j].ms;
    float env = 1 - t / total; env *= env; if (k < 0.05f) env *= k / 0.05f;    /* a short attack, a squared decay over the whole effect */
    float hz = sc[i].hz; if (sc[i].wave == 4) { float nx = i + 1 < STEPS && sc[i + 1].ms > 0 ? sc[i + 1].hz : hz; hz = hz + (nx - hz) * k; }
    float ph = fmodf(v->t * hz / 44100.0f, 1.0f), s;
    switch (sc[i].wave) {
    case 0: s = ph < 0.5f ? 0.45f : -0.45f; break;
    case 1: s = (ph < 0.5f ? ph * 4 - 1 : 3 - ph * 4) * 0.7f; break;
    case 3: { v->noise = v->noise * 1664525u + 1013904223u; float n = ((v->noise >> 9) & 0xFFFF) / 32768.0f - 1; v->lp += (n - v->lp) * (v->kind == SFX_BAD_BASS ? 0.08f : 0.25f); s = v->lp * 1.4f; break; }
    default: s = sinf(ph * 2 * (float)M_PI) * 0.6f; break;
    }
    v->t++; return s * env;
}
static int pull(stream_t *s) { /* refill s->pcm; returns 0 at end */
    while (s->pcmPos >= s->pcmLen) {
        if (s->ended || s->pos >= s->size) { if (loop && s->size) { mp3dec_init(&s->dec); s->pos = 0; s->ended = 0; continue; } s->ended = 1; return 0; }
        mp3dec_frame_info_t info; int n = mp3dec_decode_frame(&s->dec, s->data + s->pos, (int)(s->size - s->pos), s->pcm, &info);
        if (info.frame_bytes <= 0) { s->ended = 1; return 0; }
        s->pos += info.frame_bytes;
        if (n > 0) { s->pcmLen = n * info.channels; s->pcmPos = 0; s->channels = info.channels; s->hz = info.hz; }
    }
    return 1;
}
static void st_src(void *ud, float *f) { /* the next frame of song for the stretcher */
    (void)ud; f[0] = f[1] = f[2] = f[3] = 0;
    for (int b = 0; b < BUS_N; b++) { stream_t *s = &S[b]; if (!s->on || !pull(s)) continue;
        float sl, sr; if (s->channels >= 2) { sl = s->pcm[s->pcmPos] * (1.0f / 32768); sr = s->pcm[s->pcmPos + 1] * (1.0f / 32768); s->pcmPos += 2; } else { sl = sr = s->pcm[s->pcmPos++] * (1.0f / 32768); }
        s->produced++;
        if (b == BUS_PLAYER) { f[0] = sl; f[1] = sr; } else { s->gain += (s->target - s->gain) * 0.002f; float g = s->level * s->gain * busGain[b]; f[2] += sl * g; f[3] += sr * g; } }
}
static void mix_stretched(float *l, float *r) {
    float f[ST_CH]; stretch_frame(&ST, f, st_src, NULL); stream_t *s = &S[BUS_PLAYER];
    if (s->on) { s->gain += (s->target - s->gain) * 0.002f; float sl = f[0], sr = f[1]; ps_process(&sl, &sr); float g = s->level * s->gain * busGain[BUS_PLAYER]; *l += sl * g; *r += sr * g; }
    *l += f[2]; *r += f[3];
}
static void voice_start(int kind) {
    int slot = -1; for (int v = 0; v < VOICES; v++) if (!voices[v].on) { slot = v; break; }
    if (slot < 0) { int oldest = 0; for (int v = 1; v < VOICES; v++) if (voices[v].t > voices[oldest].t) oldest = v; slot = oldest; }
    voices[slot].kind = kind; voices[slot].on = 1; voices[slot].t = 0; voices[slot].lp = 0; voices[slot].noise = (unsigned)SDL_GetTicks() * 2654435761u + (unsigned)slot;
}
static void callback(void *ud, Uint8 *out, int len) {
    (void)ud; float *o = (float *)out; int frames = len / (int)(sizeof(float) * 2);
    bendCur += (bendTarget - bendCur) * 0.35f; bendRatio = powf(2.0f, -BEND_SEMIS * bendCur / 12.0f);
    SDL_AtomicLock(&lock); cbTicks = SDL_GetPerformanceCounter(); cbMixed = mixed; SDL_AtomicUnlock(&lock);
    memset(out, 0, len);
    for (int i = 0; i < frames; i++) {
        float l = 0, r = 0;
        if (running && !paused) {
            if (mixed >= 0 && stretching) mix_stretched(&l, &r);
            else if (mixed >= 0) for (int b = 0; b < BUS_N; b++) { stream_t *s = &S[b]; if (!s->on) continue;
                s->gain += (s->target - s->gain) * 0.002f;
                if (!pull(s)) { if (preview && !loop && b == BUS_PLAYER) { running = 0; preview = 0; } continue; }
                float sl, sr; if (s->channels >= 2) { sl = s->pcm[s->pcmPos] * (1.0f / 32768); sr = s->pcm[s->pcmPos + 1] * (1.0f / 32768); s->pcmPos += 2; } else { sl = sr = s->pcm[s->pcmPos++] * (1.0f / 32768); }
                if (b == BUS_PLAYER && !preview) ps_process(&sl, &sr);
                float g = s->level * s->gain * busGain[preview ? BUS_BAND : b]; l += sl * g; r += sr * g; s->produced++; }
            if (fadeStep) { fadeGain -= fadeStep; if (fadeGain <= 0) { fadeGain = 0; if (preview) { running = 0; preview = 0; fadeStep = 0; } } }
            float g = master * fadeGain; l *= g; r *= g;
            if (metroFrames > 0 && mixed >= 0 && mixed % metroFrames == 0) voice_start((mixed / metroFrames) % 4 == 0 ? SFX_GO : SFX_TICK);
            mixed++;
        }
        float fx = 0; for (int v = 0; v < VOICES; v++) if (voices[v].on) fx += voice_sample(&voices[v]);
        fx *= sfxGain * master; l += fx; r += fx;
        o[i * 2] = l > 1 ? 1 : l < -1 ? -1 : l; o[i * 2 + 1] = r > 1 ? 1 : r < -1 ? -1 : r;
    }
}
int audio_open(void) {
    SDL_AudioSpec want; SDL_zero(want); want.freq = 44100; want.format = AUDIO_F32SYS; want.channels = 2; want.samples = 512; want.callback = callback;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &spec, 0);
    if (!dev) { SDL_Log("audio: %s", SDL_GetError()); return -1; }
    SDL_PauseAudioDevice(dev, 0); return 0;
}
void audio_close(void) { if (dev) SDL_CloseAudioDevice(dev); dev = 0; }
int audio_has_device(void) { return dev != 0; }
static void skip_to(stream_t *s, double ms) { /* moves to a position (a start offset): whole frames are only parsed, the rest decoded */
    int64_t want = (int64_t)(ms / 1000.0 * 44100); int64_t have = 0;
    while (s->pos < s->size) { mp3dec_frame_info_t info; int n = mp3dec_decode_frame(&s->dec, s->data + s->pos, (int)(s->size - s->pos), NULL, &info);
        if (info.frame_bytes <= 0 || have + n + 2304 > want) break; s->pos += info.frame_bytes; have += n; }
    mp3dec_init(&s->dec);
    while (have < want && pull(s)) { int64_t left = (s->pcmLen - s->pcmPos) / (s->channels >= 2 ? 2 : 1); int64_t take = want - have < left ? want - have : left; s->pcmPos += (int)take * (s->channels >= 2 ? 2 : 1); have += take; }
}
void audio_set_stream(int bus, const uint8_t *mp3, size_t size, double levelDb) {
    if (dev) SDL_LockAudioDevice(dev);
    stream_t *s = &S[bus]; memset(s, 0, sizeof *s); mp3dec_init(&s->dec); s->data = mp3; s->size = size; s->on = mp3 && size > 0;
    s->level = (float)pow(10, levelDb / 20); s->gain = s->target = 1;
    if (dev) SDL_UnlockAudioDevice(dev);
}
void audio_clear(void) { if (dev) SDL_LockAudioDevice(dev); running = 0; stretching = 0; rate = 1; preview = 0; loop = 0; metroFrames = 0; bendTarget = bendCur = bendMix = 0; for (int b = 0; b < BUS_N; b++) S[b].on = 0; if (dev) SDL_UnlockAudioDevice(dev); }
void audio_play(double from, double lead) {
    if (dev) SDL_LockAudioDevice(dev);
    rate = rateNext; rateNext = 1; stretching = rate < 0.999; if (stretching) stretch_init(&ST, rate); else rate = 1;
    fromMs = from; leadMs = lead; mixed = -(int64_t)(lead / rate / 1000.0 * 44100); fadeStep = 0; fadeGain = 1; paused = 0; preview = 0; loop = 0; metroFrames = 0; bendTarget = bendCur = bendMix = 0;
    for (int b = 0; b < BUS_N; b++) { stream_t *s = &S[b]; if (!s->on) continue; mp3dec_init(&s->dec); s->pos = 0; s->pcmLen = s->pcmPos = 0; s->ended = 0; s->produced = 0; s->gain = s->target = 1; if (from > 0) skip_to(s, from); }
    estOn = 0; running = 1; cbTicks = SDL_GetPerformanceCounter(); cbMixed = mixed; playTicks = SDL_GetTicks();
    if (dev) SDL_UnlockAudioDevice(dev);
}
void audio_preview(const uint8_t *mp3, size_t size, double levelDb, int looping) {
    if (dev) SDL_LockAudioDevice(dev);
    for (int b = 0; b < BUS_N; b++) S[b].on = 0;
    stream_t *s = &S[BUS_PLAYER]; memset(s, 0, sizeof *s); mp3dec_init(&s->dec); s->data = mp3; s->size = size; s->on = mp3 && size > 0;
    s->level = (float)pow(10, levelDb / 20); s->gain = 0; s->target = 1;
    rate = 1; stretching = 0; fromMs = 0; leadMs = 0; mixed = 0; fadeStep = 0; fadeGain = 1; paused = 0; preview = 1; loop = looping; running = s->on; estOn = 0;
    if (dev) SDL_UnlockAudioDevice(dev);
}
void audio_preview_stop(double sec) { if (dev) SDL_LockAudioDevice(dev); if (preview && running) { if (sec <= 0) { running = 0; preview = 0; } else fadeStep = (float)(1.0 / (sec * 44100)); } if (dev) SDL_UnlockAudioDevice(dev); }
int audio_preview_on(void) { return preview && running; }
void audio_sfx(int kind) {
    if (kind < 0 || kind >= SFX_N || !dev) return;
    SDL_LockAudioDevice(dev); voice_start(kind); SDL_UnlockAudioDevice(dev);
}
void audio_metronome(double periodMs, double lead) {
    if (dev) SDL_LockAudioDevice(dev);
    for (int b = 0; b < BUS_N; b++) S[b].on = 0;
    rate = 1; stretching = 0; fromMs = 0; leadMs = lead; mixed = -(int64_t)(lead / 1000.0 * 44100); fadeStep = 0; fadeGain = 1; paused = 0; preview = 0; loop = 0;
    metroFrames = (int64_t)(periodMs / 1000.0 * 44100 + 0.5); estOn = 0; running = 1; cbTicks = SDL_GetPerformanceCounter(); cbMixed = mixed; playTicks = SDL_GetTicks();
    if (dev) SDL_UnlockAudioDevice(dev);
}
void audio_set_sfx_gain(double g) { sfxGain = (float)g; }
void audio_selftest(void) { /* decodes every loaded stream to the end and reports what came out (no device needed) */
    for (int b = 0; b < BUS_N; b++) { stream_t *s = &S[b]; if (!s->on) { printf("bus %d: empty\n", b); continue; }
        mp3dec_init(&s->dec); s->pos = 0; s->pcmLen = s->pcmPos = 0; s->ended = 0; long frames = 0; int peak = 0; Uint32 t0 = SDL_GetTicks();
        while (pull(s)) { for (int i = s->pcmPos; i < s->pcmLen; i++) { int v = s->pcm[i] < 0 ? -s->pcm[i] : s->pcm[i]; if (v > peak) peak = v; } frames += (s->pcmLen - s->pcmPos) / (s->channels >= 2 ? 2 : 1); s->pcmPos = s->pcmLen; }
        Uint32 ms = SDL_GetTicks() - t0;
        printf("bus %d: %ld frames (%.1f s at %d Hz, %d ch), peak %d/32767, level %.2f, decoded in %u ms (%.0fx realtime)\n", b, frames, frames / (double)(s->hz ? s->hz : 44100), s->hz, s->channels, peak, s->level, ms, ms ? frames / (double)(s->hz ? s->hz : 44100) * 1000 / ms : 0);
        mp3dec_init(&s->dec); s->pos = 0; s->pcmLen = s->pcmPos = 0; s->ended = 0; }
}
void audio_stop(void) { if (dev) SDL_LockAudioDevice(dev); running = 0; preview = 0; metroFrames = 0; if (dev) SDL_UnlockAudioDevice(dev); }
void audio_pause(int p) { if (dev) SDL_LockAudioDevice(dev); paused = p; estOn = 0; if (dev) SDL_UnlockAudioDevice(dev); }
int audio_running(void) { return running && !paused; }
void audio_mute(int bus, int m) { S[bus].target = m ? 0.06f : 1.0f; }
void audio_set_master(double g) { master = (float)g; }
void audio_set_bus_gain(int bus, double g) { if (bus >= 0 && bus < BUS_N) busGain[bus] = (float)g; }
void audio_fade_out(double sec) { if (sec <= 0) sec = 0.01; fadeStep = (float)(1.0 / (sec * 44100)); }
double audio_duration_ms(void) { double d = 0; for (int b = 0; b < BUS_N; b++) if (S[b].on) { /* MP3 CBR guess: 128 kbps */ double est = S[b].size / 16000.0 * 1000; if (est > d) d = est; } return d; }
double audio_song_ms(void) {
    if (!running) return fromMs - leadMs;
    if (!dev) return fromMs - leadMs + (paused ? 0 : (double)(SDL_GetTicks() - playTicks) * rate); /* no sound card: the wall clock keeps the game going */
    SDL_AtomicLock(&lock); Uint64 ct = cbTicks; int64_t cm = cbMixed; SDL_AtomicUnlock(&lock);
    double now = (double)SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency() * 1000;
    double cbAt = (double)ct / SDL_GetPerformanceFrequency() * 1000;
    double heard = (double)(cm - spec.samples) / 44100.0 * 1000; /* the buffer handed over at the callback is heard about one buffer later */
    double raw = fromMs + (heard + (paused ? 0 : now - cbAt)) * rate;   /* below full speed a real ms is `rate` ms of song */
    /* the device hands buffers over in bursts, so the raw position jitters by a buffer or more: the clock returned
     * follows the wall clock and is pulled gently toward the audio, never runs backwards, and snaps only on a big jump */
    if (!estOn) { est = raw; estAt = now; estOn = 1; return est; }
    double e = est + (paused ? 0 : now - estAt) * rate; double err = raw - e;
    if (fabs(err) > 150) e = raw; else { e += err * 0.025; if (e < est) e = est; }
    est = e; estAt = now;
    return est;
}
void audio_set_speed(double r) { rateNext = r > 0.2 && r < 1 ? r : 1; }
double audio_speed(void) { return rate; }
void audio_render(float *out, int frames) { callback(NULL, (Uint8 *)out, frames * (int)(sizeof(float) * 2)); }
