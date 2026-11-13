# FIREPOWER for the PS5

A native PS5 remake of **Firepower** (MicroIllusions, Amiga, 1987), built from
the original game's own graphics, battlefields and sounds - read straight off
your own Firepower disk - and its rules, as worked out from its disassembly.
Not an emulator: no Kickstart and no loading, and it can be changed like any of
our games.

Enhanced from the start: the whole 1920 x 1080 picture, the original's pixels at
2x, so about 2.7 times as much of the battlefield is in view as on the Amiga's
352 x 232 screen.

## Version 002

- The tank starts inside its garage, under the roof, facing out
- Your compound's gates (type 44) open as your tank drives up to them, from
  either side, and close a second after it's through (the original's rule, in
  `sub_03680`); the enemy's gates (type 67) stay shut

## Version 001

- The original title picture (press X; Triangle picks the battlefield)
- Any of the disk's 11 battlefields (MapP1 t1-t5, MapP2 t1-t5, MapP3 t1),
  64 x 64 squares of 64 x 64 pixels, drawn from the original's 50 tiles
- Your tank, in the original's 16 headings
- Walls and buildings in its way, to the pixel (a tank drives right up to them)
- Shells: each square takes the hits the original gives it, then turns into
  what the original's tile table says (buildings to rubble, walls broken)
- The original explosions and sounds (TankBlast, Boom2, Drip)
- A mini map, and the full map (Options)

Still to come (from the disassembly, in this order): the enemy's turrets and
tanks, the men to rescue and run over, the helicopters, the flags and the win,
the pick screen and its tanks, then two players split-screen.

## Controls

| DualSense | |
| --- | --- |
| Left stick | point where to go: the tank turns that way and drives |
| D-pad | as the original joystick: left/right turn, up forward, down reverse |
| Cross, R1 or R2 | fire |
| Options or the touchpad | the full map (Circle there: back to the title screen) |
| Triangle (title screen) | the next battlefield |

## You need your Firepower disk

The original game's files are not in this repository. Once, with your own
`Firepower.adf`:

```bash
python3 firepower/tools/extract_assets.py /path/to/Firepower.adf
```

It writes them into `assets/fp/` (ignored by git), and checks the disk is
Firepower. (Python 3 only, nothing to install.)

## Build and install

As QUANTUM LOCKDOWN: in WSL or Linux, put this repository inside the starter
project folder (ps5-homebrew-starter, after its `tools/setup.sh`), beside
`kit/`:

```bash
cd ~/ps5-homebrew-starter
git clone https://github.com/agrajag142/Firepower-PS5 firepower
python3 firepower/tools/extract_assets.py /path/to/Firepower.adf
chmod +x firepower/build.sh          # (if it was unpacked in Windows)
firepower/build.sh                   # build/firepower/PPSA99908 and its ZIP
firepower/build.sh deploy <PS5 IP>   # or build and copy it to /data/homebrew over FTP
```

Later versions: `git -C firepower pull`, then build again.

Then put it on the home screen with ShadowMountPlus. Title ID `PPSA99908`,
beside the C64 (`PPSA99904`), the Amiga apps (`PPSA99905`, `PPSA99907`) and
Quantum Lockdown (`PPSA99903`).

## The PC test build

```bash
firepower/build.sh sim                     # plays sim/tour.txt, screenshots in firepower/build/sim/shots
firepower/build.sh sim firepower/sim/shoot.txt   # shelling a building to rubble
firepower/build.sh sim firepower/sim/gate.txt    # out of the garage and through the gate
```

It runs the same game code with a scripted controller (see `sim/sim_main.cpp`
for the commands), so a change can be checked on the PC before it goes on the
console.

## What's in here

| | |
| --- | --- |
| `src/game.cpp` | the game: the battlefield, the tank, shells, explosions, the maps, the sound mixer (settings at the top) |
| `src/assets.cpp` | the original's files decoded: bitplane sprites (with PlanePick/PlaneOnOff and masks), full screens, the tile table, the sounds |
| `src/gfx.cpp` | drawing (from QUANTUM LOCKDOWN: shapes and the vector text) |
| `src/ps5_platform.cpp` | the PS5 side: video, the DualSense, AudioOut (from QUANTUM LOCKDOWN) |
| `tools/extract_assets.py` | reads your Firepower.adf (AmigaDOS OFS) and the game program's tables |
| `sim/` | the PC test build and its scripts |
| `docs/ORIGINAL.md` | what's known of the original: its files, formats and tables |
