// game.h - what the platform (PS5, or the PC test build) and the game say to each other

#ifndef GAME_H
#define GAME_H

#include <stdint.h>

// The controls, as the game sees them (the platform maps the DualSense onto these)
enum : uint32_t {
  BTN_LEFT = 0x01, BTN_RIGHT = 0x02, BTN_UP = 0x04, BTN_DOWN = 0x08,   // D-pad: turn, forward, reverse
  BTN_FIRE = 0x10,    // Cross / R2 / R1: fire
  BTN_MAP = 0x20,     // Options / touchpad: the settings menu
  BTN_NEXT = 0x40,    // Triangle: (unused)
  BTN_BACK = 0x80,    // Circle: out of the settings menu
  BTN_REVERSE = 0x100,   // Square: reverse (straight back)
};

// dir: where the game's files are ("/app0/assets/fp/" on the PS5)
void gameInit(uint32_t seed, const char *dir);
// One frame: dt in seconds, held = buttons down now, pressed = went down this frame,
// sx/sy = the left stick (-1..1, up is -1).
// Draws the whole picture into g_frame (gfx.h).
void gameFrame(float dt, uint32_t held, uint32_t pressed, float sx, float sy);
// The music: the platform hands over each tune's bytes once (they must stay put):
// 0 title.mod (the title and end screens), 1 game.mod (the battle). Either may be missing.
void gameSetTune(int which, signed char *data, long len);
// Stereo 16-bit samples at 48 kHz, from the platform's sound thread
void gameAudio(short *out, int frames);
// QUIT GAME chosen in the settings menu: the platform closes the app
bool gameQuitRequested();
const char *gameDebug();                          // (the PC test build: where the tank is)
void gameDebugTank(float *x, float *y, float *heading);
void gameDebugPlace(float x, float y, float heading);   // (the PC test build: put the tank somewhere)
void gameDebugMan(float x, float y, int side);           // (the PC test build: a man there, 0 yours 1 the enemy's)
void gameDebugFuel(float f);
void gameDebugArmour(int a);                             // (the PC test build: set the armour, 0..100)                             // (the PC test build: set the fuel, 0..100)
void gameDebugGod(bool on);
void gameDebugBattlefield(int m);
void gameDebugTankKind(int k);                          // (the PC test build: the tank X starts with, 0..2, with battlefield N)                       // (the PC test build: the battlefield X starts, 0..10; -1 as the settings say)                             // (the PC test build: the tank can't be hurt)

#endif // GAME_H
