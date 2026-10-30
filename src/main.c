/* Guitar Hero III for the R36S: a native player for the game's own PC data files.
 * Reads DATA/ (qb.pak, the song paks, the FSB audio banks), judges notes the way the game does, keeps the career,
 * records and settings, draws through SDL_Renderer (GLES2 on the device) and plays the MP3 streams through SDL audio. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include "gh3fmt.h"
#include "game.h"
#include "audio.h"
#include "gfx.h"
#include "art.h"
#include "render.h"
#include "fx.h"
#include "ui.h"
#include "settings.h"
#include "input.h"
#include "career.h"
#include "app.h"
#include "practice.h"
#include "stretch.h"
#include "store.h"
#include "movie.h"
#include "battle.h"
#include "cheats.h"
#include "tutorial.h"
#include "sfx.h"
#ifdef GH3_3D
#include "r3d/r3d.h"   /* the 3D build (gh3-3d): the game's venues behind the highway */
static char r3dTest[32];
#endif

/* ------------------------------------------------------------------ files */
typedef struct { char lower[300]; char path[600]; size_t size; } fentry;
static fentry *files; static int nfiles, fcap;
static void lowercase(char *s) { for (; *s; s++) *s = (char)tolower((unsigned char)*s); }
static void index_dir(const char *root, const char *rel) {
    char full[600]; snprintf(full, sizeof full, "%s%s%s", root, *rel ? "/" : "", rel);
    DIR *d = opendir(full); if (!d) return; struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char sub[300]; snprintf(sub, sizeof sub, "%s%s%s", rel, *rel ? "/" : "", e->d_name);
        char p[600]; snprintf(p, sizeof p, "%s/%s", root, sub); struct stat st; if (stat(p, &st)) continue;
        if (S_ISDIR(st.st_mode)) { index_dir(root, sub); continue; }
        if (nfiles == fcap) { fcap = fcap ? fcap * 2 : 1024; files = realloc(files, fcap * sizeof *files); }
        fentry *f = &files[nfiles++]; snprintf(f->lower, sizeof f->lower, "%s", sub); lowercase(f->lower); snprintf(f->path, sizeof f->path, "%s", p); f->size = (size_t)st.st_size;
    }
    closedir(d);
}
static const fentry *find_file(const char *dir, const char *name) {
    char want[300]; snprintf(want, sizeof want, "%s/%s", dir, name); lowercase(want);
    for (int i = 0; i < nfiles; i++) if (!strcmp(files[i].lower, want)) return &files[i];
    return NULL;
}
static uint8_t *read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(n > 0 ? (size_t)n : 1); if (n > 0 && fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; } fclose(f); *len = n > 0 ? (size_t)n : 0; return b;
}
static uint8_t *read_data(const char *dir, const char *name, size_t *len) { const fentry *e = find_file(dir, name); if (!e) { *len = 0; return NULL; } return read_file(e->path, len); }

/* ------------------------------------------------------------------ config */
typedef struct {
    char data[520]; int W, H, fullscreen, vsync, soft;
    int frames, every; char song[64], tutorial[16]; int diff, part, autoPlay, dump, audiotest, menutest, screens, stretchtest, sfxtest; char outdir[520]; char movietest[64]; int movieplay; int miss;
} cfg_t;
static cfg_t cfg;
static int moneyTest;   /* --moneytest: the career's pay rules, the store data and the encore chants */
static void cfg_defaults(void) {
    memset(&cfg, 0, sizeof cfg); snprintf(cfg.data, sizeof cfg.data, "DATA"); cfg.W = 640; cfg.H = 480; cfg.vsync = 1; cfg.diff = 3; cfg.every = 30;
    const char *e;
    if ((e = getenv("GH3_DATA")) && *e) snprintf(cfg.data, sizeof cfg.data, "%s", e);
    if ((e = getenv("GH3_SCREEN")) && sscanf(e, "%dx%d", &cfg.W, &cfg.H) != 2) { cfg.W = 640; cfg.H = 480; }
    if ((e = getenv("GH3_FULLSCREEN"))) cfg.fullscreen = atoi(e);
    if ((e = getenv("GH3_VSYNC"))) cfg.vsync = atoi(e);
    if ((e = getenv("GH3_RENDER")) && !strcmp(e, "soft")) cfg.soft = 1;
}
static int diff_index(const char *s) { for (int i = 0; i < 4; i++) if (!strcmp(s, GH3_DIFF_NAME[i])) return i; return atoi(s); }
static void cfg_args(int argc, char **argv) {
    for (int i = 1; i < argc; i++) { const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : "";
        if (!strcmp(a, "--data")) { snprintf(cfg.data, sizeof cfg.data, "%s", v); i++; }
        else if (!strcmp(a, "--dump")) cfg.dump = 1;
        else if (!strcmp(a, "--audiotest")) cfg.audiotest = 1;
        else if (!strcmp(a, "--menutest")) cfg.menutest = 1;
        else if (!strcmp(a, "--sfxtest")) cfg.sfxtest = 1;
        else if (!strcmp(a, "--stretchtest")) cfg.stretchtest = 1;
        else if (!strcmp(a, "--moneytest")) moneyTest = 1;
        else if (!strcmp(a, "--movietest")) { snprintf(cfg.movietest, sizeof cfg.movietest, "%s", v); i++; }
        else if (!strcmp(a, "--movieplay")) cfg.movieplay = 1;
        else if (!strcmp(a, "--screens")) { cfg.screens = 1; snprintf(cfg.outdir, sizeof cfg.outdir, "%s", v); i++; }
        else if (!strcmp(a, "--tutorial")) { snprintf(cfg.tutorial, sizeof cfg.tutorial, "%s", v); i++; }
        else if (!strcmp(a, "--song")) { snprintf(cfg.song, sizeof cfg.song, "%s", v); i++; }
        else if (!strcmp(a, "--diff")) { cfg.diff = diff_index(v); i++; }
        else if (!strcmp(a, "--part")) { cfg.part = !strcmp(v, "rhythm") || !strcmp(v, "bass") ? 1 : atoi(v); i++; }
        else if (!strcmp(a, "--frames")) { cfg.frames = atoi(v); i++; }
        else if (!strcmp(a, "--every")) { cfg.every = atoi(v); i++; }
        else if (!strcmp(a, "--miss")) { cfg.miss = atoi(v); i++; }   /* a battle's autoplay misses that share of the notes */
        else if (!strcmp(a, "--out")) { snprintf(cfg.outdir, sizeof cfg.outdir, "%s", v); i++; }
        else if (!strcmp(a, "--auto")) cfg.autoPlay = 1;
        else if (!strcmp(a, "--tap")) SET.tap = 1;
        else if (!strcmp(a, "--theme")) { SET.theme = atoi(v); i++; }
        else if (!strcmp(a, "--soft")) cfg.soft = 1;
        else if (!strcmp(a, "--windowed")) cfg.fullscreen = 0;
        else if (!strcmp(a, "--fullscreen")) cfg.fullscreen = 1;
        else if (!strcmp(a, "--size")) { sscanf(v, "%dx%d", &cfg.W, &cfg.H); i++; }
#ifdef GH3_3D
        else if (!strcmp(a, "--r3dtest")) { snprintf(r3dTest, sizeof r3dTest, "%s", v); i++; }   /* --r3dtest z_dive --out DIR: a venue drawn offscreen, frames saved, counts and timings printed */
#endif
        else if (!strcmp(a, "--help")) { printf("gh3 [--data DIR] [--dump [--song NAME]] [--frames N --song NAME [--diff easy|medium|hard|expert] [--part guitar|rhythm] --out DIR] [--screens DIR] [--movietest NAME|all [--movieplay] --out DIR] [--stretchtest [--song NAME]] [--sfxtest] [--r3dtest ZONE --out DIR (gh3-3d)] [--tutorial 1a..4e|1..4 [--frames N --every K --out DIR]] [--auto] [--tap] [--soft] [--windowed] [--size WxH]\n"); exit(0); }
    }
}

/* ------------------------------------------------------------------ app state */
app_t A; static gh3_library lib; static char libErr[200]; static int running = 1;

