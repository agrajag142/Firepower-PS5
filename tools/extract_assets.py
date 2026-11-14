#!/usr/bin/env python3
"""Pulls Firepower's graphics, maps, sounds and tables off YOUR OWN Firepower.adf
into assets/fp/, where the game (and build.sh) look for them.

    python3 tools/extract_assets.py /path/to/Firepower.adf

Nothing of the original game is kept in the git repository (assets/fp is in
.gitignore): like the Amiga app's Kickstart, you bring the disk.

What it writes (all names lower case):
    gr/*            the picture and sprite files, as they are on the disk
    mapp1/t1..t5    the battlefields (64 x 64 bytes each, column by column)
    mapp2/t1..t5
    mapp3/t1
    sounds/*        raw signed 8-bit samples
    ins/i1..i3      the instruction text
    tiletypes.bin   the 93 tile types (16 bytes each) from the game program
                    (bytes 0-3: the tile's picture number, byte 2 the enemy's
                    colour change; the rest as the original)
    sounds.bin      each sound's Paula period (big-endian words), in the order
                    tankblast boom2 heli splat drip
"""
import os
import struct
import sys

BLOCK = 512


def blocks(adf, n):
    return adf[n * BLOCK:(n + 1) * BLOCK]


def longs(b):
    return struct.unpack('>128I', b)


def name_of(b):
    n = b[432]
    return b[433:433 + n].decode('latin1')


def read_file(adf, hdr):
    """An OFS file: data blocks chained from the header, 488 bytes each."""
    h = longs(blocks(adf, hdr))
    size = h[81]
    out = bytearray()
    nxt = h[4]                      # first data block
    while nxt and len(out) < size:
        d = blocks(adf, nxt)
        dl = longs(d)
        n = dl[3]                   # bytes in this block
        out += d[24:24 + n]
        nxt = dl[4]
    return bytes(out[:size])


def walk(adf, dirblock, path=''):
    d = longs(blocks(adf, dirblock))
    for k in range(6, 78):          # the hash table
        n = d[k]
        while n:
            b = blocks(adf, n)
            bl = longs(b)
            sec = struct.unpack('>i', b[508:512])[0]
            nm = name_of(b)
            full = (path + '/' + nm) if path else nm
            if sec == 2:            # a directory
                yield from walk(adf, n, full)
            elif sec == -3:         # a file
                yield full, read_file(adf, n)
            n = bl[124]             # next in this hash chain


def hunks(exe):
    """CODE/DATA/BSS hunks of an Amiga program (relocations ignored)."""
    p = 0
    rd = lambda: struct.unpack('>I', exe[p:p + 4])[0]
    assert rd() == 0x3f3, 'not an Amiga program'
    p += 4
    while rd():
        p += 4 + rd() * 4
    p += 4
    first, last = struct.unpack('>II', exe[p + 4:p + 12])
    p += 12 + (last - first + 1) * 4
    out = []
    while p < len(exe):
        t = rd() & 0x3fffffff
        p += 4
        if t in (0x3e9, 0x3ea):
            n = rd() * 4
            p += 4
            out.append(exe[p:p + n])
            p += n
        elif t == 0x3eb:
            p += 4
            out.append(b'')
        elif t == 0x3ec:
            while True:
                c = rd()
                p += 4
                if not c:
                    break
                p += 4 + c * 4
        elif t == 0x3f2:
            pass
        else:
            raise SystemExit('unexpected hunk %x' % t)
    return out


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    adf = open(sys.argv[1], 'rb').read()
    if len(adf) != 901120 or adf[:3] != b'DOS':
        raise SystemExit('That is not an 880 KB AmigaDOS disk image.')
    root = longs(blocks(adf, 880))
    vol = name_of(blocks(adf, 880))
    if vol.lower() != 'firepower':
        raise SystemExit('That disk is "%s", not FirePower.' % vol)
    files = dict(walk(adf, 880))
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(os.path.dirname(here), 'assets', 'fp')
    keep = ('gr/', 'mapp1/', 'mapp2/', 'mapp3/', 'sounds/', 'ins/')
    n = 0
    for name, data in files.items():
        low = name.lower()
        if not low.startswith(keep):
            continue
        dst = os.path.join(out, low)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        open(dst, 'wb').write(data)
        n += 1
    exe = files.get('firepower')
    if not exe:
        raise SystemExit('No "firepower" program on the disk.')
    hk = hunks(exe)
    data = hk[1]
    # the tile types: 93 entries of 16 bytes at $5fa in the data hunk. The first
    # long is a pointer into the tile pictures (relocated by the loader): turned
    # here into the tile's picture number. The original keeps 28 copies of some
    # tile pictures at $6cea.. (sub_0dad0): mapped back to the picture copied.
    copies = [0x594c, 0x5968, 0x5984, 0x59d8, 0x59f4, 0x5a10, 0x5a64, 0x5a80, 0x5a9c, 0x5ab8,
              0x5ad4, 0x5b0c, 0x5b44, 0x5b7c, 0x5b98, 0x5bb4, 0x5bec, 0x5c08, 0x5c24, 0x5c40,
              0x5c78, 0x5c94, 0x5cb0, 0x5b28, 0x586c, 0x5ccc, 0x5a2c]
    tt = bytearray()
    for k in range(93):
        e = bytearray(data[0x5fa + k * 16:0x5fa + k * 16 + 16])
        ptr = struct.unpack('>I', e[:4])[0]
        enemy = 0
        if ptr >= 0x6cea:
            ptr = copies[(ptr - 0x6cea) // 28]
            enemy = 8   # the copies are drawn with colour bit 3 cleared (their byte $1a)
        pic = (ptr - 0x5770) // 28
        if not 0 <= pic < 50:
            raise SystemExit('Unexpected tile table: is this the original program?')
        e[0:4] = struct.pack('>I', pic)
        e[2] = enemy
        tt += e
    open(os.path.join(out, 'tiletypes.bin'), 'wb').write(tt)
    per = b''.join(data[o:o + 2] for o in (0x24c, 0x27c, 0x2c4, 0x2f4, 0x33c))
    open(os.path.join(out, 'sounds.bin'), 'wb').write(per)
    print('%d files and the tables into %s' % (n, out))


if __name__ == '__main__':
    main()
