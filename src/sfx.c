/* The game's own sound effects. ZONES/global_sfx.pak.xen holds 189 ".wav" entries (type 0x0a6808d4) that are really
 * MP3 streams (MPEG-2 LSF 24 kHz stereo for most, MPEG-1 44.1/48 kHz for the UI), 20 of them wrapped in a one-stream
 * FSB3 bank (the crowd loops, the battle heartbeat); an entry's key is the qbkey of the file's base name
 * ("sp_deployed"), the names themselves sit in the pak's one QB script. Which file an event plays is the table
 * global_sound_events of qb.pak: <event>_container = { Randomness, Sounds = { sound1 = { <file key> vol pitch } ... } },
 * one of the files picked at random, vol in percent, pitch in percent of the playback rate (how the browser port and the
 * game's scripts use them). Files are decoded lazily with minimp3 into 16-bit PCM at their own rate and mixed by a small
 * polyphonic mixer called from audio.c's callback, resampled on the fly (linear, like audio.c's frame_of) at the pitch.
 * The crowd follows the scripts change_crowd_looping_sfx_* / do_actual_transition_sfx_* / gh3_play_a_*_based_on_venue:
 * a looping bed per venue size and mood (bad / neutral / good by the rock meter), a transition sound when the mood
 * changes, cheers or boos on its own every few seconds, claps on the beat in star power, the swell at streaks, the
 * applause and surge at a song won (you_rock_waiting_crowd_sfx) and the boo at a song failed (crowd_fail_song_sfx). */
#include "sfx.h"
#include "audio.h"
#include "gh3fmt.h"
#include "minimp3.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SFX_LEVEL 0.7f          /* the table's vol=100 (the browser port's level) */
#define UI_LEVEL 2.2f           /* the menu sounds: their clips are quiet (scroll peaks at 0.27) and the game plays them over the menu music */
static int ui_kind(int k) { return k == SFX_MOVE || k == SFX_OK || k == SFX_BACK || k == SFX_WARN || k == SFX_WHOOSH || k == SFX_CASH; }
#define CROWD_LEVEL 0.8f
#define BED_LEVEL 0.5f          /* crowdnormal_bussset: the beds sit at -6 dB; a surge lifts them to +3 dB */
#define SURGE_LEVEL 1.41f
#define PCM_BUDGET (28u << 20)  /* decoded PCM kept around (the least recently used go first) */
#define VOICES 16

typedef struct { uint32_t key; const uint8_t *data; uint32_t len; int fsb; int16_t *pcm; int frames, ch, hz; unsigned used; char name[40]; } clip_t;
typedef struct { uint32_t key; int n, last; struct { int clip; float vol, pitch; } s[12]; int limit; float gain; double noRepeat, lastAt; } event_t;   /* limit > 0: an event of the practice kit (its own voices; sfx_event_rules) */
typedef struct { const clip_t *c; double pos, step; float gain, target, gStep; int on, loop, bed, kind; double loopA, loopB; } voice_t;   /* pos < 0: frames still to wait */

static uint8_t *bankBytes; static size_t bankLen; static clip_t *clips; static int nclips; static size_t pcmBytes; static unsigned useTick;
static event_t *events; static int nevents; static int ready; static voice_t V[VOICES];
/* the practice drum kit's own voices, so its long cymbals take none of the effects' (each drum is Instancelimit 1: at
 * most one sounding, and the hit it cuts fading out) and a clock for NoRepeatFor (frames mixed while the song plays) */
#define KIT_VOICES 20
static voice_t KV[KIT_VOICES]; static double kitClock;
static float bedBoost = 1, boostTarget = 1, boostStep = 0; static int songOnLast;