static void sfx_hook(const char *name) {
    if (!strcmp(name, "bad")) audio_sfx(SFX_BAD); else if (!strcmp(name, "badBass")) audio_sfx(SFX_BAD_BASS); else if (!strcmp(name, "ready")) audio_sfx(SFX_READY);
    else if (!strcmp(name, "deployed")) audio_sfx(SFX_STAR_ON); else if (!strcmp(name, "staroff")) audio_sfx(SFX_STAR_OFF); else if (!strcmp(name, "awarded")) audio_sfx(SFX_AWARDED);
    else if (!strncmp(name, "streak", 6)) audio_sfx(!strcmp(name, "streak100") ? SFX_STREAK100 : SFX_STREAK); else if (!strncmp(name, "lose", 4)) audio_sfx(name[4] == '4' ? SFX_LOSE4 : name[4] == '3' ? SFX_LOSE3 : SFX_LOSE);   /* lose2..4: the multiplier lost */
}
static void mute_hook(int m) { audio_mute(BUS_PLAYER, m); }
static int resultCash;   /* what the last result paid: the Cash_Sound on the results screen */

void app_session_free(void) {
    session_t *S = &A.S; if (!audio_preview_on()) audio_clear(); /* a preview or menu track owns the mixer: none of the session's streams are on */ battle_free(); gh3_chart_free(&S->chart); gh3_songdata_free(&S->sd); free(S->pakBytes); free(S->fsbBytes); free(S->datBytes); free(S->fsb.streams);
    memset(S, 0, sizeof *S);
}
static int load_song(const gh3_song *song, int part, int diff, int autoPlay, int withAudio, char *err, size_t errCap) {
    app_session_free(); session_t *S = &A.S; S->song = song; S->diff = diff;
    char nm[128]; size_t len;
    snprintf(nm, sizeof nm, "%s_song.pak.xen", song->name);
    S->pakBytes = read_data("songs", nm, &len); if (!S->pakBytes) { snprintf(err, errCap, "Não achei a partitura SONGS/%s", nm); return -1; }
    if (gh3_songpak_read(S->pakBytes, len, song->name, &S->sd)) { snprintf(err, errCap, "Sem dados de partitura em %s", nm); return -1; }
    int p = part; if (!S->sd.parts[p][diff].raw) { p = p == 1 ? 0 : 1; if (!S->sd.parts[p][diff].raw) { snprintf(err, errCap, "Essa parte não tem notas em %s", diff_name_pt(diff)); return -1; } }
    S->part = p;
    if (gh3_chart_build(&S->sd, p, diff, song->hmScale > 0 ? song->hmScale : lib.tuning.hmScale, lib.tuning.susShorten, &S->chart)) { snprintf(err, errCap, "A partitura não tem notas"); return -1; }
    S->durationMs = 0;
    if (withAudio) {
        snprintf(nm, sizeof nm, "%s.fsb.xen", song->name); size_t flen, dlen;
        S->fsbBytes = read_data("music", nm, &flen);
        if (S->fsbBytes) {
            snprintf(nm, sizeof nm, "%s.dat.xen", song->name); S->datBytes = read_data("music", nm, &dlen);
            if (gh3_fsb_read(S->fsbBytes, flen, S->datBytes, dlen, song->name, &S->fsb) == 0) {
                int mine = (p == 1 && gh3_fsb_has(&S->fsb, FSB_RHYTHM)) ? FSB_RHYTHM : FSB_GUITAR;
                int order[3] = { mine, FSB_SONG, mine == FSB_RHYTHM ? FSB_GUITAR : FSB_RHYTHM };
                for (int k = 0; k < 3; k++) { size_t sz; const uint8_t *mp3 = gh3_fsb_extract(&S->fsb, order[k], &sz); if (mp3) audio_set_stream(k == 0 ? BUS_PLAYER : k == 1 ? BUS_BAND : BUS_OTHER, mp3, sz, k == 0 ? song->guitarVol : song->bandVol); }
                for (int i = 0; i < S->fsb.nstreams; i++) { gh3_fsbstream *st = &S->fsb.streams[i]; if (st->role >= 0 && st->role != FSB_PREVIEW && st->freq > 0) { double d = (double)st->samples / st->freq * 1000; if (d > S->durationMs) S->durationMs = d; } }
            } else SDL_Log("audio bank %s not understood", nm);
        } else SDL_Log("no audio bank for %s", song->name);
    }
    gh3_runopts o; memset(&o, 0, sizeof o); o.autoPlay = autoPlay; o.noStrum = SET.tap; o.silent = !withAudio; o.bass = p == 1; o.noFail = SET.noFail || A.mode == MODE_PRACTICE; o.hyper = SET.hyper;
    const gh3_note *last = &S->chart.notes[S->chart.nnotes - 1];
    double endMs = last->t + last->sus + 3000; if (S->durationMs > 1000 && S->durationMs < endMs) endMs = S->durationMs;
    if (withAudio && A.mode == MODE_PRACTICE) { /* practice: only the sections chosen, and no star power (the game has none in practice) */
        double from, to; practice_range(&S->sd, A.practiceSec, A.practiceEnd, endMs, &from, &to); gh3_chart *c = &S->chart; int k = 0;
        for (int i = 0; i < c->nnotes; i++) if (c->notes[i].t >= from - 1 && c->notes[i].t < to) { c->notes[k] = c->notes[i]; c->notes[k].star = -1; k++; }
        if (!k) { snprintf(err, errCap, "Esse trecho não tem notas nesta parte"); return -1; }
        c->nnotes = k; if (to + 120 < endMs) endMs = to + 120; }
    o.endMs = endMs; o.speed = 100;
    game_new(&S->run, &S->chart, cheats_tuning(&lib.tuning, A.mode), diff, &o);
    S->loaded = 1; return 0;
}
const char *app_section_at(double t) {
    session_t *S = &A.S; const gh3_marker *m = NULL; for (int i = 0; i < S->sd.nmarkers; i++) if (S->sd.markers[i].t <= t) m = &S->sd.markers[i]; else break;
    if (!m) return ""; return m->text[0] ? m->text : gh3_library_text(&lib, m->key);
}
uint8_t *app_read_data(const char *dir, const char *name, size_t *len) { return read_data(dir, name, len); }
const char *app_data_path(const char *dir, const char *name) { const fentry *e = find_file(dir, name); return e ? e->path : NULL; }
int app_load_song_named(const char *name, int diff, int autoPlay, int withAudio, char *err, size_t errCap) {
    for (int i = 0; i < lib.nsongs; i++) if (!strcasecmp(lib.songs[i].name, name)) return load_song(&lib.songs[i], 0, diff, autoPlay, withAudio, err, errCap);
    snprintf(err, errCap, "%s não está na lista de músicas", name); return -1;
}
int app_prepare_song(int songIdx) { char e[300]; if (load_song(&lib.songs[songIdx], A.selPart, A.selDiff, 1, 0, e, sizeof e)) { snprintf(A.errMsg, sizeof A.errMsg, "%s", e); A.screen = SC_ERROR; return -1; } return 0; }
void app_apply_volumes(void) { audio_set_master(1); audio_set_bus_gain(BUS_PLAYER, SET.volGuitar / 100.0); audio_set_bus_gain(BUS_BAND, SET.volMusic / 100.0); audio_set_bus_gain(BUS_OTHER, SET.volMusic / 100.0); audio_set_sfx_gain(SET.volSfx / 100.0 * 0.9); }
int app_song_lock(int tier, int si) { int st = store_song_lock(&lib, tier, si); if (st) return st; /* a bonus song not bought */ if (A.mode == MODE_CAREER) return career_song_lock(&lib, A.tiers, tier, si); return A.quickOpen && A.quickOpen[si] ? LOCK_OPEN : LOCK_TIER; }
void app_calib_start(int from) { app_preview(-1); if (audio_preview_on()) audio_preview_stop(0); app_apply_volumes(); audio_metronome(500, 1200); input_reset_axes(); A.calN = 0; A.calLastTapAt = -1e9; A.calFrom = from; A.screen = SC_CALIB; }
void app_calib_stop(void) { audio_stop(); }
static int cmp_d(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
int app_calib_result(void) { if (!A.calN) return SET.offsetMs; double t[24]; memcpy(t, A.calDiff, A.calN * sizeof *t); qsort(t, A.calN, sizeof *t, cmp_d); double m = A.calN & 1 ? t[A.calN / 2] : (t[A.calN / 2 - 1] + t[A.calN / 2]) / 2; int v = (int)lround(m); return v < -300 ? -300 : v > 300 ? 300 : v; }
void app_quit(void) { running = 0; }

/* ---- career ---- */
void app_refresh_career(int announce) {
    if (!A.libOk) return;
    tierstate_t *before = NULL; if (announce && A.tiers) { before = malloc(lib.ntiers * sizeof *before); memcpy(before, A.tiers, lib.ntiers * sizeof *before); }
    if (!A.tiers) A.tiers = calloc(lib.ntiers, sizeof *A.tiers);
    career_eval(&lib, A.selDiff, SET.unlockAll, A.tiers);
    { /* Quickplay and Practice: whatever the career opened on any difficulty */
      tierstate_t *ts = malloc(lib.ntiers * sizeof *ts); A.quickOpen = realloc(A.quickOpen, lib.nsongs ? lib.nsongs : 1); memset(A.quickOpen, 0, lib.nsongs);
      for (int d = 0; d < 4; d++) { career_eval(&lib, d, SET.unlockAll, ts); for (int t = 0; t < lib.ntiers; t++) for (int k = 0; k < lib.tiers[t].nsongs; k++) { int si = lib.tiers[t].songIdx[k]; if (career_song_lock(&lib, ts, t, si) == LOCK_OPEN) A.quickOpen[si] = 1; } }
      free(ts); }
    store_refresh(&lib);
    if (before) { A.unlockMsg[0] = 0;
        for (int t = 0; t < lib.ntiers; t++) { const gh3_tier *tr = &lib.tiers[t];
            if (!before[t].open && A.tiers[t].open) snprintf(A.unlockMsg, sizeof A.unlockMsg, "Novo set liberado: %s", tr->title);
            else if (!before[t].encoreOpen && A.tiers[t].encoreOpen) snprintf(A.unlockMsg, sizeof A.unlockMsg, "Encore liberado: %s", lib.songs[tr->encoreIdx].title);
            else if (!before[t].bossOpen && A.tiers[t].bossOpen) snprintf(A.unlockMsg, sizeof A.unlockMsg, "Batalha liberada!");
            if (tr->career && !before[t].done && A.tiers[t].done && tr->movie[0]) snprintf(A.moviePending, sizeof A.moviePending, "%s", tr->movie); }   /* shown on the way back to the set list */
        if (A.unlockMsg[0]) A.unlockPending = 1; free(before); }
}

/* ---- menu audio: the set list's song previews, otherwise the game's own menu music (STREAMS/streamall, the
 * menu_music_* tracks in a shuffled row, and a guitar lick on the title screen's Start). Both are read by a worker
 * thread and started on the main thread through the mixer's one preview stream. */
enum { JOB_PREVIEW, JOB_MENU };
static SDL_Thread *prevThread; static int prevWant = -1, prevPlaying = -1; static double prevWantAt;
static struct { int kind, idx; uint8_t *fsb, *dat; size_t fl, dl; int done; } prevJob;
static uint8_t *prevBytes, *prevDat; static gh3_fsb prevFsb;
static gh3_fsb bank; static char bankPath[600]; static int bankFound;   /* the streamall index: headers only */
static int menuTracks[24], nMenu, licks[8], nLicks, menuPlaying, menuLast = -1, lickWanted; static uint8_t *menuBytes;
static void bank_index(void) {
    const fentry *e = find_file("streams", "streamall.fsb.xen"); if (!e) return;
    FILE *f = fopen(e->path, "rb"); if (!f) return; uint8_t h[24];
    if (fread(h, 1, 24, f) == 24) { int plain = h[0] == 'F' && h[1] == 'S' && h[2] == 'B'; if (!plain) gh3_fsb_descramble(h, 24, 0);
        uint32_t sh = h[8] | h[9] << 8 | h[10] << 16 | (uint32_t)h[11] << 24;
        if (!memcmp(h, "FSB3", 4) && sh < (8u << 20)) { uint8_t *hdr = malloc(24 + sh); fseek(f, 0, SEEK_SET);   /* gh3_fsb_read descrambles the header itself */
            if (fread(hdr, 1, 24 + sh, f) == 24 + sh && gh3_fsb_read(hdr, e->size, NULL, 0, NULL, &bank) == 0) { bankFound = 1; snprintf(bankPath, sizeof bankPath, "%s", e->path); } else free(hdr); } }
    fclose(f); if (!bankFound) return;
    for (int i = 0; i < bank.nstreams; i++) { const char *n = bank.streams[i].name;
        if (!strncmp(n, "menu_music_", 11) && nMenu < 24) menuTracks[nMenu++] = i; else if (!strncmp(n, "guitar_lick_", 12) && nLicks < 8) licks[nLicks++] = i; }
    SDL_Log("menu music: %d tracks, %d licks", nMenu, nLicks);
}
static int preview_worker(void *ud) { (void)ud; char nm[128];
    if (prevJob.kind == JOB_MENU) { const gh3_fsbstream *st = &bank.streams[prevJob.idx]; FILE *f = fopen(bankPath, "rb");
        if (f) { uint8_t *b = malloc(st->size ? st->size : 1); if (fseek(f, (long)st->offset, SEEK_SET) == 0 && fread(b, 1, st->size, f) == st->size) { if (!bank.plain) gh3_fsb_descramble(b, st->size, st->offset); prevJob.fsb = b; prevJob.fl = st->size; } else free(b); fclose(f); }
        prevJob.done = 1; return 0; }
    const gh3_song *s = &lib.songs[prevJob.idx];
    snprintf(nm, sizeof nm, "%s.fsb.xen", s->name); prevJob.fsb = read_data("music", nm, &prevJob.fl);
    snprintf(nm, sizeof nm, "%s.dat.xen", s->name); prevJob.dat = read_data("music", nm, &prevJob.dl);
    prevJob.done = 1; return 0; }
void app_preview(int idx) {
    if (!SET.preview || !audio_has_device()) idx = -1;
    if (idx == prevWant) return;
    prevWant = idx; prevWantAt = SDL_GetTicks() + 450;
    if ((idx >= 0 && idx != prevPlaying) || (idx < 0 && prevPlaying >= 0)) { if (audio_preview_on()) audio_preview_stop(0.4); prevPlaying = -1; menuPlaying = 0; }
}
void app_guitar_lick(void) { if (!nLicks || !SET.menuMusic) return; lickWanted = 1; if (menuPlaying && audio_preview_on()) audio_preview_stop(0.25); menuPlaying = 0; }
static int menu_music_wanted(void) {
    if (!SET.menuMusic || !bankFound || !nMenu || !audio_has_device()) return 0;
    if (A.screen == SC_SETLIST && prevWant >= 0) return 0;
    switch (A.screen) { case SC_TITLE: case SC_MAIN: case SC_TRAINING: case SC_SETLIST: case SC_SETUP: case SC_TOPROCK: case SC_ERROR: case SC_STORE: return 1;
        case SC_OPTIONS: case SC_CONTROLS: case SC_CHEATS: return A.optionsFrom != SC_PAUSE; case SC_TUTORIAL: return !tutorial_busy(); default: return 0; }
}
static void preview_tick(void) {
    double now = SDL_GetTicks();
    if (prevThread && prevJob.done) { SDL_WaitThread(prevThread, NULL); prevThread = NULL;
        if (prevJob.kind == JOB_MENU) {
            if (prevJob.fsb && menu_music_wanted() && !audio_preview_on()) { audio_preview_stop(0); free(menuBytes); menuBytes = prevJob.fsb; prevJob.fsb = NULL; audio_preview(menuBytes, prevJob.fl, -2, 0); menuPlaying = 1; prevPlaying = -1; }
        } else if (prevJob.idx == prevWant && prevWant >= 0 && A.screen == SC_SETLIST && prevJob.fsb) {
            audio_preview_stop(0); /* the callback is done with the old bytes once this returns */ free(prevBytes); free(prevDat); free(prevFsb.streams); memset(&prevFsb, 0, sizeof prevFsb); prevBytes = prevJob.fsb; prevDat = prevJob.dat; prevJob.fsb = prevJob.dat = NULL; menuPlaying = 0;
            if (gh3_fsb_read(prevBytes, prevJob.fl, prevDat, prevJob.dl, lib.songs[prevJob.idx].name, &prevFsb) == 0) { size_t sz; const uint8_t *mp3 = gh3_fsb_extract(&prevFsb, FSB_PREVIEW, &sz);
                if (!mp3) mp3 = gh3_fsb_extract(&prevFsb, FSB_SONG, &sz);
                if (mp3) { audio_preview(mp3, sz, -4, 1); prevPlaying = prevJob.idx; } } }
        free(prevJob.fsb); free(prevJob.dat); prevJob.fsb = prevJob.dat = NULL; }
    if (menuPlaying && !audio_preview_on()) menuPlaying = 0;   /* the track ended (or a song took the mixer) */
    int menuWant = menu_music_wanted();
    if (menuPlaying && !menuWant) { audio_preview_stop(0.6); menuPlaying = 0; }
    if (prevThread) return;
    if (prevWant >= 0 && prevWant != prevPlaying && now >= prevWantAt && A.screen == SC_SETLIST) { memset(&prevJob, 0, sizeof prevJob); prevJob.kind = JOB_PREVIEW; prevJob.idx = prevWant; prevThread = SDL_CreateThread(preview_worker, "preview", NULL); }
    else if (menuWant && !menuPlaying && !audio_preview_on()) { int pick;
        if (lickWanted) { pick = licks[rand() % nLicks]; lickWanted = 0; }
        else { do pick = menuTracks[rand() % nMenu]; while (nMenu > 1 && pick == menuLast); menuLast = pick; }
        memset(&prevJob, 0, sizeof prevJob); prevJob.kind = JOB_MENU; prevJob.idx = pick; prevThread = SDL_CreateThread(preview_worker, "menu-music", NULL); }
}
uint8_t *app_stream_load(const char *name, size_t *size) {
    *size = 0; if (!bankFound) return NULL;
    for (int i = 0; i < bank.nstreams; i++) { const gh3_fsbstream *st = &bank.streams[i]; if (strcmp(st->name, name)) continue;
        FILE *f = fopen(bankPath, "rb"); if (!f) return NULL; uint8_t *b = malloc(st->size ? st->size : 1);
        if (fseek(f, (long)st->offset, SEEK_SET) || fread(b, 1, st->size, f) != st->size) { fclose(f); free(b); return NULL; }
        fclose(f); if (!bank.plain) gh3_fsb_descramble(b, st->size, st->offset); *size = st->size; return b; }
    return NULL;
}
int app_play_stream(const char *name, double levelDb) {
    size_t n; uint8_t *b = app_stream_load(name, &n); if (!b) return -1;
    app_preview(-1); audio_preview_stop(0); free(menuBytes); menuBytes = b; audio_preview(menuBytes, n, levelDb, 0); menuPlaying = 1; prevPlaying = -1; menuLast = -1;
    return 0;
}
static void menu_selftest(void) {   /* --menutest: reads the menu music index and decodes each track and lick */
    bank_index(); if (!bankFound) { puts("no STREAMS/streamall.fsb.xen"); return; }
    for (int k = 0; k < nMenu + nLicks; k++) { memset(&prevJob, 0, sizeof prevJob); prevJob.kind = JOB_MENU; prevJob.idx = k < nMenu ? menuTracks[k] : licks[k - nMenu]; preview_worker(NULL);
        printf("%s: ", bank.streams[prevJob.idx].name); fflush(stdout); if (!prevJob.fsb) { puts("not read"); continue; }
        audio_preview(prevJob.fsb, prevJob.fl, -2, 0); audio_selftest(); audio_stop(); free(prevJob.fsb); prevJob.fsb = NULL; }
}
static void preview_shutdown(void) { if (prevThread) { SDL_WaitThread(prevThread, NULL); prevThread = NULL; } audio_preview_stop(0); free(prevJob.fsb); free(prevJob.dat); free(prevBytes); free(prevDat); free(prevFsb.streams); free(menuBytes); free(bank.streams); free(bank.bytes); }

/* ---- movies (DATA/MOVIES/R36S/<name>.mpg): the boot logos and the intro, the career's cutscenes. The menu music
 * stops while one plays (menu_music_wanted() is 0 on SC_MOVIE) and comes back on the screen it returns to. */
static const char *const BOOT_MOVIES[] = { "atvi", "ro_logo", "ns_logo", "intro" };
static int bootNext = -1, movieReturn = SC_MAIN; static double movieAt;
static const fentry *movie_file(const char *name) { char nm[96]; snprintf(nm, sizeof nm, "%s.mpg", name); return find_file("movies/r36s", nm); }
int app_movie_exists(const char *name) { return name && *name && movie_file(name) != NULL; }
static int movie_start(const char *name) {
    const fentry *e = name && *name ? movie_file(name) : NULL; if (!e || movie_open(e->path, A.W, A.H)) return 0;
    app_preview(-1); audio_preview_stop(0); menuPlaying = 0; lickWanted = 0; A.screen = SC_MOVIE; movieAt = SDL_GetTicks(); return 1;
}
void app_play_movie(const char *name, int returnScreen) { bootNext = -1; movieReturn = returnScreen; if (!movie_start(name)) screens_enter(returnScreen, SDL_GetTicks()); }
static void movie_finish(double now, int all) {   /* all: Start skips the rest of the boot sequence too */
    movie_close();
    if (!all) while (bootNext >= 0 && bootNext < (int)(sizeof BOOT_MOVIES / sizeof *BOOT_MOVIES)) if (movie_start(BOOT_MOVIES[bootNext++])) return;
    bootNext = -1; screens_enter(movieReturn, now);
}
static void boot_movies(void) { bootNext = 0; movieReturn = SC_TITLE; movie_finish(SDL_GetTicks(), 0); }
static void movie_event(const SDL_Event *ev, double now) {   /* A or B skip one movie, Start the whole boot sequence, after 0.6 s */
    int skip = 0, all = 0;
    if (ev->type == SDL_CONTROLLERBUTTONDOWN) { int b = ev->cbutton.button; if (b == SDL_CONTROLLER_BUTTON_START) skip = all = 1; else if (b == SDL_CONTROLLER_BUTTON_A || b == SDL_CONTROLLER_BUTTON_B) skip = 1; }
    else if (ev->type == SDL_KEYDOWN && !ev->key.repeat) { int k = ev->key.keysym.sym; if (k == SDLK_ESCAPE) skip = all = 1; else if (k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_BACKSPACE) skip = 1; }
    if (skip && now - movieAt >= 600) movie_finish(now, all);
}
static void save_named(const char *name);
static void movie_test(void) {   /* --movietest NAME|all [--movieplay]: decodes each movie, the middle frame to <out>/movie_<name>.bmp */
    if (cfg.movieplay) { SDL_Init(SDL_INIT_AUDIO); if (audio_open()) puts("no audio device: the wall clock runs the movie"); app_apply_volumes(); }
    gfx_init(NULL, cfg.W, cfg.H); A.W = cfg.W; A.H = cfg.H; int n = 0;
    for (int i = 0; i < nfiles; i++) { const char *l = files[i].lower; size_t L = strlen(l);
        if (strncmp(l, "movies/r36s/", 12) || L < 17 || strcmp(l + L - 4, ".mpg")) continue;
        char nm[96]; snprintf(nm, sizeof nm, "%.*s", (int)(L - 16), l + 12); if (strcmp(cfg.movietest, "all") && strcasecmp(cfg.movietest, nm)) continue;
        n++; gfx_begin(); if (movie_selftest(files[i].path, cfg.W, cfg.H) == 0) { char o[120]; snprintf(o, sizeof o, "movie_%s", nm); save_named(o); }
        SDL_Renderer *ren = NULL; SDL_Window *win = NULL;   /* GH3_MOVIE_GPU=1: through a renderer's YUV texture (a hidden window), read back */
        if (cfg.movieplay && getenv("GH3_MOVIE_GPU") && SDL_InitSubSystem(SDL_INIT_VIDEO) == 0 && (win = SDL_CreateWindow("movietest", 0, 0, cfg.W, cfg.H, SDL_WINDOW_HIDDEN))) { ren = SDL_CreateRenderer(win, -1, 0); if (ren) { SDL_RendererInfo ri; SDL_GetRendererInfo(ren, &ri); printf("  renderer: %s\n", ri.name); gfx_init(ren, cfg.W, cfg.H); } }
        if (cfg.movieplay && movie_open(files[i].path, cfg.W, cfg.H) == 0) {   /* the real pipeline: worker thread, ring, clock */
            double t0 = SDL_GetTicks(), now = t0; int ticks = 0; while (movie_tick(now = SDL_GetTicks()) && now - t0 < 200000) { gfx_begin(); movie_draw(cfg.W, cfg.H); ticks++;
                if (ren && ticks == 90) { SDL_Surface *s = SDL_CreateRGBSurface(0, cfg.W, cfg.H, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0); SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch); char p[700]; snprintf(p, sizeof p, "%s/movie_gpu_%s.bmp", cfg.outdir, nm); SDL_SaveBMP(s, p); SDL_FreeSurface(s); }
                gfx_end(); SDL_Delay(16); if (getenv("GH3_MOVIE_DEBUG") && ticks % 60 == 0) printf("    wall %.2f s, sound heard %.2f s\n", (now - t0) / 1000, audio_movie_ms() / 1000); }
            printf("  played in real time: %.2f s, %d ticks%s\n", (now - t0) / 1000, ticks, movie_has_audio() ? " (clock: its sound)" : " (clock: the wall)"); movie_close(); }
        if (ren) { SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); gfx_init(NULL, cfg.W, cfg.H); } }
    if (!n) printf("no MOVIES/R36S/%s.mpg under %s (tools/convert_movies.sh makes them)\n", cfg.movietest, cfg.data);
}

