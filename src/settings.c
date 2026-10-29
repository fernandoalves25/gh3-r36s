#include "settings.h"
#include "input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

settings_t SET;
static char home[520];
const char *settings_home(void) { if (!home[0]) { const char *e = getenv("GH3_HOME"); snprintf(home, sizeof home, "%s", e && *e ? e : "."); } return home; }
static void path_of(char *out, size_t n, const char *name) { snprintf(out, n, "%s/%s", settings_home(), name); }

void settings_defaults(void) {
    memset(&SET, 0, sizeof SET); SET.tap = 0; SET.lefty = 0; SET.hyper = 0; SET.noFail = 0; SET.stageFx = 1; SET.showFps = 0; SET.preview = 1;
    SET.volMusic = 100; SET.volSfx = 70; SET.offsetMs = 0; SET.bgDim = 40;
    input_apply_preset(0);
}
static const char *ACTKEY[ACT_N] = { "verde", "vermelho", "amarelo", "azul", "laranja", "palhetada_cima", "palhetada_baixo", "star_power", "whammy", "pausa" };
static void bind_to_str(const bind_t *b, char *out, size_t n) {
    if (b->type == BIND_BUTTON) snprintf(out, n, "botao:%d", b->id); else if (b->type == BIND_AXIS) snprintf(out, n, "eixo:%d:%d", b->id, b->dir); else snprintf(out, n, "nada");
}
static void str_to_bind(const char *s, bind_t *b) {
    memset(b, 0, sizeof *b); int a, c;
    if (sscanf(s, "botao:%d", &a) == 1) { b->type = BIND_BUTTON; b->id = a; }
    else if (sscanf(s, "eixo:%d:%d", &a, &c) == 2) { b->type = BIND_AXIS; b->id = a; b->dir = c; }
}
int settings_save(void) {
    char p[560]; path_of(p, sizeof p, "gh3.cfg"); FILE *f = fopen(p, "w"); if (!f) return -1;
    fprintf(f, "# Guitar Hero III (R36S) - opcoes salvas pelo jogo; apague o arquivo para voltar ao padrao\n");
    fprintf(f, "palhetada=%d\ncanhoto=%d\nvelocidade=%d\nsem_falhar=%d\npalco=%d\nfps=%d\nprevia=%d\nvolume_musica=%d\nvolume_efeitos=%d\natraso_ms=%d\nescurecer_fundo=%d\nliberar_tudo=%d\ntema=%d\n",
            SET.tap ? 0 : 1, SET.lefty, SET.hyper, SET.noFail, SET.stageFx, SET.showFps, SET.preview, SET.volMusic, SET.volSfx, SET.offsetMs, SET.bgDim, SET.unlockAll, SET.theme);
    for (int a = 0; a < ACT_N; a++) for (int s = 0; s < BIND_SLOTS; s++) { char b[40]; bind_to_str(&SET.binds[a][s], b, sizeof b); fprintf(f, "controle_%s_%d=%s\n", ACTKEY[a], s + 1, b); }
    fclose(f); return 0;
}
int settings_load(void) {
    char p[560]; path_of(p, sizeof p, "gh3.cfg"); FILE *f = fopen(p, "r"); if (!f) return -1;
    char line[200];
    while (fgets(line, sizeof line, f)) { char *eq = strchr(line, '='); if (!eq || line[0] == '#') continue; *eq = 0; char *k = line, *v = eq + 1; v[strcspn(v, "\r\n")] = 0; int n = atoi(v);
        if (!strcmp(k, "palhetada")) SET.tap = n ? 0 : 1; else if (!strcmp(k, "canhoto")) SET.lefty = !!n; else if (!strcmp(k, "velocidade")) SET.hyper = n < 0 ? 0 : n > 5 ? 5 : n;
        else if (!strcmp(k, "sem_falhar")) SET.noFail = !!n; else if (!strcmp(k, "palco")) SET.stageFx = !!n; else if (!strcmp(k, "fps")) SET.showFps = !!n; else if (!strcmp(k, "previa")) SET.preview = !!n;
        else if (!strcmp(k, "volume_musica")) SET.volMusic = n < 0 ? 0 : n > 100 ? 100 : n; else if (!strcmp(k, "volume_efeitos")) SET.volSfx = n < 0 ? 0 : n > 100 ? 100 : n;
        else if (!strcmp(k, "atraso_ms")) SET.offsetMs = n < -300 ? -300 : n > 300 ? 300 : n; else if (!strcmp(k, "escurecer_fundo")) SET.bgDim = n < 0 ? 0 : n > 100 ? 100 : n;
        else if (!strcmp(k, "liberar_tudo")) SET.unlockAll = !!n; else if (!strcmp(k, "tema")) SET.theme = n < 0 ? 0 : n >= THEME_N ? THEME_N - 1 : n;
        else if (!strncmp(k, "controle_", 9)) { for (int a = 0; a < ACT_N; a++) { size_t L = strlen(ACTKEY[a]); if (!strncmp(k + 9, ACTKEY[a], L) && k[9 + L] == '_') { int s = atoi(k + 10 + L) - 1; if (s >= 0 && s < BIND_SLOTS) str_to_bind(v, &SET.binds[a][s]); } } }
    }
    fclose(f); return 0;
}