static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static uint32_t le32(const uint8_t *p) { return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0]; }
static clip_t *clip_of(uint32_t key) { for (int i = 0; i < nclips; i++) if (clips[i].key == key) return &clips[i]; return NULL; }
static event_t *event_of(uint32_t cont) { for (int i = 0; i < nevents; i++) if (events[i].key == cont) return &events[i]; return NULL; }
static int lzss(const uint8_t *in, int inN, uint8_t *out, int outN) {   /* GH3 script bodies (as tools/qbgrep.c) */
    uint8_t win[4096]; memset(win, 0, sizeof win); int wp = 4078, i = 0, o = 0; unsigned flags = 0;
    while (i < inN && o < outN) { flags >>= 1; if (!(flags & 0x100)) { flags = in[i++] | 0xFF00; if (i >= inN) break; }
        if (flags & 1) { uint8_t c = in[i++]; out[o++] = c; win[wp] = c; wp = (wp + 1) & 4095; }
        else { if (i + 1 >= inN) break; int a = in[i] | ((in[i + 1] & 0xF0) << 4), len = (in[i + 1] & 0x0F) + 3; i += 2; for (int k = 0; k < len && o < outN; k++) { uint8_t c = win[(a + k) & 4095]; out[o++] = c; win[wp] = c; wp = (wp + 1) & 4095; } } }
    return o;
}
static void name_clips(const uint8_t *b, size_t L) {   /* the pak's script lists every file's path: "Star_Power\sp_deployed" names the entry whose key is qbkey("sp_deployed") */
    for (size_t p = 28; p + 20 <= L; p += 4) { if (be32(b + p) != 0x00200700) continue; uint32_t raw = be32(b + p + 12); if (raw + 12 > L || raw < p) continue;
        uint32_t usz = be32(b + raw + 4), csz = be32(b + raw + 8); if (usz > (1u << 20) || raw + 12 + csz > L) continue; uint8_t *out = malloc(usz + 1); int got;
        if (usz == csz) { memcpy(out, b + raw + 12, usz); got = usz; } else got = lzss(b + raw + 12, csz, out, usz);
        for (int k = 0; k < got; k++) { int l = 0; while (k + l < got && out[k + l] >= 0x20 && out[k + l] < 0x7f) l++;
            if (l >= 3 && memchr(out + k, '\\', l)) { const uint8_t *base = out + k + l; while (base > out + k && base[-1] != '\\') base--; char nm[40]; int bl = (int)(out + k + l - base); if (bl > 39) bl = 39; memcpy(nm, base, bl); nm[bl] = 0;
                clip_t *c = clip_of(gh3_qbkey(nm)); if (c && !c->name[0]) memcpy(c->name, nm, bl + 1); }
            k += l; }
        free(out); }
}
int sfx_init(const uint8_t *qbpak, size_t pl, const uint8_t *qbpab, size_t bl, uint8_t *bank, size_t len) {
    ready = 0; if (!bank || len < 64) { free(bank); return -1; }
    bankBytes = bank; bankLen = len; gh3_pakent *e; int n = gh3_pak_parse(bank, len, NULL, 0, &e); uint32_t KQB = gh3_qbkey(".qb");
    clips = calloc(n + 1, sizeof *clips); nclips = 0; int qbAt = -1;
    for (int i = 0; i < n; i++) { if (!e[i].data || e[i].len < 8) continue; if (e[i].type == KQB) { qbAt = i; continue; }
        clip_t *c = &clips[nclips++]; c->key = e[i].fullKey; c->data = e[i].data; c->len = e[i].len; c->fsb = !memcmp(e[i].data, "FSB3", 4); }
    if (qbAt >= 0) name_clips(e[qbAt].data, e[qbAt].len);
    free(e);
    /* the table */
    gh3_pakent *q; int qn = qbpak ? gh3_pak_parse(qbpak, pl, qbpab, bl, &q) : 0; uint32_t KT = gh3_qbkey("global_sound_events"); int found = 0;
    for (int i = 0; i < qn; i++) { if (q[i].type != KQB || !q[i].data || (q[i].nameKey != KT && q[i].fullKey != KT)) continue;
        qbsection *s; int ns = gh3_qb_parse(q[i].data, q[i].len, &s); if (ns < 0) continue; found = 1;
        events = calloc(ns + 1, sizeof *events); nevents = 0;
        for (int k = 0; k < ns; k++) { const qbv *snd = s[k].v && s[k].v->kind == QB_STRUCT ? qb_get(s[k].v, "Sounds") : NULL; if (!snd || snd->kind != QB_STRUCT) continue;
            event_t *ev = &events[nevents]; ev->key = s[k].id; ev->n = 0; ev->last = -1;
            for (int j = 0; j < snd->nitems && ev->n < 12; j++) { const qbv *one = snd->items[j].v; if (!one || one->kind != QB_STRUCT) continue; uint32_t fk = 0; int ok = 0;
                for (int m = 0; m < one->nitems && !ok; m++) if (one->items[m].id == 0 && qb_keyof(one->items[m].v, &fk)) ok = 1;
                clip_t *c = ok ? clip_of(fk) : NULL; if (!c) continue;
                ev->s[ev->n].clip = (int)(c - clips); ev->s[ev->n].vol = (float)qb_num(qb_get(one, "vol"), 100); ev->s[ev->n].pitch = (float)qb_num(qb_get(one, "pitch"), 100); ev->n++; }
            if (ev->n) nevents++; }
        gh3_qb_free_sections(s, ns); break; }
    if (qn) free(q);
    ready = found && nevents > 0; SDL_Log("sfx: %d sounds in the bank, %d events in the table%s", nclips, nevents, ready ? "" : " (not used)");
    return ready ? 0 : -1;
}
int sfx_ready(void) { return ready; }

