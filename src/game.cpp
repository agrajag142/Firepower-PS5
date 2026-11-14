// game.cpp - FIREPOWER for the PS5
//
// A native remake of MicroIllusions' Firepower (Amiga, 1987), built from the
// original's own graphics, maps and sounds (pulled off your disk by
// tools/extract_assets.py) and its rules as read from its disassembly.
// Enhanced: the whole 1920 x 1080 picture, the original's pixels at 2x, so
// about 2.7 times as much of the battlefield is in view.
//
// Version 023: what still stands in a ruin (wall stubs, a building's corners,
// a depot's frame) blocks, as walls do; the scattered rubble doesn't. Music
// louder (0.85).
//
// Version 022: music, as our other PS5 games: title.mod on the title and end
// screens, game.mod in the battle (in the app's assets; without them, silence).
// The goal reads CAPTURE THE ENEMY FLAG.
//
// Version 021: up to 5 helicopters at once, a new one every 30 to 50 seconds
// (as seen on the Amiga).
//
// Version 020: the original's panel on the right (from its one-player screen,
// scaled to the TV's height) with the score in its own digits, your men, your
// tanks, the fuel, the radar and your armour; the battle view beside it.
// X starts a battle on one of the five one-player battlefields at random, as
// the original; Triangle on the title screen picks one instead.
//
// Version 019: Square reverses (straight back, as it faces).
//
// Version 018: the tank moves exactly the way its picture faces (one of the
// original's 16 headings), so it no longer slides off a straight line; the
// D-pad steers as the stick does (8 ways).
//
// Version 017: the garage's three walls are solid: a tank only comes out (or
// goes in) at the bottom, as on the Amiga.
//
// Version 016: shells reach about 165 pixels (on the Amiga: three quarters of
// its battle view, which is about 218 pixels across).
//
// Version 015: turrets reach about 110 pixels (checked against the Amiga
// game: they fire only from just off the edge of its small battle view).
//
// Version 014: fuel. The tank uses it as it drives; your garage refuels it,
// and destroying one of the enemy's fuel depots (type 34, its green barrels)
// fills it up. Empty, it only crawls. A mine for every 5 men rescued (kept
// for the two-player game, where mines matter).
//
// Version 013: an extra tank for every 15 men rescued (as the original).
//
// Version 012: the men walk slower, and get out at your first-aid station
// (type 24, the red cross): stop on the square to its right and they climb
// out and run in, as the original's instructions say.
//
// Version 011: the men. Destroying a building or post lets out the men in it
// (the low 4 bits of its side byte; bit 7: yours, brown - bit 6: the enemy's,
// green - as the original's square-destroyed code, sub_02566). Running over a
// man kills him (Splat, the blood stays); stopping by one of yours takes him
// aboard (as the original's sub_044ae, with room in the tank), and your garage
// is where they're rescued.
//
// Version 010: the enemy's helicopters in the enemy's colours (green).
//
// Version 009: the original's one-player battle. One of the enemy's big
// bunkers (type 92; the original's sub_07f50 picks one at random each game)
// is its headquarters (type 69): one shell opens it to its inner bunker (81),
// and when that's destroyed your flag lies in the ruin. Pick it up and bring
// it home to your base (bit 3 of the first flags: the garage) to win, as the
// original (its B_5d36 = 4). Without the flag, your base repairs your tank.
// The other bunkers are decoys.
//
// Version 008: the enemy's walls, posts, buildings and pads in the enemy's
// colours (green), as the original draws its copies of them.
//
// Version 007: the turrets where the original has them - the enemy's corner
// posts and bunkers (bit 3 of the tile's second flags; sub_01f82 draws their
// guns, their heading kept in the square's low 3 bits), the gun at the
// original's place for each heading. The big round pads (type 37: bit 1, tied
// to the helicopters) are the enemy's helipads: the helicopters take off from
// them. Plain walls can be shot open: worn down into the original's damaged
// wall, then a gap.
//
// Version 006: the enemy's helicopters (the Heli file: 16 headings, the rotor,
// the shadow, their missiles): they fly in, circle your tank and fire; two
// shells bring one down.
//
// Version 005: the enemy's turrets (the gun positions, type 37): they turn to
// your tank when it's near and fire missiles (the Turret file's guns and
// missiles). Your tank's armour, three tanks, losing one, game over.
//
// Version 004: all of a tree over the tank, its black too.
//
// Version 003: trees - the tank drives under their branches (the leaves drawn
// over it) but not through their trunks.
//
// Version 002: the tank starts under its garage's roof (drawn over it); your
// gates (type 44: bit 6 of the tile's second flags) open as you drive up to
// them and close behind you, as the original's sub_03680 does.
//
// Version 001: the title screen, a battlefield (any of the 11 on the disk),
// your tank (16 headings, as the original), the walls and buildings in its
// way, shells that wear down and destroy what they hit (each square's
// strength and what it turns into come from the original's tile table), the
// explosions, the original sounds, a mini map and the full map.

#include "game.h"
#include "assets.h"
#include "gfx.h"
#include "micromod.h"

#include <atomic>
#include <math.h>
#include <stdio.h>
#include <string.h>

// ============================================================ settings
static const char *VERSION = "V023";
static const int S = 2;                    // screen pixels per Amiga pixel
static const int TILE = 64;                // a map square, in Amiga pixels
static const int MAPN = 64;                // squares across and down
static const float WORLD = TILE * MAPN;    // 4096
// The panel: the right of the original's one-player screen (x 240 on), scaled to the TV's height
static const float PANEL_K = FB_H / 232.0f;                     // 4.66 screen pixels an Amiga pixel
static const int PANEL_X0 = FB_W - (int)((352 - 240) * PANEL_K);   // where it starts on the TV
static const int PLAY_W = PANEL_X0;                             // the battle view: the rest
static const float VIEW_W = PLAY_W / S, VIEW_H = FB_H / S;      // ~700 x 540 Amiga pixels in view
static const float PLAY_CX = PLAY_W / 2 / UNIT_K;               // its middle, in drawing units

static const float TANK_SPEED = 72.0f;     // forward, Amiga pixels a second
static const float TANK_REVERSE = 40.0f;
static const float TANK_ACCEL = 160.0f;
static const float TANK_TURN = 3.2f;       // radians a second
static const float TANK_RADIUS = 10.0f;    // its body, for the walls
static const float SHELL_SPEED = 260.0f;
static const float SHELL_RANGE = 165.0f;   // (the Amiga: 3/4 of its ~218-pixel battle view)
static const float RELOAD = 0.45f;
static const int SHELL_DAMAGE = 5;         // a building (strength 15) takes three
static const float GATE_CLOSE = 1.0f;
static const float TRUNK = 0.3f;
// A turret acts only while it would be in the original's battle view round your
// tank (the original works its turrets as it draws the squares in view). That
// view is smaller than the Amiga's screen (its panel takes the rest): on the
// Amiga a turret fires from just off its edge, about 110 pixels away
static const float TURRET_SEE_X = 110, TURRET_SEE_Y = 110;
static const float TURRET_STEP = 0.14f;    // seconds per 45-degree turn (the original: a step per update)
static const float TURRET_RELOAD = 0.8f;   // after its missile is gone (plus a little at random)
static const int WALL_STRENGTH = 30;       // a plain wall: shells to wear it into a damaged wall (x SHELL_DAMAGE)
static const float MISSILE_SPEED = 150.0f;
static const float MISSILE_RANGE = 200.0f;
static const int MISSILE_DAMAGE = 12;      // of your tank's 100 armour
static const int TANKS = 3;                // tanks you have
static const float RESPAWN = 2.5f;         // seconds before the next tank comes out
static const int DECOY_STRENGTH = 40;      // a decoy bunker's inside (the real one's: the original's 75)
static const float MAN_SPEED = 9.0f;       // Amiga pixels a second (running: x1.5)
static const float UNLOAD_EVERY = 0.45f;
static const int MEN_PER_TANK = 15;        // rescued men for an extra tank
static const int MEN_PER_MINE = 5;         // rescued men for an extra mine
static const int MINES = 3;                // mines to start with (laid in the two-player game, to come)
static const float FUEL_RANGE = 9000.0f;   // Amiga pixels a full tank drives (about two minutes)
static const float FUEL_EMPTY_SPEED = 0.25f;   // how fast it crawls with none
static const float REFUEL_RATE = 25.0f;    // in your garage: fuel a second (of 100)   // seconds between men getting out at the first-aid station
static const int TANK_ROOM = 6;            // men your tank can carry
static const float REPAIR_EVERY = 0.25f;   // in your base without the flag: +5 armour this often (the original: 5)
static const float HELI_FIRST = 25.0f;     // seconds into a battle before the first helicopter
static const float HELI_EVERY = 30.0f;     // then one every this many seconds (+ up to HELI_EVERY_MORE)
static const float HELI_EVERY_MORE = 20.0f;
static const int HELI_MAX = 5;             // at once
static const float HELI_SPEED = 95.0f;
static const float HELI_TURN = 1.3f;       // radians a second
static const float HELI_CIRCLE = 150.0f;   // how far out it circles your tank
static const float HELI_RELOAD = 2.2f;
static const int HELI_HITS = 2;            // shells to bring one down
static const float HELI_MISSILE_SPEED = 190.0f;
static const int HELI_MISSILE_DAMAGE = 10;           // a tree's trunk: this much of its leaves' size (and at least 3 pixels)      // seconds after you're clear of a gate, it shuts

