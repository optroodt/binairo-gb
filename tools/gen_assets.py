#!/usr/bin/env python3
"""Generate graphics assets for Binairo GB (src/assets.c / assets.h).
Colors: 0 = paper, 1 = light gray, 2 = dark gray, 3 = ink.
"""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from font import GLYPHS, draw_text, text_width

OUT_C = "src/assets.c"
OUT_H = "src/assets.h"
PREVIEW = "build/preview"


def new(w, h, c=0):
    return [[c] * w for _ in range(h)]


def blit(dst, src, x, y, transparent=None):
    for yy, row in enumerate(src):
        for xx, c in enumerate(row):
            if c == transparent:
                continue
            px, py = x + xx, y + yy
            if 0 <= py < len(dst) and 0 <= px < len(dst[0]):
                dst[py][px] = c


def from_ascii(rows, cmap=None):
    cmap = cmap or {"#": 3, ".": 0, "o": 1, "x": 2, " ": 0}
    w = max(len(r) for r in rows)
    return [[cmap.get(r[x], 0) if x < len(r) else 0 for x in range(w)] for r in rows]


def to_tiles(img):
    """Row-major tiles, each 16 bytes (GB 2bpp)."""
    h = len(img); w = len(img[0])
    assert h % 8 == 0 and w % 8 == 0, (w, h)
    out = []
    for ty in range(h // 8):
        for tx in range(w // 8):
            for y in range(8):
                lo = hi = 0
                for x in range(8):
                    c = img[ty * 8 + y][tx * 8 + x]
                    if c & 1: lo |= 0x80 >> x
                    if c & 2: hi |= 0x80 >> x
                out += [lo, hi]
    return out


def save_png(img, name, scale=4):
    try:
        from PIL import Image
    except ImportError:
        return
    pal = [(177, 175, 168), (140, 138, 131), (94, 92, 85), (49, 47, 40)]
    h = len(img); w = len(img[0])
    im = Image.new("RGB", (w, h))
    for y in range(h):
        for x in range(w):
            im.putpixel((x, y), pal[img[y][x]])
    im = im.resize((w * scale, h * scale), Image.NEAREST)
    os.makedirs(PREVIEW, exist_ok=True)
    im.save(os.path.join(PREVIEW, name + ".png"))


def dilate(mask, r=1):
    h = len(mask); w = len(mask[0])
    out = [[0] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            if mask[y][x]:
                for dy in range(-r, r + 1):
                    for dx in range(-r, r + 1):
                        if 0 <= y + dy < h and 0 <= x + dx < w:
                            out[y + dy][x + dx] = 1
    return out

# ---------------------------------------------------------------- cell glyphs
# 10x10-ish glyphs drawn in a 15x15 cell interior.
G_DIGIT0 = [
    "..####..",
    ".######.",
    "###..###",
    "##....##",
    "##....##",
    "##....##",
    "##....##",
    "###..###",
    ".######.",
    "..####..",
]
G_DIGIT1 = [
    "...##...",
    "..###...",
    ".####...",
    "##.##...",
    "...##...",
    "...##...",
    "...##...",
    "...##...",
    ".######.",
    ".######.",
]
G_QUESTION = [
    "..####..",
    ".######.",
    "##....##",
    ".....##.",
    "....##..",
    "...##...",
    "...##...",
    "........",
    "...##...",
    "...##...",
]
G_RING = [
    "...####...",
    ".########.",
    ".###..###.",
    "###....###",
    "##......##",
    "##......##",
    "###....###",
    ".###..###.",
    ".########.",
    "...####...",
]
G_DISC = [
    "...####...",
    ".########.",
    ".########.",
    "##########",
    "##########",
    "##########",
    "##########",
    ".########.",
    ".########.",
    "...####...",
]
G_BOX = [
    "##########",
    "##########",
    "##......##",
    "##......##",
    "##..##..##",
    "##..##..##",
    "##......##",
    "##......##",
    "##########",
    "##########",
]
G_BLOCK = [
    "##########",
    "##########",
    "##########",
    "##########",
    "####..####",
    "####..####",
    "##########",
    "##########",
    "##########",
    "##########",
]
G_MINUS = [
    "..........",
    "..........",
    "..........",
    "..........",
    "##########",
    "##########",
    "..........",
    "..........",
    "..........",
    "..........",
]
G_PLUS = [
    "....##....",
    "....##....",
    "....##....",
    "....##....",
    "##########",
    "##########",
    "....##....",
    "....##....",
    "....##....",
    "....##....",
]

THEMES = [
    ("0/1", G_DIGIT0, G_DIGIT1),
    ("circles", G_RING, G_DISC),
    ("squares", G_BOX, G_BLOCK),
    ("-/+", G_MINUS, G_PLUS),
]

# mini 7px icons used inside text for each theme's symbols (\x05, \x06)
MINI = [
    (["....", ".##.", "#..#", "#..#", "#..#", "#..#", "#..#", ".##."][1:],
     ["..#.", ".##.", "..#.", "..#.", "..#.", "..#.", ".###"]),
    ([".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
     [".###.", "#####", "#####", "#####", "#####", "#####", ".###."]),
    (["#####", "#...#", "#...#", "#...#", "#...#", "#...#", "#####"],
     ["#####", "#####", "#####", "#####", "#####", "#####", "#####"]),
    ([".....", ".....", ".....", "#####", ".....", ".....", "....."],
     ["..#..", "..#..", "..#..", "#####", "..#..", "..#..", "..#.."]),
]


def glyph_img(rows):
    return [[1 if c == "#" else 0 for c in r] for r in rows]


def make_cell(glyph, given, dim, line=True):
    """16x16 cell: interior 0..14, grid lines at x=15 / y=15."""
    img = new(16, 16, 0)
    g = glyph_img(glyph) if glyph else None
    mask = [[0] * 16 for _ in range(16)]
    if g:
        gh = len(g); gw = len(g[0])
        ox = (15 - gw) // 2 + ((15 - gw) % 2)
        oy = (15 - gh) // 2 + ((15 - gh) % 2)
        for y in range(gh):
            for x in range(gw):
                if g[y][x]:
                    mask[oy + y][ox + x] = 1
    halo = dilate(mask, 1)
    if given:
        for y in range(15):
            for x in range(15):
                if (x % 2 == 1) and (y % 2 == 1) and not halo[y][x]:
                    img[y][x] = 1 if dim else 2
    for y in range(16):
        for x in range(16):
            if mask[y][x]:
                img[y][x] = 2 if dim else 3
    lc = 1 if dim else 3
    for i in range(16):
        img[15][i] = lc
        img[i][15] = lc
    return img


def cell_tiles_for_theme(g0, g1):
    variants = [(None, 0), (g0, 0), (g1, 0), (g0, 1), (g1, 1), (G_QUESTION, 0)]
    data = []
    sheet = new(16 * 12, 16, 0)
    k = 0
    for glyph, given in variants:
        for dim in (0, 1):
            img = make_cell(glyph, given, dim)
            blit(sheet, img, k * 16, 0); k += 1
            for qy in range(2):
                for qx in range(2):
                    sub = [row[qx * 8:qx * 8 + 8] for row in img[qy * 8:qy * 8 + 8]]
                    data += to_tiles(sub)
    return data, sheet


# ------------------------------------------------------------------- ring
def in_rrect(x, y, x0, y0, x1, y1, r):
    if x < x0 or x > x1 or y < y0 or y > y1:
        return False
    cx = min(max(x, x0 + r), x1 - r)
    cy = min(max(y, y0 + r), y1 - r)
    return (x - cx) ** 2 + (y - cy) ** 2 <= (r + 0.4) ** 2


def ring_tiles():
    out = []
    sheets = []
    for dim in (0, 1):
        c = 1 if dim else 3
        img = new(32, 32, 0)
        for y in range(32):
            for x in range(32):
                if in_rrect(x, y, 5, 5, 25, 25, 4) and not (8 <= x <= 23 and 8 <= y <= 23):
                    img[y][x] = c
        sheets.append(img)
        order = [("TL", 0, 0), ("T", 1, 0), ("TR", 3, 0), ("L", 0, 1), ("R", 3, 1),
                 ("BL", 0, 3), ("B", 1, 3), ("BR", 3, 3)]
        for name, tx, ty in order:
            sub = [row[tx * 8:tx * 8 + 8] for row in img[ty * 8:ty * 8 + 8]]
            out += to_tiles(sub)
    return out, sheets


# ------------------------------------------------------------------- sprites
SPR_CURSOR = [
    "..############..",
    ".##############.",
    "###..........###",
    "##............##",
    "##............##",
    "##............##",
    "##............##",
    "###..........###",
    "###..........###",
    "##............##",
    "##............##",
    "##............##",
    "##............##",
    "###..........###",
    ".##############.",
    "..############..",
]
# cursor with little notches mid-edge, like the Playdate one
SPR_CURSOR = [
    "..#####..#####..",
    ".######oo######.",
    "###..........###",
    "##............##",
    "##............##",
    "##............##",
    "##............##",
    ".o............o.",
    ".o............o.",
    "##............##",
    "##............##",
    "##............##",
    "##............##",
    "###..........###",
    ".######oo######.",
    "..#####..#####..",
]
SPR_CHECK = [
    "......oo",
    ".....o##",
    "oo..o##o",
    "##oo##o.",
    "o####o..",
    ".o##o...",
    "..oo....",
    "........",
]
SPR_TAB = [
    "....oooo",
    "..o#####",
    ".o######",
    "o#######",
    "o#######",
    "o##ooo##",
    "o#o###o#",
    "o#####o#",
    "o####o##",
    "o###o###",
    "o###o###",
    "o#######",
    "o###o###",
    "o#######",
    "o#######",
    ".o######",
    "..o#####",
    "....oooo",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
]
SPR_ARROW = [  # small down arrow for hints
    "oooooooo",
    "o######o",
    ".o####o.",
    "..o##o..",
    "...oo...",
    "........",
    "........",
    "........",
]


def sprite_tiles():
    cm = {"#": 3, "o": 1, ".": 0}
    data = []
    cur = from_ascii(SPR_CURSOR, cm)
    data += to_tiles([r[0:8] for r in cur[0:8]])
    data += to_tiles([r[8:16] for r in cur[0:8]])
    data += to_tiles([r[0:8] for r in cur[8:16]])
    data += to_tiles([r[8:16] for r in cur[8:16]])
    data += to_tiles(from_ascii(SPR_CHECK, cm))
    tab = from_ascii(SPR_TAB, cm)
    data += to_tiles(tab)  # 3 tiles
    data += to_tiles(from_ascii(SPR_ARROW, cm))
    return data


# --------------------------------------------------------------------- logo
LOGO_LETTERS = {
    "B": ["######..", "########", "##....##", "##....##", "#######.",
          "########", "##....##", "##....##", "########", "#######."],
    "I": ["##"] * 10,
    "N": ["##....##", "###...##", "####..##", "##.##.##", "##.##.##",
          "##..####", "##...###", "##....##", "##....##", "##....##"],
    "A": ["..####..", ".######.", "###..###", "##....##", "##....##",
          "########", "########", "##....##", "##....##", "##....##"],
    "R": ["######..", "########", "##....##", "##....##", "########",
          "#######.", "##..###.", "##...###", "##....##", "##....##"],
    "O": ["..####..", ".######.", "###..###", "##....##", "##....##",
          "##....##", "##....##", "###..###", ".######.", "..####.."],
}


def make_logo(word="BINAIRO", bs=2, gap=4):
    # build mask
    widths = [len(LOGO_LETTERS[ch][0]) * bs for ch in word]
    W = sum(widths) + gap * (len(word) - 1) + 4 * 2 + 3
    H = 10 * bs + 4 * 2 + 3
    mask = [[0] * W for _ in range(H)]
    x = 4
    for ch, w in zip(word, widths):
        L = LOGO_LETTERS[ch]
        for by, row in enumerate(L):
            for bx, c in enumerate(row):
                if c == "#":
                    for yy in range(bs):
                        for xx in range(bs):
                            mask[4 + by * bs + yy][x + bx * bs + xx] = 1
        x += w + gap
    # exterior region (flood fill from the border)
    ext = [[0] * W for _ in range(H)]
    stack = [(0, 0)]
    while stack:
        x, y = stack.pop()
        if x < 0 or y < 0 or x >= W or y >= H or ext[y][x] or mask[y][x]:
            continue
        ext[y][x] = 1
        stack += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    d1 = dilate(mask, 1)
    d2 = dilate(mask, 2)
    img = new(W, H, 0)
    for y in range(H):
        for x in range(W):
            if y >= 2 and x >= 2 and d2[y - 2][x - 2] and ext[y][x] and (x + y) % 2 == 0:
                img[y][x] = 2
    for y in range(H):
        for x in range(W):
            if d2[y][x] and ext[y][x] and not d1[y][x]:
                img[y][x] = 3
            if mask[y][x]:
                img[y][x] = 3
    # lower half of letters dithered
    top = 4 + (10 * bs) * 3 // 5
    for y in range(top, 4 + 10 * bs):
        for x in range(W):
            if mask[y][x] and (x + y) % 2 == 0:
                img[y][x] = 2
    return img


def pad_to_tiles(img, W=None, H=None, ox=0, oy=0):
    h = len(img); w = len(img[0])
    W = W or ((w + 7) // 8) * 8
    H = H or ((h + 7) // 8) * 8
    out = new(W, H, 0)
    blit(out, img, ox, oy)
    return out


# --------------------------------------------------------------- stat icons
ICON_GRID = [
    "###########",
    "#..#...#..#",
    "#..#...#..#",
    "###########",
    "#..#...#..#",
    "#..#...#..#",
    "###########",
    "#..#...#..#",
    "#..#...#..#",
    "###########",
]
ICON_GRID = [
    "#########",
    "#.#.#.#.#",
    "#########",
    "#.#.#.#.#",
    "#########",
    "#.#.#.#.#",
    "#########",
    "#.#.#.#.#",
    "#########",
]
ICON_CROWN = [
    "#...#...#",
    "##.###.##",
    "#.##.##.#",
    "#.......#",
    "#.......#",
    ".#.....#.",
    ".#######.",
    ".........",
    ".#######.",
]
ICON_FLAG = [
    "#######..",
    "#.....#..",
    "#....#...",
    "#.....#..",
    "#######..",
    "#........",
    "#........",
    "#........",
    "#........",
]


def icon_tiles():
    data = []
    for ic in (ICON_GRID, ICON_CROWN, ICON_FLAG):
        img = pad_to_tiles(from_ascii(ic), 16, 16, 3, 3)
        data += to_tiles(img)
    return data

# --------------------------------------------------------------- rules card
TINY = {
    "0": ["###", "#.#", "#.#", "#.#", "###"],
    "1": [".#.", "##.", ".#.", ".#.", "###"],
    "3": ["###", "..#", ".##", "..#", "###"],
    "x": ["...", "#.#", ".#.", "#.#", "..."],
}


def tiny_cell(img, x, y, ch, bold=False, empty=False):
    S = 9
    for i in range(S):
        img[y][x + i] = 3; img[y + S - 1][x + i] = 3
        img[y + i][x] = 3; img[y + i][x + S - 1] = 3
    if bold:
        for i in range(-1, S + 1):
            img[y - 1][x + i] = 3; img[y + S][x + i] = 3
            img[y + i][x - 1] = 3; img[y + i][x + S] = 3
    if ch and not empty:
        t = TINY[ch]
        for yy in range(5):
            for xx in range(3):
                if t[yy][xx] == "#":
                    img[y + 2 + yy][x + 3 + xx] = 3


def tiny_row(img, x, y, chars, bolds=()):
    for i, ch in enumerate(chars):
        if i not in bolds:
            tiny_cell(img, x + i * 8, y, None if ch == "_" else ch, empty=ch == "_")
    for i in bolds:
        ch = chars[i]
        tiny_cell(img, x + i * 8, y, None if ch == "_" else ch, bold=True, empty=ch == "_")
    return x + len(chars) * 8 + 1


def down_tri(img, x, y):
    for i, w in enumerate((5, 3, 1)):
        for xx in range(w):
            img[y + i][x + i + xx] = 3


def make_rules_card():
    W, H = 80, 144
    img = new(W, H, 0)
    for y in range(H):
        img[y][W - 1] = 3
        img[y][W - 2] = 3
    def title(y, s):
        w = text_width(s)
        draw_text(img, (W - 2 - w) // 2, y, s)
    def hline(y):
        for x in range(4, W - 6):
            img[y][x] = 3
    title(4, "Avoid Trios")
    tiny_row(img, 5, 16, "00_", bolds=(2,))
    down_tri(img, 20, 27)
    tiny_row(img, 5, 32, "001", bolds=(2,))
    for yy in range(17, 40):
        img[yy][38] = 3
    tiny_row(img, 45, 16, "0_0", bolds=(1,))
    down_tri(img, 52, 27)
    tiny_row(img, 45, 32, "010", bolds=(1,))
    hline(46)
    title(51, "Same number")
    title(61, "of 0's and 1's")
    draw_text(img, 2, 76, "3x")
    tiny_row(img, 14, 75, "100110")
    draw_text(img, 66, 76, "3x")
    hline(90)
    title(95, "Unique rows")
    title(105, "and columns")
    tiny_row(img, 15, 118, "010011")
    down_tri(img, 40, 129)
    down_tri(img, 56, 129)
    tiny_row(img, 15, 134, "010110", bolds=(3, 5))
    return img


# ------------------------------------------------------------------ output
def c_array(name, data, per=16):
    s = "const uint8_t %s[%d] = {\n" % (name, len(data))
    for i in range(0, len(data), per):
        s += "  " + ",".join("0x%02X" % b for b in data[i:i + per]) + ",\n"
    return s + "};\n"


def main():
    c = ["// Generated by tools/gen_assets.py - do not edit\n#pragma bank 2\n#include <stdint.h>\n#include \"assets.h\"\n"]
    c0 = ["// Generated by tools/gen_assets.py - do not edit\n#include <stdint.h>\n#include \"assets.h\"\n"]
    h = ["#ifndef ASSETS_H\n#define ASSETS_H\n#include <stdint.h>\n#define ASSET_BANK 2\n#define PUZZLE_BANK 3\n"]

    # font
    widths = [0] * 128
    bits = [0] * (128 * 9)
    for ch, (w, b) in GLYPHS.items():
        o = ord(ch)
        if o >= 128:
            continue
        widths[o] = w
        bits[o * 9:(o + 1) * 9] = b
    # default for missing printable: '?'
    for o in range(32, 127):
        if widths[o] == 0 and chr(o) != " ":
            widths[o] = widths[ord("?")]
            bits[o * 9:(o + 1) * 9] = bits[ord("?") * 9:(ord("?") + 1) * 9]
    widths[ord(" ")] = 2
    c0.append(c_array("font_widths", widths))
    c0.append(c_array("font_bits", bits, 9))
    h.append("extern const uint8_t font_widths[128];\nextern const uint8_t font_bits[128*9];\n")

    # theme mini icons (7 rows, rows start at row 0)
    mini = []
    miniw = []
    for a, b in MINI:
        for g in (a, b):
            rows = [0] * 9
            for y, r in enumerate(g):
                v = 0
                for x, ch in enumerate(r):
                    if ch == "#":
                        v |= 0x80 >> x
                rows[y] = v
            mini += rows
            miniw.append(max(len(r) for r in g))
    c0.append(c_array("theme_mini_bits", mini, 9))
    c0.append(c_array("theme_mini_widths", miniw))
    h.append("extern const uint8_t theme_mini_bits[8*9];\nextern const uint8_t theme_mini_widths[8];\n")

    # cell tiles
    all_cells = []
    for i, (name, g0, g1) in enumerate(THEMES):
        d, sheet = cell_tiles_for_theme(g0, g1)
        save_png(sheet, "cells_%d" % i)
        all_cells += d
    c.append(c_array("cell_tiles", all_cells))
    h.append("#define CELL_TILE_COUNT 48\nextern const uint8_t cell_tiles[4*48*16];\n")
    h.append("#define THEME_COUNT 4\n")

    rt, sheets = ring_tiles()
    for i, s in enumerate(sheets):
        save_png(s, "ring_%d" % i)
    c.append(c_array("ring_tiles", rt))
    h.append("extern const uint8_t ring_tiles[16*16];\n")

    st = sprite_tiles()
    c.append(c_array("sprite_tiles", st))
    h.append("#define SPR_T_CURSOR 0\n#define SPR_T_CHECK 4\n#define SPR_T_TAB 5\n#define SPR_T_ARROW 8\n#define SPRITE_TILE_COUNT %d\n" % (len(st) // 16))
    h.append("extern const uint8_t sprite_tiles[%d];\n" % len(st))

    logo = make_logo()
    lw = ((len(logo[0]) + 7) // 8) * 8
    logo_p = pad_to_tiles(logo, lw, 32, (lw - len(logo[0])) // 2, (32 - len(logo)) // 2)
    save_png(logo_p, "logo")
    c.append(c_array("logo_tiles", to_tiles(logo_p)))
    h.append("#define LOGO_W %d\n#define LOGO_H %d\n" % (lw // 8, 4))
    h.append("extern const uint8_t logo_tiles[%d];\n" % (lw // 8 * 4 * 16))

    it = icon_tiles()
    c.append(c_array("icon_tiles", it))
    h.append("extern const uint8_t icon_tiles[%d];\n" % len(it))

    card = make_rules_card()
    save_png(card, "rules_card")
    c.append(c_array("rules_card_tiles", to_tiles(card)))
    h.append("#define CARD_W 10\n#define CARD_H 18\nextern const uint8_t rules_card_tiles[%d];\n" % (10 * 18 * 16))

    h.append("#endif\n")
    open(OUT_C, "w").write("\n".join(c))
    open("src/font.c", "w").write("\n".join(c0))
    open(OUT_H, "w").write("".join(h))
    print("assets ok")


if __name__ == "__main__":
    main()