/* ---- decoding: an entry's MP3 frames (after the FSB3 header when it has one; the Xing/Info frame is skipped) */
static int decode(clip_t *c) {
    if (c->pcm) { c->used = ++useTick; return 1; }
    const uint8_t *d = c->data; size_t n = c->len;
    if (c->fsb) { if (n < 24) return 0; uint32_t hs = le32(d + 8); if (24 + hs >= n) return 0; d += 24 + hs; n -= 24 + hs; }
    mp3dec_t dec; mp3dec_init(&dec); size_t pos = 0, cap = 0, frames = 0; int16_t *pcm = NULL; int ch = 0, hz = 0; short buf[MINIMP3_MAX_SAMPLES_PER_FRAME];
    while (pos < n) { mp3dec_frame_info_t info; int got = mp3dec_decode_frame(&dec, d + pos, (int)(n - pos), buf, &info); if (info.frame_bytes <= 0) break;
        int xing = pos == 0 || !ch ? (memmem(d + pos, info.frame_bytes, "Xing", 4) || memmem(d + pos, info.frame_bytes, "Info", 4)) : 0; pos += info.frame_bytes;
        if (got <= 0 || xing) continue;
        if (!ch) { ch = info.channels; hz = info.hz; }
        if (frames + got > cap) { cap = cap ? cap * 2 : 16384; while (cap < frames + got) cap *= 2; pcm = realloc(pcm, cap * ch * sizeof *pcm); }
        memcpy(pcm + frames * ch, buf, (size_t)got * ch * sizeof *pcm); frames += got; }
    if (!frames) { free(pcm); return 0; }
    if (!c->fsb) { /* the encoder's leading silence (up to 60 ms) is cut: the bad-note clank must land on the miss */ size_t lead = 0, max = (size_t)hz * 3 / 50; while (lead < frames && lead < max) { int q = 0; for (int k = 0; k < ch; k++) if (abs(pcm[lead * ch + k]) > 48) q = 1; if (q) break; lead++; }
        if (lead) { memmove(pcm, pcm + lead * ch, (frames - lead) * ch * sizeof *pcm); frames -= lead; } }
    c->frames = (int)frames; c->ch = ch; c->hz = hz; c->used = ++useTick; pcmBytes += frames * ch * sizeof *pcm; c->pcm = pcm;
    if (pcmBytes > PCM_BUDGET) { /* drop the least recently used clips that no voice plays */ audio_lock();
        while (pcmBytes > PCM_BUDGET) { clip_t *old = NULL; for (int i = 0; i < nclips; i++) { clip_t *k = &clips[i]; if (!k->pcm || k == c) continue; int busy = 0; for (int v = 0; v < VOICES; v++) if (V[v].on && V[v].c == k) busy = 1; if (!busy && (!old || k->used < old->used)) old = k; }
            if (!old) break; pcmBytes -= (size_t)old->frames * old->ch * sizeof *old->pcm; free(old->pcm); old->pcm = NULL; }
        audio_unlock(); }
    return 1;
}
static int event_decode(event_t *ev) { int ok = 0; for (int i = 0; i < ev->n; i++) ok |= decode(&clips[ev->s[i].clip]); return ok; }

/* ---- the mixer */
static voice_t *voice_new(voice_t *P, int n) { for (int v = 0; v < n; v++) if (!P[v].on) return &P[v];
    voice_t *o = NULL; for (int v = 0; v < n; v++) if (!P[v].bed && !P[v].loop && (!o || P[v].pos > o->pos)) o = &P[v]; return o ? o : &P[0]; }
static voice_t *start_in(voice_t *P, int np, const clip_t *c, float gain, float pitch, double delayMs, int loop, int bed, int kind) {   /* under the audio lock */
    if (!c || !c->pcm) return NULL; voice_t *v = voice_new(P, np); memset(v, 0, sizeof *v); v->c = c; v->step = (double)c->hz / 44100.0 * (pitch > 0 ? pitch / 100 : 1);
    v->pos = -delayMs / 1000.0 * 44100; v->gain = v->target = gain > 4 ? 4 : gain; v->loop = loop; v->bed = bed; v->kind = kind;
    if (loop) { double trim = c->hz * 0.06; v->loopA = trim; v->loopB = c->frames - 1 - trim; if (v->loopB <= v->loopA + 1) { v->loopA = 0; v->loopB = c->frames - 1; } if (bed) v->pos = v->loopA + (rand() / (RAND_MAX + 1.0)) * fmax(0, v->loopB - v->loopA - c->hz); }
    v->on = 1; return v;
}
static voice_t *start(const clip_t *c, float gain, float pitch, double delayMs, int loop, int bed, int kind) { return start_in(V, VOICES, c, gain, pitch, delayMs, loop, bed, kind); }
static voice_t *play_event(event_t *ev, double delayMs, float level, int bed) {   /* one of the event's files, not the one before */
    if (!ev || !ev->n) return NULL; int i = ev->n > 1 ? rand() % ev->n : 0; if (ev->n > 1 && i == ev->last) i = (i + 1) % ev->n; ev->last = i;
    return start(&clips[ev->s[i].clip], ev->s[i].vol / 100 * level, ev->s[i].pitch, delayMs, 0, bed, -1);
}
static inline void voice_mix(voice_t *v, float *l, float *r, float g, int songOn) {
    if (v->bed && !songOn) return;
    if (v->pos < 0) { v->pos += 1; return; }
    const clip_t *c = v->c; int i = (int)v->pos; if (i >= c->frames - 1) { if (!v->loop) { v->on = 0; return; } v->pos = v->loopA; i = (int)v->pos; }
    float fr = (float)(v->pos - i); const int16_t *p = c->pcm + (size_t)i * c->ch; float sl, sr;
    if (c->ch >= 2) { sl = (p[0] + (p[c->ch] - p[0]) * fr) * (1.0f / 32768); sr = (p[1] + (p[c->ch + 1] - p[1]) * fr) * (1.0f / 32768); } else sl = sr = (p[0] + (p[1] - p[0]) * fr) * (1.0f / 32768);
    if (v->gain != v->target) { if (v->gStep <= 0) v->gain = v->target; else if (v->gain < v->target) { v->gain += v->gStep; if (v->gain > v->target) v->gain = v->target; } else { v->gain -= v->gStep; if (v->gain <= v->target) { v->gain = v->target; if (v->target <= 0) { v->on = 0; return; } } } }
    float k = v->gain * g * (v->bed ? bedBoost : 1); *l += sl * k; *r += sr * k;
    v->pos += v->step; if (v->loop && v->pos >= v->loopB) v->pos -= v->loopB - v->loopA;
}
void sfx_mix(float *l, float *r, float gain, int songOn) {
    if (!ready) return;
    if (boostStep > 0) { if (bedBoost < boostTarget) { bedBoost += boostStep; if (bedBoost >= boostTarget) { bedBoost = boostTarget; if (boostTarget > 1) { boostTarget = 1; boostStep = (SURGE_LEVEL - 1) / (1.2f * 44100); } else boostStep = 0; } } else { bedBoost -= boostStep; if (bedBoost <= boostTarget) { bedBoost = boostTarget; boostStep = 0; } } }
    songOnLast = songOn;
    for (int v = 0; v < VOICES; v++) if (V[v].on) voice_mix(&V[v], l, r, gain, songOn);
}
static void fade(voice_t *v, float target, double sec) { v->target = target; v->gStep = (float)(fabs(v->gain - target) / (sec * 44100)); if (v->gStep <= 0) v->gStep = 1e-6f; }

