/* Guitar Hero III for the R36S: a native player for the game's own PC data files.
 * Reads DATA/ (qb.pak, the song paks, the FSB audio banks), judges notes the way the game does,
 * draws through SDL_Renderer (GLES2 on the device) and plays the MP3 streams through SDL audio. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>
#include "gh3fmt.h"
#include "game.h"
#include "audio.h"
#include "gfx.h"
#include "render.h"

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
    char data[520]; int W, H, fullscreen, vsync, soft; int tap, lefty; double volume;
    int frames, every; char song[64]; int diff, part, autoPlay, dump; char outdir[520];
} cfg_t;
static cfg_t cfg;
static void cfg_defaults(void) {
    memset(&cfg, 0, sizeof cfg); snprintf(cfg.data, sizeof cfg.data, "DATA"); cfg.W = 640; cfg.H = 480; cfg.vsync = 1; cfg.volume = 1; cfg.diff = 3; cfg.every = 30;
    const char *e;
    if ((e = getenv("GH3_DATA")) && *e) snprintf(cfg.data, sizeof cfg.data, "%s", e);
    if ((e = getenv("GH3_SCREEN")) && sscanf(e, "%dx%d", &cfg.W, &cfg.H) != 2) { cfg.W = 640; cfg.H = 480; }
    if ((e = getenv("GH3_FULLSCREEN"))) cfg.fullscreen = atoi(e);
    if ((e = getenv("GH3_TAP"))) cfg.tap = atoi(e);
    if ((e = getenv("GH3_LEFTY"))) cfg.lefty = atoi(e);
    if ((e = getenv("GH3_VOLUME"))) cfg.volume = atof(e) / 100.0;
    if ((e = getenv("GH3_VSYNC"))) cfg.vsync = atoi(e);
    if ((e = getenv("GH3_RENDER")) && !strcmp(e, "soft")) cfg.soft = 1;
}
static int diff_index(const char *s) { for (int i = 0; i < 4; i++) if (!strcmp(s, GH3_DIFF_NAME[i])) return i; return atoi(s); }
static void cfg_args(int argc, char **argv) {
    for (int i = 1; i < argc; i++) { const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : "";
        if (!strcmp(a, "--data")) { snprintf(cfg.data, sizeof cfg.data, "%s", v); i++; }
        else if (!strcmp(a, "--dump")) cfg.dump = 1;
        else if (!strcmp(a, "--song")) { snprintf(cfg.song, sizeof cfg.song, "%s", v); i++; }
        else if (!strcmp(a, "--diff")) { cfg.diff = diff_index(v); i++; }
        else if (!strcmp(a, "--part")) { cfg.part = !strcmp(v, "rhythm") || !strcmp(v, "bass") ? 1 : atoi(v); i++; }
        else if (!strcmp(a, "--frames")) { cfg.frames = atoi(v); i++; }
        else if (!strcmp(a, "--every")) { cfg.every = atoi(v); i++; }
        else if (!strcmp(a, "--out")) { snprintf(cfg.outdir, sizeof cfg.outdir, "%s", v); i++; }
        else if (!strcmp(a, "--auto")) cfg.autoPlay = 1;
        else if (!strcmp(a, "--tap")) cfg.tap = 1;
        else if (!strcmp(a, "--soft")) cfg.soft = 1;
        else if (!strcmp(a, "--windowed")) cfg.fullscreen = 0;
        else if (!strcmp(a, "--fullscreen")) cfg.fullscreen = 1;
        else if (!strcmp(a, "--size")) { sscanf(v, "%dx%d", &cfg.W, &cfg.H); i++; }
        else if (!strcmp(a, "--help")) { printf("gh3 [--data DIR] [--dump [--song NAME]] [--frames N --song NAME [--diff easy|medium|hard|expert] [--part guitar|rhythm] --out DIR] [--auto] [--tap] [--soft] [--windowed] [--size WxH]\n"); exit(0); }
    }
}

/* ------------------------------------------------------------------ app state */
static gh3_library lib; static int libOk = 0; static char libErr[200];
typedef struct { int kind; int idx; } row_t; /* kind 0 = tier header, 1 = song */
static row_t *rows; static int nrows, cursor, scroll;
enum { SC_SETLIST, SC_SETUP, SC_PLAY, SC_PAUSE, SC_RESULTS, SC_ERROR };
static int screen = SC_SETLIST, setupRow = 0, pauseRow = 0;
static int selSong = -1, selDiff = 3, selPart = 0;
static char errMsg[300];