/* ---- playing ---- */
void app_begin_song(void) {
    char err[300]; app_preview(-1); if (audio_preview_on()) audio_preview_stop(0);
#ifdef GH3_3D
    r3d_select(A.selTier >= 0 && A.selTier < lib.ntiers ? lib.shop.tierZone[A.selTier] : "");   /* the set's venue (levelzones); a bonus song with none gets the default */
#endif
    if (battle_intro_start(A.selSong, SDL_GetTicks())) return;   /* a boss: his intro first (it comes back here) */
    if (load_song(&lib.songs[A.selSong], A.selPart, A.selDiff, A.autoPlay, 1, err, sizeof err)) { snprintf(A.errMsg, sizeof A.errMsg, "%s", err); A.screen = SC_ERROR; return; }
    app_apply_volumes();
    if (A.mode == MODE_PRACTICE) { /* the notes outside the sections are gone (load_song); the clock runs at the speed chosen */
        double sp = PRACTICE_SPEED[A.practiceSpeed >= 0 && A.practiceSpeed < PRACTICE_SPEEDS ? A.practiceSpeed : 0], from, to, at, lead;
        practice_range(&A.S.sd, A.practiceSec, A.practiceEnd, 1e12, &from, &to); practice_lead(from, sp, &at, &lead); audio_set_speed(sp); audio_play(at, lead); sfx_song_begin(NULL, lead / sp); }   /* (no crowd in practice) */
    else { double from = 0, lead = 2600; battle_start(A.selSong, &from, &lead); audio_play(from, lead);   /* a boss song is a battle (or the devil's last part) */
        sfx_song_begin(A.selTier >= 0 && A.selTier < lib.ntiers ? lib.shop.tierZone[A.selTier] : "", lead); }   /* the venue's crowd, the highway's rise and the intro kick */
    input_reset_axes(); input_action_buttons(ACT_STAR, A.starKeys, sizeof A.starKeys);
    A.screen = SC_PLAY; A.S.section[0] = 0; A.resumeAt = 0; A.S.run.held = 0; fx_clear();
}
void app_resume_play(void) { A.screen = SC_PLAY; A.resumeAt = SDL_GetTicks() + 1600; audio_sfx(SFX_TICK); }
/* the clock notes are drawn and judged on: what is heard, less the lag the player measured (Calibrar atraso) */
static double song_time(void) { return audio_song_ms() - SET.offsetMs * audio_speed(); }   /* (the lag is real time: slowed, fewer song ms) */
static int heldMask = 0;
static void play_event(SDL_Event *ev, double now, double songT) {
    double te = songT - (now - ev->common.timestamp) * audio_speed(); /* the song's clock when the press happened */
    gh3_run *r = &A.S.run; inev_t evs[4]; int n = input_translate(ev, evs, 4);
    for (int i = 0; i < n; i++) { int a = evs[i].action, pr = evs[i].pressed;
        if (a <= ACT_FRET4) { if (pr) heldMask |= 1 << a; else heldMask &= ~(1 << a); game_set_frets(r, heldMask, te, now); }
        else if (a == ACT_STRUM_UP || a == ACT_STRUM_DOWN) { if (pr && A.resumeAt == 0) game_strum(r, te, now); }
        else if (a == ACT_STAR) { if (pr) game_star(r, now); }
        else if (a == ACT_WHAMMY) game_whammy(r, evs[i].value, now);
        else if (a == ACT_PAUSE) { if (pr) { if (A.resumeAt == 0) audio_pause(1); A.resumeAt = 0; A.screen = SC_PAUSE; A.pauseRow = 0; audio_sfx(SFX_BACK); } } }
}
static int menu_event(SDL_Event *ev) {
    static int axisState = 0;
    if (ev->type == SDL_KEYDOWN && !ev->key.repeat) { switch (ev->key.keysym.sym) {
        case SDLK_UP: return IN_UP; case SDLK_DOWN: return IN_DOWN; case SDLK_LEFT: return IN_LEFT; case SDLK_RIGHT: return IN_RIGHT;
        case SDLK_RETURN: case SDLK_SPACE: return IN_OK; case SDLK_ESCAPE: case SDLK_BACKSPACE: return IN_BACK; case SDLK_x: return IN_X; case SDLK_y: return IN_Y; case SDLK_PAGEUP: case SDLK_q: return IN_L; case SDLK_PAGEDOWN: case SDLK_e: return IN_R; case SDLK_TAB: return IN_SELECT; } }
    if (ev->type == SDL_CONTROLLERBUTTONDOWN) { switch (ev->cbutton.button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return IN_UP; case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return IN_DOWN; case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return IN_LEFT; case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return IN_RIGHT;
        case SDL_CONTROLLER_BUTTON_A: case SDL_CONTROLLER_BUTTON_START: return IN_OK; case SDL_CONTROLLER_BUTTON_B: return IN_BACK; case SDL_CONTROLLER_BUTTON_X: return IN_X; case SDL_CONTROLLER_BUTTON_Y: return IN_Y;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return IN_L; case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return IN_R; case SDL_CONTROLLER_BUTTON_BACK: return IN_SELECT; } }
    if (ev->type == SDL_CONTROLLERAXISMOTION && ev->caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) { int v = ev->caxis.value; int st = v < -20000 ? -1 : v > 20000 ? 1 : 0; int was = axisState; if (st || (v > -8000 && v < 8000)) axisState = st; if (st && st != was) return st < 0 ? IN_UP : IN_DOWN; }
    return IN_NONE;
}
static SDL_GameController *pad;
static void open_pads(void) { for (int i = 0; i < SDL_NumJoysticks(); i++) if (SDL_IsGameController(i)) { pad = SDL_GameControllerOpen(i); if (pad) { SDL_Log("controller: %s", SDL_GameControllerName(pad)); break; } } }

