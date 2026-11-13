// game.h - what the platform (PS5, or the PC test build) and the game say to each other

#ifndef GAME_H
#define GAME_H

#include <stdint.h>

// The controls, as the game sees them (the platform maps the DualSense onto these)
enum : uint32_t {
  BTN_LEFT = 0x01, BTN_RIGHT = 0x02, BTN_UP = 0x04, BTN_DOWN = 0x08,   // D-pad: turn, forward, reverse
  BTN_FIRE = 0x10,    // Cross / R2 / R1: fire
  BTN_MAP = 0x20,     // Options / touchpad: the full map
  BTN_NEXT = 0x40,    // Triangle: the next battlefield (title screen)
  BTN_BACK = 0x80,    // Circle: back to the title screen (with the map open)
};

// dir: where the game's files are ("/app0/assets/fp/" on the PS5)
void gameInit(uint32_t seed, const char *dir);
// One frame: dt in seconds, held = buttons down now, pressed = went down this frame,
// sx/sy = the left stick (-1..1, up is -1).
// Draws the whole picture into g_frame (gfx.h).
void gameFrame(float dt, uint32_t held, uint32_t pressed, float sx, float sy);
// Stereo 16-bit samples at 48 kHz, from the platform's sound thread
void gameAudio(short *out, int frames);
const char *gameDebug();                          // (the PC test build: where the tank is)
void gameDebugTank(float *x, float *y, float *heading);
void gameDebugPlace(float x, float y, float heading);   // (the PC test build: put the tank somewhere)

#endif // GAME_H