/* ---- the kinds: which event each SFX_* plays (two rows for a kind: both). Confirmed in the scripts unless noted;
 * the gh3_* names stand for events whose names were not recovered (RAW below has their container keys). */
static const struct { int kind; const char *ev; } MAP[] = {
    { SFX_MOVE, "ui_sfx_scroll" }, { SFX_MOVE, "ui_sfx_scroll_add" },          /* generic_menu_up_or_down_sound */
    { SFX_OK, "ui_sfx_select" }, { SFX_BACK, "gh3_pad_back" },                  /* the menus' pad_back handlers: UI_Sound_09/10 */
    { SFX_BAD, "Single_Player_Bad_Note_Guitar" }, { SFX_BAD_BASS, "Single_Player_Bad_Note_Bass" },
    { SFX_READY, "Star_Power_Ready_SFX" }, { SFX_AWARDED, "Star_Power_Awarded_SFX" },
    { SFX_STAR_ON, "Star_Power_Deployed_SFX" }, { SFX_STAR_ON, "Star_Power_Deployed_Cheer_SFX" },
    { SFX_STREAK, "UI_SFX_50_Note_Streak_SinglePlayer" }, { SFX_STREAK100, "UI_SFX_100_Note_Streak_SinglePlayer" },
    { SFX_LOSE, "UI_SFX_Lose_Multiplier_2X" }, { SFX_LOSE3, "UI_SFX_Lose_Multiplier_3X" }, { SFX_LOSE4, "UI_SFX_Lose_Multiplier_4X" },
    { SFX_FAIL, "GH_SFX_You_Lose_Single_Player" }, { SFX_FAIL, "crowd_fail_song_sfx" },   /* GuitarEvent_SongFailed_Spawned */
    { SFX_ROCK, "You_Rock_End_SFX" },                                               /* GuitarEvent_SongWon_Spawned */
    { SFX_WIN_P1, "UI_1stPlayerWins_SFX" }, { SFX_WIN_P2, "UI_2ndPlayerWins_SFX" },
    { SFX_CASH, "Cash_Sound" },                                                      /* the store's buy, the results' pay */
    { SFX_INTRO, "Song_Intro_Kick_SFX" }, { SFX_HIGHWAY, "gh3_highway_rise" },       /* the song's start (the highway rises 1.5 s before) */
    { SFX_WHOOSH, "gh3_fly_in_whoosh" },                                                 /* guess: the ENCORE! banner's script flies text in with it */
    { SFX_FANFARE, "gh3_crowd_cheer_sm" }, { SFX_DING, "Star_Power_Awarded_SFX" },   /* guesses: the cheats' unlock plays the small cheer; the results' stars have no sound of their own */
    { SFX_TICK, "gh3_beat_tick" }, { SFX_GO, "gh3_beat_down" },                      /* GH3_Beat_Sound: the calibration screen's click (pitch 250) and the metronome's downbeat (150) */
    { SFX_ATK_LIGHTNING, "GH_SFX_BattleMode_Lightning_Player1" }, { SFX_ATK_DIFFUP, "GH_SFX_BattleMode_DiffUp_P1" }, { SFX_ATK_DOUBLE, "GH_SFX_BattleMode_DoubleNote_P1" },
    { SFX_ATK_STEAL, "GH_SFX_BattleMode_Steal_P1" }, { SFX_ATK_LEFTY, "GH_SFX_BattleMode_Lefty_P1" }, { SFX_ATK_STRING, "GH_SFX_BattleMode_StringBreak_P1" },
    { SFX_ATK_WHAMMY, "GH_SFX_BattleMode_WhammyAttack_P1" }, { SFX_ATK_DEATH, "GH_SFX_BattleMode_Death_Drain_P1" }, { SFX_ATK_OVER, "GH_SFX_BattleMode_Attack_Over_P1" },
    { SFX_BATTLE_AWARD, "Battle_Power_Awarded_SFX_P1" }, { SFX_SUDDEN_DEATH, "GH_SFX_BattleMode_Sudden_Death" }, { SFX_DEVIL_DIE, "gh3_devil_die" },
    { SFX_HEARTBEAT, "Battlemode_Heartbeat_P1" },
    { SFX_WARN, "Menu_Warning_SFX" },                                                  /* a closed song, a refused choice (Menu_Warning_01) */
};
#define NMAP ((int)(sizeof MAP / sizeof MAP[0]))
/* events the table has under names not recovered: named here by their container key (events.txt of the port's notes) */
static const struct { const char *name; uint32_t cont; } RAW[] = { { "gh3_crowd_cheer_sm", 0x69761ea2u }, { "gh3_crowd_boo_sm", 0xa3ef642cu }, { "gh3_beat_tick", 0x3be30f88u }, { "gh3_beat_down", 0xbe1211a5u }, { "gh3_devil_die", 0x7acda288u },
    { "gh3_pad_back", 0x7b2e27d0u }, { "gh3_highway_rise", 0xca25e152u }, { "gh3_fly_in_whoosh", 0x73b5f7cbu } };
