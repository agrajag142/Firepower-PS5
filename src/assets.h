// assets.h - Firepower's own files (from your disk, tools/extract_assets.py),
// decoded into pictures the game can draw.
//
// The Amiga stores its pictures as bitplanes. Each sprite file is a run of
// records: a 16-byte header (width, height, colour planes, planes stored, mask
// plane, collision plane, PlanePick, PlaneOnOff - the same fields as the
// original's LoadGraphics, sub_0d66c) then the planes one after another.
// Full screens are 5 planes of 352 x 232 then their 32-colour palette.

#ifndef ASSETS_H
#define ASSETS_H

#include <stddef.h>
#include <stdint.h>

// A picture in the PS5's pixel format (0xAABBGGRR); alpha 0 where it's see-through
struct Pic {
  int w = 0, h = 0;
  uint32_t *px = nullptr;
};

// A sprite file: its records in order (the tank's 16 headings, the explosion's frames...)
struct PicSet {
  int n = 0;
  Pic pic[64];
};

enum SoundId { SND_TANKBLAST, SND_BOOM2, SND_HELI, SND_SPLAT, SND_DRIP, SND_COUNT };

struct Sound {
  const int8_t *data = nullptr;
  int len = 0;
  int rate = 8000;            // samples a second (from the original's Paula period)
};

// The 16 bytes the original keeps for each kind of map square (its table at $5fa)
struct TileType {
  uint8_t pic;                // which picture in gr/tiles (0..49)
  uint8_t becomes;            // the square left when it's destroyed
  uint8_t destructible;       // 1: shells wear it down
  uint8_t flagsA, flagsB;     // bases, helipads... (still being worked out)
  uint8_t link;               // the other half of a two-square thing
  uint8_t block;              // movement flags: & 0x78 = solid (the original's B_25d0 map)
  uint8_t strength;           // hits it takes
  uint8_t side;               // high nibble: 1 yours, 2 the enemy's; 0 open ground
};

struct Assets {
  bool ok = false;
  char error[160] = "";
  uint32_t pal[32];           // the battle's colours (gr/oneplayer's palette)
  Pic title;                  // gr/fp.title
  Pic tiles[50];              // gr/tiles, at 2x (128 x 128), ready to copy
  uint32_t tileAvg[50];       // each tile's average colour (the map)
  uint8_t tileSolid[50][64 * 64];   // 1 where a tile's picture isn't sand (what blocks, in a solid square)
  uint8_t tileLeaves[50][64 * 64];  // 1 where it's leaves (green): drawn over a tank under the tree
  struct Tree { float x, y, r; };   // a tree in a tile: its middle and its size (Amiga pixels)
  int nTrees[50];
  Tree trees[50][4];
  PicSet tank, scorp, shadow, turret, heli, men, boom, shell, numbers;
  TileType types[93];
  uint8_t maps[11][4096];     // mapp1/t1..t5, mapp2/t1..t5, mapp3/t1 (x * 64 + y)
  Sound snd[SND_COUNT];
};

extern Assets A;

// Loads everything from dir (ending in '/'): "/app0/assets/fp/" on the PS5.
bool assetsLoad(const char *dir);

// Big blocks straight from the system (the PS5's malloc heap is small)
void *bigAlloc(size_t bytes);

#endif // ASSETS_H
