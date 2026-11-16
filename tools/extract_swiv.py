#!/usr/bin/env python3
"""The SWIV jets for FIREPOWER (CLASSIC off): read from your own SWIV files, never kept in git.

    python3 firepower/tools/extract_swiv.py /path/to/swiv/files

/path/to/swiv/files is the folder of SWIV's game files that swiv_extract.py makes (out/files):
it needs JETS.LIN and DESERT.PAM. Writes assets/swiv/jet0.png, jet1.png, jet2.png - SWIV's three
aircraft, each put back together from its 32-pixel strips (around its own middle) in the desert
level's colours. Without them the game simply has no jets. Python 3 only, nothing to install.

  jet0: the camouflaged jet drawn on the diagonal (flying down and to the left)
  jet1: the grey bomber, also on the diagonal (the same way)
  jet2: the camouflaged jet facing straight up (the game turns it to any heading)
"""
import os, struct, sys, zlib

def pam_palette(d):
    """the level's colour commands at its start: $8xxx, $RGBi (colour i = $RGB)"""
    w = struct.unpack('>%dH' % (len(d) // 2), d[:len(d) // 2 * 2])
    pal = {}
    for k in range(0, min(200, len(w) - 1), 2):
        if (w[k] & 0xff00) in (0x8000, 0x8060, 0x8008):
            pal.setdefault(w[k + 1] & 15, w[k + 1] >> 4)
    return [(((c >> 8) & 15) * 17, ((c >> 4) & 15) * 17, (c & 15) * 17) for c in (pal.get(i, 0) for i in range(16))]

def lin_frames(d):
    """word count, then frames: word size, w, h, hotx, hoty, see-through colour, flags, hotx2, hoty2,
    then 4 planes interleaved by row (each row of a plane: whole 16-pixel words)"""
    p, out = 2, []
    while p + 10 <= len(d):
        size = struct.unpack('>H', d[p:p + 2])[0]
        w, h, hx, hy, tc, fl = d[p + 2:p + 8]
        bpr = ((w + 15) // 16) * 2
        if w == 0 or h == 0 or bpr * 4 * h != size:
            break
        px = d[p + 10:p + 10 + size]
        idx = [[0] * w for _ in range(h)]
        for y in range(h):
            for pl in range(4):
                row = px[(y * 4 + pl) * bpr:(y * 4 + pl + 1) * bpr]
                for x in range(w):
                    if row[x >> 3] >> (7 - (x & 7)) & 1:
                        idx[y][x] |= 1 << pl
        sx = hx - 256 if hx > 127 else hx
        sy = hy - 256 if hy > 127 else hy
        out.append(dict(w=w, h=h, hot=(sx, sy), tc=tc & 15, new=fl & 1, idx=idx))
        p += 10 + size
    return out

def write_png(path, w, h, rgba):
    raw = b''.join(b'\0' + bytes(rgba[y * w * 4:(y + 1) * w * 4]) for y in range(h))
    def chunk(t, data):
        c = struct.pack('>I', len(data)) + t + data
        return c + struct.pack('>I', zlib.crc32(t + data) & 0xffffffff)
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) +
                chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))

def main():
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(1)
    src = sys.argv[1]
    try:
        jets = open(os.path.join(src, 'JETS.LIN'), 'rb').read()
        pam = open(os.path.join(src, 'DESERT.PAM'), 'rb').read()
    except OSError as e:
        sys.exit('need JETS.LIN and DESERT.PAM there (%s)' % e)
    pal = pam_palette(pam)
    frames = lin_frames(jets)
    groups = []
    for f in frames:   # each aircraft: its first strip, then the strips to its right (their hotspots negative)
        if f['hot'][0] >= 0 or not groups: groups.append([])
        groups[-1].append(f)
    groups = [g for g in groups if sum(f['w'] * f['h'] for f in g) > 200]
    if len(groups) != 3:
        sys.exit('JETS.LIN: expected 3 aircraft, found %d - is it SWIV\'s?' % len(groups))
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'assets', 'swiv')
    os.makedirs(out, exist_ok=True)
    for k, g in enumerate(groups):
        # each strip's top left is minus its hotspot: the aircraft's own middle is (0, 0)
        x0 = min(-f['hot'][0] for f in g); y0 = min(-f['hot'][1] for f in g)
        x1 = max(-f['hot'][0] + f['w'] for f in g); y1 = max(-f['hot'][1] + f['h'] for f in g)
        r = max(-x0, x1, -y0, y1)   # (square, round its middle: the game turns it about there)
        W = H = 2 * r
        img = bytearray(W * H * 4)
        for f in g:
            ox, oy = r - f['hot'][0], r - f['hot'][1]
            for y in range(f['h']):
                for x in range(f['w']):
                    c = f['idx'][y][x]
                    if c == f['tc']: continue
                    o = ((oy + y) * W + ox + x) * 4
                    img[o:o + 4] = bytes(pal[c]) + b'\xff'
        write_png(os.path.join(out, 'jet%d.png' % k), W, H, img)
        print('jet%d: %d x %d' % (k, W, H))
    print('done: %s' % os.path.normpath(out))

if __name__ == '__main__':
    main()