static uint32_t cont_key(const char *ev) { for (int i = 0; i < (int)(sizeof RAW / sizeof RAW[0]); i++) if (!strcmp(RAW[i].name, ev)) return RAW[i].cont; char b[96]; snprintf(b, sizeof b, "%s_container", ev); return gh3_qbkey(b); }
static event_t *kind_event(int kind, int which) { int seen = 0; for (int i = 0; i < NMAP; i++) if (MAP[i].kind == kind && seen++ == which) return event_of(cont_key(MAP[i].ev)); return NULL; }
static void crowd_swell(void);
int sfx_play(int kind) {
    if (!ready || kind == SFX_STAR_OFF) return ready;   /* (the game has no sound for star power running out) */
    event_t *e0 = kind_event(kind, 0), *e1 = kind_event(kind, 1); if (!e0 && !e1) return 0;
    if (e0) event_decode(e0); if (e1) event_decode(e1);   /* (before the lock: the first use decodes) */
    float lv = ui_kind(kind) ? UI_LEVEL : SFX_LEVEL; audio_lock(); play_event(e0, 0, lv, 0); play_event(e1, 0, lv, 0); if (kind == SFX_STREAK || kind == SFX_STREAK100) crowd_swell(); audio_unlock();
    return 1;
}
int sfx_event_find(uint32_t cont) { event_t *e = ready ? event_of(cont) : NULL; return e ? (int)(e - events) : -1; }
int sfx_event_prepare(int idx) { return ready && idx >= 0 && idx < nevents ? event_decode(&events[idx]) : 0; }
void sfx_event_rules(int idx, int limit, double noRepeatSec, double bussDb) { if (!ready || idx < 0 || idx >= nevents) return; event_t *e = &events[idx]; e->limit = limit > 0 ? limit : 1; e->noRepeat = noRepeatSec > 0 ? noRepeatSec * 44100 : 0; e->gain = (float)pow(10, bussDb / 20); e->lastAt = -1e18; }
void sfx_event_fire(int idx, float level) {
    if (!ready || idx < 0 || idx >= nevents) return; event_t *ev = &events[idx]; if (!ev->limit) { play_event(ev, 0, level, 0); return; }
    if (kitClock - ev->lastAt < ev->noRepeat || !ev->n) return; ev->lastAt = kitClock;   /* NoRepeatFor: the same drum again too soon is not played */
    int on = 0; voice_t *old = NULL; for (int v = 0; v < KIT_VOICES; v++) if (KV[v].on && KV[v].kind == idx && KV[v].target > 0) { on++; if (!old || KV[v].pos > old->pos) old = &KV[v]; }
    if (on >= ev->limit && old) fade(old, 0, 0.004);   /* instancemanagement stop_oldest: the hit before is cut (in 4 ms, no click) */
    int i = ev->n > 1 ? rand() % ev->n : 0; if (ev->n > 1 && i == ev->last) i = (i + 1) % ev->n; ev->last = i;
    start_in(KV, KIT_VOICES, &clips[ev->s[i].clip], ev->s[i].vol / 100 * ev->gain * level, ev->s[i].pitch, 0, 0, 0, idx);
}
void sfx_kit_mix(float *l, float *r, float gain) { kitClock += 1; if (ready) for (int v = 0; v < KIT_VOICES; v++) if (KV[v].on) voice_mix(&KV[v], l, r, gain, 1); }
void sfx_kit_cut(void) { for (int v = 0; v < KIT_VOICES; v++) KV[v].on = 0; kitClock = 0; for (int i = 0; i < nevents; i++) events[i].lastAt = -1e18; }
int sfx_voices_on(int kit) { int n = 0; if (kit) { for (int v = 0; v < KIT_VOICES; v++) n += KV[v].on; } else for (int v = 0; v < VOICES; v++) n += V[v].on; return n; }
void sfx_prepare(int kind) { if (!ready) return; event_t *e; for (int w = 0; w < 2 && (e = kind_event(kind, w)); w++) event_decode(e); }
void sfx_loop(int kind, int on) {
    if (!ready) return; event_t *ev = kind_event(kind, 0); if (!ev) return; if (on) event_decode(ev);
    audio_lock(); int had = -1; for (int v = 0; v < VOICES; v++) if (V[v].on && V[v].loop && V[v].kind == kind) had = v;
    if (on && had < 0) { voice_t *v = start(&clips[ev->s[0].clip], ev->s[0].vol / 100 * SFX_LEVEL, ev->s[0].pitch, 0, 1, 0, kind); if (v) v->pos = 0; }
    else if (!on && had >= 0) fade(&V[had], 0, 0.3);
    audio_unlock();
}

