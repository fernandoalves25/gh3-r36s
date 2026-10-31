/* The loading screen and the song's intro card, in the game's look (see orig_loading.h). */
#include "orig_loading.h"
#include "original.h"
#include "gamearts.h"
#include "app.h"
#include "lang.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <math.h>

static const char *F_TITLE = "fontgrid_title_a1", *F_TEXT = "fontgrid_text_a4";
#define CREAM RGB(245, 226, 190)
#define AMBER RGB(233, 176, 76)
#define SHADOW RGBA(0, 0, 0, 230)

/* the game's loading_screen_tips (qb.pak), in Portuguese (T(): the English is the game's own text, in
 * lang_song.c); the online, co-op and outfit ones left out (not in this port) */
static const char *const TIPS[] = {
    "Com dificuldade? Confira os tutoriais no menu Treino.",
    "Confira a Loja da carreira: tem músicas novas esperando por você.",
    "O treino ajuda a acertar aqueles solos mais complicados.",
    "Se você nunca gravou numa fita de 4 canais, não é uma banda de verdade.",
    "Antes de pular do palco, garanta que a plateia está a fim de te segurar.",
    "NÃO use fogos de artifício caseiros.",
    "Chegar 30 minutos atrasado no ensaio é pura tradição.",
    "Dá pra tirar um pouco dessa porcaria dos retornos?",
    "Nunca ache que vocês foram a primeira banda a subir no palco toda de preto.",
    "Sim, os retornos do palco são, de fato, trampolins.",
    "Nunca, jamais, deixe o vocalista cuidar da mixagem.",
    "Sempre dá pra ficar mais alto.",
    "Somos uma banda muito revoltada, mas isso não quer dizer que a gente não acredite na paz.",
    "Odisseias de jazz improvisado de 20 minutos NÃO estão liberadas.",
    "Ninguém paga ingresso pra ver você ficar improvisando.",
    "Leva muito tempo pra parecer que você acabou de acordar.",
    "Garanta que alguém da banda saiba trocar um pneu furado.",
    "Aumenta esse amplificador. Meus ouvidos mal estão sangrando.",
    "Eu surtava quando meu amplificador pegava fogo. Agora até curto o timbre.",
    "Guitarras destruídas parecem soar melhor que as irmãs inteiras.",
    "Você é tão bom quanto o seu último show... que foi horrível.",
    "Você tocou bem. Com certeza estavam vaiando só a sua roupa.",
    "Talvez uma bateria eletrônica chegasse no horário do ensaio.",
    "Parece que tem um problema no seu amplificador de baixo. Dá pra ouvir!",
    "Vai lá, detona e vende um monte de camiseta pra gente poder comer.",
    "Vamos abrir com o nosso último single. Assim a gente tira essa *&?#@! do caminho logo!",
    "Eu sou o baterista. Não me pagam pra entender nada disso!",
    "Juro, seu guarda, a TV do camarim se soltou da parede sozinha e voou pela janela!",
    "Errando notas demais? Use Calibrar atraso no menu Opções.",
};
#define NTIPS ((int)(sizeof TIPS / sizeof *TIPS))
/* the venues' names, as the game's levelzones titles them */
static const char *const VENUE[][2] = { { "z_party", "Backyard Bash" }, { "z_dive", "Mitch's Moose Lounge" }, { "z_video", "Video Shoot" }, { "z_artdeco", "Ye Olde Royal Odeon" },
    { "z_prison", "Shanker's Island" }, { "z_wikker", "Desert Rock Festival" }, { "z_budokan", "Kaiju Megadome" }, { "z_hell", "Lou's Inferno" } };
static const char *venue_name(int tier) {
    if (!A.lib || tier < 0 || tier >= A.lib->ntiers || !A.lib->shop.tierZone) return "";
    for (size_t i = 0; i < sizeof VENUE / sizeof *VENUE; i++) if (!strcmp(A.lib->shop.tierZone[tier], VENUE[i][0])) return VENUE[i][1];
    return "";
}
static void upper(const char *s, char *o, size_t cap) {   /* GetUpperCaseString: ASCII and the accented Latin-1 letters */
    size_t n = 0; for (const unsigned char *p = (const unsigned char *)s; *p && n + 1 < cap; p++) {
        if (*p == 0xC3 && p[1] >= 0xA0 && p[1] <= 0xBE && p[1] != 0xB7 && n + 2 < cap) { o[n++] = (char)0xC3; o[n++] = (char)(p[1] - 0x20); p++; }
        else o[n++] = (char)toupper(*p); }
    o[n] = 0;
}
/* the song's artist_text ("by", "as made famous by", "inspired by", "feat. X, as made famous by") in the language
 * (in English the game's own words as they are) */
