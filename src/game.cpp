// game.cpp - FIREPOWER for the PS5
//
// A native remake of MicroIllusions' Firepower (Amiga, 1987), built from the
// original's own graphics, maps and sounds (pulled off your disk by
// tools/extract_assets.py) and its rules as read from its disassembly.
// Enhanced: the whole 1920 x 1080 picture, the original's pixels at 2x, so
// about 2.7 times as much of the battlefield is in view.
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
static const char *VERSION = "V002";
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
static const float GATE_CLOSE = 1.0f;      // seconds after you're clear of a gate, it shuts

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
static bool solidAt(float x, float y) {
  int tx = (int)floorf(x / TILE), ty = (int)floorf(y / TILE);
  if (!solidSquare(tx, ty)) return false;
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
static int score = 0, destroyed = 0;
static float clockT = 0;

enum State { ST_TITLE, ST_PLAY };
static State state = ST_TITLE;
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
  if (!t.destructible) {
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
  field[i] = t.becomes < 93 ? t.becomes : 0;
  strength[i] = A.types[field[i]].strength;
  addBoom(tx * TILE + TILE / 2.0f, ty * TILE + TILE / 2.0f, true);
  playSound(SND_BOOM2);
  destroyed++;
  score += (t.side >> 4) == 2 ? 200 : 100;
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

static void moveShells(float dt) {
  for (Shell &s : shells) {
    if (!s.live) continue;
    // in small steps, so a fast shell can't jump a wall
    int steps = 4;
    for (int k = 0; k < steps && s.live; k++) {
      s.x += s.dx * dt / steps;
      s.y += s.dy * dt / steps;
      if (solidAt(s.x, s.y)) {
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

// The roof of anything the tank can be under (your base and the enemy's: bits 3
// and 2 of the first flags), drawn again over the tank where it isn't sand
static void drawRoofs() {
  int x0 = (int)floorf((tank.x - 16) / TILE), x1 = (int)floorf((tank.x + 16) / TILE);
  int y0 = (int)floorf((tank.y - 16) / TILE), y1 = (int)floorf((tank.y + 16) / TILE);
  for (int tx = x0; tx <= x1; tx++)
    for (int ty = y0; ty <= y1; ty++) {
      if (tx < 0 || ty < 0 || tx >= MAPN || ty >= MAPN) continue;
      const TileType &t = typeAt(tx, ty);
      if (!(t.flagsA & 0x0c)) continue;
      const Pic &pic = A.tiles[t.pic];
      const uint8_t *solid = A.tileSolid[t.pic];
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
  (void)seed;
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
  clockT += dt;
  if (pressed & BTN_MAP) mapOpen = !mapOpen;
  if (mapOpen && (pressed & BTN_BACK)) {
    state = ST_TITLE;
    titleT = 0;
    drawTitle(dt);
    return;
  }
  if (!mapOpen) {
    moveTank(dt, held, sx, sy);
    tank.reload -= dt;
    if ((held & BTN_FIRE) && tank.reload <= 0) {
      fire();
      tank.reload = RELOAD;
    }
    moveShells(dt);
    updateGates(dt);
  }

  // the view follows the tank
  camX = tank.x - VIEW_W / 2;
  camY = tank.y - VIEW_H / 2;
  camX = camX < 0 ? 0 : (camX > WORLD - VIEW_W ? WORLD - VIEW_W : camX);
  camY = camY < 0 ? 0 : (camY > WORLD - VIEW_H ? WORLD - VIEW_H : camY);
  camX = roundf(camX);
  camY = roundf(camY);

  drawField();
  for (Shell &s : shells)
    if (s.live) drawPic2x(A.shell.pic[s.frame], s.x, s.y);
  drawPic2x(A.tank.pic[frameOf(tank.heading)], tank.x, tank.y);
  drawRoofs();
  drawBooms(mapOpen ? 0 : dt);
  drawHud();
  if (mapOpen) {
    drawMap((FB_W - MAPN * 14) / 2, 100, 14, 0.8f);
    pxBlend(0, FB_H - 70, FB_W - 1, FB_H - 1, 0, 0, 0, 0.6f);
    drawVectorText("OPTIONS: BACK TO THE BATTLE    CIRCLE: TITLE SCREEN", 640, 697, 1.4f, rgb565(230, 230, 230), false);
  } else {
    drawMap(FB_W - MAPN * 3 - 24, FB_H - MAPN * 3 - 24, 3, 0.5f);
  }
  snprintf(debugText, sizeof(debugText), "tank %.0f,%.0f h%d spd %.0f score %d", tank.x, tank.y, frameOf(tank.heading), tank.speed, score);
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
