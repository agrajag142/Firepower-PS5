// assets.cpp - see assets.h

#include "assets.h"
#include "gfx.h"

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

Assets A;

void *bigAlloc(size_t bytes) {
  void *p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
  return p == MAP_FAILED ? nullptr : p;
}

static const char *gDir = "";

// A whole file (into memory from the system), or null
static uint8_t *readFile(const char *name, long *len) {
  char path[256];
  snprintf(path, sizeof(path), "%s%s", gDir, name);
  FILE *f = fopen(path, "rb");
  if (!f) {
    snprintf(A.error, sizeof(A.error), "MISSING %s", name);
    return nullptr;
  }
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *p = n > 0 ? (uint8_t *)bigAlloc((size_t)n) : nullptr;
  if (!p || fread(p, 1, (size_t)n, f) != (size_t)n) {
    fclose(f);
    snprintf(A.error, sizeof(A.error), "CAN'T READ %s", name);
    return nullptr;
  }
  fclose(f);
  *len = n;
  return p;
}

static inline int be16(const uint8_t *p) { return (int16_t)(p[0] << 8 | p[1]); }

static uint32_t amigaColour(int v) {   // $0RGB -> 0xAABBGGRR
  int r = (v >> 8 & 15) * 17, g = (v >> 4 & 15) * 17, b = (v & 15) * 17;
  return rgba(r, g, b);
}

// One record's planes into a picture, through PlanePick / PlaneOnOff and the mask
static void decodeRecord(const uint8_t *hdr, const uint8_t *data, Pic &out, bool opaque) {
  int w = be16(hdr), h = be16(hdr + 2), depth = be16(hdr + 4), planes = be16(hdr + 6);
  int mask = be16(hdr + 8), pick = be16(hdr + 12), onoff = be16(hdr + 14);
  int bpr = (w + 15) / 16 * 2;
  int target[8];   // the bitplane each stored colour plane lands in
  for (int k = 0, b = 0; k < depth && k < 8; k++) {
    if (pick < 0) { target[k] = k; continue; }
    while (b < 8 && !(pick >> b & 1)) b++;
    target[k] = b++;
  }
  int fixed = 0;   // bits from PlaneOnOff, for the planes not picked
  if (pick >= 0 && onoff > 0)
    for (int b = 0; b < 5; b++)
      if (!(pick >> b & 1) && (onoff >> b & 1)) fixed |= 1 << b;
  bool hasMask = !opaque && mask >= depth && mask < planes;
  out.w = w;
  out.h = h;
  out.px = (uint32_t *)bigAlloc((size_t)w * h * 4);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      int byte = y * bpr + (x >> 3), bit = 7 - (x & 7), c = fixed;
      for (int k = 0; k < depth && k < 8; k++)
        if (data[k * bpr * h + byte] >> bit & 1) c |= 1 << target[k];
      uint32_t px = A.pal[c & 31];
      if (hasMask && !(data[mask * bpr * h + byte] >> bit & 1)) px = 0;
      out.px[y * w + x] = px;
    }
}

static bool loadSet(const char *name, PicSet &set, bool opaque = false) {
  long len = 0;
  uint8_t *b = readFile(name, &len);
  if (!b) return false;
  set.n = 0;
  for (long o = 0; o + 16 <= len && set.n < 64;) {
    int w = be16(b + o), h = be16(b + o + 2), planes = be16(b + o + 6);
    if (w <= 0 || h <= 0 || planes <= 0 || planes > 8) break;
    long size = (long)((w + 15) / 16 * 2) * h * planes;
    if (o + 16 + size > len) break;
    decodeRecord(b + o, b + o + 16, set.pic[set.n++], opaque);
    o += 16 + size;
  }
  munmap(b, (size_t)len);
  return set.n > 0;
}

// A full screen: 5 planes of 352 x 232 ($27e0 bytes each), then 32 colours
static bool loadScreen(const char *name, Pic &out) {
  long len = 0;
  uint8_t *b = readFile(name, &len);
  if (!b) return false;
  const int W = 352, H = 232, plane = 0x27e0;
  if (len < plane * 5 + 64) return false;
  uint32_t pal[32];
  for (int i = 0; i < 32; i++) pal[i] = amigaColour(be16(b + plane * 5 + i * 2));
  out.w = W;
  out.h = H;
  out.px = (uint32_t *)bigAlloc(W * H * 4);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      int c = 0, byte = y * (W / 8) + (x >> 3), bit = 7 - (x & 7);
      for (int p = 0; p < 5; p++)
        if (b[p * plane + byte] >> bit & 1) c |= 1 << p;
      out.px[y * W + x] = pal[c];
    }
  munmap(b, (size_t)len);
  return true;
}