/* ---- the crowd. The venue sizes of the scripts: z_dive and z_party small; z_budokan and z_wikker the big (generic)
 * crowd; z_hell its own; the rest (z_artdeco, z_video, z_prison, z_soundcheck) medium. Beds (file, vol) are the areas of
 * skate8_sfx_background_areas_and_states, transitions and swells the per-venue cases of the scripts named above. */
typedef struct { const char *f; float vol; } cf_t;
typedef struct { cf_t bed[3]; cf_t up01, up12, down21, down10; const char *swell, *swell2; cf_t applause; float swellVol; } crowdset_t;
static const crowdset_t CROWD[4] = {
    { { { "Small_Crowd_Loop_Bad", 60 }, { "Small_Crowd_Loop_Neutral", 16 }, { "Small_Crowd_Loop_Good", 88 } }, { "Small_Crowd_Bad_To_Med_SFX", 120 }, { "Small_Crowd_Med_To_Good", 65 }, { "Small_Crowd_Good_To_Med", 50 }, { "Small_Crowd_Med_To_Bad", 60 }, "Small_Crowd_Swell", NULL, { "Small_Crowd_Applause", 100 }, 40 },
    { { { "Medium_Crowd_Loop_Bad_01", 140 }, { "Medium_Crowd_Loop_Neutral_01", 100 }, { "Medium_Crowd_Loop_Good_02", 70 } }, { "Medium_Crowd_Bad_to_Medium", 130 }, { "Medium_Crowd_Medium_to_Good", 90 }, { "Medium_Crowd_Good_to_Medium", 100 }, { "Medium_Crowd_Medium_to_Bad", 100 }, "Medium_Crowd_Swell_02", "Medium_Crowd_Swell_03", { "Medium_Crowd_Applause_02", 25 }, 60 },
    { { { "Heavy_Crowd_Loop_Bad_01", 135 }, { "Heavy_Crowd_Loop_Nuetral_02", 160 }, { "Heavy_Crowd_Loop_Good_02", 40 } }, { "Crowd_Bad_To_Med", 60 }, { "Crowd_Med_To_Good", 60 }, { "Crowd_Good_To_Medium", 75 }, { "Crowd_Med_To_Bad", 70 }, "Crowd_Surge_Fast_01", NULL, { "Large_Crowd_Applause", 100 }, 50 },
    { { { "Hell_Crowd_Loop_Bad", 250 }, { "Hell_Crowd_Loop_Neutral", 150 }, { "Hell_Crowd_Loop_Good", 150 } }, { "Hell_Crowd_Bad_Neutral_FX", 40 }, { "Hell_Neutral_Good_SFX", 40 }, { "Hell_Crowd_Good_Neutral_FX", 45 }, { "Hell_Crowd_Neutral_Bad", 50 }, "Hell_Crowd_Swell", NULL, { "Medium_Crowd_Applause_02", 25 }, 30 },
};
static struct { int on, set, state; double nextShot, introAt; int introDone, beat; } CR;
static int venue_set(const char *z) { if (!z) return -1; if (!strcmp(z, "z_dive") || !strcmp(z, "z_party")) return 0; if (!strcmp(z, "z_budokan") || !strcmp(z, "z_wikker")) return 2; if (!strcmp(z, "z_hell")) return 3; return z[0] ? 1 : -1; }
static clip_t *named(const char *f) { return f ? clip_of(gh3_qbkey(f)) : NULL; }
static void one(const cf_t *f, double delayMs) { clip_t *c = named(f->f); if (c && c->pcm) start(c, f->vol / 100 * CROWD_LEVEL, 100, delayMs, 0, 0, -1); }
static void bed(int st, double fadeSec) {   /* the mood's loop, faded in over the one before */
    const crowdset_t *S = &CROWD[CR.set]; for (int v = 0; v < VOICES; v++) if (V[v].on && V[v].bed) fade(&V[v], 0, fadeSec);
    clip_t *c = named(S->bed[st].f); if (!c || !c->pcm) return; voice_t *v = start(c, 0, 100, 0, 1, 1, -1); if (v) { v->gain = 0; fade(v, S->bed[st].vol / 100 * CROWD_LEVEL * BED_LEVEL, fadeSec); }
}
static void crowd_swell(void) { if (!CR.on) return; const crowdset_t *S = &CROWD[CR.set]; clip_t *c = named(S->swell2 && rand() % 2 ? S->swell2 : S->swell); if (c && c->pcm) start(c, S->swellVol / 100 * CROWD_LEVEL, 100, 0, 0, 0, -1); boostTarget = SURGE_LEVEL; boostStep = (SURGE_LEVEL - 1) / (1.2f * 44100); }
static void crowd_decode(int set) { const crowdset_t *S = &CROWD[set]; const char *fs[] = { S->bed[0].f, S->bed[1].f, S->bed[2].f, S->up01.f, S->up12.f, S->down21.f, S->down10.f, S->swell, S->swell2, S->applause.f };
    for (int i = 0; i < 10; i++) { clip_t *c = named(fs[i]); if (c) decode(c); } for (int i = 0; i < 2; i++) { event_t *e = event_of(RAW[i].cont); if (e) event_decode(e); } event_t *e = event_of(cont_key("crowd_individual_clap_to_beat")); if (e) event_decode(e); }
