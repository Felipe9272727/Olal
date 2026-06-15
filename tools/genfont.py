#!/usr/bin/env python3
"""Gera kernel/font8x16.h a partir de uma TTF monoespacada."""
from PIL import Image, ImageFont, ImageDraw
CW, CH = 8, 16
font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf", 14)
chars = [chr(c) for c in range(32, 127)]
glyphs = {}
for ch in chars:
    img = Image.new("L", (CW, CH), 0)
    d = ImageDraw.Draw(img)
    d.text((0, 0), ch, fill=255, font=font)
    if ch == "0":
        d.line((1, 11, 6, 3), fill=255)          # zero cortado
    rows = []
    for y in range(CH):
        b = 0
        for x in range(CW):
            if img.getpixel((x, y)) > 100:
                b |= (1 << x)                      # LSB = pixel da esquerda
        rows.append(b)
    glyphs[ch] = rows
with open("kernel/font8x16.h", "w") as f:
    f.write("/* Fonte 8x16 (ASCII 32..126) - gerada por tools/genfont.py */\n")
    f.write("#ifndef FONT8X16_H\n#define FONT8X16_H\n#include <stdint.h>\n")
    f.write("static const uint8_t font8x16[95][16] = {\n")
    for ch in chars:
        f.write("  {" + ",".join("0x%02X" % b for b in glyphs[ch]) + "},\n")
    f.write("};\n#endif\n")
# preview
scale = 6
prev = Image.new("RGB", (16*CW*scale, 6*CH*scale), (20,20,30))
pd = ImageDraw.Draw(prev)
for i, ch in enumerate(chars):
    cx, cy = (i % 16)*CW*scale, (i//16)*CH*scale
    for y in range(CH):
        for x in range(CW):
            if glyphs[ch][y] & (1 << x):
                pd.rectangle([cx+x*scale, cy+y*scale, cx+x*scale+scale-1, cy+y*scale+scale-1], fill=(220,230,120))
prev.save("/tmp/font16.png")
print("font8x16.h gerado")
