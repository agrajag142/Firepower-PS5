// game.cpp - FIREPOWER for the PS5
//
// A native remake of MicroIllusions' Firepower (Amiga, 1987), built from the
// original's own graphics, maps and sounds (pulled off your disk by
// tools/extract_assets.py) and its rules as read from its disassembly.
// Enhanced: the whole 1920 x 1080 picture, the original's pixels at 2x, so
// about 2.7 times as much of the battlefield is in view.
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

#include <atomic>
#include <math.h>
#include <stdio.h>
#include <string.h>

// ============================================================ settings
static const char *VERSION = "V007";
static const int S = 2;                    // screen pixels per Amiga pixel
static const int TILE = 64;                // a map square, in Amiga pixels
static const int MAPN = 64;                // squares across and down
static const float WORLD = TILE * MAPN;    // 4096
static const float VIEW_W = FB_W / S, VIEW_H = FB_H / S;   // 960 x 540 Amiga pixels in view

static const float TANK_SPEED = 72.0f;     // forward, Amiga pixels a second
static const float TANK_REVERSE = 40.0f;
static const float TANK_ACCEL = 160.0f;
static const float TANK_TURN = 3.2f;       // radians a second
static const float TANK_RADIUS = 10.0f;    // its body, for the walls
static const float SHELL_SPEED = 260.0f;
static const float SHELL_RANGE = 300.0f;
static const float RELOAD = 0.45f;
static const int SHELL_DAMAGE = 5;         // a building (strength 15) takes three
static const float GATE_CLOSE = 1.0f;
static const float TRUNK = 0.3f;
// A turret acts only while it would be on the original's 352 x 232 screen round
// your tank (the original works its turrets as it draws the squares in view)
static const float TURRET_SEE_X = 176 + 32, TURRET_SEE_Y = 116 + 32;
static const float TURRET_STEP = 0.14f;    // seconds per 45-degree turn (the original: a step per update)
static const float TURRET_RELOAD = 0.8f;   // after its missile is gone (plus a little at random)
static const int WALL_STRENGTH = 30;       // a plain wall: shells to wear it into a damaged wall (x SHELL_DAMAGE)
static const float MISSILE_SPEED = 150.0f;
static const float MISSILE_RANGE = 360.0f;
static const int MISSILE_DAMAGE = 12;      // of your tank's 100 armour
static const int TANKS = 3;                // tanks you have
static const float RESPAWN = 2.5f;         // seconds before the next tank comes out
static const float HELI_FIRST = 25.0f;     // seconds into a battle before the first helicopter
static const float HELI_EVERY = 30.0f;     // then one every this many seconds (+ up to 15)
static const int HELI_MAX = 2;             // at once
static const float HELI_SPEED = 95.0f;
static const float HELI_TURN = 1.3f;       // radians a second
static const float HELI_CIRCLE = 150.0f;   // how far out it circles your tank
static const float HELI_RELOAD = 2.2f;
static const int HELI_HITS = 2;            // shells to bring one down
static const float HELI_MISSILE_SPEED = 190.0f;
static const int HELI_MISSILE_DAMAGE = 10;           // a tree's trunk: this much of its leaves' size (and at least 3 pixels)      // seconds after you're clear of a gate, it shuts

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
  for (int i = 0; i < frames; i++) {
    float l = 0, r = 0;
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
    // a tree: only its trunk, in the middle of its leaves, stops a tank
    if (!trees || tx < 0 || ty < 0 || tx >= MAPN || ty >= MAPN) return false;
    int pic = typeAt(tx, ty).pic;
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
static Heli helis[HELI_MAX];
static float heliTimer = 0;
static int helisDown = 0;
static int startSquare = 0;
static uint32_t rng = 1;
static float rnd() {   // 0..1
  rng = rng * 1664525u + 1013904223u;
  return (rng >> 8) / 16777216.0f;
}
static float clockT = 0;

enum State { ST_TITLE, ST_PLAY, ST_OVER };
static State state = ST_TITLE;
static float overT = 0;   // on the game over screen
static bool mapOpen = false;
static float titleT = 0;
static char debugText[160];

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
  helisDown = 0;
  for (Shell &s : shells) s.live = false;
  for (Boom &b : booms) b.live = false;
  score = destroyed = 0;
  clockT = 0;
  mapOpen = false;
  state = ST_PLAY;
}

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
  field[i] = t.becomes < 93 ? t.becomes : 0;
  strength[i] = A.types[field[i]].strength;
  if (plainWall) {   // only damaged: the original's damaged wall, a few more hits to go
    addBoom(hx, hy, false);
    playSound(SND_BOOM2, 0.6f);
    return;
  }
  addBoom(tx * TILE + TILE / 2.0f, ty * TILE + TILE / 2.0f, true);
  playSound(SND_BOOM2);
  destroyed++;
  score += wasTurret ? 250 : (t.side & 0x20) ? 200 : 100;
}