static void artist_line(const char *en, char *o, size_t cap) {
    char b[96]; *o = 0; if (!en || !*en) return; const char *f = strcasestr(en, "as made famous by");
    if (lang_get() == LANG_EN) snprintf(b, sizeof b, "%s", en);
    else if (!strcasecmp(en, "by")) snprintf(b, sizeof b, "%s", T("de"));
    else if (!strcasecmp(en, "inspired by")) snprintf(b, sizeof b, "%s", T("inspirada em"));
    else if (f) { const char *p = en; int feat = !strncasecmp(p, "feat. ", 6); if (feat) p += 6; int n = (int)(f - p); while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == ',')) n--;
        if (n > 0) snprintf(b, sizeof b, "%s%.*s, %s", feat ? T("com ") : "", n, p, T("sucesso de")); else snprintf(b, sizeof b, "%s", T("sucesso de")); }
    else snprintf(b, sizeof b, "%s", en);
    upper(b, o, cap);
}
/* words of s set in lines no wider than maxW; returns the count */
static int wrap(const ga_font *f, const char *s, float size, float maxW, char lines[][160], int maxLines) {
    int n = 0; char cur[160] = ""; const char *p = s;
    while (*p && n < maxLines) { while (*p == ' ') p++; if (!*p) break; const char *e = p; while (*e && *e != ' ') e++;
        char next[160]; snprintf(next, sizeof next, "%s%s%.*s", cur, *cur ? " " : "", (int)(e - p), p);
        if (*cur && ga_text_w(f, next, size) > maxW) { snprintf(lines[n++], 160, "%s", cur); snprintf(cur, sizeof cur, "%.*s", (int)(e - p), p); } else snprintf(cur, sizeof cur, "%s", next);
        p = e; }
    if (*cur && n < maxLines) snprintf(lines[n++], 160, "%s", cur);
    return n;
}

/* ================================================================== the loading screen */
static struct { double at; int drawn, loaded, tip; } L = { 0, 0, 0, -1 };
void loading_start(double now) { int t = rand() % NTIPS; if (t == L.tip) t = (t + 1) % NTIPS; L.at = now; L.drawn = L.loaded = 0; L.tip = t; }
int loading_phase(double now) { return !L.drawn ? 0 : !L.loaded ? 1 : now - L.at >= LOADING_MIN_MS ? 2 : 0; }
void loading_loaded(void) { L.loaded = 1; }
static void spinner(float cx, float cy, float r, double now) {   /* ten dots round a ring, the bright one going round */
    int head = (int)(now / 80) % 10;
    for (int i = 0; i < 10; i++) { float a = i * (float)M_PI / 5 - (float)M_PI / 2, k = (float)((head - i + 10) % 10) / 9, s = 3 + 2 * (1 - k), x = cx + cosf(a) * r - s / 2, y = cy + sinf(a) * r - s / 2;
        gfx_rect(x + 1, y + 1, s, s, RGBA(0, 0, 0, 160)); gfx_rect(x, y, s, s, fb_alpha(RGB(255, 255, 255), 0.25f + 0.75f * (1 - k) * (1 - k))); }
}
void loading_draw(double now) {
    int W = A.W, H = A.H; L.drawn = 1;
    const gh3_song *s = A.lib && A.selSong >= 0 && A.selSong < A.lib->nsongs ? &A.lib->songs[A.selSong] : NULL;
    const char *tip = T(L.tip >= 0 ? TIPS[L.tip] : TIPS[0]), *venue = venue_name(A.selTier); char b[200], u[200];
    gfx_rect(0, 0, (float)W, (float)H, RGB(0, 0, 0));
    gfx_sprite *pic = orig_ready() ? ga_img("loading_flying_static") : NULL; const ga_font *ft = ga_font_get(F_TITLE), *fx = ga_font_get(F_TEXT);
    if (!pic || !ft || !fx) {   /* without the game's art: the pixel font */
        gfx_text_sh_center(W / 2, H / 2 - 60, 3, T("CARREGANDO"), C_INK, C_SHADOW); gfx_text_box(W / 6, H / 2, W * 2 / 3, 1, tip, C_MUTED);
        if (s) gfx_text_sh_center(W / 2, 24, 2, s->title, C_AMP, C_SHADOW); spinner(W / 2.0f, H - 40.0f, 10, now); return; }
    /* the picture over the whole height (its letterbox bars are part of it), the sides cut */
    int pw = gfx_sprite_w(pic), ph = gfx_sprite_h(pic); float k = fmaxf((float)W / pw, (float)H / ph), ox = (W - pw * k) / 2, oy = (H - ph * k) / 2;
    gfx_blit(pic, 0, 0, pw, ph, ox, oy, pw * k, ph * k, RGB(255, 255, 255), 1);
    float ik = pw / 1280.0f;   /* the game's 1280 x 720 stage on the picture */
    /* the tip at (860, 300) of the stage, centred in its block, the game's hard shadow */
    { char lines[8][160]; float size = 23, lh = size * 1.12f, cx = ox + 860 * ik * k, cy = oy + 300 * (ph / 720.0f) * k, bw = fminf(290, (W - cx) * 2 - 16);
      int n = wrap(fx, tip, size, bw, lines, 8); float y = cy - n * lh / 2;
      for (int i = 0; i < n; i++, y += lh) { ga_text_center(fx, cx + 3, y + 3, size, lines[i], RGB(0, 0, 0)); ga_text_center(fx, cx, y, size, lines[i], RGB(255, 255, 255)); } }
    /* the word LOADING is in the picture (x 405..580, y 335..390 of the 1024 x 512 picture): the spinner beside it */
    spinner(ox + 606 * (pw / 1024.0f) * k, oy + 362 * (ph / 512.0f) * k, 9, now);
    /* the black bands: the song above, the venue and the difficulty below */
    if (s) { float top = oy + 8 * (ph / 512.0f) * k; upper(s->title, u, sizeof u); ga_text_fit(ft, 16, top, 27, W * 0.6f, u, CREAM, 0);
        snprintf(b, sizeof b, "%s%s%s", s->artist, *s->year ? ", " : "", s->year); ga_text_fit(fx, W - 16.0f, top + 8, 17, W * 0.34f, b, fb_alpha(CREAM, 0.75f), 2); }
    { float y = oy + 466 * (ph / 512.0f) * k; if (*venue) ga_text_fit(fx, 16, y, 18, W * 0.55f, venue, fb_alpha(CREAM, 0.8f), 0);
      snprintf(b, sizeof b, "%s%s", T(diff_name_pt(A.selDiff)), A.mode == MODE_PRACTICE ? T(" • Treino") : ""); ga_text_right(fx, W - 16.0f, y, 18, b, AMBER); }
}