typedef struct {
    gh3_songdata sd; gh3_chart chart; gh3_run run; uint8_t *pakBytes, *fsbBytes, *datBytes; gh3_fsb fsb; double durationMs; int loaded;
    const gh3_song *song; char section[64];
} session_t;
static session_t S;

static void build_rows(void) {
    nrows = 0; rows = realloc(rows, (lib.nsongs + lib.ntiers + 1) * sizeof *rows);
    for (int t = 0; t < lib.ntiers; t++) { rows[nrows].kind = 0; rows[nrows].idx = t; nrows++; for (int k = 0; k < lib.tiers[t].nsongs; k++) { rows[nrows].kind = 1; rows[nrows].idx = lib.tiers[t].songIdx[k]; nrows++; } }
    cursor = 0; while (cursor < nrows && rows[cursor].kind == 0) cursor++;
}
static void move_cursor(int d) { int c = cursor; do { c += d; if (c < 0 || c >= nrows) return; } while (rows[c].kind == 0); cursor = c; }

static void session_free(void) {
    audio_clear(); gh3_chart_free(&S.chart); gh3_songdata_free(&S.sd); free(S.pakBytes); free(S.fsbBytes); free(S.datBytes); free(S.fsb.streams);
    memset(&S, 0, sizeof S);
}
static void sfx_hook(const char *name) { (void)name; }
static void mute_hook(int m) { audio_mute(BUS_PLAYER, m); }

static int load_song(const gh3_song *song, int part, int diff, int autoPlay, int withAudio, char *err, size_t errCap) {
    session_free(); S.song = song;
    char nm[128]; size_t len;
    snprintf(nm, sizeof nm, "%s_song.pak.xen", song->name);
    S.pakBytes = read_data("songs", nm, &len); if (!S.pakBytes) { snprintf(err, errCap, "No chart file: SONGS/%s", nm); return -1; }
    if (gh3_songpak_read(S.pakBytes, len, song->name, &S.sd)) { snprintf(err, errCap, "No chart data in %s", nm); return -1; }
    int p = part; if (!S.sd.parts[p][diff].raw) { p = p == 1 ? 0 : 1; if (!S.sd.parts[p][diff].raw) { snprintf(err, errCap, "That part has no notes on %s", GH3_DIFF_NAME[diff]); return -1; } }
    if (gh3_chart_build(&S.sd, p, diff, song->hmScale > 0 ? song->hmScale : lib.tuning.hmScale, lib.tuning.susShorten, &S.chart)) { snprintf(err, errCap, "Chart has no notes"); return -1; }
    S.durationMs = 0;
    if (withAudio) {
        snprintf(nm, sizeof nm, "%s.fsb.xen", song->name); size_t flen, dlen;
        S.fsbBytes = read_data("music", nm, &flen);
        if (S.fsbBytes) {
            snprintf(nm, sizeof nm, "%s.dat.xen", song->name); S.datBytes = read_data("music", nm, &dlen);
            if (gh3_fsb_read(S.fsbBytes, flen, S.datBytes, dlen, song->name, &S.fsb) == 0) {
                int mine = (p == 1 && gh3_fsb_has(&S.fsb, FSB_RHYTHM)) ? FSB_RHYTHM : FSB_GUITAR;
                int order[3] = { mine, FSB_SONG, mine == FSB_RHYTHM ? FSB_GUITAR : FSB_RHYTHM };
                for (int k = 0; k < 3; k++) { size_t sz; const uint8_t *mp3 = gh3_fsb_extract(&S.fsb, order[k], &sz); if (mp3) audio_set_stream(k == 0 ? BUS_PLAYER : k == 1 ? BUS_BAND : BUS_OTHER, mp3, sz, k == 0 ? song->guitarVol : song->bandVol); }
                for (int i = 0; i < S.fsb.nstreams; i++) { gh3_fsbstream *st = &S.fsb.streams[i]; if (st->role >= 0 && st->freq > 0) { double d = (double)st->samples / st->freq * 1000; if (d > S.durationMs) S.durationMs = d; } }
            } else SDL_Log("audio bank %s not understood", nm);
        } else SDL_Log("no audio bank for %s", song->name);
    }
    gh3_runopts o; memset(&o, 0, sizeof o); o.autoPlay = autoPlay; o.noStrum = cfg.tap; o.silent = !withAudio; o.bass = p == 1;
    const gh3_note *last = &S.chart.notes[S.chart.nnotes - 1];
    double endMs = last->t + last->sus + 3000; if (S.durationMs > 1000 && S.durationMs < endMs) endMs = S.durationMs;
    o.endMs = endMs; o.speed = 100;
    game_new(&S.run, &S.chart, &lib.tuning, diff, &o);
    S.loaded = 1; return 0;
}
static const char *section_at(double t) {
    const char *s = ""; for (int i = 0; i < S.sd.nmarkers; i++) if (S.sd.markers[i].t <= t) s = S.sd.markers[i].text; else break; return s;
}