static bool loadSound(const char *name, Sound &s, int period) {
  long len = 0;
  uint8_t *b = readFile(name, &len);
  if (!b) return false;
  s.data = (const int8_t *)b;   // played as it is, header and all (as the original does)
  s.len = (int)len;
  s.rate = period > 0 ? 3546895 / period : 8000;
  return true;
}

bool assetsLoad(const char *dir) {
  gDir = dir;
  A.ok = false;
  long len = 0;
  // the battle's palette: the last 64 bytes of gr/oneplayer
  uint8_t *op = readFile("gr/oneplayer", &len);
  if (!op || len < 64) return false;
  for (int i = 0; i < 32; i++) A.pal[i] = amigaColour(be16(op + len - 64 + i * 2));
  munmap(op, (size_t)len);

  if (!loadScreen("gr/fp.title", A.title)) return false;
  PicSet tiles;
  if (!loadSet("gr/tiles", tiles, true) || tiles.n < 50) return false;
  // the sand: every colour in the two plain sand tiles (0 and 1)
  static uint8_t sand[32];
  memset(sand, 0, sizeof(sand));
  for (int t = 0; t < 2; t++)
    for (int i = 0; i < 64 * 64; i++)
      for (int c = 0; c < 32; c++)
        if (tiles.pic[t].px[i] == A.pal[c]) sand[c] = 1;
  for (int t = 0; t < 50; t++) {   // at 2x, and their average colour for the map
    for (int i = 0; i < 64 * 64; i++) {
      bool isSand = false;
      for (int c = 0; c < 32 && !isSand; c++)
        if (sand[c] && tiles.pic[t].px[i] == A.pal[c]) isSand = true;
      A.tileSolid[t][i] = isSand ? 0 : 1;
    }
    Pic &src = tiles.pic[t], &dst = A.tiles[t];
    dst.w = dst.h = 128;
    dst.px = (uint32_t *)bigAlloc(128 * 128 * 4);
    unsigned r = 0, g = 0, bl = 0;
    for (int y = 0; y < 128; y++)
      for (int x = 0; x < 128; x++) {
        uint32_t p = src.px[(y >> 1) * src.w + (x >> 1)];
        dst.px[y * 128 + x] = p;
        r += p & 255; g += p >> 8 & 255; bl += p >> 16 & 255;
      }
    A.tileAvg[t] = rgba(r / 16384, g / 16384, bl / 16384);
  }
  if (!loadSet("gr/tank0", A.tank) || !loadSet("gr/scorp", A.scorp) || !loadSet("gr/shadow", A.shadow) ||
      !loadSet("gr/turret", A.turret) || !loadSet("gr/heli", A.heli) || !loadSet("gr/men", A.men) ||
      !loadSet("gr/boom", A.boom) || !loadSet("gr/tank_miss", A.shell) || !loadSet("gr/numbers", A.numbers))
    return false;

  uint8_t *tt = readFile("tiletypes.bin", &len);
  if (!tt || len < 93 * 16) return false;
  for (int k = 0; k < 93; k++) {
    const uint8_t *e = tt + k * 16;
    TileType &t = A.types[k];
    t.pic = e[3] < 50 ? e[3] : 0;
    t.becomes = e[4];
    t.destructible = e[5];
    t.flagsA = e[6];
    t.flagsB = e[7];
    t.link = e[9];
    t.block = e[10];
    t.strength = e[13];
    t.side = e[14];
  }
  munmap(tt, (size_t)len);

  static const char *mapNames[11] = { "mapp1/t1", "mapp1/t2", "mapp1/t3", "mapp1/t4", "mapp1/t5", "mapp2/t1",
                                      "mapp2/t2", "mapp2/t3", "mapp2/t4", "mapp2/t5", "mapp3/t1" };
  for (int m = 0; m < 11; m++) {
    uint8_t *mp = readFile(mapNames[m], &len);
    if (!mp || len < 4096) return false;
    for (int i = 0; i < 4096; i++) A.maps[m][i] = mp[i] < 93 ? mp[i] : 0;
    munmap(mp, (size_t)len);
  }

  uint8_t *per = readFile("sounds.bin", &len);
  int periods[SND_COUNT] = { 480, 700, 500, 300, 1000 };
  if (per && len >= 10)
    for (int i = 0; i < SND_COUNT; i++) periods[i] = be16(per + i * 2);
  static const char *sndNames[SND_COUNT] = { "sounds/tankblast", "sounds/boom2", "sounds/heli", "sounds/splat",
                                             "sounds/drip" };
  for (int i = 0; i < SND_COUNT; i++)
    if (!loadSound(sndNames[i], A.snd[i], periods[i])) return false;
  A.error[0] = 0;
  A.ok = true;
  return true;
}
