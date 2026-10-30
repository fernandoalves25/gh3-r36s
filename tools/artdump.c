/* Dumps the game's 2D art (src/gamearts.c) to BMP files, to see what a picture, a highway texture or a font looks like.
 *   cc -O2 -Isrc tools/artdump.c src/gamearts.c src/gh3fmt.c src/gfx.c src/fb.c src/font.c -o build/artdump $(sdl2-config --cflags --libs) -lm
 *   build/artdump DATA/ZONES/global OUTDIR probe                 # which of the names the browser port knows are in the pak
 *   build/artdump DATA/ZONES/global OUTDIR img:menu_main_bg mat:sys_Gem2D_Green fnt:fontgrid_title_a1 tex:2c0ed3fd
 * Transparent pixels are shown over dark grey (the BMPs have no alpha). */
#include "gamearts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void save(const fb_t *f, const char *dir, const char *name) {
    char p[600]; snprintf(p, sizeof p, "%s/%s.bmp", dir, name); for (char *c = p + strlen(dir) + 1; *c; c++) if (*c == ':' || *c == '/') *c = '_';
    FILE *o = fopen(p, "wb"); if (!o) { perror(p); return; } uint32_t sz = 54 + (uint32_t)f->w * f->h * 4;
    uint8_t h[54] = { 'B', 'M', sz, sz >> 8, sz >> 16, sz >> 24, 0, 0, 0, 0, 54, 0, 0, 0, 40, 0, 0, 0 }; int32_t w = f->w, hh = -f->h; memcpy(h + 18, &w, 4); memcpy(h + 22, &hh, 4); h[26] = 1; h[28] = 32; uint32_t img = (uint32_t)f->w * f->h * 4; memcpy(h + 34, &img, 4);
    fwrite(h, 1, 54, o);
    for (int i = 0; i < f->w * f->h; i++) { uint32_t c = f->px[i], a = c >> 24, bg = 0x40; uint8_t px[4] = { (uint8_t)(((c & 255) * a + bg * (255 - a)) / 255), (uint8_t)((((c >> 8) & 255) * a + bg * (255 - a)) / 255), (uint8_t)((((c >> 16) & 255) * a + bg * (255 - a)) / 255), 255 }; fwrite(px, 1, 4, o); }
    fclose(o); printf("%s %dx%d\n", p, f->w, f->h);
}
static const char *KNOWN[] = { "hud_rock_body", "hud_rock_bg_green", "hud_rock_bg_red", "hud_rock_bg_yellow", "hud_rock_lights_all", "hud_rock_lights_green", "hud_rock_lights_red", "hud_rock_lights_yellow", "hud_rock_needle",
    "hud_rock_tube", "hud_rock_tube_glow_full", "hud_rock_tube_glow_full_b", "hud_score_body", "hud_score_light_0", "hud_score_light_1", "hud_score_light_2", "hud_score_nixie_1a", "hud_score_nixie_2a", "hud_score_nixie_3a", "hud_score_nixie_4a",
    "hud_score_nixie_2b", "hud_score_nixie_4b", "hud_score_nixie_6b", "hud_score_nixie_8b", "hud_score_flash", "hud_counter_body", "hud_counter_drum", "hud_counter_drum_icon", "loading_flying_static",
    "menu_main_bg", "menu_main_bg_2", "menu_main_illustrations", "menu_main_eyes_bl", "menu_main_eyes_br", "menu_main_eyes_c", "menu_main_eyes_tl", "menu_main_eyes_tr", "character_hub_hilite_bookend", "white", "menu_venue_bg", "1p_difficulty_poster",
    "tape_01", "tape_02", "difficulty_title_en", "light_overlay_3spots", "practice_poster_1", "practice_poster_2", "practice_typebar_1", "practice_typebar_2", "tape_h_02", "tape_v_02", "helper_pill_body", "helper_pill_end",
    "setlist_bg_head_676", "setlist_shoeprint", "setlist_page3_head_9x6", "setlist_page2_head_80x54", "setlist_page1_line_red", "setlist_page1_head_lines", "setlist_page1_title", "setlist_star_gold", "setlist_clip", "setlist_overshadow", "setlist_icon_generic",
    "setlist_bg_loop_1308", "setlist_page1_head_90x75", "setlist_page1_loop_89x33", "setlist_page1_line_dotted_1", "setlist_page1_line_solid_1", "menu_pause_bg_tile", "menu_pause_frame_banner", "star_solid_32", "star_empty_32",
    "menu_pause_frame_1", "menu_pause_frame_2", "menu_pause_bunny", "gradient_128", "GH3_Pause_Bunny_Flame1", "dialog_title_bg", "dialog_menu_bg", "dialog_menu_hilight", "dialog_frame_joiner", "options_controller_check", "options_controller_x",
    "options_main_poster", "options_main_highlight", "options_main_monster", "tape_h_03", "tape_h_04", "menu_venue_overlay", "graphics_options_poster_part1", "options_audio_poster", "options_controller_poster", "options_video_poster", "options_calibrate_paper",
    "options_calibrate_highlight", "controller_config_poster", "toprockers_bg", "toprockers_poster", "Toprockers_Bar_1", "Toprockers_Bar_2", "top_rockers_hilite_skull", "data_settings_poster" };