/* ------------------------------------------------------------------ the screens */
static void draw_setlist(double perf) {
    render_backdrop(perf);
    gfx_text(16, 12, 3, "GUITAR HERO III", C_INK); gfx_text(16, 36, 1, "R36S EDITION - PICK A SONG", C_MUTED);
    int rowH = 18, top = 58, vis = (cfg.H - top - 30) / rowH;
    if (cursor < scroll) scroll = cursor; if (cursor >= scroll + vis) scroll = cursor - vis + 1; if (scroll < 0) scroll = 0;
    for (int i = scroll, y = top; i < nrows && i < scroll + vis; i++, y += rowH) {
        if (rows[i].kind == 0) { gfx_text(16, y + 4, 1, lib.tiers[rows[i].idx].title, C_AMP); continue; }
        const gh3_song *s = &lib.songs[rows[i].idx]; int sel = i == cursor;
        if (sel) gfx_rect(10, y - 1, cfg.W - 20, rowH, RGBA(233, 176, 76, 40));
        gfx_text(28, y + 2, 2, s->title, sel ? C_INK : RGB(200, 195, 185));
        char line[200]; snprintf(line, sizeof line, "%s%s%s", s->artist, *s->year ? "  " : "", s->year); gfx_text_right(cfg.W - 16, y + 5, 1, line, C_MUTED);
    }
    gfx_text(16, cfg.H - 20, 1, "A/START: CHOOSE   UP/DOWN: MOVE   B: QUIT", C_MUTED);
}
static void draw_setup(double perf) {
    render_backdrop(perf); const gh3_song *s = &lib.songs[selSong];
    gfx_text(16, 12, 3, s->title, C_INK); gfx_text(16, 36, 1, s->artist, C_MUTED);
    const char *labels[4] = { "DIFFICULTY", "PART", "STRUMMING", "PLAY" }; const char *val[3];
    static const char *DN[4] = { "EASY", "MEDIUM", "HARD", "EXPERT" };
    val[0] = DN[selDiff]; val[1] = selPart == 1 ? (s->hasRhythm ? "BASS / RHYTHM" : "RHYTHM") : "LEAD GUITAR"; val[2] = cfg.tap ? "TAP (NO STRUM)" : "STRUM REQUIRED";
    for (int i = 0; i < 4; i++) { int y = 100 + i * 46, sel = setupRow == i;
        if (sel) gfx_rect(10, y - 8, cfg.W - 20, 40, RGBA(233, 176, 76, 40));
        gfx_text(30, y, 2, labels[i], sel ? C_AMP : C_MUTED);
        if (i < 3) { char b[80]; snprintf(b, sizeof b, "< %s >", val[i]); gfx_text_right(cfg.W - 30, y, 2, b, C_INK); } }
    char info[120]; snprintf(info, sizeof info, "%s ON %s: %s", selPart ? "RHYTHM" : "GUITAR", DN[selDiff], S.sd.parts[selPart][selDiff].raw ? "CHARTED" : "NOT CHARTED (THE OTHER PART IS USED)");
    gfx_text(30, 300, 1, info, C_MUTED);
    gfx_text(16, cfg.H - 20, 1, "A/START: PLAY   LEFT/RIGHT: CHANGE   B: BACK", C_MUTED);
}
static void draw_results(double perf) {
    render_backdrop(perf); gh3_run *r = &S.run; char b[120]; int W = cfg.W;
    gfx_text_center(W / 2, 30, 3, r->failed ? "SONG FAILED" : "YOU ROCK!", r->failed ? C_BAD : C_AMP);
    gfx_text_center(W / 2, 60, 2, S.song->title, C_INK); gfx_text_center(W / 2, 80, 1, S.song->artist, C_MUTED);
    snprintf(b, sizeof b, "%ld", (long)r->score); gfx_text_center(W / 2, 120, 6, b, C_INK); gfx_text_center(W / 2, 170, 1, "SCORE", C_MUTED);
    if (!r->failed) { int st = game_stars(r->score, S.chart.base); char stars[8] = ""; for (int i = 0; i < 5; i++) stars[i] = i < st ? '*' : '-'; stars[5] = 0; gfx_text_center(W / 2, 190, 4, stars, C_AMP); }
    int total = r->nnotes, hit = r->hits; int pct = total ? (int)(100.0 * hit / total + 0.5) : 0;
    snprintf(b, sizeof b, "NOTES HIT  %d / %d  (%d%%)", hit, total, pct); gfx_text_center(W / 2, 240, 2, b, C_INK);
    snprintf(b, sizeof b, "BEST STREAK  %d", r->best); gfx_text_center(W / 2, 264, 2, b, C_INK);
    snprintf(b, sizeof b, "OVERSTRUMS  %d", r->overstrums); gfx_text_center(W / 2, 288, 2, b, C_MUTED);
    gfx_text(16, cfg.H - 20, 1, "A/START: BACK TO THE SET LIST", C_MUTED);
}
static void draw_pause(double perf) {
    hud_info hi = { S.song->title, S.song->artist, S.section, 0, cfg.lefty }; render_game(&S.run, perf, &hi);
    gfx_rect(0, 0, cfg.W, cfg.H, RGBA(12, 10, 17, 190));
    gfx_text_center(cfg.W / 2, 150, 4, "PAUSED", C_INK);
    const char *it[3] = { "RESUME", "RESTART", "QUIT TO SET LIST" };
    for (int i = 0; i < 3; i++) gfx_text_center(cfg.W / 2, 220 + i * 30, 2, it[i], i == pauseRow ? C_AMP : C_MUTED);
}
static void draw_error(double perf) { render_backdrop(perf); gfx_text_center(cfg.W / 2, 180, 2, "SOMETHING WENT WRONG", C_BAD); gfx_text_center(cfg.W / 2, 220, 1, errMsg, C_INK); gfx_text_center(cfg.W / 2, 260, 1, "PRESS A OR B", C_MUTED); }

