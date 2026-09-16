# Changelog

## 1.0 (2026-09-16)

First public release. The game is feature complete: eleven levels, swipe
controls, rewind, zoom, per-level records and progress that persists on the
watch. This release adds no gameplay; it is 0.3.1 with the repository opened up,
full attribution to Brandon Sterner and Block Dude CE in the README, and a CI
gate that runs the host tests, builds both watches and checks the app-image
ceiling on every push.

### 0.3.1
- Screens no longer slide. The transition stuttered on the watch and two rounds
  of making each frame cheaper did not fix it, so pushing and popping a screen
  are now instant, in both directions.

### 0.3.0
- Values read at the same size as the labels they belong to, so LVL 2, DEFAULT
  and ON are no longer set two sizes down from the row they are part of.
- The win screen leads with the figures. Moves and time are now the biggest
  thing on their rows, and the record they are measured against has a row of
  its own instead of shrinking them to share one.
- Holding SELECT peeks the instant you press it. Undo moves to a held UP, where
  it rewinds for as long as you hold rather than one move per hold.
- A lot less to draw per redraw: the title outline is four passes instead of
  eight, and the brick bands draw a span per course instead of nine fills per
  tile. Same pixels, roughly half the work.
- The launcher icon is a brick wall, matching the bands behind every title.

### 0.2.0
- The game opens on the main menu. The controls screen is still on the menu,
  where it belongs; it no longer stands between you and a first game.
- The top row reads NEW GAME until you have finished a level, instead of
  offering to continue a game that does not exist yet.
- Tap picks up and puts down a crate. The downward swipe that used to do it is
  gone, and undo has moved to SELECT.
- Hold SELECT to zoom out and look around, rather than holding the screen. A
  finger resting on the board no longer does anything at all.
- Redo is gone. Undo still runs all the way back to the start of the level.
- Zoom is FURTHER, DEFAULT and CLOSER in settings instead of a pixel count.
- The menus sit still. Short lists no longer drift under the cursor, and the
  selection no longer stretches from one row to the next as it moves.
- The selected row is highlighted across the full width of the screen.
- HOW TO PLAY fits the credit at the same size as everything else on it.
- The launcher icon is the dude, and is actually visible.

### 0.1.0
- Rules: walking, turning, climbing, carrying, gravity, and reaching the door.
- Unlimited undo and redo.
- The eleven original levels, stored as editable text.
- Swipe controls, with thresholds taken from the firmware's own recognizers so
  gestures feel the same here as anywhere else on the watch.
- Zoom at 18, 24 or 30 pixels per tile, with hold-to-peek at 12.
- Main menu, level select, settings, how to play, pause and a win screen that
  reports moves and time against your best.
- Progress, best moves and best times persist. Levels unlock in order.
- Both watches from one source tree: Time 2 and Round 2.
- Host test suite covering the rules, including an undo/redo round trip over
  every level.
