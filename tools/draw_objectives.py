#!/usr/bin/env python3
"""The bonus objectives' pictures (CLASSIC off), drawn in the original's own colours and style:
64 x 64 Amiga pixels each, see-through where the sand shows (the game puts its sand under them).

  radar.png        the radar installation (its dish, radar_dish.png, turns on top at run time)
  ammo.png         the ammunition depot
  launcher.png     the missile launcher
  prison.png       the prison camp (your captured soldiers inside)
  silo_pad.png     the missile silo's pad: its two domes (SWIV's, from your disk) go on top at run time

  python3 tools/draw_objectives.py        (writes them into assets/objectives/, and a preview)
"""
import os, sys
from PIL import Image

# the battle's palette (gr/oneplayer's)
P = {
    'blk': (0, 0, 0), 'sand': (255, 204, 136), 'org': (255, 119, 0), 'sandd': (187, 153, 102),
    'dred': (187, 0, 0), 'yel': (255, 255, 0), 'gry': (136, 136, 136), 'dgry': (85, 85, 85),
    'red': (255, 0, 0), 'lgrn': (136, 238, 0), 'mgrn': (51, 170, 0), 'dgrn': (0, 85, 0),
    'g0': (68, 170, 119), 'g1': (34, 136, 85), 'g2': (17, 85, 68), 'g3': (0, 51, 34),
    'b0': (221, 136, 51), 'b1': (170, 119, 51), 'b2': (136, 85, 34), 'b3': (85, 51, 17),
}

class Pic:
    def __init__(s, w=64, h=64):
        s.w, s.h = w, h
        s.px = [[None] * w for _ in range(h)]
    def put(s, x, y, c):
        if 0 <= x < s.w and 0 <= y < s.h: s.px[y][x] = P[c] if isinstance(c, str) else c
    def get(s, x, y):
        return s.px[y][x] if 0 <= x < s.w and 0 <= y < s.h else None
    def rect(s, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1): s.put(x, y, c)
    def frame(s, x0, y0, x1, y1, c):
        for x in range(x0, x1 + 1): s.put(x, y0, c); s.put(x, y1, c)
        for y in range(y0, y1 + 1): s.put(x0, y, c); s.put(x1, y, c)
    def disc(s, cx, cy, r, c, ry=None):
        ry = ry or r
        for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
            for x in range(int(cx - r) - 1, int(cx + r) + 2):
                if ((x + .5 - cx) / r) ** 2 + ((y + .5 - cy) / ry) ** 2 <= 1: s.put(x, y, c)
    def line(s, x0, y0, x1, y1, c):
        n = max(abs(x1 - x0), abs(y1 - y0), 1)
        for i in range(n + 1):
            s.put(round(x0 + (x1 - x0) * i / n), round(y0 + (y1 - y0) * i / n), c)
    def bevel(s, x0, y0, x1, y1, face, light, dark, edge=2):
        """the original's raised box: black outline, light top and left, dark bottom and right"""
        s.rect(x0, y0, x1, y1, face)
        for k in range(1, edge + 1):
            for x in range(x0 + k, x1 - k + 1): s.put(x, y0 + k - 1 + 1 - 1, light) if False else None
        for k in range(edge):
            for x in range(x0 + k, x1 - k + 1): s.put(x, y0 + k, light); s.put(x, y1 - k, dark)
            for y in range(y0 + k, y1 - k + 1): s.put(x0 + k, y, light); s.put(x1 - k, y, dark)
        s.frame(x0, y0, x1, y1, 'blk')
    def shadow(s, dx=3, dy=3):
        """the dark sand dither down and to the right of everything drawn so far"""
        solid = [[s.px[y][x] is not None for x in range(s.w)] for y in range(s.h)]
        for y in range(s.h):
            for x in range(s.w):
                if solid[y][x]: continue
                sx, sy = x - dx, y - dy
                near = any(0 <= sx - k < s.w and 0 <= sy - k < s.h and solid[sy - k][sx - k] for k in range(0, 1))
                if near and (x + y) % 2 == 0: s.put(x, y, 'sandd')
    def image(s, scale=1):
        im = Image.new('RGBA', (s.w, s.h), (0, 0, 0, 0))
        for y in range(s.h):
            for x in range(s.w):
                c = s.px[y][x]
                if c: im.putpixel((x, y), c + (255,))
        return im.resize((s.w * scale, s.h * scale), Image.NEAREST) if scale > 1 else im