/* ------------------------------------------------------------------ input */
enum { IN_NONE = 0, IN_UP, IN_DOWN, IN_LEFT, IN_RIGHT, IN_OK, IN_BACK, IN_QUIT };
static int fretKeys[5] = { SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5 };
static int fretKeys2[5] = { SDLK_a, SDLK_s, SDLK_d, SDLK_f, SDLK_g };
static int fretButtons[5] = { SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER };
static int heldMask = 0; static int strumAxisState = 0;
static SDL_GameController *pad;
static void open_pads(void) { for (int i = 0; i < SDL_NumJoysticks(); i++) if (SDL_IsGameController(i)) { pad = SDL_GameControllerOpen(i); if (pad) { SDL_Log("controller: %s", SDL_GameControllerName(pad)); break; } } }
static int fret_of_key(int key) { for (int i = 0; i < 5; i++) if (key == fretKeys[i] || key == fretKeys2[i]) return i; return -1; }
static int fret_of_button(int b) { for (int i = 0; i < 5; i++) if (b == fretButtons[i]) return i; return -1; }

static void begin_song(void) {
    char err[300];
    if (load_song(&lib.songs[selSong], selPart, selDiff, cfg.autoPlay, 1, err, sizeof err)) { snprintf(errMsg, sizeof errMsg, "%s", err); screen = SC_ERROR; return; }
    audio_set_master(cfg.volume); audio_play(0, 2200);
    screen = SC_PLAY; heldMask = 0; S.section[0] = 0;
}
static void play_event(SDL_Event *ev, double now, double songT) {
    double te = songT - (now - ev->common.timestamp); /* the song's clock when the press happened */
    gh3_run *r = &S.run;
    if (ev->type == SDL_KEYDOWN && !ev->key.repeat) { int k = ev->key.keysym.sym, f = fret_of_key(k);
        if (f >= 0) { heldMask |= 1 << f; game_set_frets(r, heldMask, te, now); }
        else if (k == SDLK_UP || k == SDLK_DOWN) game_strum(r, te, now);
        else if (k == SDLK_SPACE) game_star(r, now);
        else if (k == SDLK_w) game_whammy(r, 1, now);
        else if (k == SDLK_ESCAPE || k == SDLK_RETURN) { audio_pause(1); screen = SC_PAUSE; pauseRow = 0; }
    } else if (ev->type == SDL_KEYUP) { int k = ev->key.keysym.sym, f = fret_of_key(k);
        if (f >= 0) { heldMask &= ~(1 << f); game_set_frets(r, heldMask, te, now); } else if (k == SDLK_w) game_whammy(r, 0, now);
    } else if (ev->type == SDL_CONTROLLERBUTTONDOWN) { int b = ev->cbutton.button, f = fret_of_button(b);
        if (f >= 0) { heldMask |= 1 << f; game_set_frets(r, heldMask, te, now); }
        else if (b == SDL_CONTROLLER_BUTTON_DPAD_UP || b == SDL_CONTROLLER_BUTTON_DPAD_DOWN) game_strum(r, te, now);
        else if (b == SDL_CONTROLLER_BUTTON_LEFTSHOULDER || b == SDL_CONTROLLER_BUTTON_BACK) game_star(r, now);
        else if (b == SDL_CONTROLLER_BUTTON_START) { audio_pause(1); screen = SC_PAUSE; pauseRow = 0; }
    } else if (ev->type == SDL_CONTROLLERBUTTONUP) { int f = fret_of_button(ev->cbutton.button);
        if (f >= 0) { heldMask &= ~(1 << f); game_set_frets(r, heldMask, te, now); }
    } else if (ev->type == SDL_CONTROLLERAXISMOTION) { int ax = ev->caxis.axis, v = ev->caxis.value;
        if (ax == SDL_CONTROLLER_AXIS_LEFTY) { int st = v < -16000 ? -1 : v > 16000 ? 1 : 0; if (st && st != strumAxisState) game_strum(r, te, now); if (st || (v > -8000 && v < 8000)) strumAxisState = st; }
        else if (ax == SDL_CONTROLLER_AXIS_RIGHTY) game_whammy(r, fabs(v) / 32767.0, now);
        else if (ax == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) game_whammy(r, v / 32767.0, now);
    }
}
static int menu_event(SDL_Event *ev) {
    if (ev->type == SDL_KEYDOWN && !ev->key.repeat) { switch (ev->key.keysym.sym) {
        case SDLK_UP: return IN_UP; case SDLK_DOWN: return IN_DOWN; case SDLK_LEFT: return IN_LEFT; case SDLK_RIGHT: return IN_RIGHT;
        case SDLK_RETURN: case SDLK_SPACE: return IN_OK; case SDLK_ESCAPE: case SDLK_BACKSPACE: return IN_BACK; case SDLK_q: return IN_QUIT; } }
    if (ev->type == SDL_CONTROLLERBUTTONDOWN) { switch (ev->cbutton.button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return IN_UP; case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return IN_DOWN; case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return IN_LEFT; case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return IN_RIGHT;
        case SDL_CONTROLLER_BUTTON_A: case SDL_CONTROLLER_BUTTON_START: return IN_OK; case SDL_CONTROLLER_BUTTON_B: return IN_BACK; } }
    if (ev->type == SDL_CONTROLLERAXISMOTION && ev->caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) { int v = ev->caxis.value; int st = v < -20000 ? -1 : v > 20000 ? 1 : 0; int was = strumAxisState; if (st || (v > -8000 && v < 8000)) strumAxisState = st; if (st && st != was) return st < 0 ? IN_UP : IN_DOWN; }
    return IN_NONE;
}