// ============================================================ the music
#define TITLE_MUSIC_VOLUME 0.85f   // title.mod: 0.0 = off ... 1.0 = full
#define GAME_MUSIC_VOLUME  0.85f   // game.mod, in the battle
#define MUSIC_FADE_SECONDS 0.8f    // the two cross over
extern "C" {   // (micromod2.c: a second copy of the MOD player, so each tune keeps its place)
long micromod2_calculate_mod_file_len(signed char *module_header);
long micromod2_initialise(signed char *data, long sampling_rate);
void micromod2_set_gain(long value);
void micromod2_set_position(long pos);
void micromod2_get_audio(short *output_buffer, long count);
}
#define TUNES 2
static bool tuneReady[TUNES] = { false, false };
static float tuneGain[TUNES] = { 0, 0 };            // (the sound thread's)
static std::atomic<int> tuneWanted{0};              // 0 the title's, 1 the battle's
static std::atomic<bool> tuneFromStart[TUNES];      // (start it again from the top as it comes in)
static long (*const modLen[TUNES])(signed char *) = { micromod_calculate_mod_file_len, micromod2_calculate_mod_file_len };
static long (*const modInit[TUNES])(signed char *, long) = { micromod_initialise, micromod2_initialise };
static void (*const modGain[TUNES])(long) = { micromod_set_gain, micromod2_set_gain };
static void (*const modPos[TUNES])(long) = { micromod_set_position, micromod2_set_position };
static void (*const modAudio[TUNES])(short *, long) = { micromod_get_audio, micromod2_get_audio };

void gameSetTune(int which, signed char *data, long len) {
  if (which < 0 || which >= TUNES || !data || len < 1084) return;
  long need = modLen[which](data);
  if (need < 0 || need > len) return;            // not a MOD, or cut short: no music
  if (modInit[which](data, 48000) != 0) return;
  modGain[which](64);
  tuneReady[which] = true;
}

// ============================================================ the sound
// A few voices mixing the original's 8-bit samples up to 48 kHz. The game
// asks for a sound by setting its bit; the sound thread starts it.
struct Voice {
  const int8_t *data = nullptr;
  int len = 0;
  double pos = 0, step = 0;
  float vol = 0, pan = 0.5f;
};
static Voice voices[8];
static std::atomic<uint32_t> soundAsk{0};
static std::atomic<int> soundVolAsk[SND_COUNT];

static void playSound(int id, float vol = 1.0f) {
  if (id < 0 || id >= SND_COUNT || !A.snd[id].data) return;
  soundVolAsk[id].store((int)(vol * 256), std::memory_order_relaxed);
  soundAsk.fetch_or(1u << id, std::memory_order_release);
}

void gameAudio(short *out, int frames) {
  uint32_t ask = soundAsk.exchange(0, std::memory_order_acquire);
  for (int id = 0; id < SND_COUNT; id++) {
    if (!(ask >> id & 1)) continue;
    int slot = 0;   // a free voice, or the one nearest its end
    double best = -1;
    for (int v = 0; v < 8; v++) {
      if (!voices[v].data) { slot = v; best = 2; break; }
      double done = voices[v].pos / voices[v].len;
      if (done > best) { best = done; slot = v; }
    }
    Voice &vc = voices[slot];
    vc.data = A.snd[id].data;
    vc.len = A.snd[id].len;
    vc.pos = 0;
    vc.step = A.snd[id].rate / 48000.0;
    vc.vol = soundVolAsk[id].load(std::memory_order_relaxed) / 256.0f;
  }
  // the music: each tune fading towards its level
  if (frames > 2048) frames = 2048;
  static float music[2048 * 2];
  static short tuneBuf[2048 * 2];
  memset(music, 0, (size_t)frames * 2 * sizeof(float));
  int want = tuneWanted.load(std::memory_order_relaxed);
  for (int k = 0; k < TUNES; k++) {
    float vol = k == 0 ? TITLE_MUSIC_VOLUME : GAME_MUSIC_VOLUME;
    float target = (tuneReady[k] && want == k) ? vol : 0.0f;
    if (!tuneReady[k] || (tuneGain[k] <= 0 && target <= 0)) continue;
    if (tuneGain[k] <= 0 && tuneFromStart[k].exchange(false)) modPos[k](0);
    memset(tuneBuf, 0, (size_t)frames * 4);
    modAudio[k](tuneBuf, frames);
    float step = vol / (MUSIC_FADE_SECONDS * 48000);
    for (int n = 0; n < frames; n++) {
      if (tuneGain[k] < target) tuneGain[k] = fminf(tuneGain[k] + step, target);
      else if (tuneGain[k] > target) tuneGain[k] = fmaxf(tuneGain[k] - step, target);
      // the MOD's channels are panned hard left and right: each side gets a little of the other
      float ml = tuneBuf[n * 2] / 32768.0f, mr = tuneBuf[n * 2 + 1] / 32768.0f;
      music[n * 2] += (ml * 0.62f + mr * 0.25f) * tuneGain[k];
      music[n * 2 + 1] += (mr * 0.62f + ml * 0.25f) * tuneGain[k];
    }
  }
  for (int i = 0; i < frames; i++) {
    float l = music[i * 2], r = music[i * 2 + 1];
    for (Voice &v : voices) {
      if (!v.data) continue;
      int p = (int)v.pos;
      if (p >= v.len) { v.data = nullptr; continue; }
      float s = v.data[p] / 128.0f * v.vol;
      l += s * 0.8f;
      r += s * 0.8f;
      v.pos += v.step;
    }
    l = l > 1 ? 1 : (l < -1 ? -1 : l);
    r = r > 1 ? 1 : (r < -1 ? -1 : r);
    out[i * 2] = (short)(l * 30000);
    out[i * 2 + 1] = (short)(r * 30000);
  }
}

// ============================================================ the battlefield
static uint8_t field[MAPN * MAPN];   // the squares now (x * 64 + y, as the disk has them)
static int strength[MAPN * MAPN];    // what's left of each square
static int mapNo = 0;                // 0..10: mapp1/t1..t5, mapp2/t1..t5, mapp3/t1
static int mapChoice = -1;           // the title screen's: -1 at random (of the one-player five), else that one

static inline int sq(int tx, int ty) { return tx * MAPN + ty; }
static inline const TileType &typeAt(int tx, int ty) { return A.types[field[sq(tx, ty)]]; }
static bool solidSquare(int tx, int ty) {
  if (tx < 0 || ty < 0 || tx >= MAPN || ty >= MAPN) return true;
  return (typeAt(tx, ty).block & 0x78) != 0;
}
// A point: in a solid square, solid where its picture isn't sand (so a tank can
// drive up to a wall or a building, not stop a square short of it)
static bool solidAt(float x, float y, bool trees = true) {
  int tx = (int)floorf(x / TILE), ty = (int)floorf(y / TILE);
  if (!solidSquare(tx, ty)) {
    if (tx < 0 || ty < 0 || tx >= MAPN || ty >= MAPN) return false;
    // a base's garage (bits 3, 2 of the first flags): its left, top and right walls are
    // solid, open only at the bottom (the picture's walls: left to x 19, right from 45, top to y 15)
    if (typeAt(tx, ty).flagsA & 0x0c) {
      int px = (int)x - tx * TILE, py = (int)y - ty * TILE;
      if (py >= 8 && py < 56 && px >= 9 && px < 56 && (px < 19 || px >= 45 || py < 15)) return true;
    }
    // a ruin: what still stands in it blocks (the scattered rubble doesn't)
    int pic = typeAt(tx, ty).pic;
    {
      int px = (int)x - tx * TILE, py = (int)y - ty * TILE;
      if (A.tileDebris[pic][py * TILE + px]) return true;
    }
    // a tree: only its trunk, in the middle of its leaves, stops a tank
    if (!trees) return false;
    for (int k = 0; k < A.nTrees[pic]; k++) {
      const Assets::Tree &tr = A.trees[pic][k];
      float dx = x - (tx * TILE + tr.x), dy = y - (ty * TILE + tr.y), r = tr.r * TRUNK;
      if (r < 3) r = 3;
      if (dx * dx + dy * dy < r * r) return true;
    }
    return false;
  }
  if (tx < 0 || ty < 0 || tx >= MAPN || ty >= MAPN) return true;
  int px = (int)x - tx * TILE, py = (int)y - ty * TILE;
  return A.tileSolid[typeAt(tx, ty).pic][py * TILE + px] != 0;
}

// ============================================================ things
struct Tank {
  float x = 0, y = 0;
  float heading = 0;        // radians, 0 north, clockwise (the frames' order)
  float speed = 0;
  float reload = 0;
  int armour = 100;
  float fuel = 100;
  bool alive = true;
  float down = 0;           // since it was destroyed
};
struct Turret {
  int i;                    // its square
  float heading;            // the gun, radians (one of 8)
  float reload;             // after its missile is gone, before the next can go
  float step;               // till it turns another 45 degrees
  bool loaded;              // no missile of its own in the air (the original: one at a time)
};
struct Missile {
  bool live = false;
  float x, y, dx, dy, left;
  int frame;                // 0..7
  int from;                 // the turret's square (it flies out over its own base), or -1
  int owner;                // the turret that fired it (turrets[]), or -1
  bool air;                 // a helicopter's (its own pictures; it flies over walls)
};
struct Heli {
  bool live = false;
  float x, y, heading, reload, sound;
  int hits;
  float side;               // which way it circles (+1 / -1)
  float ring;               // how far out it circles (each its own, so they don't stack)
};
struct Shell {
  bool live = false;
  float x, y, dx, dy, left;
  int frame;
};
struct Boom {
  bool live = false;
  float x, y, t;
  bool big;
};

