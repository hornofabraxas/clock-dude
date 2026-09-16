# Design notes

Decisions that are not obvious from the code, and the reasoning behind them.

## Why not fit the whole level on screen

The obvious approach is to pick a tile size per level so the whole board fits.
It does not work. The largest level is 29 by 19 tiles, and fitting that on
either watch lands at 6 or 7 pixels per tile. At that size the dude is a
two-colour blob with no legs and no facing cue, and you cannot tell whether he
is carrying a crate. The smallest tile at which the sprite reads is 10 pixels,
and no bundled level fits whole above 11.

So the zoom ladder is fixed at 18, 24 and 30 pixels per tile with a camera that
follows the dude. Holding SELECT drops to 12 for as long as you hold, which
shows roughly half of even the largest level. That is a deliberate floor: 12 is
above the readability threshold, and going lower to show more would trade away
the thing the peek exists to help you do.

Settings names the three rungs FURTHER, DEFAULT and CLOSER rather than printing
the pixel counts. A player choosing between 18 and 24 pixels per tile has no way
to picture the difference before they have seen it, and which way the camera
moves is the only thing the setting actually decides.

Rough context at each step, measured against the largest level:

| Tile | Visible on Time 2 | Share of level 11 |
| --- | --- | --- |
| 30px | 6 x 6 | 6% |
| 24px | 8 x 8 | 11% |
| 18px | 11 x 11 | 21% |
| 12px | 16 x 17 | 49% |

## Why raw touch instead of the navigation bridge

PebbleOS has a touch navigation bridge that turns swipes and taps into button
presses for apps that opt in. It is the right thing for lists, and the menus
here use it. It is the wrong thing for the board, for two reasons: it collapses
every tap on a hand-drawn window to a plain SELECT, and it reserves a
left-to-right swipe for BACK. This game needs that swipe to mean "walk right".

So the play screen subscribes to raw touch instead. A raw subscriber suppresses
the bridge only while it is installed, which is exactly the behaviour wanted:
the bridge is on for the menus and off on the board, with no global switch to
keep in sync.

The cost is that the gesture recognition has to be written out by hand. The
thresholds in `gesture.c` are taken from the firmware's own recognizers so the
feel matches the rest of the watch: 30 pixels of travel on the major axis, 300
milliseconds maximum, and a straightness test that fails the swipe once the
path commits and then wanders. One firmware detail is easy to miss and breaks
everything if you do: the driver reports liftoff at (0, 0) rather than at the
finger, so the end of a gesture has to come from the last position update.

A contact that outlives that window is neither a swipe nor a tap and reports
nothing at all, so resting a finger on the screen to study the level never moves
the dude. Peek used to live there and does not any more, for exactly that
reason: the screen is where every movement input lives, and a finger parked on
it to widen the view was one slip away from walking off a stack. It is on a held
SELECT instead, where nothing else can be triggered by accident.

SELECT carries peek and nothing else, which is the point. A button that has to
tell a press from a hold cannot report either until the hold has been ruled out,
so anything sharing SELECT would put a third of a second of stillness between
the press and the view widening, and stillness is exactly how a watch tells you
it missed the button. Peek runs off the raw press instead. Undo moves to a held
UP, where the hold rewinds continuously rather than costing one hold per move,
and UP pays the cost that SELECT no longer does: zooming in acts on release.

## Why the menus are not lists that scroll

Every menu but the level picker has three or four rows and fits its window with
nothing left over. A MenuLayer still treats such a list as scrollable: the
bottom pad makes the content taller than the view, and the selection is pulled
towards a comfortable zone as the cursor descends, so a menu that has nothing to
reveal moves anyway. The pad is turned off and the selection is moved with
`MenuRowAlignNone`, which is documented as leaving the scroll offset alone.

The selection animation goes the same way. The firmware draws the outgoing and
incoming cells part-highlighted while it runs, which is what makes the plank
look like it is being dragged between rows, and there is no knob to turn it off.
Two changes remove it: the list drives `menu_layer_set_selected_next` itself
with `animated` false, and rows decide whether they are highlighted by asking
the MenuLayer for its selected index rather than asking the cell whether the
animation has reached it. Neither costs any of the behaviour that made a
MenuLayer worth using: drag to scroll, tap to pick and swipe right to go back
all belong to the widget, not to its click config.

## Why screens do not slide

