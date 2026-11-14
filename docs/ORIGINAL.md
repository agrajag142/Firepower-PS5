# The original Firepower: what's known

Worked out from the disk and the disassembly of its program (`firepower`, 81 KB:
Manx Aztec C, small model - a4 points at the data hunk + $7FFE, far calls go
through an 83-entry jump table at the start of the data hunk). Addresses below
are offsets in the code hunk (`sub_xxxxx`) or the data/BSS hunk (`D_`/`B_`).

## The disk

A plain AmigaDOS (OFS) disk, volume `FirePower`. `s/startup-sequence` runs
`FIREPOWER`. The `PRESS` icon program only says to boot from the disk.

| | |
| --- | --- |
| `firepower` | the game |
| `gr/` | pictures and sprites (below) |
| `MapP1/t1..t5`, `MapP2/t1..t5`, `MapP3/t1` | battlefields, 4096 bytes each |
| `sounds/` | TankBlast, Boom2, Heli, Splat, Drip: raw signed 8-bit |
| `Ins/I1..I3` | the instructions, as text |
| `Modem` | "ENTER NUMBER" (the modem game) |

## Sprite files (gr/*)

LoadGraphics (`sub_0d66c`) reads records one after another, each a 16-byte
header of big-endian words then the planes, one after another:

| word | |
| --- | --- |
| 0 | width (pixels) |
| 1 | height |
| 2 | colour planes |
| 3 | planes stored |
| 4 | mask plane (-1: none) |
| 5 | collision plane (-1: none) |
| 6 | PlanePick (-1: planes in order) |
| 7 | PlaneOnOff |

Each plane is `((width + 15) / 16) * 2 * height` bytes. Stored colour plane k
goes to the k-th bit set in PlanePick; bitplanes not picked take their bit from
PlaneOnOff (so the tanks, 4 planes with PlaneOnOff $10, use colours 16-31).

| file | records |
| --- | --- |
| tank0, shadow, scorp | 16 headings, 25 x 25 (0 north, clockwise) |
| turret | 8 gun emplacements, 8 small missiles |
| heli | 42: 16 headings and the rotor, landing frames |
| men | 7 |
| boom | 12: two small puffs, then the big explosion |
| tank_miss | the shell, 16 headings |
| numbers | 13 digits |
| tiles | 50 map tiles, 64 x 64 |
| play, pick, modem, copyright | panels (158 x 200 and so on) |

## Full screens

`fp.title`, `loadscreen`, `oneplayer`, `pickscreen`: 5 planes of 352 x 232
($27e0 bytes each, read by `sub_07eaa`), then 32 colours ($0RGB words).
`oneplayer`'s colours are the battle's palette.

## Battlefields

64 x 64 bytes, **column by column** (square x, y at `x * 64 + y`; the game's
map at `B_15d0`). Each byte is a tile type (0-92).

## The tile types (`D_05fa`, 16 bytes each)

| byte | |
| --- | --- |
| 0-3 | pointer to the tile's picture (28-byte records from `B_5770`; 27 of them are copied to `B_6cea`.. by `sub_0dad0` for the enemy's versions) |
| 4 | the type it becomes when destroyed (plain walls too, though they aren't marked destructible: 42 -> 51 -> 56, a gap) |
| 5 | 1: destructible |
| 6 | flags: bit 3 your base (type 20), bit 2 the enemy's (type 32) |
| 7 | flags: bit 6 a gate of one side (type 44), bit 7 the other's (type 67): `sub_03680` swaps in the linked type (byte 9, the open gate) when that side's tank is on the gate square or the square before it in its row; bits 0, 1 (sets $88 in an object: not yet known) |
| 7 (more) | bit 2: one of your side's turrets, bit 3 one of the enemy's (corner posts 41/43/48/50 and 64/66/71/73, bunkers 27/39, 85/87). `sub_03680` (run for each square as it's drawn: so only turrets on screen act) calls `sub_020fe`, which turns the gun one of 8 headings towards the tank (its heading kept in the low 3 bits of `B_25d0`) and says fire when it's lined up; `sub_0228e` fires, and a missile's end sets bit 3 of the square again (one missile at a time). `sub_01f82` draws the gun with its top left at the square + the tables at `D_0eaa`/`D_0eba` (x 25 26 26 27 27 19 17 19, y 16 20 27 27 27 27 27 20). Bits 0 and 1: the helipads (types 25, 37) |
| 9 | the other square of a two-square thing |
| 10 | movement: `& $78` copied into the second map `B_25d0` (bit 7 there: something standing on the square) |
| 13 | strength |
| 14 | side and kind: high nibble 1 yours, 2 the enemy's; low nibble compared by `sub_06954`; 0 open ground |

## Sounds

`sub_01c68` loads each file whole and plays it as it is. Paula periods (from the
structs at `D_024c`, `D_027c`, `D_02c4`, `D_02f4`, `D_033c`): TankBlast 480,
Boom2 700, Heli 500, Splat 300, Drip 1000 (3546895 / period samples a second).

## Strings that tell the story

"YOU HAVE WON!!! and such a glorious battle it was too...", "YOUR DEAD!!!",
"FINAL REPORT", "CASUALTIES: Tanks / Heli / Men", "RESCUED", "INFLICTED",
"[F1] Restart Game [F2] Repick Tanks [F3] Repick Type of Game".