void app_finish_song(double now) {
    if (battle_end(now)) return;   /* a battle's banner first; it comes back here */
    gh3_run *r = &A.S.run; int total = r->nnotes; A.resultPct = total ? (int)(100.0 * r->hits / total + 0.5) : 0; A.resultScore = (long)r->score;
    A.resultStars = r->failed || BTL.on ? 0 : game_stars(r->score, A.S.chart.base);   /* (a battle is won or lost, without stars) */ A.resultsAt = now; A.starsSounded = 0; A.resRow = r->failed ? 0 : 1; A.newRecord = 0;
    sfx_song_end(r->failed); if (!r->failed && A.mode != MODE_PRACTICE) audio_sfx(SFX_ROCK);   /* You Rock! (and the crowd's applause after it), or the boo */
    if (r->failed) { audio_fade_out(1.5); audio_sfx(SFX_FAIL); }
    else if (A.mode == MODE_PRACTICE) audio_fade_out(0.8);   /* the run may end mid-song, at its last section */
    else if (!r->autoPlay && !SET.noFail && cheats_records_ok(A.mode) && A.mode != MODE_PRACTICE) { A.newRecord = records_put(A.S.song->name, A.S.diff, A.S.part, A.resultScore, A.resultPct, r->best, A.resultStars, A.mode == MODE_CAREER); app_refresh_career(A.mode == MODE_CAREER); }
    resultCash = store_result(A.S.song, A.S.diff, A.resultStars, !r->failed && !r->autoPlay && !SET.noFail && A.mode == MODE_CAREER);   /* the career's pay (none elsewhere); its Cash_Sound comes with the stars */
    A.screen = SC_RESULTS;
}