def radar():
    p = Pic()
    # the concrete pad, the original's grey road colours, its edge bolted
    p.bevel(8, 8, 55, 55, 'dgry', 'gry', 'blk', 1)
    for k in range(12, 52, 6): p.put(k, 10, 'gry'); p.put(k, 53, 'blk'); p.put(10, k, 'gry'); p.put(53, k, 'blk')
    # the mast's turntable in the middle (the dish turns on it)
    p.disc(31.5, 31.5, 9, 'blk')
    p.disc(31.5, 31.5, 8, 'g2')
    p.disc(30.5, 30.5, 6, 'g1')
    p.disc(31.5, 31.5, 2.2, 'blk')
    # the control hut, bottom left: green, bevelled, a window
    p.bevel(11, 41, 24, 52, 'g1', 'g0', 'g3', 2)
    p.rect(15, 45, 19, 47, 'blk'); p.rect(16, 45, 18, 46, 'g3')
    # the generator, top right, and cables to the mast
    p.bevel(42, 11, 52, 20, 'g1', 'g0', 'g3', 1)
    p.rect(45, 14, 49, 17, 'dgry'); p.line(45, 15, 49, 15, 'blk'); p.line(45, 17, 49, 17, 'blk')
    p.line(41, 18, 37, 25, 'blk'); p.line(25, 41, 27, 37, 'blk')
    # aerial masts at two corners
    for (x, y) in ((12, 13), (51, 50)):
        p.put(x, y, 'blk'); p.put(x, y - 1, 'gry'); p.put(x + 1, y, 'gry')
    p.shadow()
    return p

def radar_dish():
    """the dish from above, pointing up (the game turns it): 28 x 28"""
    p = Pic(28, 28)
    # the arm from the turntable
    p.rect(13, 13, 14, 18, 'blk')
    # the dish: a wide shallow bowl (light grey, its ribs dark), the feed horn in front
    p.disc(13.5, 9, 12, 'blk', 6)
    p.disc(13.5, 9, 11, 'gry', 5)
    p.disc(13.5, 10, 9, (190, 190, 190), 3.4)
    for x in (6, 10, 17, 21): p.line(x, 6, 13 if x < 13 else 14, 11, 'dgry')
    p.line(13, 9, 13, 2, 'blk'); p.line(14, 9, 14, 2, 'blk')
    p.rect(12, 0, 15, 2, 'blk'); p.rect(13, 1, 14, 1, 'red')
    return p

