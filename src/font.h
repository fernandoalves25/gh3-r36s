/* The pixel font: 5 x 11 cells (two rows for accents above a 5 x 7 letter, two below for descenders and the
 * cedilla), ASCII 32..126, the accented letters Portuguese needs and a few symbols (stars, arrows, a note).
 * Strings are UTF-8. */
#ifndef FONT_H_INCLUDED
#define FONT_H_INCLUDED
#include <stdint.h>
#define FONT_W 5
#define FONT_H 11
#define FONT_ADV 6
#define FONT_LINE 12
int font_count(void);                                /* glyphs in the atlas */
int font_next(const char **s);                       /* decodes one UTF-8 char from *s (advancing it) to a glyph index */
const uint16_t *font_glyph(int idx);                 /* 5 columns, bit r = row r (0 = top) */
int font_strlen(const char *s);                      /* glyphs in a UTF-8 string */
#endif
