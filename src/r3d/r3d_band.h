/* The band on the 3D stage (the gh3-3d build): the game's musicians read from PAK/<char>.pak (meshes, textures) with their
 * skeletons from ZONES/global.pab and their animations from <char>_anims, perm_anims and the song, skinned on the GPU and
 * drawn by r3d.c after the venue's solid parts, with the same camera. Who plays and what they play follow the game's lists
 * (musician_profiles, the anim sets) as the browser port does: docs/research/band.md. */
#ifndef R3D_BAND_H
#define R3D_BAND_H
#include "r3d_zone.h"
enum { BAND_GUITARIST, BAND_BASSIST, BAND_DRUMMER, BAND_VOCALIST, BAND_ROLES };
int r3d_band_init(void);   /* after the GL entry points are loaded: the skinning shader; 0 when fine */
/* a venue was loaded (r3d.c load_zone): where each one stands (<zone>_trg_waypoint_<role>_start) and the lights kept for the band */
void r3d_band_zone(const char *zone, const gh3_pakent *ents, int n, const r3d_light *lights, int nlights);
void r3d_band_select(void);   /* the band the song about to be played wants (A.selSong: a boss battle puts the boss in the bassist's place); loads who is not loaded yet. Blocking */
/* after the venue's solid parts, with its camera (column-major 4x4). 1 when it drew (and changed the program and the vertex arrays) */
int r3d_band_draw(const float *view, const float *proj, double perf, int sp, float energy);
int r3d_band_where(int role, float pos[3], float *yaw);   /* where a member stands now (the shots that follow one); 0 when nobody is there. role -1: 1 when anyone is on stage */
void r3d_band_test_clock(double ms, const char *song);   /* --r3dtest --band: a song's cues played on this clock, with no run (song NULL / "": none) */
/* The song's end (play_win_anims / play_lose_anims of the game's scripts): the band sees the run finish or fail by itself
 * and takes the win or lose stance (the _smstg ones in the small venues; in a battle the boss the other one) until the
 * next song or the menus. This is the same for --r3dtest: how 1 won, 2 lost, 0 back to the song */
void r3d_band_test_finale(int how);
int r3d_band_finale(void);   /* that end while it lasts (r3d.c's camera cuts to <zone>_cameras_win / _lose): 1 won, 2 lost (a battle: the player's), 0 none */
/* a boss battle's attack as it lands (battle.c): from side 0 (the player, the guitarist) or 1 (the boss, in the bassist's
 * place), the power-up's name key. player1/2_battlemode_anims give each side's kind: the attacker its attack_anim, the
 * other its response_anim (none for the Death Drain, which is in neither) */
void r3d_band_attack(int fromSide, uint32_t powerup);
typedef struct { int members, draws, tris, verts, bones, pal; double poseMs, drawMs, loadMs; size_t vram, animBytes; int clips; char who[BAND_ROLES][24]; } r3d_bandstats;
const r3d_bandstats *r3d_band_stats(void);
void r3d_band_shutdown(void);
#endif