def ammo():
    p = Pic()
    # sandbags round it (the brown of the original's posts), open at the bottom
    def bag(x, y, horiz=True):
        if horiz:
            p.rect(x, y, x + 4, y + 2, 'b1'); p.line(x, y, x + 4, y, 'b0'); p.frame(x, y, x + 4, y + 2, 'b3')
        else:
            p.rect(x, y, x + 2, y + 4, 'b1'); p.line(x, y, x, y + 4, 'b0'); p.frame(x, y, x + 2, y + 4, 'b3')
    for x in range(6, 56, 5): bag(x, 6); bag(x + 2, 9)
    for y in range(12, 50, 5): bag(6, y, False); bag(55, y, False)
    for x in list(range(6, 24, 5)) + list(range(41, 56, 5)): bag(x, 53)
    # the bunker: a low green building with the yellow warning band
    p.bevel(12, 14, 40, 34, 'g1', 'g0', 'g3', 2)
    for x in range(14, 39):
        p.put(x, 27, 'yel' if (x // 2) % 2 == 0 else 'blk'); p.put(x, 28, 'blk' if (x // 2) % 2 == 0 else 'yel')
    # a warning sign on the roof: a yellow triangle, a black mark in it
    for k in range(7):
        p.line(26 - k, 16 + k + 1, 26 + k, 16 + k + 1, 'yel')
    p.line(19, 24, 33, 24, 'blk'); p.line(19, 24, 26, 16, 'blk'); p.line(33, 24, 26, 16, 'blk')
    p.line(26, 19, 26, 21, 'blk'); p.put(26, 23, 'blk')
    # crates, stacked: green with their lids
    for (x, y) in ((43, 15), (43, 25), (16, 38), (25, 38), (43, 36)):
        p.bevel(x, y, x + 8, y + 8, 'g2', 'g1', 'g3', 1)
        p.line(x + 2, y + 4, x + 6, y + 4, 'yel'); p.line(x + 1, y + 1, x + 7, y + 7, 'g3')
    # shells in a row on a pallet: brass with grey noses
    p.rect(30, 44, 51, 50, 'b2'); p.frame(30, 44, 51, 50, 'blk')
    for x in range(31, 51, 3):
        p.rect(x, 45, x + 1, 48, 'b0'); p.put(x, 45, 'gry'); p.put(x + 1, 45, 'gry'); p.put(x, 49, 'b3')
    p.shadow()
    return p

def launcher():
    p = Pic()
    # the blast pad: concrete, scorched black in front of the rails
    p.bevel(6, 6, 57, 57, 'dgry', 'gry', 'blk', 1)
    for (x, y, r) in ((31, 12, 6), (28, 10, 3), (36, 11, 3)):
        p.disc(x, y, r, 'blk', r * 0.6)
    for k in range(9, 55, 7): p.put(k, 55, 'gry')
    # the launcher vehicle: a long green body, cab at the bottom, wheels down the sides
    for y in range(20, 54, 6):
        p.rect(15, y, 17, y + 3, 'blk'); p.rect(46, y, 48, y + 3, 'blk')
    p.bevel(17, 18, 46, 55, 'g1', 'g0', 'g3', 2)
    p.bevel(20, 45, 43, 54, 'g2', 'g1', 'g3', 1)               # the cab
    p.rect(23, 47, 40, 49, 'blk'); p.rect(24, 47, 39, 48, 'g3')  # its windscreen
    # the erector rails and two missiles on them, noses forward (up): white bodies, red noses, fins
    for mx in (24, 35):
        p.rect(mx - 1, 14, mx + 4, 44, 'blk')
        p.rect(mx, 17, mx + 3, 43, (225, 225, 225))
        p.line(mx + 3, 17, mx + 3, 43, 'gry')
        p.rect(mx, 13, mx + 3, 16, 'red'); p.rect(mx + 1, 11, mx + 2, 12, 'red'); p.put(mx + 1, 11, 'dred')
        p.line(mx, 25, mx + 3, 25, 'dgry')
        p.put(mx - 2, 41, 'blk'); p.put(mx - 2, 42, 'gry'); p.put(mx + 5, 41, 'blk'); p.put(mx + 5, 42, 'gry')
    p.shadow()
    return p

def prison():
    p = Pic()
    # the yard (sand, see-through) inside a wire fence: grey posts, the wire between them
    x0, y0, x1, y1 = 5, 5, 58, 58
    for x in range(x0, x1 + 1):
        if (x - x0) % 2 == 0: p.put(x, y0, 'gry'); p.put(x, y1, 'gry')
        else: p.put(x, y0 + 1, 'dgry'); p.put(x, y1 - 1, 'dgry')
    for y in range(y0, y1 + 1):
        if (y - y0) % 2 == 0: p.put(x0, y, 'gry'); p.put(x1, y, 'gry')
        else: p.put(x0 + 1, y, 'dgry'); p.put(x1 - 1, y, 'dgry')
    for x in range(x0, x1 + 1, 7):
        p.rect(x, y0 - 1, x + 1, y0 + 1, 'blk'); p.rect(x, y1 - 1, x + 1, y1 + 1, 'blk')
    for y in range(y0, y1 + 1, 7):
        p.rect(x0 - 1, y, x0 + 1, y + 1, 'blk'); p.rect(x1 - 1, y, x1 + 1, y + 1, 'blk')
    # the gate at the bottom: a gap with a bar
    for x in range(26, 38): p.put(x, y1, None); p.put(x, y1 - 1, None)
    p.line(26, y1, 37, y1, 'red')
    # two huts (green roofs, a ridge down the middle)
    for (hx, hy) in ((11, 11), (11, 34)):
        p.bevel(hx, hy, hx + 22, hy + 14, 'g1', 'g0', 'g3', 2)
        p.line(hx + 2, hy + 7, hx + 20, hy + 7, 'g3'); p.line(hx + 2, hy + 6, hx + 20, hy + 6, 'g0')
    # the watchtower, top right: brown legs, its platform, the guard's lamp
    p.bevel(41, 10, 53, 22, 'b1', 'b0', 'b3', 1)
    p.rect(44, 13, 50, 19, 'b2'); p.frame(44, 13, 50, 19, 'b3')
    p.put(47, 16, 'yel'); p.put(46, 16, 'org')
    # men in the yard (yours: the brown of your men)
    for (mx, my) in ((40, 32), (47, 37), (41, 44), (50, 46)):
        p.rect(mx, my + 2, mx + 2, my + 5, 'b2'); p.line(mx, my + 2, mx + 2, my + 2, 'b0')
        p.rect(mx, my, mx + 2, my + 1, (255, 204, 136)); p.frame(mx, my, mx + 2, my + 1, 'b3')
        p.put(mx, my + 6, 'b3'); p.put(mx + 2, my + 6, 'b3')
    p.shadow()
    return p

def silo_pad():
    """the concrete the two silo domes stand on: a hazard-striped edge, cables between them"""
    p = Pic()
    p.bevel(3, 14, 60, 49, 'dgry', 'gry', 'blk', 1)
    for x in range(5, 59):   # yellow and black edges, top and bottom
        c = 'yel' if (x // 3) % 2 == 0 else 'blk'
        p.put(x, 16, c); p.put(x, 47, c)
    for y in range(17, 47):
        c = 'yel' if (y // 3) % 2 == 0 else 'blk'
        p.put(5, y, c); p.put(58, y, c)
    for cx in (16, 48):      # where each dome sits: a dark ring
        p.disc(cx - 0.5, 31.5, 15, 'blk'); p.disc(cx - 0.5, 31.5, 13, 'dgry')
    p.line(30, 30, 34, 30, 'blk'); p.line(30, 33, 34, 33, 'blk')
    p.shadow()
    return p

if __name__ == '__main__':
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'assets', 'objectives')
    os.makedirs(out, exist_ok=True)
    pics = {'radar': radar(), 'radar_dish': radar_dish(), 'ammo': ammo(), 'launcher': launcher(), 'prison': prison(),
            'silo_pad': silo_pad()}
    for n, p in pics.items(): p.image().save(os.path.join(out, n + '.png'))
    if len(sys.argv) > 1:   # a preview: each on the sand, at 4x
        sand = Image.open(sys.argv[2]).convert('RGBA') if len(sys.argv) > 2 else Image.new('RGBA', (64, 64), P['sand'] + (255,))
        prev = Image.new('RGB', (4 * 64 * 4 + 5 * 16, 64 * 4 + 32), (30, 28, 26))
        for i, n in enumerate(['radar', 'ammo', 'launcher', 'prison']):
            t = sand.copy(); t.alpha_composite(pics[n].image())
            if n == 'radar':
                d = pics['radar_dish'].image().rotate(-30, resample=Image.NEAREST, expand=False)
                t.alpha_composite(d, (32 - 14, 32 - 14 - 4))
            prev.paste(t.resize((256, 256), Image.NEAREST), (16 + i * (256 + 16), 16))
        prev.save(sys.argv[1])
