#include "audio.h"
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#include "minimp3.h"
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
static float master = 1.0f, fadeStep = 0, fadeGain = 1;
static int running = 0, paused = 0;
static int64_t mixed = 0;          /* frames mixed since play (negative start = lead-in) */
static double fromMs = 0, leadMs = 0;
static Uint64 cbTicks = 0; static int64_t cbMixed = 0; static SDL_SpinLock lock = 0;
static double est = 0, estAt = 0; static int estOn = 0;

static int pull(stream_t *s) { /* refill s->pcm; returns 0 at end */
    while (s->pcmPos >= s->pcmLen) {
        if (s->ended || s->pos >= s->size) { s->ended = 1; return 0; }
        mp3dec_frame_info_t info; int n = mp3dec_decode_frame(&s->dec, s->data + s->pos, (int)(s->size - s->pos), s->pcm, &info);
        if (info.frame_bytes <= 0) { s->ended = 1; return 0; }
        s->pos += info.frame_bytes;
        if (n > 0) { s->pcmLen = n * info.channels; s->pcmPos = 0; s->channels = info.channels; s->hz = info.hz; }
    }
    return 1;
}
static void callback(void *ud, Uint8 *out, int len) {
    (void)ud; float *o = (float *)out; int frames = len / (int)(sizeof(float) * 2);
    SDL_AtomicLock(&lock); cbTicks = SDL_GetPerformanceCounter(); cbMixed = mixed; SDL_AtomicUnlock(&lock);
    memset(out, 0, len);
    if (!running || paused) return;
    for (int i = 0; i < frames; i++) {
        float l = 0, r = 0;
        if (mixed >= 0) for (int b = 0; b < BUS_N; b++) { stream_t *s = &S[b]; if (!s->on) continue;
            s->gain += (s->target - s->gain) * 0.002f;
            if (!pull(s)) continue;
            float sl, sr; if (s->channels >= 2) { sl = s->pcm[s->pcmPos] * (1.0f / 32768); sr = s->pcm[s->pcmPos + 1] * (1.0f / 32768); s->pcmPos += 2; } else { sl = sr = s->pcm[s->pcmPos++] * (1.0f / 32768); }
            float g = s->level * s->gain; l += sl * g; r += sr * g; s->produced++; }
        if (fadeStep) { fadeGain -= fadeStep; if (fadeGain < 0) fadeGain = 0; }
        float g = master * fadeGain; l *= g; r *= g;
        o[i * 2] = l > 1 ? 1 : l < -1 ? -1 : l; o[i * 2 + 1] = r > 1 ? 1 : r < -1 ? -1 : r;
        mixed++;
    }
}
int audio_open(void) {
    SDL_AudioSpec want; SDL_zero(want); want.freq = 44100; want.format = AUDIO_F32SYS; want.channels = 2; want.samples = 1024; want.callback = callback;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &spec, 0);
    if (!dev) { SDL_Log("audio: %s", SDL_GetError()); return -1; }
    SDL_PauseAudioDevice(dev, 0); return 0;
}
void audio_close(void) { if (dev) SDL_CloseAudioDevice(dev); dev = 0; }
static void skip_to(stream_t *s, double ms) { /* decode forward to a position (only used for a start offset) */
    int64_t want = (int64_t)(ms / 1000.0 * 44100); int64_t have = 0;
    while (have < want && pull(s)) { int64_t left = (s->pcmLen - s->pcmPos) / (s->channels >= 2 ? 2 : 1); int64_t take = want - have < left ? want - have : left; s->pcmPos += (int)take * (s->channels >= 2 ? 2 : 1); have += take; }
}
void audio_set_stream(int bus, const uint8_t *mp3, size_t size, double levelDb) {
    if (dev) SDL_LockAudioDevice(dev);
    stream_t *s = &S[bus]; memset(s, 0, sizeof *s); mp3dec_init(&s->dec); s->data = mp3; s->size = size; s->on = mp3 && size > 0;
    s->level = (float)pow(10, levelDb / 20); s->gain = s->target = 1;
    if (dev) SDL_UnlockAudioDevice(dev);
}
void audio_clear(void) { if (dev) SDL_LockAudioDevice(dev); running = 0; for (int b = 0; b < BUS_N; b++) S[b].on = 0; if (dev) SDL_UnlockAudioDevice(dev); }
void audio_play(double from, double lead) {
    if (dev) SDL_LockAudioDevice(dev);
    fromMs = from; leadMs = lead; mixed = -(int64_t)(lead / 1000.0 * 44100); fadeStep = 0; fadeGain = 1; paused = 0;
    for (int b = 0; b < BUS_N; b++) { stream_t *s = &S[b]; if (!s->on) continue; mp3dec_init(&s->dec); s->pos = 0; s->pcmLen = s->pcmPos = 0; s->ended = 0; s->produced = 0; s->gain = s->target = 1; if (from > 0) skip_to(s, from); }
    estOn = 0; running = 1; cbTicks = SDL_GetPerformanceCounter(); cbMixed = mixed;
    if (dev) SDL_UnlockAudioDevice(dev);
}
void audio_stop(void) { if (dev) SDL_LockAudioDevice(dev); running = 0; if (dev) SDL_UnlockAudioDevice(dev); }
void audio_pause(int p) { if (dev) SDL_LockAudioDevice(dev); paused = p; estOn = 0; if (dev) SDL_UnlockAudioDevice(dev); }
int audio_running(void) { return running && !paused; }
void audio_mute(int bus, int m) { S[bus].target = m ? 0.06f : 1.0f; }
void audio_set_master(double g) { master = (float)g; }
void audio_fade_out(double sec) { if (sec <= 0) sec = 0.01; fadeStep = (float)(1.0 / (sec * 44100)); }
double audio_duration_ms(void) { double d = 0; for (int b = 0; b < BUS_N; b++) if (S[b].on) { /* MP3 CBR guess: 128 kbps */ double est = S[b].size / 16000.0 * 1000; if (est > d) d = est; } return d; }
double audio_song_ms(void) {
    if (!running) return fromMs - leadMs;
    SDL_AtomicLock(&lock); Uint64 ct = cbTicks; int64_t cm = cbMixed; SDL_AtomicUnlock(&lock);
    double now = (double)SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency() * 1000;
    double cbAt = (double)ct / SDL_GetPerformanceFrequency() * 1000;
    double heard = (double)(cm - spec.samples) / 44100.0 * 1000; /* the buffer handed over at the callback is heard about one buffer later */
    double raw = fromMs + heard + (paused ? 0 : now - cbAt);
    if (!estOn) { est = raw; estAt = now; estOn = 1; return est; }
    double e = est + (now - estAt); double err = raw - e;
    est = fabs(err) > 45 ? raw : e + err * 0.06; estAt = now;
    return est;
}
