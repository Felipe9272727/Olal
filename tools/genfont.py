#!/usr/bin/env python3
"""Gera kernel/font8x8.inc (fonte 8x8 ASCII 32..126) a partir de uma TTF.

Uso:  python3 tools/genfont.py
Requer: pillow  (pip install pillow)
"""
from PIL import Image, ImageFont, ImageDraw

W = H = 8
FONT_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"
SIZE = 9
OFFSET_Y = -1
THRESHOLD = 90

chars = [chr(c) for c in range(32, 127)]
font = ImageFont.truetype(FONT_PATH, SIZE)

glyphs = {}
for ch in chars:
    img = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(img)
    d.text((0, OFFSET_Y), ch, fill=255, font=font)
    if ch == "0":                      # zero cortado (distinguir de O/D)
        d.line((1, 6, 6, 1), fill=255)
    rows = []
    for y in range(H):
        b = 0
        for x in range(W):
            if img.getpixel((x, y)) > THRESHOLD:
                b |= (1 << x)          # LSB = pixel da esquerda
        rows.append(b)
    glyphs[ch] = rows

with open("kernel/font8x8.inc", "w") as f:
    f.write("; Fonte 8x8 (ASCII 32..126) - DejaVuSansMono-Bold. LSB = pixel esquerdo.\n")
    f.write("; Cada glifo = 8 bytes (linhas de cima p/ baixo). Gerado por tools/genfont.py\n")
    f.write("font8x8:\n")
    for ch in chars:
        rows = glyphs[ch]
        label = ch if ch not in "\\;'" else "?"
        f.write("    db " + ",".join("0x%02X" % b for b in rows))
        f.write("    ; '%s' (%d)\n" % (label, ord(ch)))

print("kernel/font8x8.inc gerado (%d glifos)." % len(chars))