// ============================================================ the tank
static void moveTank(float dt, uint32_t held, float sx, float sy) {
  float want = 0;   // speed asked for
  float mag = sqrtf(sx * sx + sy * sy);
  if (mag > 0.35f) {
    // the stick: point it where to go; the tank turns that way and drives once it faces it
    float target = atan2f(sx, -sy);
    float diff = wrapAngle(target - tank.heading);
    float turn = TANK_TURN * dt;
    tank.heading += diff > turn ? turn : (diff < -turn ? -turn : diff);
    if (fabsf(diff) < 0.8f) want = TANK_SPEED * (mag > 1 ? 1 : mag) * cosf(diff);
  } else {
    // the D-pad: as the original's joystick - left and right turn, up and down drive
    if (held & BTN_LEFT) tank.heading -= TANK_TURN * dt;
    if (held & BTN_RIGHT) tank.heading += TANK_TURN * dt;
    if (held & BTN_UP) want = TANK_SPEED;
    else if (held & BTN_DOWN) want = -TANK_REVERSE;
  }
  tank.heading = wrapAngle(tank.heading);
  if (tank.heading < 0) tank.heading += TWO_PI;
  float dv = TANK_ACCEL * dt;
  tank.speed += want > tank.speed + dv ? dv : (want < tank.speed - dv ? -dv : want - tank.speed);

  // it moves along its heading; a wall stops it on that axis (so it slides along)
  float dx = sinf(tank.heading) * tank.speed * dt, dy = -cosf(tank.heading) * tank.speed * dt;
  auto clear = [](float x, float y) {   // eight points round its body
    const float r = TANK_RADIUS, d = TANK_RADIUS * 0.7071f;
    return !solidAt(x - r, y) && !solidAt(x + r, y) && !solidAt(x, y - r) && !solidAt(x, y + r) &&
           !solidAt(x - d, y - d) && !solidAt(x + d, y - d) && !solidAt(x - d, y + d) && !solidAt(x + d, y + d);
  };
  if (clear(tank.x + dx, tank.y)) tank.x += dx;
  else tank.speed *= 0.5f;
  if (clear(tank.x, tank.y + dy)) tank.y += dy;
  else tank.speed *= 0.5f;
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

static void tankHit(int damage, float x, float y) {
  if (!tank.alive) return;
  tank.armour -= damage;
  addBoom(x, y, false);
  playSound(SND_BOOM2, 0.5f);
  if (tank.armour <= 0) {
    tank.armour = 0;
    tank.alive = false;
    tank.down = 0;
    tank.speed = 0;
    tanksLeft--;
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
    heliTimer = HELI_EVERY + rnd() * 15;
  }
  for (Heli &h : helis) {
    if (!h.live) continue;
    // where it heads: a point on a circle round your tank, ahead of it on its way round
    float dx = h.x - tank.x, dy = h.y - tank.y, dist = sqrtf(dx * dx + dy * dy) + 0.01f;
    float tx, ty;
    if (dist > HELI_CIRCLE * 1.6f) {
      tx = tank.x;
      ty = tank.y;
    } else {
      float a = atan2f(dy, dx) + h.side * 0.7f;
      tx = tank.x + cosf(a) * HELI_CIRCLE;
      ty = tank.y + sinf(a) * HELI_CIRCLE;
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

// After a tank is lost: the next comes out of the garage, or it's over
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
      const Pic &t = A.tiles[typeAt(tx, ty).pic];
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
      uint32_t c = A.tileAvg[t.pic];
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
}

static void drawHud() {
  pxBlend(0, 0, FB_W - 1, 59, 0, 0, 0, 0.55f);
  uint16_t ink = rgb565(255, 210, 120), dim = rgb565(170, 140, 90);
  drawVectorText("FIREPOWER", 90, 20, 2.4f, ink, true);
  char buf[64];
  snprintf(buf, sizeof(buf), "SCORE %06d", score);
  drawVectorText(buf, 640, 20, 2.4f, ink, true);
  snprintf(buf, sizeof(buf), "BATTLEFIELD %d-%d", mapNo < 5 ? 1 : mapNo < 10 ? 2 : 3, mapNo < 10 ? mapNo % 5 + 1 : 1);
  drawVectorText(buf, 1130, 20, 2.0f, ink, true);
  drawVectorText(VERSION, 1250, 708, 1.2f, dim, false);
  snprintf(buf, sizeof(buf), "DESTROYED %d", destroyed);
  drawVectorText(buf, 90, 708, 1.4f, dim, false);
  // armour and the tanks left, bottom left
  pxBlend(16, FB_H - 120, 420, FB_H - 70, 0, 0, 0, 0.5f);
  drawVectorText("ARMOUR", 70, 650, 1.4f, ink, false);
  int bx0 = 170, by0 = FB_H - 108, bw = 230, bh = 26;
  pxRect(bx0 - 2, by0 - 2, bx0 + bw + 1, by0 + bh + 1, rgba(60, 50, 40));
  int fillW = bw * tank.armour / 100;
  uint32_t col = tank.armour > 50 ? rgba(90, 200, 60) : tank.armour > 25 ? rgba(240, 200, 40) : rgba(230, 60, 40);
  if (fillW > 0) pxRect(bx0, by0, bx0 + fillW - 1, by0 + bh - 1, col);
  for (int k = 0; k < tanksLeft - (tank.alive ? 1 : 0) && k < 9; k++)
    drawPic2x(A.tank.pic[0], camX + (440 + k * 56) / (float)S, camY + (FB_H - 95) / (float)S);
}

static void drawOver(float dt) {
  overT += dt;
  pxRect(0, 0, FB_W - 1, FB_H - 1, rgba(0, 0, 0));
  drawVectorText("YOUR TANKS ARE ALL GONE", 640, 220, 3.6f, rgb565(230, 80, 50), true);
  drawVectorText("***** FINAL REPORT *****", 640, 320, 2.2f, rgb565(255, 210, 120), true);
  char buf[64];
  snprintf(buf, sizeof(buf), "SCORE %d", score);
  drawVectorText(buf, 640, 390, 2.2f, rgb565(255, 255, 255), true);
  snprintf(buf, sizeof(buf), "INFLICTED: %d DESTROYED", destroyed);
  drawVectorText(buf, 640, 440, 1.8f, rgb565(220, 220, 220), false);
  snprintf(buf, sizeof(buf), "HELICOPTERS DOWNED: %d", helisDown);
  drawVectorText(buf, 640, 520, 1.8f, rgb565(220, 220, 220), false);
  snprintf(buf, sizeof(buf), "CASUALTIES: TANKS %d", tanksLost);
  drawVectorText(buf, 640, 480, 1.8f, rgb565(220, 220, 220), false);
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
  snprintf(buf, sizeof(buf), "TRIANGLE: BATTLEFIELD %d-%d", mapNo < 5 ? 1 : mapNo < 10 ? 2 : 3, mapNo < 10 ? mapNo % 5 + 1 : 1);
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
      const Pic &pic = A.tiles[t.pic];
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
  if (state == ST_TITLE) {
    if (A.ok && (pressed & BTN_NEXT)) mapNo = (mapNo + 1) % 11;
    if (A.ok && (pressed & BTN_FIRE)) startBattle(mapNo);
    else {
      drawTitle(dt);
      return;
    }
  }
  if (state == ST_OVER) {
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
  }

  // the view follows the tank
  camX = tank.x - VIEW_W / 2;
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
  for (Shell &s : shells)
    if (s.live) drawPic2x(A.shell.pic[s.frame], s.x, s.y);
  for (Missile &mm : missiles)
    if (mm.live) drawPic2x(mm.air ? A.heli.pic[34 + mm.frame] : A.turret.pic[8 + mm.frame], mm.x, mm.y);
  // helicopters' shadows on the ground, below and to the right of them
  for (Heli &h : helis)
    if (h.live && A.heli.n > 33) drawPic2x(A.heli.pic[18 + frameOf(h.heading)], h.x + 18, h.y + 26);
  if (tank.alive) drawPic2x(A.tank.pic[frameOf(tank.heading)], tank.x, tank.y);
  drawRoofs();
  // and the helicopters themselves, over everything, their rotors turning
  for (Heli &h : helis) {
    if (!h.live || A.heli.n < 18) continue;
    drawPic2x(A.heli.pic[frameOf(h.heading)], h.x, h.y);
    drawPic2x(A.heli.pic[16 + ((int)(clockT * 20) & 1)], h.x, h.y);
  }
  drawBooms(mapOpen ? 0 : dt);
  drawHud();
  if (mapOpen) {
    drawMap((FB_W - MAPN * 14) / 2, 100, 14, 0.8f);
    pxBlend(0, FB_H - 70, FB_W - 1, FB_H - 1, 0, 0, 0, 0.6f);
    drawVectorText("OPTIONS: BACK TO THE BATTLE    CIRCLE: TITLE SCREEN", 640, 697, 1.4f, rgb565(230, 230, 230), false);
  } else {
    drawMap(FB_W - MAPN * 3 - 24, FB_H - MAPN * 3 - 24, 3, 0.5f);
  }
  float hd = -1;
  int nm = 0;
  for (Heli &h : helis)
    if (h.live) hd = sqrtf((h.x - tank.x) * (h.x - tank.x) + (h.y - tank.y) * (h.y - tank.y));
  for (Missile &mm : missiles) nm += mm.live && mm.air;
  snprintf(debugText, sizeof(debugText), "tank %.0f,%.0f h%d spd %.0f score %d armour %d tanks %d heli %.0f air-missiles %d",
           tank.x, tank.y, frameOf(tank.heading), tank.speed, score, tank.armour, tanksLeft, hd, nm);
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