void sfx_song_prepare(const char *zone) {   /* (the first use of a sound decodes its MP3 on the game's thread: 0.1-0.3 s on the A35 for the long cheers) */
    if (!ready) return; int set = venue_set(zone); if (set >= 0) crowd_decode(set);
    static const int K[] = { SFX_HIGHWAY, SFX_INTRO, SFX_BAD, SFX_BAD_BASS, SFX_READY, SFX_AWARDED, SFX_STAR_ON, SFX_STREAK, SFX_STREAK100, SFX_LOSE, SFX_LOSE3, SFX_LOSE4, SFX_FAIL, SFX_ROCK,
        SFX_ATK_LIGHTNING, SFX_ATK_DIFFUP, SFX_ATK_DOUBLE, SFX_ATK_STEAL, SFX_ATK_LEFTY, SFX_ATK_STRING, SFX_ATK_WHAMMY, SFX_ATK_DEATH, SFX_ATK_OVER, SFX_BATTLE_AWARD, SFX_SUDDEN_DEATH, SFX_DEVIL_DIE, SFX_HEARTBEAT, SFX_WIN_P1, SFX_WIN_P2 };
    for (size_t i = 0; i < sizeof K / sizeof K[0]; i++) sfx_prepare(K[i]);
    for (int i = 0; i < 2; i++) { event_t *e = event_of(RAW[i].cont); if (e) event_decode(e); }   /* the crowd's one-shot cheers and boos */
}
void sfx_song_begin(const char *zone, double leadMs) {
    if (!ready) return; sfx_song_stop(); int set = venue_set(zone); CR.introDone = 0; CR.beat = -1; CR.state = -1; CR.nextShot = 0;
    event_t *hw = kind_event(SFX_HIGHWAY, 0), *kick = kind_event(SFX_INTRO, 0); if (hw) event_decode(hw); if (kick) event_decode(kick); if (set >= 0) crowd_decode(set);
    audio_lock(); CR.on = set >= 0; CR.set = set < 0 ? 1 : set; play_event(hw, 0, SFX_LEVEL, 0); play_event(kick, leadMs > 0 ? leadMs : 0, SFX_LEVEL, 0); audio_unlock();
}
void sfx_song_tick(const gh3_run *r, double songMs) {
    if (!ready || !CR.on || !r) return; const crowdset_t *S = &CROWD[CR.set]; int st = r->health < 0.66 ? 0 : r->health < 1.33 ? 1 : 2;
    audio_lock();
    if (st != CR.state) { if (CR.state >= 0) { const cf_t *t = st > CR.state ? (CR.state == 0 ? &S->up01 : &S->up12) : (CR.state == 2 ? &S->down21 : &S->down10); one(t, 0); } bed(st, CR.state < 0 ? 2.5 : 1.2); CR.state = st; CR.nextShot = songMs + 5000 * (0.5 + rand() / (RAND_MAX + 1.0)); }
    if (songMs >= CR.nextShot) { if (st != 1) { event_t *e = event_of(RAW[st == 2 ? 0 : 1].cont); play_event(e, 0, CROWD_LEVEL, 0); } CR.nextShot = songMs + 5000 * (0.7 + 0.9 * rand() / (RAND_MAX + 1.0)); }   /* cheers when it goes well, boos when badly, every few seconds */
    if (r->spOn && r->hasLastBeat) { int b = (int)floor(r->lastBeat); if (b != CR.beat) { CR.beat = b; play_event(event_of(cont_key("crowd_individual_clap_to_beat")), 0, CROWD_LEVEL, 0); } } else CR.beat = -1;
    audio_unlock();
}
void sfx_song_end(int failed) {
    if (!ready) return; audio_lock();
    if (CR.on) { const crowdset_t *S = &CROWD[CR.set]; if (!failed) { clip_t *c = named(S->swell); if (c && c->pcm) start(c, S->swellVol / 100 * CROWD_LEVEL, 100, 2000, 0, 0, -1); one(&S->applause, 2000); }
        for (int v = 0; v < VOICES; v++) if (V[v].on && V[v].bed) fade(&V[v], 0, failed ? 1.5 : 6); }
    CR.on = 0; audio_unlock();
}
void sfx_song_stop(void) { if (!ready) return; audio_lock(); for (int v = 0; v < VOICES; v++) if (V[v].bed || V[v].loop) V[v].on = 0; CR.on = 0; bedBoost = boostTarget = 1; boostStep = 0; audio_unlock(); }

/* ---- --sfxtest */
static const char *KIND_NAME[SFX_N] = { "MOVE", "OK", "BACK", "BAD", "BAD_BASS", "READY", "STAR_ON", "STAR_OFF", "STREAK", "DING", "FANFARE", "TICK", "GO", "FAIL", "LOSE", "LOSE3", "LOSE4", "STREAK100", "AWARDED", "ROCK", "CASH", "WHOOSH", "INTRO", "HIGHWAY", "WIN_P1", "WIN_P2",
    "ATK_LIGHTNING", "ATK_DIFFUP", "ATK_DOUBLE", "ATK_STEAL", "ATK_LEFTY", "ATK_STRING", "ATK_WHAMMY", "ATK_DEATH", "ATK_OVER", "BATTLE_AWARD", "SUDDEN_DEATH", "DEVIL_DIE", "HEARTBEAT", "WARN" };