static const char *FONTS[] = { "fontgrid_title_a1", "fontgrid_text_a6", "fontgrid_text_a4", "fontgrid_text_a5", "fontgrid_text_a11", "buttons_x360", "fontgrid_text_a10", "fontgrid_text_a11_large", "fontgrid_text_a3", "fontgrid_text_a10_large", "fontgrid_numeral_a9", "fontgrid_numeral_a7" };
static const char *MATS[] = { "sys_Gem2D_Green", "sys_Gem2D_Green_hammer", "sys_Star2D_Green", "sys_Star2D_Green_Hammer", "sys_Whammy2D_Green", "sys_NowBar_Button01_Green_Lip", "sys_NowBar_Button01_Green_Mid2", "sys_NowBar_Button01_Green_Down", "sys_NowBar_Head_Green", "sys_NowBar_Head_GreenL",
    "sys_Gem2D_StarPower", "sys_Gem2D_StarPower_hammer", "sys_Whammy2D_StarPower", "sys_Whammy2D_Dead", "sys_NowBar_Neck01", "sys_fretbar_small", "sys_fretbar_medium", "sys_fretbar_large", "sys_sidebar2D", "sys_Starpower_SDGLOW", "sys_note_hit01", "sys_note_hit_blue_01", "sys_whammy_hold01", "sys_Big_Bolt01" };
int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: artdump PATH-without-.pak.xen OUTDIR probe | img:NAME mat:NAME fnt:NAME tex:HEX ...\n"); return 1; }
    char pk[600], pb[600]; snprintf(pk, sizeof pk, "%s.pak.xen", argv[1]); snprintf(pb, sizeof pb, "%s.pab.xen", argv[1]);
    if (ga_open(pk, pb)) { fprintf(stderr, "cannot open %s\n", pk); return 1; } gfx_init(NULL, 64, 64);
    for (int i = 3; i < argc; i++) { const char *a = argv[i]; fb_t f;
        if (!strcmp(a, "probe")) {
            for (size_t k = 0; k < sizeof KNOWN / sizeof *KNOWN; k++) { if (ga_img_fb(KNOWN[k], &f) == 0) { printf("img %-34s %4dx%d\n", KNOWN[k], f.w, f.h); fb_free(&f); } else printf("img %-34s MISSING\n", KNOWN[k]); }
            for (size_t k = 0; k < sizeof MATS / sizeof *MATS; k++) { if (ga_mat_fb(MATS[k], &f) == 0) { printf("mat %-34s %4dx%d\n", MATS[k], f.w, f.h); fb_free(&f); } else printf("mat %-34s MISSING\n", MATS[k]); }
            for (size_t k = 0; k < sizeof FONTS / sizeof *FONTS; k++) { ga_font *fn = ga_font_get(FONTS[k]); if (fn) { gfx_sprite *s = ga_font_sheet(fn); printf("fnt %-34s size %.0f sheet %dx%d\n", FONTS[k], ga_font_size(fn), gfx_sprite_w(s), gfx_sprite_h(s)); } else printf("fnt %-34s MISSING\n", FONTS[k]); }
            uint32_t key; if (ga_tex_biggest(512, 1024, &key) == 0) printf("highway 512x1024 key %08x\n", key); continue; }
        if (!strncmp(a, "img:", 4)) { if (ga_img_fb(a + 4, &f) == 0) { save(&f, argv[2], a); fb_free(&f); } else printf("%s missing\n", a); }
        else if (!strncmp(a, "mat:", 4)) { if (ga_mat_fb(a + 4, &f) == 0) { save(&f, argv[2], a); fb_free(&f); } else printf("%s missing\n", a); }
        else if (!strncmp(a, "tex:", 4)) { if (ga_tex_fb((uint32_t)strtoul(a + 4, NULL, 16), &f) == 0) { save(&f, argv[2], a); fb_free(&f); } else printf("%s missing\n", a); }
        else if (!strncmp(a, "fnt:", 4)) { ga_font *fn = ga_font_get(a + 4); if (!fn) { printf("%s missing\n", a); continue; }
            /* a sample line drawn with it (software gfx draws into a readable framebuffer) */
            int W = 900, H = (int)(ga_font_size(fn) * 1.4f) + 8; gfx_init(NULL, W, H); gfx_begin();
            ga_text(fn, 4, 4, ga_font_size(fn), "CARREIRA Jogo rápido Opções 0123456789 ÁÉÍÓÚ ãõç", RGB(255, 255, 255)); save(gfx_soft_fb(), argv[2], a); printf("%s: size %.0f\n", a, ga_font_size(fn)); }
    }
    return 0;
}