/* ------------------------------------------------------------------ headless modes */
static void dump(void) {
    printf("songs: %d  tiers: %d  career movie: %s\n", lib.nsongs, lib.ntiers, lib.careerMovie[0] ? lib.careerMovie : "-");
    for (int t = 0; t < lib.ntiers; t++) { const gh3_tier *tr = &lib.tiers[t]; printf("== %s%s (default unlocked %d%s%s%s%s)\n", tr->title, tr->career ? " [career]" : "", tr->defaultUnlocked, tr->bossIdx >= 0 ? ", boss" : "", tr->encoreIdx >= 0 ? ", encore" : "", tr->movie[0] ? ", movie " : "", tr->movie);
        for (int k = 0; k < tr->nsongs; k++) { const gh3_song *s = &lib.songs[tr->songIdx[k]]; printf("   %-24s %s - %s (%s)%s%s%s\n", s->name, s->title, s->artist, s->year, s->hasRhythm ? " [bass]" : "", tr->songIdx[k] == tr->bossIdx ? " [BOSS]" : "", tr->songIdx[k] == tr->encoreIdx ? " [ENCORE]" : ""); } }
    const gh3_tuning *tn = &lib.tuning;
    printf("tuning: early %.0f late %.0f hamEarly %.0f hamLate %.0f strumBefore %.0f formBefore %.0f susShorten %.2f split %.2f spDrain %.1f hmScale %.2f\n", tn->early, tn->late, tn->hamEarly, tn->hamLate, tn->strumBefore, tn->formBefore, tn->susShorten, tn->split, tn->spDrain, tn->hmScale);
    for (int d = 0; d < 4; d++) printf("  %-6s lookahead %.3f good %.4f bad %.4f star %.4f\n", GH3_DIFF_NAME[d], tn->diffs[d].lookahead, tn->diffs[d].good, tn->diffs[d].bad, tn->diffs[d].star);
    bank_index(); battle_dump(&lib);
    if (!*cfg.song) return;
    int si = -1; for (int i = 0; i < lib.nsongs; i++) if (!strcmp(lib.songs[i].name, cfg.song)) si = i;
    if (si < 0) { printf("song %s not in the list\n", cfg.song); return; }
    char err[300]; if (load_song(&lib.songs[si], cfg.part, cfg.diff, 1, 1, err, sizeof err)) { printf("load: %s\n", err); return; }
    session_t *S = &A.S;
    printf("song %s: fretbars %d timesig %d markers %d\n", S->sd.name, S->sd.nfret, S->sd.nsig, S->sd.nmarkers);
    for (int p = 0; p < 4; p++) for (int d = 0; d < 4; d++) if (S->sd.parts[p][d].raw) printf("  part %-10s %-6s notes %d star phrases %d\n", GH3_PART_NAME[p], GH3_DIFF_NAME[d], S->sd.parts[p][d].nraw / 3, S->sd.parts[p][d].nstar);
    int hopo = 0, sus = 0, chords = 0; for (int i = 0; i < S->chart.nnotes; i++) { hopo += S->chart.notes[i].hopo; sus += S->chart.notes[i].sus > 0; chords += S->chart.notes[i].n > 1; }
    printf("chart %s/%s: %d notes, %d hopos, %d sustains, %d chords, %d phrases, base %.0f, end %d ms\n", GH3_PART_NAME[cfg.part], GH3_DIFF_NAME[cfg.diff], S->chart.nnotes, hopo, sus, chords, S->chart.nphrases, S->chart.base, S->chart.end);
    for (int i = 0; i < 8 && i < S->chart.nnotes; i++) printf("  note %d: t %d len %d mask %02x hopo %d sus %d star %d\n", i, S->chart.notes[i].t, S->chart.notes[i].len, S->chart.notes[i].mask, S->chart.notes[i].hopo, S->chart.notes[i].sus, S->chart.notes[i].star);
    for (int i = 0; i < S->sd.nmarkers && i < 6; i++) printf("  marker %d ms: %s\n", S->sd.markers[i].t, app_section_at(S->sd.markers[i].t));
    printf("audio: %d streams, duration %.1f s\n", S->fsb.nstreams, S->durationMs / 1000);
    for (int i = 0; i < S->fsb.nstreams; i++) { const gh3_fsbstream *st = &S->fsb.streams[i]; printf("  stream %d: %-28s role %d samples %u freq %u ch %u size %u\n", i, st->name, st->role, st->samples, st->freq, st->channels, st->size); }
}
static void save_named(const char *name) {
    fb_t *fb = gfx_soft_fb();
    SDL_Surface *s = SDL_CreateRGBSurfaceFrom(fb->px, fb->w, fb->h, 32, fb->w * 4, 0x00FF0000, 0x0000FF00, 0x000000FF, 0);
    char p[700]; snprintf(p, sizeof p, "%s/%s.bmp", *cfg.outdir ? cfg.outdir : ".", name); SDL_SaveBMP(s, p); SDL_FreeSurface(s);
}
static void headless_init(void) { gfx_init(NULL, cfg.W, cfg.H); art_init(cfg.H >= 400 ? cfg.H / 240 : 1); fx_init(art_px()); render_init(cfg.W, cfg.H); A.W = cfg.W; A.H = cfg.H; A.lib = &lib; A.libOk = 1; app_refresh_career(0); screens_build_rows(); }
static void screens_mode(void) { /* the menus, drawn headlessly for a look */
    headless_init(); A.selDiff = 1; app_refresh_career(0);
    A.cursor = 0; while (A.cursor < A.nrows && A.rows[A.cursor].kind == 0) A.cursor++; A.cursor += 2; A.selSong = A.rows[A.cursor].idx; A.selTier = A.rows[A.cursor].tier;
    A.screen = SC_TITLE; A.titleAt = -400; gfx_begin(); screens_draw(0); gfx_end(); save_named("title");
    A.screen = SC_MAIN; A.mainRow = 2; gfx_begin(); screens_draw(0); gfx_end(); save_named("main");
    A.screen = SC_TRAINING; A.trainRow = 1; gfx_begin(); screens_draw(0); gfx_end(); save_named("training");
    A.screen = SC_TOPROCK; A.topDiff = 1; gfx_begin(); screens_draw(0); gfx_end(); save_named("toprockers");
    for (int i = 0; i < lib.nsongs; i++) if (!strcmp(lib.songs[i].name, "bosstom")) { BTL.songIdx = i; BTL.phase = BPH_INTRO; BTL.phaseAt = -2000; BTL.phaseMs = 15000; A.screen = SC_BATTLE; gfx_begin(); screens_draw(0); gfx_end(); save_named("battle_intro"); BTL.phase = BPH_NONE; }
    A.screen = SC_OPTIONS; A.optCat = -1; A.optRow = 0; gfx_begin(); screens_draw(0); gfx_end(); save_named("options_menu");
    A.screen = SC_SETLIST; A.mode = MODE_CAREER; gfx_begin(); screens_draw(0); gfx_end(); save_named("setlist");
    A.mode = MODE_QUICK; gfx_begin(); screens_draw(0); gfx_end(); save_named("setlist_quick"); A.mode = MODE_CAREER;
    { int c0 = A.cursor; store_demo(2350);   /* the bonus songs with their prices, the store (a pretend balance: nothing saved) */
      for (int i = 0; i < A.nrows; i++) if (A.rows[i].kind == 0 && lib.tiers[A.rows[i].tier].tab == 1) { A.cursor = i + 3; A.scroll = i; break; }
      gfx_begin(); screens_draw(0); gfx_end(); save_named("setlist_bonus"); A.mode = MODE_QUICK; gfx_begin(); screens_draw(0); gfx_end(); save_named("setlist_bonus_quick"); A.mode = MODE_CAREER;
      store_open(0); store_event(IN_DOWN, 0); gfx_begin(); screens_draw(0); gfx_end(); save_named("store"); store_event(IN_OK, 0); gfx_begin(); screens_draw(0); gfx_end(); save_named("store_buy"); store_event(IN_BACK, 0);
      store_event(IN_RIGHT, 0); gfx_begin(); screens_draw(0); gfx_end(); save_named("store_videos"); store_event(IN_LEFT, 0); store_event(IN_BACK, 0); A.cursor = c0; A.scroll = 0; }
    if (app_prepare_song(A.selSong)) { fprintf(stderr, "load: %s\n", A.errMsg); return; }
    A.screen = SC_SETUP; A.setupRow = 0; gfx_begin(); screens_draw(0); gfx_end(); save_named("setup");
    A.mode = MODE_PRACTICE; A.practiceSec = 3; A.practiceEnd = 5; A.practiceSpeed = 2; A.setupRow = 4; gfx_begin(); screens_draw(0); gfx_end(); save_named("setup_practice"); A.mode = MODE_CAREER; A.practiceSec = A.practiceEnd = A.practiceSpeed = 0;
    A.screen = SC_OPTIONS; A.optCat = 0; A.optRow = 1; gfx_begin(); screens_draw(0); gfx_end(); save_named("options");
    A.optCat = 3; A.optRow = 99; gfx_begin(); screens_draw(0); gfx_end(); save_named("options_video");   /* Vídeo, on its last row (Vídeos de abertura) */
    A.screen = SC_CONTROLS; A.ctlRow = 3; A.testHeld = 0x0A; A.testWhammy = 0.6f; gfx_begin(); screens_draw(0); gfx_end(); save_named("controls");
    A.optionsFrom = SC_MAIN; A.optRow = 5; cheats_open(); gfx_begin(); screens_draw(0); gfx_end(); save_named("cheats");
    { /* the Hyperspeed code of the game (O B O Y O B O Y) typed on the keyboard frets, to see the matcher unlock it */
      static const SDL_Keycode code[8] = { SDLK_5, SDLK_4, SDLK_5, SDLK_3, SDLK_5, SDLK_4, SDLK_5, SDLK_3 }; int hyper = SET.hyper; SET.hyper = 0; screens_event(IN_OK, 1000);
      for (int k = 0; k < 8; k++) { SDL_Event e; memset(&e, 0, sizeof e); double at = 2000 + k * 300;
          e.type = SDL_KEYDOWN; e.key.keysym.sym = code[k]; screens_raw_event(&e, at); e.key.keysym.sym = SDLK_DOWN; screens_raw_event(&e, at + 50);
          e.type = SDL_KEYUP; screens_raw_event(&e, at + 80); e.key.keysym.sym = code[k]; screens_raw_event(&e, at + 100); }
      gfx_begin(); screens_draw(4500); gfx_end(); save_named("cheats_code"); printf("cheat code test: hyperspeed %d, codes 0x%x\n", SET.hyper, SET.cheatCodes); SET.hyper = hyper; }
    tutorial_open(0); gfx_begin(); screens_draw(0); gfx_end(); save_named("tutorials");
    A.screen = SC_CALIB; A.calN = 9; for (int i = 0; i < 9; i++) A.calDiff[i] = 40 + (i % 3) * 6; A.calLastDiff = 46; A.calLastTapAt = -100; gfx_begin(); screens_draw(0); gfx_end(); save_named("calib");
    double t = -2000, perf = 0; for (; t < 14000; t += 1000.0 / 60, perf += 1000.0 / 60) game_step(&A.S.run, t, perf);
    snprintf(A.S.section, sizeof A.S.section, "%s", app_section_at(t)); A.screen = SC_PAUSE; gfx_begin(); screens_draw(perf); gfx_end(); save_named("pause");
    for (; !A.S.run.finished; t += 1000.0 / 60, perf += 1000.0 / 60) game_step(&A.S.run, t, perf);
    A.S.run.autoPlay = 0; app_finish_song(perf);
    for (double rt = 0; rt <= 1.06; rt += 1.0 / 60) { A.resultsAt = perf - rt * 1000; gfx_begin(); screens_draw(perf); gfx_end(); } save_named("results_reveal");   /* the third star landing, its sparks flying */
    A.resultsAt = perf - 3000; gfx_begin(); screens_draw(perf); gfx_end(); save_named("results");
    { tierstate_t *ts = &A.tiers[A.selTier]; ts->regDone = ts->required; ts->encoreOpen = 1;   /* in memory only: as if the set's songs were done, the encore is offered */
      A.resRow = 1; gfx_begin(); screens_draw(perf); gfx_end(); save_named("results_encore");
      screens_event(IN_OK, perf); if (A.screen == SC_SETUP) { gfx_begin(); screens_draw(perf + 1500); gfx_end(); save_named("encore"); encore_event(IN_BACK, perf); } }
    printf("screens written to %s\n", cfg.outdir);
}
static void frames_mode(void) {
    int si = -1; for (int i = 0; i < lib.nsongs; i++) if (!strcmp(lib.songs[i].name, cfg.song)) si = i;
    if (si < 0) { fprintf(stderr, "song %s not in the list\n", cfg.song); exit(1); }
    char err[300]; if (load_song(&lib.songs[si], cfg.part, cfg.diff, 1, 0, err, sizeof err)) { fprintf(stderr, "load: %s\n", err); exit(1); }
    headless_init();
    { double from = 0, lead = 0; A.selSong = si; A.selPart = cfg.part; A.selDiff = cfg.diff; A.mode = MODE_CAREER; if (battle_start(si, &from, &lead) && BTL.on) { battle_headless(cfg.frames, cfg.every, cfg.miss, save_named); return; } }   /* a boss song: the battle */
    double t = -2000, perf = 0; int saved = 0; Uint32 t0 = SDL_GetTicks();
    for (int f = 0; f < cfg.frames; f++, t += 1000.0 / 60, perf += 1000.0 / 60) {
        game_step(&A.S.run, t, perf); { const char *wv = getenv("GH3_TEST_WHAMMY"); if (wv) A.S.run.whammy = atof(wv); }
        gfx_begin(); hud_info hi = { A.S.song->title, A.S.song->artist, app_section_at(t), 1, SET.lefty, SET.stageFx, SET.bgDim, 0, SET.noFail, "Select / L2 / L1" }; render_game(&A.S.run, perf, &hi); gfx_end();
        if (f % cfg.every == 0) { char nm[32]; snprintf(nm, sizeof nm, "frame_%04d", f); save_named(nm); saved++; }
        if (A.S.run.finished) break;
    }
    Uint32 ms = SDL_GetTicks() - t0;
    printf("frames: %d rendered in %u ms (%.1f ms/frame, software), %d saved; score %ld, hits %d/%d, best streak %d, stars %d\n", cfg.frames, ms, (double)ms / cfg.frames, saved, (long)A.S.run.score, A.S.run.hits, A.S.run.nnotes, A.S.run.best, game_stars(A.S.run.score, A.S.chart.base));
}