static Tank tank;
static Shell shells[16];
static Boom booms[32];
static int score = 0, destroyed = 0, tanksLeft = TANKS, tanksLost = 0;
static Turret turrets[512];
static int nTurrets = 0;
static Turret ours[256];   // your side's turrets (bit 2): drawn, for now (they'll fire at the enemy's tanks)
static int nOurs = 0;
static Missile missiles[48];
struct Man {
  bool live = false;
  float x, y, dx, dy, turn;
  int side;                 // 0 yours (brown), 1 the enemy's (green): the Men file's pictures 0/2 and 1/3
  bool inside;              // getting out of your tank and running into the first-aid station
  float gx, gy;             // (where to)
};
struct Splat { float x, y; };
static Man men[200];
static Splat splats[96];
static int nSplats = 0, aboard = 0, rescued = 0, menLost = 0, menKilled = 0;
static float unloadT = 0, extraT = 0, fuelT = 0;   // (fuelT: the REFUELLED message)
static int mines = MINES;   // (extraT: the EXTRA TANK message, while > 0)
static float aidX = 20, aidY = 32;   // the red cross in the first-aid station's square (from its picture)
static Heli helis[HELI_MAX];
static float heliTimer = 0;
// the flag: in the enemy's headquarters, lying loose, or on your tank
enum FlagState { FLAG_HIDDEN, FLAG_LOOSE, FLAG_CARRIED };
static FlagState flagState = FLAG_HIDDEN;
static float flagX = 0, flagY = 0, repairT = 0;
static int hqSquare = -1;
static uint8_t decoy[64 * 64];   // 1: a decoy bunker (no flag inside)
static int helisDown = 0;
static int startSquare = 0;
static uint32_t rng = 1;
static float rnd() {   // 0..1
  rng = rng * 1664525u + 1013904223u;
  return (rng >> 8) / 16777216.0f;
}
static float clockT = 0;

enum State { ST_TITLE, ST_PLAY, ST_OVER, ST_WON };
static State state = ST_TITLE;
static float overT = 0;   // on the game over screen
static bool mapOpen = false;
static float titleT = 0;
static char debugText[256];

static inline int roundi(float v) { return (int)floorf(v + 0.5f); }
// An angle into -PI..PI
static inline float wrapAngle(float a) { return a - TWO_PI * floorf((a + PI) / TWO_PI); }

static int frameOf(float heading) {
  int f = roundi(heading / TWO_PI * 16);
  return ((f % 16) + 16) % 16;
}

static void addBoom(float x, float y, bool big) {
  for (Boom &b : booms)
    if (!b.live) {
      b = { true, x, y, 0, big };
      return;
    }
}

// ============================================================ gates
// The original opens a gate (a square with bit 6 of its second flags, yours, or
// bit 7, the enemy's) when that side's tank is on it or beside it in its row:
// the square becomes its linked type (byte 9: the open gate).
struct Gate {
  int i;                 // its square
  uint8_t shut, open;    // its two types
  float clear;           // how long nothing has been near it
};
static Gate gates[64];
static int nGates = 0;

static void findGates() {
  nGates = 0;
  for (int i = 0; i < MAPN * MAPN && nGates < 64; i++) {
    const TileType &t = A.types[field[i]];
    if ((t.flagsB & 0x40) && t.link < 93) gates[nGates++] = { i, field[i], t.link, 0 };
  }
}

static void updateGates(float dt) {
  int tx = (int)floorf(tank.x / TILE), ty = (int)floorf(tank.y / TILE);
  for (int k = 0; k < nGates; k++) {
    Gate &g = gates[k];
    int gx = g.i / MAPN, gy = g.i % MAPN;
    // near: in its row (or column), on it or a square either side; or any part of the tank on it
    bool near = (ty == gy && tx >= gx - 1 && tx <= gx + 1) || (tx == gx && ty >= gy - 1 && ty <= gy + 1);
    float r = TANK_RADIUS + 2;
    bool onIt = tank.x + r > gx * TILE && tank.x - r < gx * TILE + TILE && tank.y + r > gy * TILE &&
                tank.y - r < gy * TILE + TILE;
    if (field[g.i] == g.shut) {
      if (near) {
        field[g.i] = g.open;
        g.clear = 0;
        playSound(SND_DRIP, 0.4f);
      }
    } else if (field[g.i] == g.open) {
      g.clear = (near || onIt) ? 0 : g.clear + dt;
      if (g.clear >= GATE_CLOSE) field[g.i] = g.shut;
    }
  }
}

static void startBattle(int m) {
  mapNo = m;
  memcpy(field, A.maps[m], sizeof(field));
  for (int i = 0; i < MAPN * MAPN; i++) strength[i] = A.types[field[i]].strength;
  // the start: your base square (the type with bit 3 of its first flags: 20)
  int start = -1;
  for (int i = 0; i < MAPN * MAPN && start < 0; i++)
    if (A.types[field[i]].flagsA & 0x08) start = i;
  if (start < 0)
    for (int i = MAPN * MAPN / 2; i < MAPN * MAPN && start < 0; i++)
      if (!solidSquare(i / MAPN, i % MAPN)) start = i;
  tank = Tank();
  tank.x = (start / MAPN) * TILE + TILE / 2.0f;
  tank.y = (start % MAPN) * TILE + TILE / 2.0f;
  tank.heading = PI;   // facing out of the garage (its door is to the south)
  findGates();
  startSquare = start;
  tanksLeft = TANKS;
  tanksLost = 0;
  // the enemy's turrets: every square whose type has bit 3 of its second flags
  // (the corner posts of the enemy's walls, its bunkers)
  nTurrets = 0;
  for (int i = 0; i < MAPN * MAPN && nTurrets < 512; i++)
    if (A.types[field[i]].flagsB & 0x08)
      turrets[nTurrets++] = { i, (int)(rnd() * 8) * TWO_PI / 8, 1 + rnd() * 2, 0, true };
  // the enemy's headquarters: one of its big bunkers (type 92), at random; the rest are decoys
  {
    int cands[64], n = 0;
    for (int i = 0; i < MAPN * MAPN && n < 64; i++)
      if (field[i] == 92) cands[n++] = i;
    memset(decoy, 0, sizeof(decoy));
    hqSquare = n ? cands[(int)(rnd() * n) % n] : -1;
    for (int k = 0; k < n; k++)
      if (cands[k] != hqSquare) decoy[cands[k]] = 1;
    if (hqSquare >= 0) {
      field[hqSquare] = 69;
      strength[hqSquare] = A.types[69].strength;
    }
    flagState = FLAG_HIDDEN;
    repairT = 0;
  }
  nOurs = 0;
  for (int i = 0; i < MAPN * MAPN && nOurs < 256; i++)
    if (A.types[field[i]].flagsB & 0x04) ours[nOurs++] = { i, (int)(rnd() * 8) * TWO_PI / 8, 0, 0, true };
  // plain walls (solid, not destructible, but with a damaged form) can be shot open
  for (int i = 0; i < MAPN * MAPN; i++) {
    const TileType &t = A.types[field[i]];
    if ((t.block & 0x78) && !t.destructible && t.becomes != field[i] && t.becomes < 93) strength[i] = WALL_STRENGTH;
  }
  for (Missile &mm : missiles) mm.live = false;
  for (Heli &h : helis) h.live = false;
  heliTimer = HELI_FIRST;
  for (Man &mn : men) mn.live = false;
  unloadT = 0;
  extraT = 0;
  fuelT = 0;
  mines = MINES;
  {   // where the red cross is in the first-aid station's picture
    const Pic &p = A.tiles[A.types[24].pic];
    float sxs = 0, sys = 0;
    int n = 0;
    for (int y = 0; y < 128; y++)
      for (int x = 0; x < 128; x++)
        if (p.px[y * 128 + x] == A.pal[8]) { sxs += x; sys += y; n++; }
    if (n) { aidX = sxs / n / 2; aidY = sys / n / 2; }
  }
  nSplats = aboard = rescued = menLost = menKilled = 0;
  helisDown = 0;
  for (Shell &s : shells) s.live = false;
  for (Boom &b : booms) b.live = false;
  score = destroyed = 0;
  clockT = 0;
  mapOpen = false;
  state = ST_PLAY;
}

static void releaseMen(int tx, int ty, int n, int side);   // (the men, below)