static int show_clip(clip_t *c, float vol, float pitch) {
    int ok = decode(c); printf("      %-36s %7u B %s", c->name[0] ? c->name : "?", c->len, c->fsb ? "fsb3/mp3" : "mp3");
    if (ok) { double pk = 0, sq = 0; size_t n = (size_t)c->frames * c->ch; for (size_t i = 0; i < n; i++) { double v = c->pcm[i] / 32768.0; if (fabs(v) > pk) pk = fabs(v); sq += v * v; }
        printf(" %d Hz %dch %.2f s peak %.2f rms %.3f", c->hz, c->ch, c->frames / (double)c->hz, pk, n ? sqrt(sq / n) : 0); } else printf(" DECODE FAILED");
    if (vol != 100) printf(" vol %.0f", vol); if (pitch != 100) printf(" pitch %.0f", pitch); printf("\n"); return !ok;
}
void sfx_event_show(int idx) { if (!ready || idx < 0 || idx >= nevents) return; event_t *e = &events[idx]; for (int j = 0; j < e->n; j++) show_clip(&clips[e->s[j].clip], e->s[j].vol, e->s[j].pitch);
    if (e->limit) printf("      (Instancelimit %d stop_oldest, NoRepeatFor %.2f s, its buss x%.3f = %+.1f dB, on its own voices)\n", e->limit, e->noRepeat / 44100, e->gain, 20 * log10(e->gain)); }
int sfx_selftest(void) {
    if (!ready) { puts("sfx: no ZONES/global_sfx.pak.xen or no global_sound_events table"); return 1; }
    int bad = 0, unnamed = 0; for (int i = 0; i < nclips; i++) if (!clips[i].name[0]) unnamed++;
    printf("bank: %d sounds (%zu bytes, %d unnamed), table: %d events\n", nclips, bankLen, unnamed, nevents);
    for (int k = 0; k < SFX_N; k++) { printf("SFX_%s:%s\n", KIND_NAME[k], k == SFX_STAR_OFF ? " (silent: the game has none)" : "");
        for (int i = 0; i < NMAP; i++) { if (MAP[i].kind != k) continue; uint32_t ck = cont_key(MAP[i].ev); event_t *e = event_of(ck);
            printf("   %s_container (%08x)%s\n", MAP[i].ev, ck, e ? "" : "  NOT IN TABLE"); if (!e) { bad++; continue; }
            for (int j = 0; j < e->n; j++) bad += show_clip(&clips[e->s[j].clip], e->s[j].vol, e->s[j].pitch); } }
    static const char *SETN[4] = { "small (z_dive, z_party)", "medium (z_artdeco, z_video, z_prison, z_soundcheck)", "big (z_budokan, z_wikker)", "hell (z_hell)" };
    for (int s = 0; s < 4; s++) { const crowdset_t *S = &CROWD[s]; printf("crowd %s:\n", SETN[s]); const cf_t all[] = { S->bed[0], S->bed[1], S->bed[2], S->up01, S->up12, S->down21, S->down10, { S->swell, S->swellVol }, { S->swell2, S->swellVol }, S->applause };
        for (int i = 0; i < 10; i++) { if (!all[i].f) continue; clip_t *c = named(all[i].f); if (!c) { printf("      %-36s NOT IN BANK\n", all[i].f); bad++; continue; } bad += show_clip(c, all[i].vol, 100); } }
    printf("crowd one-shots and claps:\n"); for (int i = 0; i < 2; i++) { event_t *e = event_of(RAW[i].cont); if (!e) { bad++; continue; } for (int j = 0; j < e->n; j++) bad += show_clip(&clips[e->s[j].clip], e->s[j].vol, e->s[j].pitch); }
    { event_t *e = event_of(cont_key("crowd_individual_clap_to_beat")); if (!e) bad++; else for (int j = 0; j < e->n; j++) bad += show_clip(&clips[e->s[j].clip], e->s[j].vol, e->s[j].pitch); }
    /* and the mixer: a song's worth of everything at once, rendered without a device */
    sfx_song_begin("z_budokan", 2600); gh3_run r; memset(&r, 0, sizeof r); r.health = 1; double peak = 0; long n = 0;
    for (int i = 0; i < 44100 * 12 / 512; i++) { double ms = i * 512 / 44.1; r.health = ms < 3000 ? 1 : ms < 6000 ? 1.8 : 0.3; r.spOn = ms > 4000 && ms < 5500; r.hasLastBeat = 1; r.lastBeat = ms / 500; sfx_song_tick(&r, ms);
        if (i % 40 == 0) sfx_play(SFX_BAD); if (i == 200) sfx_play(SFX_STREAK); if (i == 600) sfx_song_end(0);
        for (int k = 0; k < 512; k++) { float l = 0, rr = 0; sfx_mix(&l, &rr, 0.8f, 1); if (fabs(l) > peak) peak = fabs(l); n += l != 0; } }
    printf("mixer: %ld non-zero frames of %d, peak %.2f, %zu bytes of PCM decoded\n", n, 44100 * 12 / 512 * 512, peak, pcmBytes); if (!n) bad++;
    sfx_song_stop(); printf("%d failures\n", bad); return bad;
}
