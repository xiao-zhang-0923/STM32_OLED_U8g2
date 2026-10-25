# -*- coding: utf-8 -*-
"""Generate C assets for the OLED project.

Outputs (all ASCII-only C sources):
  App/Inc/cn_font.h / App/Src/cn_font.c
  App/Inc/duck_anim.h / App/Src/duck_anim.c
The animated picture is converted from 图例.jpg (student ID photo).
"""
import os
from PIL import Image, ImageDraw, ImageFont, ImageOps, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "图例.jpg")
CROP = (150, 60, 710, 760)          # full head, a little shoulder
PORTRAIT_SIZE = (48, 64)            # keep the photo aspect, centre on 64x64
SIZE = (64, 64)

CN_CHARS = [("张", 0x5F20), ("雅", 0x96C5), ("琳", 0x7433),
            ("学", 0x5B66), ("号", 0x53F7), ("姓", 0x59D3), ("名", 0x540D)]


def to_bytes(img, w, h):
    """Convert '1' image (white = lit) to u8g2 XBMP bytes (LSB first)."""
    bpr = (w + 7) // 8
    data = bytearray(bpr * h)
    px = img.load()
    for y in range(h):
        for x in range(w):
            if px[x, y]:
                data[y * bpr + x // 8] |= 1 << (x % 8)
    return data


def carray(name, data, per_line=12, static=True):
    out = ["%sconst unsigned char %s[%d] = {" % ("static " if static else "", name, len(data))]
    for i in range(0, len(data), per_line):
        out.append("    " + ", ".join("0x%02X" % b for b in data[i:i + per_line]) + ",")
    out.append("};")
    return "\n".join(out)


def glyph16(ch):
    f = ImageFont.truetype(r"C:\Windows\Fonts\simhei.ttf", 16)
    img = Image.new("L", (16, 16), 0)
    d = ImageDraw.Draw(img)
    bbox = d.textbbox((0, 0), ch, font=f)
    x = (16 - (bbox[2] - bbox[0])) // 2 - bbox[0]
    y = (16 - (bbox[3] - bbox[1])) // 2 - bbox[1]
    d.text((x, y), ch, fill=255, font=f)
    return img.point(lambda v: 255 if v > 96 else 0).convert("1")


def portrait_frame(inset, thr=185):
    rgb = Image.open(SRC).convert("RGB").crop(CROP)
    if inset:
        w, h = rgb.size
        rgb = rgb.crop((inset, inset, w - inset, h - inset))

    small = rgb.resize(PORTRAIT_SIZE, Image.LANCZOS)
    gray = ImageOps.autocontrast(small.convert("L"))

    # blue studio background: remove it, keep the person and the uniform
    w2, h2 = rgb.size
    px = rgb.load()
    m = Image.new("L", (w2, h2), 0)
    mp = m.load()
    for y in range(h2):
        for x in range(w2):
            r, g, b = px[x, y]
            if (b > r + 25) and (b > g + 15) and (r + g + b) > 120:
                continue
            mp[x, y] = 255
    m = m.resize(PORTRAIT_SIZE, Image.LANCZOS).point(lambda v: 255 if v > 100 else 0)

    gp, mpp = gray.load(), m.load()
    out = Image.new("1", PORTRAIT_SIZE, 0)
    op = out.load()
    for y in range(PORTRAIT_SIZE[1]):
        for x in range(PORTRAIT_SIZE[0]):
            if mpp[x, y] and gp[x, y] > thr:
                op[x, y] = 1

    # thin contour so the black hair is still visible on the black background
    edge = m.filter(ImageFilter.FIND_EDGES).point(lambda v: 255 if v > 60 else 0)
    ep = edge.load()
    for y in range(2, PORTRAIT_SIZE[1] - 2):
        for x in range(2, PORTRAIT_SIZE[0] - 2):
            if ep[x, y]:
                op[x, y] = 1

    canvas = Image.new("1", SIZE, 0)
    canvas.paste(out, ((SIZE[0] - PORTRAIT_SIZE[0]) // 2, 0))
    return canvas


def star(size, points=5):
    scale = 8
    big = Image.new("L", (size * scale, size * scale), 0)
    d = ImageDraw.Draw(big)
    import math
    cx = cy = size * scale / 2.0
    r1 = size * scale * 0.48
    r2 = r1 * 0.42
    pts = []
    for i in range(points * 2):
        ang = -math.pi / 2 + i * math.pi / points
        r = r1 if i % 2 == 0 else r2
        pts.append((cx + r * math.cos(ang), cy + r * math.sin(ang)))
    d.polygon(pts, fill=255)
    small = big.resize((size, size), Image.LANCZOS)
    return small.point(lambda v: 255 if v > 110 else 0).convert("1")


def preview(imgs, path, scale=4):
    pad = 6
    w = sum(i.width for i in imgs) + pad * (len(imgs) + 1)
    h = max(i.height for i in imgs) + pad * 2
    c = Image.new("RGB", (w, h), (50, 50, 50))
    x = pad
    for im in imgs:
        c.paste(im.convert("RGB"), (x, pad))
        x += im.width + pad
    c.resize((c.width * scale, c.height * scale), Image.NEAREST).save(path)


def main():
    # --- Chinese font ---
    glyphs = []
    for ch, code in CN_CHARS:
        img = glyph16(ch)
        glyphs.append((code, img))
    src = ["/* cn_font.c - 16x16 Chinese glyphs generated from SimHei by tools/generate_assets.py */",
           '#include "cn_font.h"',
           ""]
    for code, img in glyphs:
        src.append("/* U+%04X */" % code)
        src.append(carray("cn_glyph_%04X" % code, to_bytes(img, 16, 16)))
        src.append("")
    src.append("const CnGlyph cn_glyph_table[] = {")
    for code, _ in glyphs:
        src.append("    {0x%04X, cn_glyph_%04X}," % (code, code))
    src.append("};")
    src.append("const uint8_t cn_glyph_count = sizeof(cn_glyph_table) / sizeof(cn_glyph_table[0]);")
    src.append("")
    src.append("uint8_t CnFont_Draw(u8g2_t *u8g2, int16_t x, int16_t y, uint16_t code)")
    src.append("{")
    src.append("    uint8_t i;")
    src.append("    if (x < 0 || y < 0)"
              " return 0;")
    src.append("    for (i = 0; i < cn_glyph_count; i++)")
    src.append("    {")
    src.append("        if (cn_glyph_table[i].code == code)")
    src.append("        {")
    src.append("            u8g2_DrawXBMP(u8g2, (u8g2_uint_t)x, (u8g2_uint_t)y, 16, 16, cn_glyph_table[i].data);")
    src.append("            return 16;")
    src.append("        }")
    src.append("    }")
    src.append("    return 0;")
    src.append("}")
    src.append("")
    src.append("void CnFont_DrawStr(u8g2_t *u8g2, int16_t x, int16_t y, const uint16_t *codes)")
    src.append("{")
    src.append("    while (*codes != 0)")
    src.append("    {")
    src.append("        (void)CnFont_Draw(u8g2, x, y, *codes);")
    src.append("        x = (int16_t)(x + 16);")
    src.append("        codes++;")
    src.append("    }")
    src.append("}")
    src.append("")
    src.append("int16_t CnFont_StrWidth(const uint16_t *codes)")
    src.append("{")
    src.append("    int16_t w = 0;")
    src.append("    while (*codes != 0) { w = (int16_t)(w + 16); codes++; }")
    src.append("    return w;")
    src.append("}")
    with open(os.path.join(ROOT, "App", "Src", "cn_font.c"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(src) + "\n")

    hdr = """/* cn_font.h - 16x16 Chinese glyphs for u8g2_DrawXBMP */
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
"""
    with open(os.path.join(ROOT, "App", "Inc", "cn_font.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write(hdr)

    # --- animated portrait frames (from 图例.jpg) ---
    frames = [portrait_frame(0), portrait_frame(24), portrait_frame(48)]
    anim_src = ["/* duck_anim.c - 64x64 1-bit portrait frames from 图例.jpg (u8g2 XBMP) */",
                '#include "duck_anim.h"',
                ""]
    for i, fr in enumerate(frames):
        anim_src.append("/* frame %d: breathing zoom */" % i)
        anim_src.append(carray("duck_frame_%d" % i, to_bytes(fr, 64, 64)))
        anim_src.append("")
    anim_src.append("const uint8_t *const duck_frames[DUCK_FRAME_COUNT] = {")
    anim_src.append("    duck_frame_0, duck_frame_1, duck_frame_2")
    anim_src.append("};")
    anim_src.append("")

    s9 = star(9)
    s7 = star(7)
    anim_src.append("/* twinkling star, 9x9 */")
    anim_src.append(carray("star_9x9", to_bytes(s9, 9, 9), static=False))
    anim_src.append("")
    anim_src.append("/* small star, 7x7 */")
    anim_src.append(carray("star_7x7", to_bytes(s7, 7, 7), static=False))
    with open(os.path.join(ROOT, "App", "Src", "duck_anim.c"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(anim_src) + "\n")

    duck_hdr = """/* duck_anim.h - animated 1-bit portrait data (converted from 图例.jpg) */
#ifndef DUCK_ANIM_H
#define DUCK_ANIM_H

#include <stdint.h>

#define DUCK_FRAME_W 64
#define DUCK_FRAME_H 64
#define DUCK_FRAME_COUNT 3

extern const uint8_t *const duck_frames[DUCK_FRAME_COUNT];
extern const unsigned char star_9x9[];
extern const unsigned char star_7x7[];

#endif
"""
    with open(os.path.join(ROOT, "App", "Inc", "duck_anim.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write(duck_hdr)

    preview(frames + [s9, s7], os.path.join(ROOT, "Tools", "preview_final_assets.png"), 4)
    print("assets written: %d glyphs, %d portrait frames" % (len(glyphs), len(frames)))


if __name__ == "__main__":
    main()