// A shell (or anything) hits square tx,ty
static void hitSquare(int tx, int ty, float hx, float hy) {
  if (tx < 0 || ty < 0 || tx >= MAPN || ty >= MAPN) return;
  int i = sq(tx, ty);
  const TileType &t = A.types[field[i]];
  bool plainWall = (t.block & 0x78) && !t.destructible && t.becomes != field[i] && t.becomes < 93;
  if (!t.destructible && !plainWall) {
    addBoom(hx, hy, false);
    playSound(SND_DRIP, 0.5f);
    return;
  }
  strength[i] -= SHELL_DAMAGE;
  if (strength[i] > 0) {
    addBoom(hx, hy, false);
    playSound(SND_DRIP, 0.6f);
    return;
  }
  bool wasTurret = t.flagsB & 0x08;
  bool wasHQ = (t.flagsB & 0x20) && !decoy[i];   // the headquarters' inside (type 81)
  uint8_t was = field[i];
  field[i] = t.becomes < 93 ? t.becomes : 0;
  if (was == 92) field[i] = 81;                   // a decoy (the original's 92 becomes nothing): as the real one
  strength[i] = A.types[field[i]].strength;
  if (was == 92) strength[i] = DECOY_STRENGTH;
  if (wasHQ && flagState == FLAG_HIDDEN) {        // your flag, in the ruin
    flagState = FLAG_LOOSE;
    flagX = tx * TILE + TILE / 2.0f;
    flagY = ty * TILE + TILE / 2.0f;
    score += 1000;
  }
  if (plainWall) {   // only damaged: the original's damaged wall, a few more hits to go
    addBoom(hx, hy, false);
    playSound(SND_BOOM2, 0.6f);
    return;
  }
  addBoom(tx * TILE + TILE / 2.0f, ty * TILE + TILE / 2.0f, true);
  playSound(SND_BOOM2);
  // the men inside run out: bit 7 of the side byte yours, bit 6 the enemy's; how many: its low 4 bits
  if (t.side & 0xc0) releaseMen(tx, ty, t.side & 0x0f, (t.side & 0x80) ? 0 : 1);
  if (was == 34 && tank.alive) {   // one of the enemy's fuel depots: yours now
    tank.fuel = 100;
    fuelT = 2.5f;
  }
  destroyed++;
  score += wasTurret ? 250 : (t.side & 0x20) ? 200 : 100;
}

// ============================================================ the tank
static void moveTank(float dt, uint32_t held, float sx, float sy) {
  float want = 0;   // speed asked for
  // the D-pad: as the stick, 8 ways
  if (held & (BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN)) {
    sx = (held & BTN_RIGHT ? 1.0f : 0.0f) - (held & BTN_LEFT ? 1.0f : 0.0f);
    sy = (held & BTN_DOWN ? 1.0f : 0.0f) - (held & BTN_UP ? 1.0f : 0.0f);
  }
  float mag = sqrtf(sx * sx + sy * sy);
  if (held & BTN_REVERSE) {
    // Square: straight back, the way it faces
    want = -TANK_REVERSE;
  } else if (mag > 0.35f) {
    // the stick: point it where to go; the tank turns that way and drives once it faces it
    float target = atan2f(sx, -sy);
    float diff = wrapAngle(target - tank.heading);
    float turn = TANK_TURN * dt;
    tank.heading += diff > turn ? turn : (diff < -turn ? -turn : diff);
    if (fabsf(diff) < 0.8f) want = TANK_SPEED * (mag > 1 ? 1 : mag) * cosf(diff);
  }
  tank.heading = wrapAngle(tank.heading);
  if (tank.heading < 0) tank.heading += TWO_PI;
  if (tank.fuel <= 0) want *= FUEL_EMPTY_SPEED;   // no fuel: it only crawls
  float dv = TANK_ACCEL * dt;
  tank.speed += want > tank.speed + dv ? dv : (want < tank.speed - dv ? -dv : want - tank.speed);

  // it moves along its heading; a wall stops it on that axis (so it slides along)
  // it moves the way its picture faces: the nearest of the original's 16 headings
  float along = frameOf(tank.heading) * TWO_PI / 16;
  float dx = sinf(along) * tank.speed * dt, dy = -cosf(along) * tank.speed * dt;
  auto clear = [](float x, float y) {   // eight points round its body
    const float r = TANK_RADIUS, d = TANK_RADIUS * 0.7071f;
    return !solidAt(x - r, y) && !solidAt(x + r, y) && !solidAt(x, y - r) && !solidAt(x, y + r) &&
           !solidAt(x - d, y - d) && !solidAt(x + d, y - d) && !solidAt(x - d, y + d) && !solidAt(x + d, y + d);
  };
  float ox = tank.x, oy = tank.y;
  if (clear(tank.x + dx, tank.y)) tank.x += dx;
  else tank.speed *= 0.5f;
  if (clear(tank.x, tank.y + dy)) tank.y += dy;
  else tank.speed *= 0.5f;
  // the fuel it used, for how far it went
  float moved = sqrtf((tank.x - ox) * (tank.x - ox) + (tank.y - oy) * (tank.y - oy));
  tank.fuel -= moved / FUEL_RANGE * 100;
  if (tank.fuel < 0) tank.fuel = 0;
}

static void fire() {
  for (Shell &s : shells)
    if (!s.live) {
      int f = frameOf(tank.heading);
      float a = f * TWO_PI / 16;   // shells fly along the barrel's frame, as the sprites show
      s.live = true;
      s.dx = sinf(a) * SHELL_SPEED;
      s.dy = -cosf(a) * SHELL_SPEED;
      s.x = tank.x + sinf(a) * 14;
      s.y = tank.y - cosf(a) * 14;
      s.left = SHELL_RANGE / SHELL_SPEED;
      s.frame = f;
      playSound(SND_TANKBLAST, 0.8f);
      return;
    }
}

static bool shellHitsHeli(float x, float y);   // (the helicopters, below)

static void moveShells(float dt) {
  for (Shell &s : shells) {
    if (!s.live) continue;
    // in small steps, so a fast shell can't jump a wall
    int steps = 4;
    for (int k = 0; k < steps && s.live; k++) {
      s.x += s.dx * dt / steps;
      s.y += s.dy * dt / steps;
      if (shellHitsHeli(s.x, s.y)) {
        s.live = false;
      } else if (solidAt(s.x, s.y, false)) {   // (shells fly over trees)
        hitSquare((int)floorf(s.x / TILE), (int)floorf(s.y / TILE), s.x, s.y);
        s.live = false;
      }
    }
    s.left -= dt;
    if (s.live && s.left <= 0) {
      s.live = false;
      addBoom(s.x, s.y, false);
    }
  }
}

// ============================================================ the turrets
// A turret's gun turns on the middle of its square. The original draws each of
// its 8 headings with its top left here in the square (its tables at $eaa, $eba)
static const int GUN_DX[8] = { 25, 26, 26, 27, 27, 19, 17, 19 };
static const int GUN_DY[8] = { 16, 20, 27, 27, 27, 27, 27, 20 };
static inline bool isTurret(int i) { return (A.types[field[i]].flagsB & 0x08) != 0; }

static bool godMode = false;   // (the PC test build)
static void tankHit(int damage, float x, float y) {
  if (!tank.alive) return;
  if (godMode) damage = 0;
  tank.armour -= damage;
  addBoom(x, y, false);
  playSound(SND_BOOM2, 0.5f);
  if (tank.armour <= 0) {
    tank.armour = 0;
    tank.alive = false;
    tank.down = 0;
    tank.speed = 0;
    tanksLeft--;
    menLost += aboard;   // the men aboard go with it
    aboard = 0;
    if (flagState == FLAG_CARRIED) {   // it drops where the tank was
      flagState = FLAG_LOOSE;
      flagX = tank.x;
      flagY = tank.y;
    }
    tanksLost++;
    addBoom(tank.x, tank.y, true);
    addBoom(tank.x + 10, tank.y - 8, true);
    playSound(SND_BOOM2);
  }
}

static void fireMissile(float x, float y, float heading, int from, int owner) {
  for (Missile &mm : missiles)
    if (!mm.live) {
      int f = roundi(heading / TWO_PI * 8) & 7;   // (the picture: the nearest of 8)
      float a = heading;                          // it flies where the gun points
      mm = { true, x + sinf(a) * 12, y - cosf(a) * 12, sinf(a) * MISSILE_SPEED, -cosf(a) * MISSILE_SPEED,
             MISSILE_RANGE / MISSILE_SPEED, f, from, owner, false };
      playSound(SND_TANKBLAST, 0.45f);
      return;
    }
}

static void updateTurrets(float dt) {
  for (int k = 0; k < nTurrets; k++) {
    Turret &t = turrets[k];
    if (!isTurret(t.i)) continue;   // destroyed
    float gx = (t.i / MAPN) * TILE + TILE / 2.0f, gy = (t.i % MAPN) * TILE + TILE / 2.0f;
    float dx = tank.x - gx, dy = tank.y - gy;
    if (t.loaded) t.reload -= dt;
    if (!tank.alive || fabsf(dx) > TURRET_SEE_X || fabsf(dy) > TURRET_SEE_Y) continue;
    // turn a step at a time towards your tank (sub_020fe); fire when it's lined up
    int want = roundi(atan2f(dx, -dy) / TWO_PI * 8) & 7, have = roundi(t.heading / TWO_PI * 8) & 7;
    t.step -= dt;
    if (want != have && t.step <= 0) {
      int d = (want - have) & 7;
      have = (have + (d <= 4 ? 1 : 7)) & 7;
      t.heading = have * TWO_PI / 8;
      t.step = TURRET_STEP;
    }
    if (want == have && t.loaded && t.reload <= 0) {
      fireMissile(gx, gy, atan2f(dx, -dy), t.i, k);
      t.loaded = false;
    }
  }
}

