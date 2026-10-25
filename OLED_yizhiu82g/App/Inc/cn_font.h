/* cn_font.h - 16x16 Chinese glyphs for u8g2_DrawXBMP */
#ifndef CN_FONT_H
#define CN_FONT_H

#include <stdint.h>
#include "u8g2.h"

typedef struct
{
    uint16_t code;          /* Unicode code point */
    const uint8_t *data;    /* 16x16 XBMP, LSB first, 2 bytes per row */
} CnGlyph;

/* Unicode code points used by this project */
#define CN_ZHANG  0x5F20u
#define CN_YA     0x96C5u
#define CN_LIN    0x7433u
#define CN_XUE    0x5B66u
#define CN_HAO    0x53F7u
#define CN_XING   0x59D3u
#define CN_MING   0x540Du

extern const CnGlyph cn_glyph_table[];
extern const uint8_t cn_glyph_count;

/* Draw one 16x16 Chinese glyph; returns the glyph advance (16) or 0. */
uint8_t CnFont_Draw(u8g2_t *u8g2, int16_t x, int16_t y, uint16_t code);
/* Draw a string of Unicode code points (0 terminated), 16 px advance. */
void CnFont_DrawStr(u8g2_t *u8g2, int16_t x, int16_t y, const uint16_t *codes);
/* Width of a code point string. */
int16_t CnFont_StrWidth(const uint16_t *codes);

#endif