/* ---- records ---- */
static record_t *recs; static int nrecs, reccap;
static void rec_key(char *out, size_t n, const char *song, int diff, int part) { snprintf(out, n, "%s/%d/%d", song, diff, part); }
const record_t *records_get(const char *song, int diff, int part) { char k[96]; rec_key(k, sizeof k, song, diff, part); for (int i = 0; i < nrecs; i++) if (!strcmp(recs[i].key, k)) return &recs[i]; return NULL; }
int records_put(const char *song, int diff, int part, long score, int pct, int streak, int stars) {
    char k[96]; rec_key(k, sizeof k, song, diff, part); record_t *r = NULL; for (int i = 0; i < nrecs; i++) if (!strcmp(recs[i].key, k)) r = &recs[i];
    int best = 0;
    if (!r) { if (nrecs == reccap) { reccap = reccap ? reccap * 2 : 64; recs = realloc(recs, reccap * sizeof *recs); } r = &recs[nrecs++]; memset(r, 0, sizeof *r); snprintf(r->key, sizeof r->key, "%s", k); best = 1; }
    r->plays++;
    if (score > r->score || best) { r->score = score; r->pct = pct; r->stars = stars; best = 1; }
    if (streak > r->streak) r->streak = streak;
    if (pct > r->pct) r->pct = pct;
    if (stars > r->stars) r->stars = stars;
    records_save(); return best;
}
int records_save(void) {
    char p[560]; path_of(p, sizeof p, "gh3-recordes.cfg"); FILE *f = fopen(p, "w"); if (!f) return -1;
    fprintf(f, "# musica/dificuldade/parte=pontos,acertos%%,sequencia,estrelas,vezes\n");
    for (int i = 0; i < nrecs; i++) fprintf(f, "%s=%ld,%d,%d,%d,%d\n", recs[i].key, recs[i].score, recs[i].pct, recs[i].streak, recs[i].stars, recs[i].plays);
    fclose(f); return 0;
}
int records_load(void) {
    char p[560]; path_of(p, sizeof p, "gh3-recordes.cfg"); FILE *f = fopen(p, "r"); if (!f) return -1; char line[200]; nrecs = 0;
    while (fgets(line, sizeof line, f)) { char *eq = strchr(line, '='); if (!eq || line[0] == '#') continue; *eq = 0;
        if (nrecs == reccap) { reccap = reccap ? reccap * 2 : 64; recs = realloc(recs, reccap * sizeof *recs); }
        record_t *r = &recs[nrecs]; memset(r, 0, sizeof *r); snprintf(r->key, sizeof r->key, "%s", line);
        if (sscanf(eq + 1, "%ld,%d,%d,%d,%d", &r->score, &r->pct, &r->streak, &r->stars, &r->plays) >= 4) nrecs++; }
    fclose(f); return 0;
}
int records_count_stars(void) { int n = 0; for (int i = 0; i < nrecs; i++) n += recs[i].stars; return n; }