static void moveMissiles(float dt) {
  for (Missile &mm : missiles) {
    if (!mm.live) continue;
    for (int k = 0; k < 3 && mm.live; k++) {
      mm.x += mm.dx * dt / 3;
      mm.y += mm.dy * dt / 3;
      float dx = mm.x - tank.x, dy = mm.y - tank.y;
      if (tank.alive && dx * dx + dy * dy < 12 * 12) {
        tankHit(mm.air ? HELI_MISSILE_DAMAGE : MISSILE_DAMAGE, mm.x, mm.y);
        mm.live = false;
      } else if (!mm.air && solidAt(mm.x, mm.y, false) && sq((int)floorf(mm.x / TILE), (int)floorf(mm.y / TILE)) != mm.from) {
        // the enemy's missiles wear down what they hit, as your shells do
        addBoom(mm.x, mm.y, false);
        mm.live = false;
      }
    }
    if (!mm.live && mm.owner >= 0) {   // gone: its turret may fire again
      turrets[mm.owner].loaded = true;
      turrets[mm.owner].reload = TURRET_RELOAD * (0.6f + 0.8f * rnd());
      mm.owner = -1;
      continue;
    }
    mm.left -= dt;
    if (mm.live && mm.left <= 0) {
      mm.live = false;
      addBoom(mm.x, mm.y, false);
      if (mm.owner >= 0) {
        turrets[mm.owner].loaded = true;
        turrets[mm.owner].reload = TURRET_RELOAD * (0.6f + 0.8f * rnd());
        mm.owner = -1;
      }
    }
  }
}

// ============================================================ the helicopters
static void spawnHeli() {
  for (Heli &h : helis)
    if (!h.live) {
      // from one of the enemy's helipads (bit 1 of the second flags: type 37) out
      // of sight, or else from beyond the view, on a random side of your tank
      int pads[256], n = 0;
      for (int i = 0; i < MAPN * MAPN && n < 256; i++) {
        if (!(A.types[field[i]].flagsB & 0x02)) continue;
        float px = (i / MAPN) * TILE + TILE / 2.0f, py = (i % MAPN) * TILE + TILE / 2.0f;
        float dx = px - tank.x, dy = py - tank.y;
        if (dx * dx + dy * dy > 560.0f * 560 && dx * dx + dy * dy < 1600.0f * 1600) pads[n++] = i;
      }
      float a = rnd() * TWO_PI, d = 750;
      h.x = tank.x + sinf(a) * d;
      h.y = tank.y - cosf(a) * d;
      if (n) {
        int i = pads[(int)(rnd() * n) % n];
        h.x = (i / MAPN) * TILE + TILE / 2.0f;
        h.y = (i % MAPN) * TILE + TILE / 2.0f;
      }
      h.x = h.x < -100 ? -100 : (h.x > WORLD + 100 ? WORLD + 100 : h.x);
      h.y = h.y < -100 ? -100 : (h.y > WORLD + 100 ? WORLD + 100 : h.y);
      h.heading = atan2f(tank.x - h.x, -(tank.y - h.y));
      h.reload = HELI_RELOAD;
      h.sound = 0;
      h.hits = 0;
      h.side = rnd() < 0.5f ? 1 : -1;
      h.ring = HELI_CIRCLE * (0.7f + 0.7f * rnd());
      h.live = true;
      return;
    }
}

static void heliMissile(const Heli &h, float aim) {
  for (Missile &mm : missiles)
    if (!mm.live) {
      int f = roundi(aim / TWO_PI * 8) & 7;   // (the picture: the nearest of its 8)
      float a = aim;                          // straight at your tank
      mm = { true, h.x + sinf(a) * 22, h.y - cosf(a) * 22, sinf(a) * HELI_MISSILE_SPEED, -cosf(a) * HELI_MISSILE_SPEED,
             MISSILE_RANGE / HELI_MISSILE_SPEED, f, -1, -1, true };
      playSound(SND_TANKBLAST, 0.4f);
      return;
    }
}

static void updateHelis(float dt) {
  int live = 0;
  for (Heli &h : helis) live += h.live;
  heliTimer -= dt;
  if (heliTimer <= 0 && tank.alive) {
    if (live < HELI_MAX) spawnHeli();
    heliTimer = HELI_EVERY + rnd() * HELI_EVERY_MORE;
  }
  for (Heli &h : helis) {
    if (!h.live) continue;
    // where it heads: a point on a circle round your tank, ahead of it on its way round
    float dx = h.x - tank.x, dy = h.y - tank.y, dist = sqrtf(dx * dx + dy * dy) + 0.01f;
    float tx, ty;
    if (dist > h.ring * 1.6f) {
      tx = tank.x;
      ty = tank.y;
    } else {
      float a = atan2f(dy, dx) + h.side * 0.7f;
      tx = tank.x + cosf(a) * h.ring;
      ty = tank.y + sinf(a) * h.ring;
    }
    float want = atan2f(tx - h.x, -(ty - h.y));
    float diff = wrapAngle(want - h.heading), turn = HELI_TURN * dt;
    h.heading = wrapAngle(h.heading + (diff > turn ? turn : (diff < -turn ? -turn : diff)));
    h.x += sinf(h.heading) * HELI_SPEED * dt;
    h.y -= cosf(h.heading) * HELI_SPEED * dt;
    // fire when it faces your tank, near enough
    h.reload -= dt;
    float toTank = atan2f(tank.x - h.x, -(tank.y - h.y));
    // fire when your tank is ahead of it (not behind), near enough
    if (tank.alive && dist < 330 && h.reload <= 0 && fabsf(wrapAngle(toTank - h.heading)) < 1.75f) {
      heliMissile(h, toTank);
      h.reload = HELI_RELOAD * (0.8f + 0.5f * rnd());
    }
    // its rotor, louder near
    h.sound -= dt;
    if (h.sound <= 0 && dist < 700) {
      playSound(SND_HELI, 0.25f + 0.5f * (1 - dist / 700));
      h.sound = 0.16f;
    }
  }
}

// A shell reaching a helicopter
static bool shellHitsHeli(float x, float y) {
  for (Heli &h : helis) {
    if (!h.live) continue;
    float dx = x - h.x, dy = y - h.y;
    if (dx * dx + dy * dy > 21 * 21) continue;
    addBoom(x, y, false);
    if (++h.hits >= HELI_HITS) {
      h.live = false;
      addBoom(h.x, h.y, true);
      addBoom(h.x - 8, h.y + 6, true);
      playSound(SND_BOOM2);
      score += 300;
      helisDown++;
      destroyed++;
    } else {
      playSound(SND_DRIP, 0.6f);
    }
    return true;
  }
  return false;
}

// ============================================================ the men
static void manHeading(Man &m, float a) {
  m.dx = sinf(a);
  m.dy = -cosf(a);
}

static void releaseMen(int tx, int ty, int n, int side) {
  for (int k = 0; k < n; k++)
    for (Man &m : men)
      if (!m.live) {
        m.live = true;
        m.side = side;
        m.inside = false;
        m.x = tx * TILE + 16 + rnd() * 32;
        m.y = ty * TILE + 16 + rnd() * 32;
        manHeading(m, rnd() * TWO_PI);
        m.turn = 0.5f + rnd() * 2;
        break;
      }
}

static void splatAt(float x, float y) {
  splats[nSplats % 96] = { x, y };
  nSplats++;
  playSound(SND_SPLAT);
}

static void updateMen(float dt) {
  bool moving = fabsf(tank.speed) > 8;
  for (Man &m : men) {
    if (!m.live) continue;
    if (m.inside) {   // into the first-aid station: rescued when he gets there
      float gx = m.gx - m.x, gy = m.gy - m.y, gd = sqrtf(gx * gx + gy * gy) + 0.01f;
      m.dx = gx / gd;
      m.dy = gy / gd;
      float step = MAN_SPEED * 1.5f * dt;
      if (gd <= step + 1) {
        m.live = false;
        rescued++;
        score += 200;
        playSound(SND_DRIP, 0.5f);
        if (rescued % MEN_PER_MINE == 0) mines++;
        if (rescued % MEN_PER_TANK == 0) {   // an extra tank
          tanksLeft++;
          extraT = 3;
          playSound(SND_BOOM2, 0.4f);
        }
      } else {
        m.x += m.dx * step;
        m.y += m.dy * step;
      }
      continue;
    }
    float tx = tank.x - m.x, ty = tank.y - m.y, d = sqrtf(tx * tx + ty * ty) + 0.01f;
    float speed = MAN_SPEED;
    m.turn -= dt;
    if (tank.alive && m.side == 1 && d < 120) {         // the enemy's run from your tank
      m.dx = -tx / d;
      m.dy = -ty / d;
      speed *= 1.5f;
    } else if (tank.alive && m.side == 0 && d < 160 && !moving) {   // yours run to it
      m.dx = tx / d;
      m.dy = ty / d;
      speed *= 1.5f;
    } else if (m.turn <= 0) {                           // or wander
      manHeading(m, rnd() * TWO_PI);
      m.turn = 1 + rnd() * 2.5f;
    }
    float nx = m.x + m.dx * speed * dt, ny = m.y + m.dy * speed * dt;
    if (!solidAt(nx, ny, false) && nx > 2 && ny > 2 && nx < WORLD - 2 && ny < WORLD - 2) {
      m.x = nx;
      m.y = ny;
    } else {
      manHeading(m, rnd() * TWO_PI);
      m.turn = 0.4f;
    }
    // your tank: the enemy's men are run over; yours too if it's moving - stopped, it takes them aboard
    if (!tank.alive || d > 11) continue;
    if (m.side == 0 && !moving && aboard < TANK_ROOM) {
      aboard++;
      m.live = false;
      playSound(SND_DRIP, 0.7f);
    } else if (m.side == 1 || moving) {
      splatAt(m.x, m.y);
      m.live = false;
      if (m.side == 1) {
        menKilled++;
        score += 50;
      } else {
        menLost++;
      }
    }
  }
  // stopped on the square to the right of your first-aid station (type 24), the men get out
  // one at a time and run in
  int sx = (int)floorf(tank.x / TILE) - 1, sy = (int)floorf(tank.y / TILE);
  bool atAid = tank.alive && aboard > 0 && fabsf(tank.speed) < 2 && sx >= 0 && sy >= 0 && sx < MAPN &&
               sy < MAPN && field[sq(sx, sy)] == 24;
  if (!atAid) {
    unloadT = 0;
    return;
  }
  unloadT -= dt;
  if (unloadT > 0) return;
  unloadT = UNLOAD_EVERY;
  for (Man &m : men)
    if (!m.live) {
      m.live = true;
      m.side = 0;
      m.inside = true;
      m.x = tank.x - 8;
      m.y = tank.y + (rnd() - 0.5f) * 8;
      m.gx = sx * TILE + aidX;
      m.gy = sy * TILE + aidY;
      m.turn = 0;
      aboard--;
      break;
    }
}