/* ================================================================== the intro card */
/* intro_song_info_text (the title) at (255, 75) of the 1280 x 720 stage, the artist_text line at (255, 135) and
 * intro_artist_info_text at (255, 150), all upper case; current_intro fades them in over 700 ms, holds them 3 s and
 * fades them out over 700 ms. Here the card slides in from the left as it fades in. */
#define INTRO_DELAY 250.0
#define INTRO_FADE 700.0
#define INTRO_HOLD 2500.0   /* (the game's 3000 ends before its highway rises; here the highway is up from the start) */
void orig_intro_card(const hud_info *hi, double t, int W, int H) {
    static double t0, last; static int seen;
    if (!seen || t < last - 250 || t > last + 2500) t0 = t; seen = 1; last = t;   /* a new run (or a restart): the card again */
    const ga_font *ft = ga_font_get(F_TITLE), *fx = ga_font_get(F_TEXT); char u[200], at[120];
    if (hi->section && *hi->section) { if (fx) ga_text_sh(fx, 14, 8, 17, hi->section, AMBER, SHADOW); else gfx_text_sh(14, 10, 1, hi->section, C_AMP, C_SHADOW); }
    double e = t - t0 - INTRO_DELAY; if (e < 0 || e > INTRO_FADE * 2 + INTRO_HOLD || !hi->title) return;
    float a = (float)(e < INTRO_FADE ? e / INTRO_FADE : e > INTRO_FADE + INTRO_HOLD ? 1 - (e - INTRO_FADE - INTRO_HOLD) / INTRO_FADE : 1), in = (float)fmin(1, e / INTRO_FADE);
    float slide = -70 * (1 - in) * (1 - in), x = W * 255 / 1280.0f + slide, sy = H / 720.0f, maxW = W - x - 16;
    const gh3_song *s = A.S.song; artist_line(s && hi->artist == s->artist ? s->artistText : "", at, sizeof at);
    if (!ft || !fx) { gfx_text_sh((int)x, (int)(60 * sy), 2, hi->title, fb_alpha(C_INK, a), fb_alpha(C_SHADOW, a)); if (hi->artist) gfx_text_sh((int)x, (int)(140 * sy), 1, hi->artist, fb_alpha(C_MUTED, a), fb_alpha(C_SHADOW, a)); return; }
    upper(hi->title, u, sizeof u); ga_text_fit(ft, x + 2, 52 * sy + 2, 34, maxW, u, fb_alpha(SHADOW, a), 0); ga_text_fit(ft, x, 52 * sy, 34, maxW, u, fb_alpha(CREAM, a), 0);
    if (*at) { ga_text_fit(fx, x + 1, 124 * sy + 1, 14, maxW, at, fb_alpha(SHADOW, a), 0); ga_text_fit(fx, x, 124 * sy, 14, maxW, at, fb_alpha(AMBER, a), 0); }
    if (hi->artist && *hi->artist) { upper(hi->artist, u, sizeof u); ga_text_fit(ft, x + 2, 146 * sy + 2, 24, maxW, u, fb_alpha(SHADOW, a), 0); ga_text_fit(ft, x, 146 * sy, 24, maxW, u, fb_alpha(RGB(255, 255, 255), a), 0); }
}