/* ------------------------------------------------------------------ main */
int main(int argc, char **argv) {
    settings_defaults(); settings_load(); records_load();
    cfg_defaults(); cfg_args(argc, argv);
    index_dir(cfg.data, "");
    if (!nfiles) fprintf(stderr, "no game files under %s (set GH3_DATA or --data)\n", cfg.data);
    { size_t pl, bl; uint8_t *pak = read_data("pak", "qb.pak.xen", &pl), *pab = read_data("pak", "qb.pab.xen", &bl);
      if (pak && pab && gh3_library_read(pak, pl, pab, bl, &lib) == 0) A.libOk = 1; else snprintf(libErr, sizeof libErr, "Não consegui ler PAK/qb.pak.xen + qb.pab.xen em %s. Copie a pasta DATA do jogo (PAK, SONGS e MUSIC) para ports/gh3/DATA.", cfg.data);
      { size_t sl; uint8_t *sb = read_data("zones", "global_sfx.pak.xen", &sl); sfx_init(pak, pl, pab, bl, sb, sl); }   /* the game's sound effects (sfx keeps the bank) */
      free(pak); free(pab); }
    A.lib = &lib; A.autoPlay = cfg.autoPlay; A.selDiff = 1;
    if (A.libOk) store_load(&lib);
    if (cfg.dump) { if (!A.libOk) { puts(libErr); return 1; } dump(); return 0; }
    if (*cfg.tutorial) { if (!A.libOk) { puts(libErr); return 1; } SDL_Init(0); headless_init(); bank_index(); return tutorial_headless(cfg.tutorial, cfg.frames > 0 ? cfg.frames : 3600, cfg.every, save_named); }   /* --tutorial 1b [--frames N --every K --out DIR]: a lesson played by the bot */
    if (cfg.frames > 0) { if (!A.libOk) { puts(libErr); return 1; } SDL_Init(0); frames_mode(); return 0; }
    if (cfg.screens) { if (!A.libOk) { puts(libErr); return 1; } SDL_Init(0); screens_mode(); return 0; }
    if (cfg.menutest) { SDL_Init(0); menu_selftest(); return 0; }
    if (cfg.sfxtest) { SDL_Init(0); return sfx_selftest() ? 1 : 0; }   /* --sfxtest: the game's sound effects, listed and decoded */
    if (cfg.stretchtest) { SDL_Init(0); int bad = stretch_selftest(); if (*cfg.song && A.libOk) { int si = -1; for (int i = 0; i < lib.nsongs; i++) if (!strcmp(lib.songs[i].name, cfg.song)) si = i;   /* and a song through the mixer at each speed */
            char e[300]; if (si < 0) puts("song not found"); else if (load_song(&lib.songs[si], cfg.part, cfg.diff, 1, 1, e, sizeof e)) puts(e); else { practice_bench(A.S.durationMs);
                A.mode = MODE_PRACTICE; A.practiceSec = 3; A.practiceEnd = 4; int all = A.S.chart.nnotes; double from, to, at, lead;   /* and a practice run of sections 3 to 4 played by the bot */
                if (load_song(&lib.songs[si], cfg.part, cfg.diff, 1, 1, e, sizeof e)) puts(e); else { gh3_run *r = &A.S.run; practice_range(&A.S.sd, 3, 4, 1e12, &from, &to); practice_lead(from, 0.5, &at, &lead);
                    double t = at - lead; while (!r->finished && t < 1e7) { game_step(r, t, t); t += 1000.0 / 60; }
                    printf("practice sections 3-4 (%.0f..%.0f ms) at 50%%: sound from %.0f ms after a %.0f ms count-in (song ms); %d of the song's %d notes, first %d last %d; run ends at %.0f ms, %d hits, %d misses\n",
                        from, to, at, lead, r->nnotes, all, r->notes[0].t, r->notes[r->nnotes - 1].t, t, r->hits, r->misses); } } } return bad ? 1 : 0; }
    if (moneyTest) { if (!A.libOk) { puts(libErr); return 1; } SDL_Init(0); bank_index(); return store_selftest(&lib) ? 1 : 0; }
    if (cfg.movietest[0]) { SDL_Init(0); movie_test(); audio_close(); return 0; }
#ifdef GH3_3D
    if (r3dTest[0]) return r3d_test(r3dTest, cfg.outdir, cfg.W, cfg.H);
#endif
    if (cfg.audiotest) { if (!A.libOk) { puts(libErr); return 1; } SDL_Init(0); int si = -1; for (int i = 0; i < lib.nsongs; i++) if (!strcmp(lib.songs[i].name, cfg.song)) si = i; if (si < 0) { puts("song not found"); return 1; }
        char e[300]; if (load_song(&lib.songs[si], cfg.part, cfg.diff, 1, 1, e, sizeof e)) { puts(e); return 1; } audio_selftest(); return 0; }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) < 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