They used to. The firmware's window stack animates a push, and on the watch it
stuttered: every frame of a slide re-runs the update procs of both windows, and
these screens are drawn from primitives rather than blitted, so a frame is a few
hundred fills and a dozen text draws. Two rounds of making the frame cheaper did
not fix it, so the slide is gone. Every push and every pop is now instant.

The cheaper drawing stayed, because it is paid on every redraw and not only
during a transition that no longer happens.

The title band used to draw its text nine times, once in white and once per
neighbouring pixel for the outline. Four diagonal passes do the same job: a
diagonal pass lands ink directly above and below the glyph wherever it is two
pixels wide at that row, which for a bold face is everywhere, so the corners
cover the edges too. Text is the most expensive thing on the screen and that is
four fewer draws of a 28 pixel face.

The band itself was a nested loop calling `render_wall` per tile, nine fills
each. Every horizontal in the brick pattern runs the full width of its tile, so
the band draws them as one span per course instead, and only the verticals stay
per tile. The spans run over the black right-hand edge of each tile and the
verticals are laid down afterwards to put it back, which is what keeps the
result identical to the per-tile version.

Turning the slide off means taking BACK over on every screen. The firmware's own
back handler pops with the animation switched on, so a screen that arrived
instantly would still have left on a slide. The replacement removes the window
and nothing else, which is what the default does, including on the last window:
draining the stack is how the firmware is told an app has finished, and that is
what BACK on the main menu is supposed to mean.

What none of this could have been is the page turn in pebble-crossword, which
captures new content into an offscreen bitmap and blits it per frame. That is a
slide between two states of one window, which the app drives. A window stack
transition belongs to the firmware: it is started by `window_stack_push` and
there is no hook to render it from a snapshot. The only levers the app has are
the cost of a frame and whether the transition runs at all.

## Why there is no mid-level save

Pebble caps a persisted value at 256 bytes. The largest board is 551 cells, so
a level in progress does not fit in one record and would have to be split
across keys. CONTINUE resumes at the level you were on, from the top, which is
also exactly what the calculator original did. The levels are short enough that
this costs little.

## Rules that differ from other ports

**Walking while carrying is refused when the crate would not clear the
ceiling.** Some ports drop the crate behind you instead. That loses crates by
accident and is hard to read on a small screen, where the crate you just lost
may be off camera.

**Auto-climb is on by default.** The calculator needed a separate key to climb
because it had keys to spare and no gestures. Walking into a single step climbs
it here. Turning on the spot still costs an input, so the first swipe toward a
wall turns and the second climbs, which is what makes the two distinguishable.
Turn it off in settings to score moves the way the original did.

**Undo runs to the start of the level.** The original allowed exactly one undo.
On a touch screen, where a misread swipe is a real possibility, a single undo is
not enough. Undoing returns the move to the counter, so it does not corrupt a
best score.

There is no redo. Every input the player can reach is worth more spent on a
control they will use constantly, and a rewind that runs all the way back to the
start is already the whole recovery story. That rewind is literal: holding UP
keeps undoing until the history is spent or the button comes up. `game_redo` survives in the rules
engine because the host suite rewinds a level and replays it, which is what
proves undo restores the board exactly rather than merely plausibly.

The stack is capped rather than truly unlimited. At six bytes a record, an
uncapped stack on a 128 KB budget is a heap exhaustion waiting for a long
session, and the failure mode is losing the whole history mid-level. The cap is
two thousand records, about 12 KB, which no level here needs more than a small
fraction of. Space for the record is reserved before a move touches the board,
so the board and the history can never disagree.

## The clock

Elapsed time is counted with a repeating timer, not by differencing the wall
clock. The watch corrects its clock from the phone, and a correction landing
mid-level would either discard the interval it interrupted or bank an hour the
player never spent. That figure is what gets saved as a personal best, so it
has to be immune to the clock moving.

The same reasoning applies to gesture recognition, where a clock step would
otherwise let a five second drag be scored as a flick. There, a step in either
direction reports a duration that fails every test, so the gesture is dropped
rather than misread.

## Level format

Levels are plain text, one string per row, with `#` for wall, `o` for crate,
`D` for door, space for empty, and `<` or `>` for the dude and the way he
faces. Ragged rows are fine; short rows are padded with empty.

The row count in the table is derived with `ARRAY_ROWS` rather than written out,
so editing the text cannot leave the count stale and walk off the end of the
array. The host test suite checks every bundled level for exactly one dude,
exactly one door, at least one crate, and a dude who starts on solid ground.
