# FIREPOWER for the PS5

A native PS5 remake of **Firepower** (MicroIllusions, Amiga, 1987), built from
the original game's own graphics, battlefields and sounds - read straight off
your own Firepower disk - and its rules, as worked out from its disassembly.
Not an emulator: no Kickstart and no loading, and it can be changed like any of
our games.

Enhanced from the start: the whole 1920 x 1080 picture, the original's pixels at
2x, so about 2.7 times as much of the battlefield is in view as on the Amiga's
352 x 232 screen.

## Version 023

- What still stands in a ruin - wall stubs, a building's corners, a depot's
  frame - blocks the tank, as walls do; the scattered rubble doesn't
- Music louder: both tunes at 0.85

## Version 022

- Music, as our other PS5 games: `title.mod` on the title and end screens,
  `game.mod` in the battle (put them in `assets/`, or copy them over FTP into
  `/data/homebrew/PPSA99908/assets/`; without them it's silent)
- The goal reads CAPTURE THE ENEMY FLAG

## Version 021

- Up to 5 helicopters at once, a new one every 30 to 50 seconds (as on the Amiga),
  each circling at its own distance so they spread out round your tank

## Version 020

- The original's panel on the right (from its one-player screen, scaled to the
  TV's height, in its camouflage): the score in its own digits, your men
  aboard, your tanks, the fuel, the radar, and your armour (a bar in the gap
  between the men and the tanks). The battle view is the rest of the screen
- X starts a battle on one of the five one-player battlefields at random, as
  the original. Triangle on the title screen picks one instead (any of the 11)

## Version 019

- Square reverses: the tank backs straight up, the way it faces

## Version 018

- The tank moves exactly the way its picture faces (one of the original's 16
  headings): it no longer drifts off a straight line
- The D-pad steers as the stick does: press the way to go (diagonals too) and
  the tank turns that way and drives

## Version 017

- The garage's left, top and right walls are solid: the tank only comes out
  (and goes back in) at the bottom, as on the Amiga

## Version 016

- Your shells reach about 165 pixels: three quarters of the Amiga's battle
  view (about 218 pixels across), as there

## Version 015

- Turrets only fire from about 110 pixels away (as on the Amiga, where they
  reach from just off the edge of its small battle view), and their missiles
  don't fly as far

## Version 014: fuel

- The tank uses fuel as it drives (a full tank: about two minutes of driving;
  standing still uses none). The gauge is under the armour; LOW FUEL when it's
  nearly gone, and with none the tank only crawls (a quarter speed)
- Your garage refuels it (as well as repairing it); destroying one of the
  enemy's fuel depots (its green barrels) fills it up: REFUELLED
- Mines: 3 to start, one more for every 5 men rescued (as the original). Laying
  them comes with the two-player game, where they matter

## Version 013

- An extra tank for every 15 men rescued (as the original), with EXTRA TANK! on the screen

## Version 012

- The men walk slower
- They get out at your first-aid station (the red cross, type 24), as the
  original's instructions say: stop on the square to its right and they climb
  out one at a time and run in - rescued (200 each). (Your garage only repairs.)

## Version 011: the men

- Destroying a building or post lets out the men inside it, as the original:
  how many is in its tile type (5 from a building, 15 from a big one, 2 from
  a post), brown ones yours and green ones the enemy's (the Men file's little
  soldiers, facing the way they run)
- The enemy's men run from your tank; run them over (Splat, and the blood
  stays: 50 points each)
- Your men run to your tank when it's stopped and climb aboard (6 at most);
  bring them to your first-aid station and they're rescued (200 each). Drive into
  them and they're lost; lose the tank and so are the men aboard
- The final report: INFLICTED (destroyed, helicopters, men), RESCUED,
  CASUALTIES (tanks, men)

## Version 010

- The enemy's helicopters in the enemy's colours (green), as the original's
  copies of them

## Version 009: the original's one-player battle

- One of the enemy's big green bunkers is its headquarters, picked at random
  each game (as the original); the others are decoys. One shell opens it,
  its inner bunker takes the original's 75, and your flag lies in the ruin