// After a tank is lost: the next comes out of the garage, or it's over
// The flag: picked up by driving over it; brought home to your base, the battle is won.
// Without it, your base repairs your tank.
static void updateFlag(float dt) {
  if (!tank.alive) return;
  if (flagState == FLAG_LOOSE) {
    float dx = tank.x - flagX, dy = tank.y - flagY;
    if (dx * dx + dy * dy < 20 * 20) {
      flagState = FLAG_CARRIED;
      playSound(SND_DRIP);
    }
  }
  int tx = (int)floorf(tank.x / TILE), ty = (int)floorf(tank.y / TILE);
  bool home = tx >= 0 && ty >= 0 && tx < MAPN && ty < MAPN && (typeAt(tx, ty).flagsA & 0x08);
  if (!home) return;
  if (flagState == FLAG_CARRIED) {
    state = ST_WON;
    overT = 0;
    score += 5000 + 1000 * tanksLeft;
    return;
  }
  tank.fuel = tank.fuel + REFUEL_RATE * dt > 100 ? 100 : tank.fuel + REFUEL_RATE * dt;
  repairT += dt;
  while (repairT >= REPAIR_EVERY) {
    repairT -= REPAIR_EVERY;
    if (tank.armour < 100) tank.armour = tank.armour + 5 > 100 ? 100 : tank.armour + 5;
  }
}

static void updateRespawn(float dt) {
  if (tank.alive) return;
  tank.down += dt;
  if (tank.down < RESPAWN) return;
  if (tanksLeft <= 0) {
    state = ST_OVER;
    overT = 0;
    return;
  }
  tank.x = (startSquare / MAPN) * TILE + TILE / 2.0f;
  tank.y = (startSquare % MAPN) * TILE + TILE / 2.0f;
  tank.heading = PI;
  tank.speed = 0;
  tank.armour = 100;
  tank.fuel = 100;
  tank.alive = true;
  tank.reload = 0.5f;
}

// ============================================================ drawing
static float camX = 0, camY = 0;   // the view's top left, Amiga pixels

// A picture at 2x, centred on world point (x, y)
static void drawPic2x(const Pic &p, float wx, float wy) {
  int ox = roundi((wx - camX) * S) - p.w * S / 2, oy = roundi((wy - camY) * S) - p.h * S / 2;
  for (int y = 0; y < p.h; y++) {
    int sy = oy + y * S;
    if (sy + S <= 0 || sy >= FB_H) continue;
    for (int x = 0; x < p.w; x++) {
      uint32_t c = p.px[y * p.w + x];
      if (!(c >> 24)) continue;
      int sx = ox + x * S;
      if (sx + S <= 0 || sx >= FB_W) continue;
      for (int j = 0; j < S; j++) {
        int yy = sy + j;
        if (yy < 0 || yy >= FB_H) continue;
        uint32_t *row = g_frame + (size_t)yy * FB_W;
        for (int i = 0; i < S; i++) {
          int xx = sx + i;
          if (xx >= 0 && xx < FB_W) row[xx] = c;
        }
      }
    }
  }
}

// A turret's gun on its square, where the original puts each heading's picture
static void drawGun(int i, float heading) {
  int f = roundi(heading / TWO_PI * 8) & 7;
  const Pic &g = A.turret.pic[f];
  float x = (i / MAPN) * TILE + GUN_DX[f] + g.w / 2.0f, y = (i % MAPN) * TILE + GUN_DY[f] + g.h / 2.0f;
  drawPic2x(g, x, y);
}

static void drawField() {
  int x0 = (int)floorf(camX / TILE), y0 = (int)floorf(camY / TILE);
  int x1 = (int)floorf((camX + VIEW_W) / TILE), y1 = (int)floorf((camY + VIEW_H) / TILE);
  for (int tx = x0; tx <= x1; tx++)
    for (int ty = y0; ty <= y1; ty++) {
      int sx = roundi((tx * TILE - camX) * S), sy = roundi((ty * TILE - camY) * S);
      if (tx < 0 || ty < 0 || tx >= MAPN || ty >= MAPN) {
        pxRect(sx, sy, sx + TILE * S - 1, sy + TILE * S - 1, rgba(0, 0, 0));
        continue;
      }
      const TileType &tt = typeAt(tx, ty);
      const Pic &t = tt.enemy ? A.tilesEnemy[tt.pic] : A.tiles[tt.pic];
      int cx0 = sx < 0 ? -sx : 0, cx1 = sx + 128 > FB_W ? FB_W - sx : 128;
      if (cx1 <= cx0) continue;
      for (int y = 0; y < 128; y++) {
        int yy = sy + y;
        if (yy < 0 || yy >= FB_H) continue;
        memcpy(g_frame + (size_t)yy * FB_W + sx + cx0, t.px + y * 128 + cx0, (size_t)(cx1 - cx0) * 4);
      }
    }
}

static void drawBooms(float dt) {
  for (Boom &b : booms) {
    if (!b.live) continue;
    b.t += dt;
    // the original's frames: two small puffs, then the big explosion (frames 3..11)
    int f;
    if (b.big) {
      f = 3 + (int)(b.t * 18);
      if (f > 11) { b.live = false; continue; }
    } else {
      f = (int)(b.t * 14);
      if (f > 1) { b.live = false; continue; }
    }
    if (f < A.boom.n) drawPic2x(A.boom.pic[f], b.x, b.y);
  }
}

// The map: each square its tile's average colour; walls and buildings darker
static void drawMap(int x0, int y0, int cell, float alpha) {
  pxBlend(x0 - 6, y0 - 6, x0 + MAPN * cell + 5, y0 + MAPN * cell + 5, 0, 0, 0, alpha);
  for (int tx = 0; tx < MAPN; tx++)
    for (int ty = 0; ty < MAPN; ty++) {
      const TileType &t = typeAt(tx, ty);
      uint32_t c = t.enemy ? A.tileAvgEnemy[t.pic] : A.tileAvg[t.pic];
      if (t.block & 0x78) c = t.destructible && t.strength >= 15 ? rgba(150, 80, 30) : rgba(40, 40, 46);
      pxRect(x0 + tx * cell, y0 + ty * cell, x0 + tx * cell + cell - 1, y0 + ty * cell + cell - 1, c);
    }
  // the view and the tank
  int vx0 = x0 + (int)(camX / TILE * cell), vy0 = y0 + (int)(camY / TILE * cell);
  int vx1 = x0 + (int)((camX + VIEW_W) / TILE * cell), vy1 = y0 + (int)((camY + VIEW_H) / TILE * cell);
  uint32_t w = rgba(255, 255, 255);
  pxRect(vx0, vy0, vx1, vy0, w);
  pxRect(vx0, vy1, vx1, vy1, w);
  pxRect(vx0, vy0, vx0, vy1, w);
  pxRect(vx1, vy0, vx1, vy1, w);
  float blink = fmodf(clockT, 0.6f) < 0.4f ? 1 : 0;
  if (blink > 0) {
    float tx = x0 + tank.x / TILE * cell, ty = y0 + tank.y / TILE * cell;
    pxCircle(tx, ty, cell < 6 ? 4.5f : 7, rgba(255, 230, 60));
  }
  for (Heli &h : helis)
    if (h.live) pxCircle(x0 + h.x / TILE * cell, y0 + h.y / TILE * cell, cell < 6 ? 3.5f : 6, rgba(255, 60, 40));
  // your base, and the flag once it's out
  float bx = x0 + ((startSquare / MAPN) + 0.5f) * cell, by = y0 + ((startSquare % MAPN) + 0.5f) * cell;
  pxRect((int)bx - (cell < 6 ? 4 : 7), (int)by - (cell < 6 ? 4 : 7), (int)bx + (cell < 6 ? 4 : 7), (int)by + (cell < 6 ? 4 : 7), rgba(80, 160, 255));
  if (flagState == FLAG_LOOSE && blink > 0)
    pxCircle(x0 + flagX / TILE * cell, y0 + flagY / TILE * cell, cell < 6 ? 4.5f : 8, rgba(255, 40, 40));
}