#ifdef GH3_3D
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16); if (!SDL_GetHint(SDL_HINT_RENDER_DRIVER)) SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");   /* the venue needs a depth buffer; GLES2 as on the device (SDL falls back when it is not there) */
#endif
    SDL_Window *win = SDL_CreateWindow("Guitar Hero III", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, cfg.W, cfg.H, cfg.fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
    if (!win) { fprintf(stderr, "window: %s\n", SDL_GetError()); return 1; }
    SDL_Renderer *ren = NULL; SDL_Texture *softTex = NULL;
    SDL_SetHint("SDL_RENDER_BATCHING", "1");
    if (!cfg.soft) {
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | (cfg.vsync ? SDL_RENDERER_PRESENTVSYNC : 0));
        if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (ren) { SDL_RendererInfo ri; SDL_GetRendererInfo(ren, &ri); SDL_Log("renderer: %s%s", ri.name, (ri.flags & SDL_RENDERER_PRESENTVSYNC) ? " (vsync)" : ""); if (!(ri.flags & SDL_RENDERER_ACCELERATED)) { SDL_Log("not accelerated: drawing in software"); SDL_DestroyRenderer(ren); ren = NULL; } }
    }
    if (!ren) { /* software: draw into our own buffer and show it through a streaming texture */
        SDL_Renderer *soft = SDL_CreateRenderer(win, -1, cfg.vsync ? SDL_RENDERER_PRESENTVSYNC : 0); if (!soft) { fprintf(stderr, "renderer: %s\n", SDL_GetError()); return 1; }
        softTex = SDL_CreateTexture(soft, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, cfg.W, cfg.H);
        gfx_init(NULL, cfg.W, cfg.H); ren = soft; SDL_Log("renderer: software");
    } else gfx_init(ren, cfg.W, cfg.H);