- Drive over the flag to take it, and bring it home into your garage to win:
  "YOU HAVE WON!!! and such a glorious battle it was too...", and the final
  report. Lose your tank on the way and the flag drops where it was
- Without the flag, your garage repairs your tank (+5 at a time, as the
  original's sub_066c8 call)
- Your base (blue) and the loose flag (red) on the maps; what to do next at
  the top of the screen

## Version 008

- The enemy's walls, corner posts, bunkers, buildings and helipads in the
  enemy's colours (greens where yours are browns), as the original draws its
  copies of them (each colour's bit 3 cleared)

## Version 007

- The turrets are where the original has them: on the enemy's corner posts and
  bunkers (tiles with bit 3 of their second flags), the gun drawn where the
  original draws each of its 8 headings. As the original: a turret only acts
  while it would be on the Amiga's screen round your tank, turns 45 degrees a
  step, and fires when lined up, one missile at a time. Your side's corner
  posts have their guns too (they'll fire once there are enemy tanks)
- The big round pads (type 37) are the enemy's helipads again: the helicopters
  take off from them
- Plain walls can be shot open (about seven shells): worn into the original's
  damaged wall, then a gap

## Version 006

- The enemy's helicopters (the Heli file: 16 headings, the turning rotor, the
  shadow on the ground, their own missiles): the first comes in after 25
  seconds, then one every 30-45 (two at most). They fly over walls, circle your
  tank and fire at it. Two shells bring one down (300 points). Red dots on the
  maps; their rotor sound when near
- Missiles fly straight at what they're aimed at (the picture is the nearest of
  its 8); shells fly over trees

## Version 005

- The enemy's turrets: every gun position (type 37) has its gun (the Turret
  file's 8 headings) turning to your tank when it's within about four squares,
  firing the original's missiles when it's lined up. Walls and buildings stop
  the missiles. Three shells destroy a turret (250 points)
- Your tank's armour (missiles take 12 of 100), three tanks: a lost tank blows
  up and the next comes out of the garage; after the third, the final report

## Version 004

- All of a tree is drawn over a tank under it (its black parts were see-through)

## Version 003

- Trees: the tank drives under their branches (the leaves are drawn over it)
  and stops at the trunk in the middle of each tree

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

Still to come (from the disassembly, in this order): the flags and the win,
the pick screen and its tanks, then two players split-screen.

## Controls

| DualSense | |
| --- | --- |
| Left stick | point where to go: the tank turns that way and drives |
| D-pad | the same, 8 ways: the tank turns the way pressed and drives |
| Square | reverse (straight back) |
| Cross, R1 or R2 | fire |
| Options or the touchpad | the full map (Circle there: back to the title screen) |
| Triangle (title screen) | choose a battlefield (or back to any of the five, at random) |

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
firepower/build.sh sim firepower/sim/trees.txt   # under the trees, up to their trunks
firepower/build.sh sim firepower/sim/turret.txt  # a turret fires at the tank; the tank destroys it
firepower/build.sh sim firepower/sim/gameover.txt   # all three tanks lost
firepower/build.sh sim firepower/sim/heli.txt    # a helicopter attacks; the tank shoots it down
firepower/build.sh sim firepower/sim/wall.txt    # shooting a wall open
firepower/build.sh sim firepower/sim/flag.txt    # the bunkers, the HQ, the flag, home: won
firepower/build.sh sim firepower/sim/men.txt     # men out of a building, aboard, rescued; run over
firepower/build.sh sim firepower/sim/extra.txt   # 15 men rescued: an extra tank
firepower/build.sh sim firepower/sim/fuel.txt    # out of fuel, an enemy depot, the garage
firepower/build.sh sim firepower/sim/garage.txt  # the garage's walls: out only at the bottom
firepower/build.sh sim firepower/sim/ruin.txt    # a ruin: its corners block, its middle doesn't
```

(The scripts press Triangle first, so they play battlefield 1-1.) It runs the same game code with a scripted controller (see `sim/sim_main.cpp`
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
