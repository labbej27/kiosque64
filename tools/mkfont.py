#!/usr/bin/env python3
"""Police de Kiosque 64 : jeu G0 du Minitel en 8x8, à partir d'unscii-8
(domaine public, viznut.fi/unscii), et semi-graphiques G1 2x3.

mkfont.py unscii-8.hex sortie.s [apercu.txt]

Codes internes (128 caractères) :
  $20-$7F  ASCII, avec les particularités du Minitel ($5E flèche haut,
           $60 trait horizontal, $7F pavé plein) ;
  $00-$1F  caractères obtenus par SS2 (accents, £, °, flèches, fractions).
"""
import sys

glyphs = {}
for line in open(sys.argv[1]):
    code, data = line.strip().split(":")
    if len(data) == 16:
        glyphs[int(code, 16)] = bytes.fromhex(data)

# Accents redessinés comme dans Plume : unscii les trace à plat.
DRAWN = {
    "aigu": ["....##..", "...##..."],
    "grave": ["..##....", "...##..."],
    "circ": ["...##...", "..#..#.."],
}
REDRAW = {"é": ("e", "aigu"), "è": ("e", "grave"), "ê": ("e", "circ"), "à": ("a", "grave"),
          "â": ("a", "circ"), "ù": ("u", "grave"), "û": ("u", "circ"), "ô": ("o", "circ"),
          "î": ("i", "circ")}
for ch, (base, accent) in REDRAW.items():
    rows = bytearray(glyphs[ord(base)])
    if base == "i":
        rows[0] = 0
    for r, line in enumerate(DRAWN[accent]):
        rows[r] = int(line.replace("#", "1").replace(".", "0"), 2)
    glyphs[ord(ch)] = bytes(rows)


def bits(*lines):
    return bytes(int(l.replace("#", "1").replace(".", "0"), 2) for l in lines)


# Caractères propres au Minitel.
SPECIAL = {
    0x5E: bits("...#....", "..###...", ".#.#.#..", "...#....", "...#....", "...#....", "...#....", "........"),
    0x60: bits("........", "........", "........", "########", "........", "........", "........", "........"),
    0x7E: bits("########", "........", "........", "........", "........", "........", "........", "........"),
    0x7F: bytes([0xFF] * 8),
}
LOW = "\x20ÂÉ£âÇËÀùÈŒÊ←î→↓°±éëïçûà÷èœê¼½¾ô"   # codes $00 à $1F

out = []
for code in range(128):
    if code < 32:
        ch = LOW[code]
        g = glyphs.get(ord(ch))
    elif code in SPECIAL:
        ch, g = chr(code), SPECIAL[code]
    else:
        ch = chr(code)
        g = glyphs.get(code)
    if g is None:
        sys.exit(f"glyphe manquant : {ch!r} (U+{ord(ch):04X})")
    out.append((ch, g))


def mosaic(v, separated):
    """Semi-graphique : 6 pavés (bits 0-5), 2 colonnes x 3 rangées
    (3, 2 et 3 lignes). En mode disjoint, chaque pavé perd sa dernière
    colonne et sa dernière ligne."""
    rows = []
    bands = [(0, 3), (3, 5), (5, 8)]
    for y in range(8):
        band = next(i for i, (a, b) in enumerate(bands) if a <= y < b)
        a, b = bands[band]
        byte = 0
        for side in (0, 1):
            if v >> (band * 2 + side) & 1:
                if separated and y == b - 1:
                    continue
                half = 0xF0 if side == 0 else 0x0F
                if separated:
                    half &= 0xE0 if side == 0 else 0x0E
                byte |= half
        rows.append(byte)
    return bytes(rows)


with open(sys.argv[2], "w") as f:
    f.write("; Généré par tools/mkfont.py depuis unscii-8 (domaine public).\n")
    f.write("\t.export _font_g0, _font_g1, _font_g1s\n\t.rodata\n")
    f.write("_font_g0:\t\t\t; 128 caractères, 8 octets chacun\n")
    for code, (ch, g) in enumerate(out):
        label = ch if ch.isprintable() else ""
        f.write("\t.byte " + ",".join(f"${b:02X}" for b in g) + f"\t; ${code:02X} {label}\n")
    f.write("_font_g1:\t\t\t; semi-graphiques joints, 64 motifs\n")
    for v in range(64):
        f.write("\t.byte " + ",".join(f"${b:02X}" for b in mosaic(v, False)) + "\n")
    f.write("_font_g1s:\t\t\t; semi-graphiques disjoints\n")
    for v in range(64):
        f.write("\t.byte " + ",".join(f"${b:02X}" for b in mosaic(v, True)) + "\n")

if len(sys.argv) > 3:
    with open(sys.argv[3], "w", encoding="utf-8") as f:
        for base in range(0, 128, 16):
            for y in range(8):
                f.write(" ".join("".join("#" if out[base + i][1][y] >> (7 - x) & 1 else "."
                                         for x in range(8)) for i in range(16)) + "\n")
            f.write("\n")