/* ------------------------------------------------------------------ headless modes */
static void dump(void) {
    printf("songs: %d  tiers: %d\n", lib.nsongs, lib.ntiers);
    for (int t = 0; t < lib.ntiers; t++) { printf("== %s\n", lib.tiers[t].title); for (int k = 0; k < lib.tiers[t].nsongs; k++) { const gh3_song *s = &lib.songs[lib.tiers[t].songIdx[k]]; printf("   %-24s %s - %s (%s)%s\n", s->name, s->title, s->artist, s->year, s->hasRhythm ? " [bass]" : ""); } }
    const gh3_tuning *tn = &lib.tuning;
    printf("tuning: early %.0f late %.0f hamEarly %.0f hamLate %.0f strumBefore %.0f formBefore %.0f susShorten %.2f split %.2f spDrain %.1f hmScale %.2f\n", tn->early, tn->late, tn->hamEarly, tn->hamLate, tn->strumBefore, tn->formBefore, tn->susShorten, tn->split, tn->spDrain, tn->hmScale);
    for (int d = 0; d < 4; d++) printf("  %-6s lookahead %.3f good %.4f bad %.4f star %.4f\n", GH3_DIFF_NAME[d], tn->diffs[d].lookahead, tn->diffs[d].good, tn->diffs[d].bad, tn->diffs[d].star);
    if (!*cfg.song) return;
    int si = -1; for (int i = 0; i < lib.nsongs; i++) if (!strcmp(lib.songs[i].name, cfg.song)) si = i;
    if (si < 0) { printf("song %s not in the list\n", cfg.song); return; }
    char err[300]; if (load_song(&lib.songs[si], cfg.part, cfg.diff, 1, 1, err, sizeof err)) { printf("load: %s\n", err); return; }
    printf("song %s: fretbars %d timesig %d markers %d\n", S.sd.name, S.sd.nfret, S.sd.nsig, S.sd.nmarkers);
    for (int p = 0; p < 4; p++) for (int d = 0; d < 4; d++) if (S.sd.parts[p][d].raw) printf("  part %-10s %-6s notes %d star phrases %d\n", GH3_PART_NAME[p], GH3_DIFF_NAME[d], S.sd.parts[p][d].nraw / 3, S.sd.parts[p][d].nstar);
    int hopo = 0, sus = 0, chords = 0; for (int i = 0; i < S.chart.nnotes; i++) { hopo += S.chart.notes[i].hopo; sus += S.chart.notes[i].sus > 0; chords += S.chart.notes[i].n > 1; }
    printf("chart %s/%s: %d notes, %d hopos, %d sustains, %d chords, %d phrases, base %.0f, end %d ms\n", GH3_PART_NAME[cfg.part], GH3_DIFF_NAME[cfg.diff], S.chart.nnotes, hopo, sus, chords, S.chart.nphrases, S.chart.base, S.chart.end);
    for (int i = 0; i < 8 && i < S.chart.nnotes; i++) printf("  note %d: t %d len %d mask %02x hopo %d sus %d star %d\n", i, S.chart.notes[i].t, S.chart.notes[i].len, S.chart.notes[i].mask, S.chart.notes[i].hopo, S.chart.notes[i].sus, S.chart.notes[i].star);
    for (int i = 0; i < S.sd.nmarkers && i < 6; i++) printf("  marker %d ms: %s\n", S.sd.markers[i].t, S.sd.markers[i].text);
    printf("audio: %d streams, duration %.1f s\n", S.fsb.nstreams, S.durationMs / 1000);
    for (int i = 0; i < S.fsb.nstreams; i++) { const gh3_fsbstream *st = &S.fsb.streams[i]; printf("  stream %d: %-28s role %d samples %u freq %u ch %u size %u\n", i, st->name, st->role, st->samples, st->freq, st->channels, st->size); }
}
static void save_frame(int n) {
    fb_t *fb = gfx_soft_fb();
    SDL_Surface *s = SDL_CreateRGBSurfaceFrom(fb->px, fb->w, fb->h, 32, fb->w * 4, 0x00FF0000, 0x0000FF00, 0x000000FF, 0);
    char p[600]; snprintf(p, sizeof p, "%s/frame_%04d.bmp", *cfg.outdir ? cfg.outdir : ".", n); SDL_SaveBMP(s, p); SDL_FreeSurface(s);
}
static void frames_mode(void) {
    int si = -1; for (int i = 0; i < lib.nsongs; i++) if (!strcmp(lib.songs[i].name, cfg.song)) si = i;
    if (si < 0) { fprintf(stderr, "song %s not in the list\n", cfg.song); exit(1); }
    char err[300]; if (load_song(&lib.songs[si], cfg.part, cfg.diff, 1, 0, err, sizeof err)) { fprintf(stderr, "load: %s\n", err); exit(1); }
    gfx_init(NULL, cfg.W, cfg.H); render_init(cfg.W, cfg.H);
    double t = -2000, perf = 0; int saved = 0; Uint32 t0 = SDL_GetTicks();
    for (int f = 0; f < cfg.frames; f++, t += 1000.0 / 60, perf += 1000.0 / 60) {
        game_step(&S.run, t, perf);
        gfx_begin(); hud_info hi = { S.song->title, S.song->artist, section_at(t), 1, cfg.lefty }; render_game(&S.run, perf, &hi); gfx_end();
        if (f % cfg.every == 0) { save_frame(f); saved++; }
        if (S.run.finished) break;
    }
    Uint32 ms = SDL_GetTicks() - t0;
    printf("frames: %d rendered in %u ms (%.1f ms/frame, software), %d saved; score %ld, hits %d/%d, best streak %d\n", cfg.frames, ms, (double)ms / cfg.frames, saved, (long)S.run.score, S.run.hits, S.run.nnotes, S.run.best);
}