// A picture scaled by k (nearest pixel), its top left at sx, sy
static void drawPicK(const Pic &p, float sx, float sy, float k) {
  int w = (int)(p.w * k), h = (int)(p.h * k);
  for (int y = 0; y < h; y++) {
    int yy = (int)sy + y;
    if (yy < 0 || yy >= FB_H) continue;
    const uint32_t *src = p.px + (int)(y / k) * p.w;
    uint32_t *row = g_frame + (size_t)yy * FB_W;
    for (int x = 0; x < w; x++) {
      int xx = (int)sx + x;
      uint32_t c = src[(int)(x / k)];
      if (xx >= 0 && xx < FB_W && (c >> 24)) row[xx] = c;
    }
  }
}

// The original's panel coordinates (its one-player screen, 352 x 232) on the TV
static inline float panelX(float ax) { return PANEL_X0 + (ax - 240) * PANEL_K; }
static inline float panelY(float ay) { return ay * PANEL_K; }

// A number in the original's digits (gr/numbers 0..9), right-aligned to Amiga x right
static void panelDigits(int value, int digits, float right, float ay) {
  if (A.numbers.n < 10) return;
  for (int k = 0; k < digits; k++) {
    int d = value % 10;
    value /= 10;
    drawPicK(A.numbers.pic[d], panelX(right - (k + 1) * 9 + 2), panelY(ay), PANEL_K);
  }
}

static void drawPanel() {
  // the right of the one-player screen: the camouflage and the panel
  const Pic &p = A.onePlayer;
  for (int y = 0; y < FB_H; y++) {
    const uint32_t *src = p.px + (int)(y / PANEL_K) * p.w;
    uint32_t *row = g_frame + (size_t)y * FB_W;
    for (int x = PANEL_X0; x < FB_W; x++) row[x] = src[240 + (int)((x - PANEL_X0) / PANEL_K)];
  }
  // the score (5 digits, 6 when it needs them), your men aboard, your tanks
  panelDigits(score, score > 99999 ? 6 : 5, 313, 45);
  panelDigits(aboard, 2, 302, 66);
  int spare = tanksLeft - (tank.alive ? 1 : 0);
  panelDigits(spare < 0 ? 0 : spare > 9 ? 9 : spare, 1, 302, 100);
  // fuel: the bar in its box
  float fw = (313 - 266) * tank.fuel / 100;
  uint32_t fcol = tank.fuel > 25 ? rgba(190, 220, 255) : (fmodf(clockT, 0.6f) < 0.35f ? rgba(255, 70, 50) : rgba(120, 30, 20));
  if (fw > 0) pxRect((int)panelX(267), (int)panelY(139), (int)panelX(267 + fw) - 1, (int)panelY(146) - 1, fcol);
  // armour (not on the original's panel): a bar in the gap between the men and the tanks
  int ax0 = (int)panelX(266), ax1 = (int)panelX(313), ay0 = (int)panelY(80), ay1 = (int)panelY(90);
  pxRect(ax0 - 4, ay0 - 4, ax1 + 4, ay1 + 4, rgba(40, 30, 20));
  pxRect(ax0 - 2, ay0 - 2, ax1 + 2, ay1 + 2, rgba(200, 140, 70));
  pxRect(ax0, ay0, ax1, ay1, rgba(0, 0, 0));
  int aw = (ax1 - ax0) * tank.armour / 100;
  uint32_t acol = tank.armour > 50 ? rgba(110, 220, 60) : tank.armour > 25 ? rgba(240, 200, 40) : rgba(230, 60, 40);
  if (aw > 0) pxRect(ax0, ay0, ax0 + aw, ay1, acol);
  // the radar: the battlefield in its box
  int box = (int)((313 - 266) * PANEL_K), cell = 3;
  drawMap((int)panelX(266) + (box - MAPN * cell) / 2, (int)panelY(158) + (box - MAPN * cell) / 2, cell, 1.0f);
}

static void drawHud() {
  drawPanel();
  // over the battle view: the battlefield, what to do, the messages
  uint16_t ink = rgb565(255, 210, 120), dim = rgb565(170, 140, 90);
  char buf[96];
  pxBlend(0, 0, PLAY_W - 1, 44, 0, 0, 0, 0.5f);
  snprintf(buf, sizeof(buf), "BATTLEFIELD %d-%d", mapNo < 5 ? 1 : mapNo < 10 ? 2 : 3, mapNo < 10 ? mapNo % 5 + 1 : 1);
  drawVectorText(buf, 90, 15, 1.4f, ink, false);
  const char *goal = flagState == FLAG_HIDDEN ? "FIND THE ENEMY HQ AND CAPTURE THE ENEMY FLAG"
                     : flagState == FLAG_LOOSE ? "THE ENEMY FLAG IS LOOSE - PICK IT UP"
                                               : "BRING THE FLAG HOME TO YOUR BASE";
  drawVectorText(goal, PLAY_CX + 60, 15, 1.4f, flagState == FLAG_CARRIED ? rgb565(255, 230, 90) : rgb565(230, 230, 230), false);
  drawVectorText(VERSION, 30, 708, 1.1f, dim, false);
  if (tank.alive && tank.fuel < 15 && fmodf(clockT, 0.8f) < 0.5f)
    drawVectorText(tank.fuel <= 0 ? "OUT OF FUEL - BACK TO YOUR GARAGE" : "LOW FUEL", PLAY_CX, 200, 2.2f, rgb565(255, 90, 60), true);
  if (fuelT > 0) {
    fuelT -= 1 / 60.0f;
    drawVectorText("REFUELLED", PLAY_CX, 200, 2.4f, rgb565(120, 190, 255), true);
  }
  if (extraT > 0) {
    extraT -= 1 / 60.0f;
    if (fmodf(extraT, 0.5f) < 0.35f) drawVectorText("EXTRA TANK!", PLAY_CX, 160, 3.0f, rgb565(255, 230, 90), true);
  }
  if (aboard) {
    snprintf(buf, sizeof(buf), "MEN ABOARD %d/%d - STOP RIGHT OF THE FIRST-AID STATION", aboard, TANK_ROOM);
    pxBlend(0, FB_H - 64, PLAY_W - 1, FB_H - 20, 0, 0, 0, 0.45f);
    drawVectorText(buf, PLAY_CX, 694, 1.4f, rgb565(255, 230, 90), false);
  }
}

static void drawOver(float dt) {
  overT += dt;
  pxRect(0, 0, FB_W - 1, FB_H - 1, rgba(0, 0, 0));
  if (state == ST_WON) {   // (the original's words)
    drawVectorText("YOU HAVE WON!!!", 640, 160, 4.0f, rgb565(255, 220, 90), true);
    drawVectorText("AND SUCH A GLORIOUS BATTLE IT WAS TOO...", 640, 240, 1.8f, rgb565(230, 230, 230), false);
  } else {
    drawVectorText("YOUR TANKS ARE ALL GONE", 640, 220, 3.6f, rgb565(230, 80, 50), true);
  }
  drawVectorText("***** FINAL REPORT *****", 640, 320, 2.2f, rgb565(255, 210, 120), true);
  char buf[64];
  snprintf(buf, sizeof(buf), "SCORE %d", score);
  drawVectorText(buf, 640, 390, 2.2f, rgb565(255, 255, 255), true);
  snprintf(buf, sizeof(buf), "INFLICTED: %d DESTROYED  HELI %d  MEN %d", destroyed, helisDown, menKilled);
  drawVectorText(buf, 640, 440, 1.8f, rgb565(220, 220, 220), false);
  snprintf(buf, sizeof(buf), "RESCUED: %d MEN", rescued);
  drawVectorText(buf, 640, 480, 1.8f, rgb565(220, 220, 220), false);
  snprintf(buf, sizeof(buf), "CASUALTIES: TANKS %d  MEN %d", tanksLost, menLost);
  drawVectorText(buf, 640, 520, 1.8f, rgb565(220, 220, 220), false);
  if (overT > 1.0f && fmodf(overT, 1.0f) < 0.7f) drawVectorText("PRESS X", 640, 600, 2.4f, rgb565(255, 220, 120), true);
}

static void drawTitle(float dt) {
  titleT += dt;
  pxRect(0, 0, FB_W - 1, FB_H - 1, rgba(0, 0, 0));
  if (!A.ok) {
    drawVectorText("FIREPOWER", 640, 250, 6, rgb565(220, 120, 60), true);
    drawVectorText("THE GAME'S FILES ARE MISSING", 640, 360, 2.2f, rgb565(255, 255, 255), true);
    drawVectorText(A.error, 640, 410, 1.8f, rgb565(255, 120, 120), false);
    drawVectorText("RUN TOOLS/EXTRACT_ASSETS.PY WITH YOUR FIREPOWER.ADF, THEN BUILD AGAIN", 640, 470, 1.4f,
                   rgb565(200, 200, 200), false);
    drawVectorText("(OR COPY ASSETS/FP OVER FTP INTO THE APP'S ASSETS FOLDER)", 640, 505, 1.4f, rgb565(200, 200, 200), false);
    return;
  }
  // the original title picture at 4x
  const Pic &t = A.title;
  int k = 4, ox = (FB_W - t.w * k) / 2, oy = 30;
  for (int y = 0; y < t.h * k; y++) {
    uint32_t *row = g_frame + (size_t)(oy + y) * FB_W + ox;
    const uint32_t *src = t.px + (y / k) * t.w;
    for (int x = 0; x < t.w * k; x++) row[x] = src[x / k];
  }
  if (fmodf(titleT, 1.0f) < 0.7f) drawVectorText("PRESS X TO START", 640, 655, 2.6f, rgb565(255, 220, 120), true);
  char buf[64];
  if (mapChoice < 0)
    snprintf(buf, sizeof(buf), "BATTLEFIELD: ANY OF THE FIVE (TRIANGLE TO CHOOSE)");
  else
    snprintf(buf, sizeof(buf), "BATTLEFIELD %d-%d (TRIANGLE TO CHANGE)", mapChoice < 5 ? 1 : mapChoice < 10 ? 2 : 3,
             mapChoice < 10 ? mapChoice % 5 + 1 : 1);
  drawVectorText(buf, 640, 692, 1.5f, rgb565(170, 170, 170), false);
  drawVectorText(VERSION, 1250, 708, 1.2f, rgb565(110, 110, 110), false);
}

