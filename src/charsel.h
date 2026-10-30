/* Your rocker, outfit, guitar and finish, as GH3's character hub picks them (charsel.c: the data and the logic;
 * orig_charsel.c: the screens in the game's look). The game's own lists, read from qb.pak (gh3_library_section):
 * musician_profiles (the guitarists, their outfits and styles), musician_body / musician_instrument (each outfit's and
 * each guitar finish's file), and the store's lists: the three guitars a player starts with, the guitars and special
 * guitars for sale, the finishes for sale, the secret rockers for sale. Bought items are kept by store.c. */
#ifndef CHARSEL_H
#define CHARSEL_H
#include "gfx.h"
#include <stdint.h>
#include <stddef.h>
enum { SC_CHARSEL = 41 };   /* the character hub's screen (kept apart from app.h's list, as store.h does) */
/* ---- the data */
typedef struct { char name[24], full[40]; const char *blurb; uint32_t body, inst; int secret, price; char pak[48], anims[48]; int hasPak; } cs_rocker;
typedef struct { uint32_t id; int rocker, outfit, style; char oname[40], sname[40], pak[48], anims[48]; int hasPak; } cs_outfit;   /* one style of one outfit */
typedef struct { uint32_t id; int model, price, deflt, cat; char finish[40], line[40], maker[24], pak[48]; const char *blurb; int hasPak; } cs_guitar;   /* one finish; cat: CS_GUITARS / CS_FINISHES list it is sold in, -1 none */
typedef struct { char name[40]; int first, n; } cs_model;   /* a guitar: its finishes are g[first .. first+n-1] */
enum { CS_ROCKERS, CS_GUITARS, CS_FINISHES, CS_CATS };     /* what the store sells from here */
typedef struct { cs_rocker r[24]; int nr; cs_outfit o[200]; int no; cs_guitar g[128]; int ng; cs_model m[64]; int nm; int ok; } cs_data;
const cs_data *charsel_data(void);                   /* read once from A.lib; ok = 0 when qb.pak lacks the lists */
/* ---- the choice (SET.rocker / SET.outfit / SET.guitar), checked against what is owned and on disk */
const char *charsel_rocker(void);                    /* the chosen rocker's profile NAME ("Axel" by default) */
void charsel_paks(const char *rocker, char *body, char *anims, char *guitar, size_t n);   /* the pak basenames (DATA/PAK) of that rocker's chosen outfit and guitar; "" = the profile's own */
gfx_sprite *charsel_photo(const char *rocker);       /* the rocker's photo (the .img every character pak carries); NULL rocker = the chosen one */
/* ---- the store's three categories of them (store.c) */
int charsel_store_n(int cat);
void charsel_store_item(int cat, int i, const char **title, const char **sub, int *price, int *have, const char **info, const char **why, char *key, size_t keyCap);   /* why: why it cannot be bought yet, or NULL */
int charsel_store_find(int cat, const char *key);    /* the item of that key, -1 */
/* ---- the screen */
int charsel_open(double now);                        /* from Career / Quickplay: 1 when the hub is shown (0: no data, carry on) */
void charsel_draw(double now);
void charsel_event(int in, double now);
void charsel_screens(void (*shot)(const char *name));   /* --screens: the hub, the lists, a locked rocker, the store's new shelves */
/* what a screen shows (charsel.c fills it, orig_charsel.c draws it) */
typedef struct { char label[48], sub[64], tag[24]; int locked, chosen, off; } cs_row;
typedef struct { const char *title; cs_row rows[72]; int n, sel, hub; gfx_sprite *photo; char name[48], outfit[96], guitar[112], cash[24], info[200]; const char *blurb; const char *bt[4], *tx[4]; int nh; } cs_view;
void charsel_view(cs_view *v);
int orig_charsel_draw(double now, const cs_view *v);   /* 0 when the game's art is missing */
#endif