/* ------------------------------------------------------------------ main */
int main(int argc, char **argv) {
    cfg_defaults(); cfg_args(argc, argv);
    index_dir(cfg.data, "");
    if (!nfiles) fprintf(stderr, "no game files under %s (set GH3_DATA or --data)\n", cfg.data);
    { size_t pl, bl; uint8_t *pak = read_data("pak", "qb.pak.xen", &pl), *pab = read_data("pak", "qb.pab.xen", &bl);
      if (pak && pab && gh3_library_read(pak, pl, pab, bl, &lib) == 0) libOk = 1; else snprintf(libErr, sizeof libErr, "Could not read PAK/qb.pak.xen + qb.pab.xen under %s", cfg.data);
      free(pak); free(pab); }
    if (cfg.dump) { if (!libOk) { puts(libErr); return 1; } dump(); return 0; }
    if (cfg.frames > 0) { if (!libOk) { puts(libErr); return 1; } SDL_Init(0); frames_mode(); return 0; }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) < 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    SDL_Window *win = SDL_CreateWindow("Guitar Hero III", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, cfg.W, cfg.H, cfg.fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
    if (!win) { fprintf(stderr, "window: %s\n", SDL_GetError()); return 1; }
    SDL_Renderer *ren = NULL; SDL_Texture *softTex = NULL;
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
    SDL_ShowCursor(SDL_DISABLE);
    render_init(cfg.W, cfg.H);
    if (audio_open()) SDL_Log("no audio device");
    open_pads();
    gh3_sfx_hook = sfx_hook; gh3_mute_hook = mute_hook;
    if (!libOk) { snprintf(errMsg, sizeof errMsg, "%s", libErr); screen = SC_ERROR; } else build_rows();
    int running = 1; Uint32 last = SDL_GetTicks();
    while (running) {
        double now = SDL_GetTicks(); SDL_Event ev;
        double songT = screen == SC_PLAY ? audio_song_ms() : 0;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            else if (ev.type == SDL_CONTROLLERDEVICEADDED && !pad) open_pads();
            else if (screen == SC_PLAY) play_event(&ev, now, songT);
            else { int in = menu_event(&ev); if (in == IN_NONE) continue;
                if (screen == SC_SETLIST) { if (in == IN_UP) move_cursor(-1); else if (in == IN_DOWN) move_cursor(1); else if (in == IN_OK && cursor < nrows) { selSong = rows[cursor].idx; setupRow = 3; char e2[300]; if (!load_song(&lib.songs[selSong], selPart, selDiff, 1, 0, e2, sizeof e2)) screen = SC_SETUP; else { snprintf(errMsg, sizeof errMsg, "%s", e2); screen = SC_ERROR; } } else if (in == IN_BACK || in == IN_QUIT) running = 0; }
                else if (screen == SC_SETUP) { const gh3_song *s = &lib.songs[selSong];
                    if (in == IN_UP) setupRow = (setupRow + 3) % 4; else if (in == IN_DOWN) setupRow = (setupRow + 1) % 4;
                    else if (in == IN_LEFT || in == IN_RIGHT) { int d = in == IN_RIGHT ? 1 : -1; if (setupRow == 0) selDiff = (selDiff + 4 + d) % 4; else if (setupRow == 1) selPart = (s->hasRhythm || S.sd.parts[1][selDiff].raw) ? !selPart : 0; else if (setupRow == 2) cfg.tap = !cfg.tap; }
                    else if (in == IN_OK) begin_song(); else if (in == IN_BACK) { session_free(); screen = SC_SETLIST; } }
                else if (screen == SC_PAUSE) { if (in == IN_UP) pauseRow = (pauseRow + 2) % 3; else if (in == IN_DOWN) pauseRow = (pauseRow + 1) % 3;
                    else if (in == IN_OK) { if (pauseRow == 0) { audio_pause(0); screen = SC_PLAY; } else if (pauseRow == 1) begin_song(); else { session_free(); screen = SC_SETLIST; } }
                    else if (in == IN_BACK) { audio_pause(0); screen = SC_PLAY; } }
                else if (screen == SC_RESULTS) { if (in == IN_OK || in == IN_BACK) { session_free(); screen = SC_SETLIST; } }
                else if (screen == SC_ERROR) { if (in == IN_OK || in == IN_BACK) { session_free(); if (libOk) screen = SC_SETLIST; else running = 0; } }
            }
        }
        gfx_begin();
        if (screen == SC_PLAY) {
            gh3_run *r = &S.run; songT = audio_song_ms(); game_step(r, songT, now);
            snprintf(S.section, sizeof S.section, "%s", section_at(songT));
            hud_info hi = { S.song->title, S.song->artist, S.section, 1, cfg.lefty }; render_game(r, now, &hi);
            if (r->failed) { audio_fade_out(1.5); screen = SC_RESULTS; }
            else if (r->finished) screen = SC_RESULTS;
        }
        else if (screen == SC_SETLIST) draw_setlist(now);
        else if (screen == SC_SETUP) draw_setup(now);
        else if (screen == SC_PAUSE) draw_pause(now);
        else if (screen == SC_RESULTS) draw_results(now);
        else draw_error(now);
        if (softTex) { fb_t *fb = gfx_soft_fb(); SDL_UpdateTexture(softTex, NULL, fb->px, fb->w * 4); SDL_RenderCopy(ren, softTex, NULL, NULL); SDL_RenderPresent(ren); }
        else gfx_end();
        if (!cfg.vsync) { Uint32 t2 = SDL_GetTicks(); if (t2 - last < 16) SDL_Delay(16 - (t2 - last)); } last = SDL_GetTicks();
    }
    session_free(); audio_close(); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}
