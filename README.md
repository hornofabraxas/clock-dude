# Clock Dude

[![ci](https://github.com/hornofabraxas/clock-dude/actions/workflows/ci.yml/badge.svg)](https://github.com/hornofabraxas/clock-dude/actions/workflows/ci.yml)

A block-pushing puzzle game for the Pebble Time 2 and Pebble Round 2, played
with swipes.

You are stuck in a walled maze with a door somewhere in it. You can walk, climb
a single step, and carry one crate at a time. Getting out means stacking crates
into stairs, and the crates you need are rarely where you want them.

This is a port of **Block Dude**, written by **Brandon Sterner** for the TI-83
in 1999. The eleven levels are his. The name is a watch pun and nothing more.
Full attribution is in [Credits](#credits).

## Controls

Moving the dude is done entirely on the screen. The buttons carry everything
around the move: undoing it, changing how much of the level you can see, and
getting out.

| Input | Action |
| --- | --- |
| Swipe left or right | Turn and walk that way |
| Swipe up | Climb the step ahead |
| Tap | Pick up the crate ahead, or put down the one you are holding |
| Hold SELECT | Zoom out while you hold, to see more of the level |
| Hold UP | Rewind, one move at a time, for as long as you hold |
| UP / DOWN buttons | Change the zoom level |
| BACK button | Pause |

SELECT does nothing but peek, which is why it answers the instant you press it
rather than waiting to find out whether you meant something else by it. The cost
lands on UP, where zooming in now acts when you let go.

Rewind runs to the start of the level, so you can back all the way out of one
you have painted yourself into. It gives each move back on the counter. The
stack is capped at a couple of thousand moves, which no level here comes close
to needing.

Auto-climb is on by default, so walking into a single step climbs it and the
upward swipe is only a backup. Turn it off in settings to play the way the
calculator did, where climbing is always a separate input.

## Screens

Screens do not slide. The firmware's window transition stuttered here, because
every frame of it redraws both screens from primitives rather than blitting
them, so pushing and popping are instant instead. BACK is handled per screen for
the same reason; on the main menu it still leaves the app.

## Progress

Levels unlock in order. Each one remembers your best move count and your best
time, and the clock only runs while the level is actually on screen. The top row
of the menu reads NEW GAME until you have finished something; after that it is
CONTINUE, and it takes you to the furthest level you have reached, so replaying
an earlier one from the picker never costs you your place.

Everything is stored on the watch. There is no phone app and nothing leaves the
device.

## Zoom

Three steps, called FURTHER, DEFAULT and CLOSER, which are 18, 24 and 30 pixels
per tile. UP and DOWN change it during a game and the choice is remembered, so
settings always shows where you left it. Holding SELECT drops to 12 pixels per
tile for as long as you hold, which shows roughly half of even the largest
level. Whole-level fit is not offered: the biggest level would need 6 pixel
tiles, at which the dude stops being legible.

## Building

Requires the Pebble SDK (4.33 or newer) and the `pebble` command line tool.

```
pebble build
pebble install --emulator emery
```

Both watches are built from one source tree; `emery` is the Time 2 and `gabbro`
is the Round 2.

## Tests

The rules live in `src/c/game.c` and have no dependency on the Pebble SDK, so
they are exercised by a host test suite that needs nothing but a C compiler.

```
make -C test/host run
```

It builds with the address and undefined-behaviour sanitizers on, and covers
every bundled level with a round trip that plays a long pseudo-random sequence,
rewinds the whole history checking the board at each step, then replays it
forward and checks again.

There is also a screenshot harness for driving one screen at a time on the
emulator, compiled out of normal builds:

```
CD_SHOT=4 pebble build && pebble install --emulator emery
```

## CI

Every push to `main` and every pull request runs the host tests, builds both
watch targets against a pinned SDK, and checks each app image against the
65,535 byte ceiling the firmware enforces. Pushing a `vX.Y` tag runs the same
gate, checks the tag against `package.json`, and publishes a release with the
`.pbw` attached. The build steps live in one composite action,
`.github/actions/pebble-build`, so CI and release cannot drift apart.

## Credits

**Brandon Sterner** wrote Block Dude for the TI-83 in 1999. It reached most of
the people who remember it through **PuzzPack**, the Detached Solutions flash
application that bundled it for the TI-83 Plus. The game, its rules and its
eleven level layouts are his work, and this port exists because of it.

This port was made from **[Block Dude CE](https://github.com/merthsoft/blockdudece)**,
the TI-84 Plus CE version by **Shaun McFall** of Merthsoft Creations, whose
non-asset code is released under the Unlicense. The eleven level layouts here
were transcribed from its level data into the plain text form in
`src/c/levels.c`. The rules engine, the renderer and every screen were written
for this port.

Block Dude CE's sprites are by **Aleksandr Makarov**
([iknowkingrabbit](https://www.patreon.com/iknowkingrabbit/)) and carry their
own licence, which does not permit redistribution. **None of that artwork is in
this repository.** Clock Dude has no sprite sheet at all: the dude, the crates,
the walls and the door are drawn from primitives in `src/c/render.c` at whatever
size the current zoom calls for, which is what lets one set of tiles read from
the 12 pixel peek up to 30 pixels. The only bitmap the app ships is its launcher
icon.

## Licence

Clock Dude is MIT, see [LICENSE](LICENSE). That covers everything in this
repository: the rules engine, the renderer, the screens, the tests and the
launcher icon.

The one thing here that did not originate with this port is the eleven level
layouts, which are Brandon Sterner's design and were transcribed from Block Dude
CE's Unlicensed level data. No code and no artwork from Block Dude CE or any
other version is included.
