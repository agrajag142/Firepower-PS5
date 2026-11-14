// The PC test build: the game with a scripted controller, saving screenshots.
//
//   ./build.sh sim [script.txt]     (screenshots in build/sim/shots)
//
// Script lines:  hold BUTTONS FRAMES   (BUTTONS: LEFT+FIRE etc, or NONE)
//                tap BUTTONS           (one frame down, one up)
//                stick X Y FRAMES      the left stick held at X,Y (-1..1, up is -1)
//                place X Y H           put the tank at X,Y (Amiga pixels) facing H (0..15)
//                shot NAME             (out/NAME.ppm, made a PNG by build.sh)
//                wav SECONDS NAME      the sound for the next SECONDS: out/NAME.wav
//                man X Y SIDE          a man at X,Y (0 yours, 1 the enemy's)
//                fuel N                the tank's fuel (0..100)
//                god on|off            the tank can't be hurt (to test what's past the turrets)
//                seed N                (before anything else)
//                # comment
#include "game.h"
#include "gfx.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t parseButtons(const char *s) {
  uint32_t b = 0;
  char tmp[128];
  snprintf(tmp, sizeof(tmp), "%s", s);
  for (char *t = strtok(tmp, "+"); t; t = strtok(nullptr, "+")) {
    if (!strcmp(t, "LEFT")) b |= BTN_LEFT;
    else if (!strcmp(t, "RIGHT")) b |= BTN_RIGHT;
    else if (!strcmp(t, "UP")) b |= BTN_UP;
    else if (!strcmp(t, "DOWN")) b |= BTN_DOWN;
    else if (!strcmp(t, "FIRE")) b |= BTN_FIRE;
    else if (!strcmp(t, "MAP")) b |= BTN_MAP;
    else if (!strcmp(t, "NEXT")) b |= BTN_NEXT;
    else if (!strcmp(t, "BACK")) b |= BTN_BACK;
    else if (!strcmp(t, "REVERSE")) b |= BTN_REVERSE;
  }
  return b;
}

static void shot(const char *dir, const char *name) {
  char path[256];
  snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
  FILE *f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6\n%d %d\n255\n", FB_W, FB_H);
  static unsigned char row[FB_W * 3];
  for (int y = 0; y < FB_H; y++) {
    for (int x = 0; x < FB_W; x++) {
      uint32_t p = g_frame[y * FB_W + x];
      row[x * 3] = p & 255;
      row[x * 3 + 1] = (p >> 8) & 255;
      row[x * 3 + 2] = (p >> 16) & 255;
    }
    fwrite(row, 1, sizeof(row), f);
  }
  fclose(f);
}

int main(int argc, char **argv) {
  if (argc < 4) {
    fprintf(stderr, "usage: sim script.txt outdir assetsdir/ [title.mod [game.mod]]\n");
    return 2;
  }
  for (int k = 0; k < 2 && argc > 4 + k; k++) {   // the tunes, as the PS5 build loads them
    FILE *mf = fopen(argv[4 + k], "rb");
    if (!mf) continue;
    fseek(mf, 0, SEEK_END);
    long len = ftell(mf);
    fseek(mf, 0, SEEK_SET);
    signed char *data = (signed char *)malloc(len);
    if (data && fread(data, 1, len, mf) == (size_t)len) gameSetTune(k, data, len);
    fclose(mf);
  }
  FILE *sf = fopen(argv[1], "r");
  if (!sf) return 2;
  char line[256];
  uint32_t seed = 1, prev = 0;
  bool started = false;
  long frame = 0;
  const float dt = 1.0f / 60.0f;
  auto step = [&](uint32_t b, float sx, float sy) {
    gameFrame(dt, b, b & ~prev, sx, sy);
    prev = b;
    frame++;
  };
  while (fgets(line, sizeof(line), sf)) {
    char cmd[32] = "", a[128] = "";
    int n = 0;
    if (line[0] == '#' || sscanf(line, "%31s %127s %d", cmd, a, &n) < 1) continue;
    if (!strcmp(cmd, "seed")) { seed = (uint32_t)atoi(a); continue; }
    if (!started) { gameInit(seed, argv[3]); started = true; }
    if (!strcmp(cmd, "hold")) {
      uint32_t b = strcmp(a, "NONE") ? parseButtons(a) : 0;
      for (int i = 0; i < n; i++) step(b, 0, 0);
    } else if (!strcmp(cmd, "tap")) {
      step(parseButtons(a), 0, 0);
      step(0, 0, 0);
    } else if (!strcmp(cmd, "stick")) {
      float x = 0, y = 0;
      int frames = 0;
      sscanf(line, "%*s %f %f %d", &x, &y, &frames);
      for (int i = 0; i < frames; i++) step(0, x, y);
    } else if (!strcmp(cmd, "place")) {
      float x = 0, y = 0, h = 0;
      sscanf(line, "%*s %f %f %f", &x, &y, &h);
      gameDebugPlace(x, y, h * 6.28318531f / 16);
      step(0, 0, 0);
    } else if (!strcmp(cmd, "wav")) {
      float secs = (float)atof(a);
      char name[64] = "sound";
      sscanf(line, "%*s %*s %63s", name);
      char path[256];
      snprintf(path, sizeof(path), "%s/%s.wav", argv[2], name);
      FILE *wf = fopen(path, "wb");
      int total = (int)(secs * 48000), per = 800;
      unsigned hdr[11] = { 0x46464952u, (unsigned)(36 + total * 4), 0x45564157u, 0x20746d66u, 16, 0x00020001u,
                           48000, 48000 * 4, 0x00100004u, 0x61746164u, (unsigned)(total * 4) };
      if (wf) fwrite(hdr, 4, 11, wf);
      static short buf[800 * 2];
      for (int done = 0; done < total; done += per) {
        step(prev, 0, 0);
        gameAudio(buf, per);
        if (wf) fwrite(buf, 4, per, wf);
      }
      if (wf) fclose(wf);
    } else if (!strcmp(cmd, "man")) {      // man X Y SIDE: a man there (0 yours, 1 the enemy's)
      float x = 0, y = 0;
      int side = 0;
      sscanf(line, "%*s %f %f %d", &x, &y, &side);
      gameDebugMan(x, y, side);
    } else if (!strcmp(cmd, "fuel")) {     // fuel N: the tank's fuel (0..100)
      gameDebugFuel((float)atof(a));
    } else if (!strcmp(cmd, "god")) {      // god on|off: the tank can't be hurt
      gameDebugGod(!strcmp(a, "on"));
    } else if (!strcmp(cmd, "shot")) {
      shot(argv[2], a);
    }
    size_t L = strlen(line);
    if (L && line[L - 1] == '\n') line[L - 1] = 0;
    printf("%6ld  %-28s %s%s\n", frame, line, gameDebug(), gameQuitRequested() ? "  [QUIT GAME]" : "");
  }
  return 0;
}