// What the tank can be under, drawn again over it: the roof of your base and
// the enemy's (bits 3 and 2 of the first flags), and trees' leaves
static void drawRoofs() {
  int x0 = (int)floorf((tank.x - 16) / TILE), x1 = (int)floorf((tank.x + 16) / TILE);
  int y0 = (int)floorf((tank.y - 16) / TILE), y1 = (int)floorf((tank.y + 16) / TILE);
  for (int tx = x0; tx <= x1; tx++)
    for (int ty = y0; ty <= y1; ty++) {
      if (tx < 0 || ty < 0 || tx >= MAPN || ty >= MAPN) continue;
      const TileType &t = typeAt(tx, ty);
      const uint8_t *solid;
      if (t.flagsA & 0x0c) solid = A.tileSolid[t.pic];          // a roof
      else if (A.nTrees[t.pic]) solid = A.tileLeaves[t.pic];     // a tree's leaves
      else continue;
      const Pic &pic = t.enemy ? A.tilesEnemy[t.pic] : A.tiles[t.pic];
      int sx = roundi((tx * TILE - camX) * S), sy = roundi((ty * TILE - camY) * S);
      for (int y = 0; y < 128; y++) {
        int yy = sy + y;
        if (yy < 0 || yy >= FB_H) continue;
        uint32_t *row = g_frame + (size_t)yy * FB_W;
        for (int x = 0; x < 128; x++) {
          int xx = sx + x;
          if (xx < 0 || xx >= FB_W || !solid[(y >> 1) * TILE + (x >> 1)]) continue;
          row[xx] = pic.px[y * 128 + x];
        }
      }
    }
}

// ============================================================ the frame
void gameInit(uint32_t seed, const char *dir) {
  rng = seed | 1;
  gfxInit();
  assetsLoad(dir);
  state = ST_TITLE;
}

void gameFrame(float dt, uint32_t held, uint32_t pressed, float sx, float sy) {
  if (dt > 0.1f) dt = 0.1f;
  {   // the music for where we are: the battle's in a battle, the title's otherwise (each from the top)
    int want = state == ST_PLAY ? 1 : 0;
    if (want != tuneWanted.load(std::memory_order_relaxed)) {
      tuneFromStart[want].store(true);
      tuneWanted.store(want, std::memory_order_relaxed);
    }
  }
  if (state == ST_TITLE) {
    if (A.ok && (pressed & BTN_NEXT)) mapChoice = mapChoice >= 10 ? -1 : mapChoice + 1;
    if (A.ok && (pressed & BTN_FIRE)) startBattle(mapChoice >= 0 ? mapChoice : (int)(rnd() * 5) % 5);
    else {
      drawTitle(dt);
      return;
    }
  }
  if (state == ST_OVER || state == ST_WON) {
    drawOver(dt);
    if (overT > 1.0f && (pressed & BTN_FIRE)) {
      state = ST_TITLE;
      titleT = 0;
    }
    return;
  }
  clockT += dt;
  if (pressed & BTN_MAP) mapOpen = !mapOpen;
  if (mapOpen && (pressed & BTN_BACK)) {
    state = ST_TITLE;
    titleT = 0;
    drawTitle(dt);
    return;
  }
  if (!mapOpen) {
    if (tank.alive) moveTank(dt, held, sx, sy);
    tank.reload -= dt;
    if (tank.alive && (held & BTN_FIRE) && tank.reload <= 0) {
      fire();
      tank.reload = RELOAD;
    }
    moveShells(dt);
    updateGates(dt);
    updateTurrets(dt);
    updateHelis(dt);
    moveMissiles(dt);
    updateRespawn(dt);
    updateMen(dt);
    updateFlag(dt);
  }

  // the view follows the tank
  camX = tank.x - VIEW_W / 2;   // (the battle view: left of the panel)
  camY = tank.y - VIEW_H / 2;
  camX = camX < 0 ? 0 : (camX > WORLD - VIEW_W ? WORLD - VIEW_W : camX);
  camY = camY < 0 ? 0 : (camY > WORLD - VIEW_H ? WORLD - VIEW_H : camY);
  camX = roundf(camX);
  camY = roundf(camY);

  drawField();
  for (int k = 0; k < nTurrets; k++)
    if (isTurret(turrets[k].i)) drawGun(turrets[k].i, turrets[k].heading);
  for (int k = 0; k < nOurs; k++)
    if (A.types[field[ours[k].i]].flagsB & 0x04) drawGun(ours[k].i, ours[k].heading);
  for (int k = 0; k < (nSplats < 96 ? nSplats : 96); k++)
    if (A.men.n > 4) drawPic2x(A.men.pic[4], splats[k].x, splats[k].y);
  for (Man &m : men)
    if (m.live && A.men.n > 3) drawPic2x(A.men.pic[m.side + (m.dx < 0 ? 2 : 0)], m.x, m.y);
  for (Shell &s : shells)
    if (s.live) drawPic2x(A.shell.pic[s.frame], s.x, s.y);
  for (Missile &mm : missiles)
    if (mm.live) drawPic2x(mm.air ? A.heli.pic[34 + mm.frame] : A.turret.pic[8 + mm.frame], mm.x, mm.y);
  // helicopters' shadows on the ground, below and to the right of them
  for (Heli &h : helis)
    if (h.live && A.heli.n > 33) drawPic2x(A.heli.pic[18 + frameOf(h.heading)], h.x + 18, h.y + 26);
  const Pic *flagPic = A.numbers.n > 11 ? &A.numbers.pic[11] : nullptr;
  if (flagPic && flagState == FLAG_LOOSE) drawPic2x(*flagPic, flagX, flagY - 2 * fabsf(sinf(clockT * 4)));
  if (tank.alive) drawPic2x(A.tank.pic[frameOf(tank.heading)], tank.x, tank.y);
  drawRoofs();
  if (flagPic && flagState == FLAG_CARRIED && tank.alive) drawPic2x(*flagPic, tank.x + 6, tank.y - 16);
  // and the helicopters themselves, over everything, their rotors turning
  for (Heli &h : helis) {
    if (!h.live || A.heli.n < 18) continue;
    drawPic2x(A.heliEnemy.pic[frameOf(h.heading)], h.x, h.y);   // the enemy's: green
    drawPic2x(A.heli.pic[16 + ((int)(clockT * 20) & 1)], h.x, h.y);
  }
  drawBooms(mapOpen ? 0 : dt);
  drawHud();
  if (mapOpen) {
    drawMap((PLAY_W - MAPN * 14) / 2, 100, 14, 0.8f);
    pxBlend(0, FB_H - 70, PLAY_W - 1, FB_H - 1, 0, 0, 0, 0.6f);
    drawVectorText("OPTIONS: BACK TO THE BATTLE    CIRCLE: TITLE SCREEN", PLAY_CX, 697, 1.4f, rgb565(230, 230, 230), false);
  }
  float hd = -1;
  int nm = 0;
  int nh = 0;
  for (Heli &h : helis)
    if (h.live) {
      hd = sqrtf((h.x - tank.x) * (h.x - tank.x) + (h.y - tank.y) * (h.y - tank.y));
      nh++;
    }
  for (Missile &mm : missiles) nm += mm.live && mm.air;
  snprintf(debugText, sizeof(debugText), "tank %.0f,%.0f h%d spd %.0f score %d armour %d tanks %d helis %d heli %.0f air-missiles %d hq %d,%d flag %d state %d men %d aboard %d rescued %d killed %d lost %d fuel %.0f mines %d",
           tank.x, tank.y, frameOf(tank.heading), tank.speed, score, tank.armour, tanksLeft, nh, hd, nm,
           hqSquare / MAPN, hqSquare % MAPN, (int)flagState, (int)state, [] {
             int n = 0;
             for (Man &m : men) n += m.live;
             return n;
           }(), aboard, rescued, menKilled, menLost, tank.fuel, mines);
}

const char *gameDebug() { return debugText; }
void gameDebugTank(float *x, float *y, float *heading) {
  *x = tank.x;
  *y = tank.y;
  *heading = tank.heading;
}
void gameDebugPlace(float x, float y, float heading) {
  tank.x = x;
  tank.y = y;
  tank.heading = heading;
  tank.speed = 0;
}
void gameDebugGod(bool on) { godMode = on; }
void gameDebugMan(float x, float y, int side) {
  for (Man &m : men)
    if (!m.live) {
      m.live = true;
      m.side = side;
      m.inside = false;
      m.x = x;
      m.y = y;
      m.dx = m.dy = 0;   // standing (until he sees a tank, or 5 seconds)
      m.turn = 5;
      return;
    }
}
void gameDebugFuel(float f) { tank.fuel = f; }
