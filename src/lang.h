/* The language of everything the player reads: Português (the strings written in the code), English, Español.
 * Every text shown goes through T("...") with its Portuguese as the key; English and Spanish come from the tables in
 * src/lang/ (one file per group of screens, lang_entry rows). A key no table has stays in Portuguese
 * (GH3_LANG_LOG=1 logs each one once). Formats keep their conversions in the same order in every language. */
#ifndef LANG_H
#define LANG_H
enum { LANG_PT, LANG_EN, LANG_ES, LANG_N };
typedef struct { const char *pt, *en, *es; } lang_entry;   /* a table ends with a row whose pt is NULL */
const char *T(const char *pt);       /* the text in the chosen language */
void lang_set(int lang);             /* LANG_PT / LANG_EN / LANG_ES */
int lang_get(void);
const char *lang_name(int lang);     /* "Português", "English", "Español" (each in its own language) */
const char *lang_code(int lang);     /* "pt", "en", "es": gh3.cfg's idioma= */
int lang_from_code(const char *code);
int lang_check(void);                /* --langtest: rows with an empty column and keys given twice; 0 when all is well */
#endif