#ifdef GH3_3D
    if (!softTex && r3d_init(ren)) SDL_Log("palco 3D indisponível: palco 2D");
#endif
    SDL_ShowCursor(SDL_DISABLE);
    art_init(cfg.H >= 400 ? cfg.H / 240 : 1); fx_init(art_px()); render_init(cfg.W, cfg.H); A.W = cfg.W; A.H = cfg.H;
    if (audio_open()) SDL_Log("no audio device"); app_apply_volumes();
    open_pads();
    gh3_sfx_hook = sfx_hook; gh3_mute_hook = mute_hook;
    bank_index(); srand(SDL_GetTicks() ^ (unsigned)time(NULL));
    if (!A.libOk) { snprintf(A.errMsg, sizeof A.errMsg, "%s", libErr); A.screen = SC_ERROR; } else { app_refresh_career(0); screens_build_rows(); A.screen = SC_TITLE; A.titleAt = SDL_GetTicks(); if (SET.introMovies) boot_movies(); }
    Uint32 last = SDL_GetTicks(), fpsAt = last; int fpsN = 0;
    while (running) {
        double now = SDL_GetTicks(); SDL_Event ev;
        double songT = A.screen == SC_PLAY ? song_time() : 0;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_CONTROLLERBUTTONDOWN || ev.type == SDL_CONTROLLERBUTTONUP) { static int selDown, startDown, guideDown; int dn = ev.type == SDL_CONTROLLERBUTTONDOWN;
                if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_BACK) selDown = dn; else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_START) startDown = dn; else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE) guideDown = dn;
                if (dn && startDown && (selDown || guideDown)) { SDL_Log("Select+Start: quitting"); running = 0; break; } }
            if (ev.type == SDL_QUIT) running = 0;
            else if (ev.type == SDL_CONTROLLERDEVICEADDED && !pad) open_pads();
            else if (A.screen == SC_PLAY) play_event(&ev, now, songT);
            else if (A.screen == SC_MOVIE) movie_event(&ev, now);
            else { if (screens_raw_event(&ev, now)) continue; int in = menu_event(&ev); if (in != IN_NONE) screens_event(in, now); }
        }
        preview_tick(); battle_frame(now);
        gfx_begin();
        if (A.screen == SC_PLAY) {
            gh3_run *r = &A.S.run;
            if (A.resumeAt > 0) { if (now >= A.resumeAt) { A.resumeAt = 0; audio_pause(0); input_reset_axes(); audio_sfx(SFX_GO); } else { static int lastTick = -1; int tk = (int)((A.resumeAt - now) / 1000); if (tk != lastTick) { lastTick = tk; } } }
            if (A.resumeAt == 0) { songT = song_time(); game_step(r, songT, now); battle_tick(now); sfx_song_tick(r, songT); snprintf(A.S.section, sizeof A.S.section, "%s", app_section_at(songT)); }
            audio_set_bend(A.resumeAt == 0 && r->sus && !r->autoPlay && !r->failed ? r->whammy : 0);
            hud_info hi = { A.S.song->title, A.S.song->artist, A.S.section, 1, SET.lefty, SET.stageFx, SET.bgDim, SET.showFps ? A.fps : 0, SET.noFail, A.starKeys }; if (BTL.on) render_battle(now, 1); else render_game(r, now, &hi);
            if (A.resumeAt > 0) { char b[8]; snprintf(b, sizeof b, "%d", (int)ceil((A.resumeAt - now) / 1000)); ui_dim(A.W, A.H, 90); ui_title(A.W / 2 - gfx_text_w(b, 8) / 2, A.H / 2 - 50, 8, b, C_INK, C_SHADOW, 3); gfx_text_sh_center(A.W / 2, A.H / 2 + 50, 2, "Continuando…", C_AMP, C_SHADOW); }
            else if (r->failed || r->finished) app_finish_song(now);
        } else if (A.screen == SC_MOVIE) { if (!movie_tick(now)) movie_finish(now, 0); if (A.screen == SC_MOVIE) movie_draw(A.W, A.H); else screens_draw(now);
        } else {
            if (A.screen == SC_RESULTS && !A.S.run.failed) { double T = (now - A.resultsAt) / 1000; int shown = (int)(T / 0.35) + 1; if (shown > A.resultStars) shown = A.resultStars; while (A.starsSounded < shown) { A.starsSounded++; audio_sfx(SFX_DING); } if (resultCash > 0 && T > 1.4) { resultCash = 0; audio_sfx(SFX_CASH); } if (A.newRecord && A.starsSounded == A.resultStars && T > 1.9 && A.starsSounded < 99) { A.starsSounded = 99; audio_sfx(SFX_FANFARE); } }
            screens_draw(now);
            if (SET.showFps) { char b[16]; snprintf(b, sizeof b, "%d fps", A.fps); gfx_text(A.W / 2 - 20, 60, 1, b, C_MUTED); }
        }
        if (softTex) { fb_t *fb = gfx_soft_fb(); SDL_UpdateTexture(softTex, NULL, fb->px, fb->w * 4); SDL_RenderCopy(ren, softTex, NULL, NULL); SDL_RenderPresent(ren); }
        else gfx_end();
        fpsN++; Uint32 t2 = SDL_GetTicks(); if (t2 - fpsAt >= 1000) { A.fps = fpsN; fpsN = 0; fpsAt = t2; }
        if (!cfg.vsync) { if (t2 - last < 16) SDL_Delay(16 - (t2 - last)); } last = SDL_GetTicks();
    }
    settings_save(); movie_close(); preview_shutdown(); app_session_free(); audio_close();
#ifdef GH3_3D
    r3d_shutdown();
#endif
    SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}
