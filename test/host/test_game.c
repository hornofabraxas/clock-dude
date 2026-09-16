// SPDX-License-Identifier: MIT
//
// Host-side tests for the rules. Builds with a plain compiler because game.c
// and levels.c do not touch the Pebble SDK.
//
//   make -C test/host && test/host/test_game

#include "../../src/c/game.h"
#include "../../src/c/levels.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks = 0, failures = 0;

#define CHECK(cond, ...)                                     \
  do {                                                       \
    checks++;                                                \
    if (!(cond)) {                                           \
      failures++;                                            \
      printf("  FAIL %s:%d  ", __FILE__, __LINE__);          \
      printf(__VA_ARGS__);                                   \
      printf("\n");                                          \
    }                                                        \
  } while (0)

// ---------------------------------------------------------------------------
// Scratch levels
// ---------------------------------------------------------------------------

//! Load a scratch level, taking the row count from the array itself.
#define LOAD(rows) load_text((rows), (int)(sizeof(rows) / sizeof((rows)[0])))

static Game load_text(const char *const *rows, int n) {
  Game g;
  memset(&g, 0, sizeof(g));
  Level lv = { rows, n };
  if (!game_load(&g, &lv)) {
    printf("  FAIL could not load scratch level\n");
    failures++;
  }
  return g;
}

// ---------------------------------------------------------------------------
// Bundled level integrity
// ---------------------------------------------------------------------------

static void test_bundled_levels(void) {
  printf("bundled levels\n");
  CHECK(levels_count() == 11, "expected 11 levels, got %d", levels_count());

  for (int i = 0; i < levels_count(); i++) {
    const Level *lv = levels_get(i);
    CHECK(lv != NULL, "level %d missing", i + 1);
    if (!lv) continue;

    Game g;
    memset(&g, 0, sizeof(g));
    CHECK(game_load(&g, lv), "level %d failed to load", i + 1);
    if (!g.map) continue;

    CHECK(g.w <= GAME_MAX_W && g.h <= GAME_MAX_H, "level %d is %ux%u, over the cap",
          i + 1, g.w, g.h);

    int doors = 0, blocks = 0;
    for (int y = 0; y < g.h; y++)
      for (int x = 0; x < g.w; x++) {
        if (game_at(&g, x, y) == TILE_DOOR) doors++;
        if (game_at(&g, x, y) == TILE_BLOCK) blocks++;
      }
    CHECK(doors == 1, "level %d has %d doors, expected 1", i + 1, doors);
    CHECK(blocks > 0, "level %d has no blocks", i + 1);
    CHECK(!g.won, "level %d starts already won", i + 1);
    CHECK(game_at(&g, g.px, g.py + 1) != TILE_EMPTY,
          "level %d starts the dude in mid-air", i + 1);
    game_free(&g);
  }

  CHECK(levels_get(-1) == NULL, "negative index should be NULL");
  CHECK(levels_get(levels_count()) == NULL, "index past the end should be NULL");
}

static void test_malformed_levels(void) {
  printf("malformed levels\n");
  Game g;

  static const char *const no_dude[] = { "###", "#D#", "###" };
  memset(&g, 0, sizeof(g));
  Level a = { no_dude, 3 };
  CHECK(!game_load(&g, &a), "a level with no dude must be rejected");

  static const char *const two_dudes[] = { "#####", "#< >#", "#####" };
  memset(&g, 0, sizeof(g));
  Level b = { two_dudes, 3 };
  CHECK(!game_load(&g, &b), "a level with two dudes must be rejected");

  static const char *const bad_char[] = { "#####", "#<X_#", "#####" };
  memset(&g, 0, sizeof(g));
  Level c = { bad_char, 3 };
  CHECK(!game_load(&g, &c), "an unknown legend character must be rejected");

  static const char *const no_door[] = { "#####", "#  <#", "#####" };
  memset(&g, 0, sizeof(g));
  Level nd = { no_door, 3 };
  CHECK(!game_load(&g, &nd), "a level with no door cannot be won and must be rejected");

  static const char *const two_doors[] = { "#####", "#D<D#", "#####" };
  memset(&g, 0, sizeof(g));
  Level td = { two_doors, 3 };
  CHECK(!game_load(&g, &td), "a level with two doors is ambiguous and must be rejected");

  static const char *const starts_won[] = { "#####", "# < #", "##D##", "#####" };
  memset(&g, 0, sizeof(g));
  Level sw = { starts_won, 4 };
  CHECK(!game_load(&g, &sw), "a level the dude falls straight into must be rejected");

  memset(&g, 0, sizeof(g));
  CHECK(!game_load(&g, NULL), "a NULL level must be rejected");

  static const char *const empty[] = { "" };
  memset(&g, 0, sizeof(g));
  Level d = { empty, 1 };
  CHECK(!game_load(&g, &d), "a zero-width level must be rejected");
}

// ---------------------------------------------------------------------------
// Walking, turning, climbing
// ---------------------------------------------------------------------------

static void test_turn_then_walk(void) {
  printf("turning and walking\n");
  static const char *const rows[] = {
    "        ",
    "  <     ",
    "########",
    "D",  // unreachable, keeps the level legal
  };
  Game g = LOAD(rows);
  CHECK(g.dir == -1, "dude should start facing left");
  int x0 = g.px;

  // Open ground ahead: one input both turns and steps.
  CHECK(game_walk(&g, 1), "walking right should register");
  CHECK(g.dir == 1, "facing should be right");
  CHECK(g.px == x0 + 1, "dude should have turned and stepped in one action");
  CHECK(g.moves == 1, "the step should score one move");

  game_free(&g);
}

//! Facing away from a wall, the first input only turns; it costs no move and
//! does not climb. That is what makes the second input climb the step.
static void test_turn_against_a_wall_is_free(void) {
  printf("turning to face a wall\n");
  static const char *const rows[] = {
    "      ",
    "      ",
    " <#   ",
    "######",
    "D",  // unreachable, keeps the level legal
  };
  Game g = LOAD(rows);
  g.auto_climb = true;
  int x0 = g.px, y0 = g.py;

  CHECK(game_walk(&g, 1), "turning to face the wall should register");
  CHECK(g.dir == 1, "facing should be right");
  CHECK(g.px == x0 && g.py == y0, "turning to face a wall must not move the dude");
  CHECK(g.moves == 0, "a turn on the spot must not score a move");

  CHECK(game_walk(&g, 1), "the second input should climb");
  CHECK(g.px == x0 + 1 && g.py == y0 - 1, "dude should be on top of the step");
  CHECK(g.moves == 1, "the climb should score one move");

  game_free(&g);
}

static void test_gravity(void) {
  printf("gravity\n");
  static const char *const rows[] = {
    "  >     ",
    "###     ",
    "        ",
    "########",
    "D",  // unreachable, keeps the level legal
  };
  Game g = LOAD(rows);
  CHECK(g.py == 0, "dude should start on the ledge");
  CHECK(game_walk(&g, 1), "walking off the ledge should register");
  CHECK(g.px == 3, "dude should be one cell right");
  CHECK(g.py == 2, "dude should have fallen to the floor");
  game_free(&g);
}

static void test_auto_climb(void) {
  printf("auto-climb\n");
  static const char *const rows[] = {
    "      ",
    "      ",
    " >#   ",
    "######",
    "D",  // unreachable, keeps the level legal
  };
  Game g = LOAD(rows);
  g.auto_climb = false;

  int x0 = g.px, y0 = g.py;
  CHECK(!game_walk(&g, 1), "walking into a step with auto-climb off does nothing");
  CHECK(g.px == x0 && g.py == y0, "dude should not have moved");
  CHECK(game_climb(&g), "an explicit climb should still work");
  CHECK(g.px == x0 + 1 && g.py == y0 - 1, "dude should be on top of the step");

  game_free(&g);

  Game h = LOAD(rows);
  h.auto_climb = true;
  x0 = h.px; y0 = h.py;
  CHECK(game_walk(&h, 1), "walking into a step with auto-climb on should climb");
  CHECK(h.px == x0 + 1, "dude should be on top of the step");
  CHECK(h.py == y0 - 1, "dude should be one row higher");
  game_free(&h);
}

static void test_climb_needs_headroom(void) {
  printf("climb clearance\n");
  static const char *const rows[] = {
    "  ##  ",
    "   #  ",
    " > #  ",
    "######",
    "D",  // unreachable, keeps the level legal
  };
  Game g = LOAD(rows);
  g.auto_climb = true;
  CHECK(!game_climb(&g), "climbing with a ceiling overhead must be refused");
  game_free(&g);
}

// ---------------------------------------------------------------------------
// Blocks
// ---------------------------------------------------------------------------

static void test_pickup_and_drop(void) {
  printf("pick up and drop\n");
  static const char *const rows[] = {
    "        ",
    "        ",
    "  >o    ",
    "########",
    "D",  // unreachable, keeps the level legal
  };
  Game g = LOAD(rows);
  CHECK(game_at(&g, 3, 2) == TILE_BLOCK, "block should be ahead");
  CHECK(game_act(&g), "pick up should succeed");
  CHECK(g.carry, "dude should be carrying");
  CHECK(game_at(&g, 3, 2) == TILE_EMPTY, "the block cell should be cleared");
  CHECK(g.moves == 1, "a pick up should score a move");

  CHECK(game_act(&g), "drop should succeed");
  CHECK(!g.carry, "dude should no longer be carrying");
  CHECK(game_at(&g, 3, 2) == TILE_BLOCK, "the block should have fallen back to the floor");
  game_free(&g);
}

static void test_pickup_needs_clearance(void) {
  printf("pick up clearance\n");
  static const char *const rows[] = {
    "        ",
    "   #    ",
    "  >o    ",
    "########",
    "D",  // unreachable, keeps the level legal
  };
  Game g = LOAD(rows);
  CHECK(!game_act(&g), "picking up a block with something above it must be refused");
  CHECK(!g.carry, "dude should not be carrying");
  game_free(&g);
}

static void test_carry_refuses_low_ceiling(void) {
  printf("carrying under a low ceiling\n");
  static const char *const rows[] = {
    "        ",
    "    #   ",
    "  >o    ",
    "########",
    "D",  // unreachable, keeps the level legal
  };
  Game g = LOAD(rows);
  CHECK(game_act(&g), "pick up should succeed");
  CHECK(g.carry, "dude should be carrying");

  CHECK(game_walk(&g, 1), "the first step has headroom and should succeed");
  CHECK(g.px == 3, "dude should have stepped into the cleared cell");

  int x0 = g.px;
  CHECK(!game_walk(&g, 1), "walking under a low ceiling while carrying must be refused");
  CHECK(g.px == x0, "dude should not have moved");
  CHECK(g.carry, "the carried block must not be lost");

  // The blocked cell is exactly where a forward drop would put the block, so
  // dropping forward is refused for the same reason.
  CHECK(!game_act(&g), "dropping into the blocked cell must be refused");
  CHECK(g.carry, "a refused drop must not lose the block");

  // Turning back gives the block somewhere to go.
  CHECK(game_walk(&g, -1), "turning back should register");
  CHECK(game_act(&g), "dropping the other way should succeed");
  CHECK(!g.carry, "the block should be down");
  game_free(&g);
}

static void test_block_never_plugs_the_door(void) {
  printf("blocks and the door\n");
  static const char *const rows[] = {
    "        ",
    "        ",
    "  >o    ",
    "  #D#   ",
    "########",
  };
  Game g = LOAD(rows);
  CHECK(game_act(&g), "pick up should succeed");
  CHECK(game_act(&g), "drop should succeed");
  CHECK(game_at(&g, 3, 3) == TILE_DOOR, "the door must survive a block dropped onto it");
  CHECK(game_at(&g, 3, 2) == TILE_BLOCK, "the block should rest on top of the door");
  game_free(&g);
}

static void test_win_on_door(void) {
  printf("reaching the door\n");
  static const char *const rows[] = {
    "      ",
    " > D  ",
    "######",
  };
  Game g = LOAD(rows);
  CHECK(!g.won, "should not start won");
  game_walk(&g, 1);
  CHECK(!g.won, "not there yet");
  game_walk(&g, 1);
  CHECK(g.won, "standing on the door should win");
  CHECK(!game_walk(&g, 1), "a won level should ignore further input");
  game_free(&g);
}

// ---------------------------------------------------------------------------
// Undo and redo
// ---------------------------------------------------------------------------

typedef struct {
  int px, py, dir, carry, moves, won;
  uint8_t map[GAME_MAX_W * GAME_MAX_H];
} Snap;

static void snap(const Game *g, Snap *s) {
  s->px = g->px; s->py = g->py; s->dir = g->dir;
  s->carry = g->carry; s->moves = g->moves; s->won = g->won;
  memcpy(s->map, g->map, (size_t)g->w * g->h);
}

static int snap_eq(const Game *g, const Snap *a, const Snap *b) {
  return a->px == b->px && a->py == b->py && a->dir == b->dir &&
         a->carry == b->carry && a->moves == b->moves && a->won == b->won &&
         memcmp(a->map, b->map, (size_t)g->w * g->h) == 0;
}

//! Drive every bundled level through a long pseudo-random sequence of legal
//! inputs, then rewind the entire history and replay it, checking the board
//! against a snapshot at every step in both directions.
static void test_undo_redo_roundtrip(void) {
  printf("undo/redo round trip\n");
  enum { STEPS = 400 };
  static Snap snaps[STEPS + 1];

  for (int i = 0; i < levels_count(); i++) {
    Game g;
    memset(&g, 0, sizeof(g));
    if (!game_load(&g, levels_get(i))) continue;
    g.auto_climb = true;

    unsigned seed = 12345u + (unsigned)i * 977u;
    int taken = 0;
    snap(&g, &snaps[0]);

    for (int s = 0; s < STEPS && !g.won; s++) {
      seed = seed * 1103515245u + 12345u;
      switch ((seed >> 16) & 3) {
        case 0: if (!game_walk(&g, -1)) continue; break;
        case 1: if (!game_walk(&g, 1))  continue; break;
        case 2: if (!game_climb(&g))    continue; break;
        default: if (!game_act(&g))     continue; break;
      }
      snap(&g, &snaps[++taken]);
    }
    CHECK(taken > 20, "level %d only produced %d moves; the walk is too short to be a test",
          i + 1, taken);

    int rewound = 0;
    while (game_can_undo(&g)) {
      game_undo(&g);
      rewound++;
      Snap now;
      snap(&g, &now);
      CHECK(snap_eq(&g, &now, &snaps[taken - rewound]),
            "level %d: undo %d did not restore the recorded state", i + 1, rewound);
      if (!snap_eq(&g, &now, &snaps[taken - rewound])) break;
    }
    CHECK(rewound == taken, "level %d: undid %d of %d moves", i + 1, rewound, taken);

    Snap start;
    snap(&g, &start);
    CHECK(snap_eq(&g, &start, &snaps[0]),
          "level %d: full undo did not return to the starting position", i + 1);
    CHECK(g.moves == 0, "level %d: move count should be back to zero, got %u", i + 1, g.moves);

    int replayed = 0;
    while (game_can_redo(&g)) {
      game_redo(&g);
      replayed++;
      Snap now;
      snap(&g, &now);
      CHECK(snap_eq(&g, &now, &snaps[replayed]),
            "level %d: redo %d did not reproduce the recorded state", i + 1, replayed);
      if (!snap_eq(&g, &now, &snaps[replayed])) break;
    }
    CHECK(replayed == taken, "level %d: redid %d of %d moves", i + 1, replayed, taken);

    game_free(&g);
  }
}

static void test_new_move_drops_redo(void) {
  printf("diverging clears redo\n");
  static const char *const rows[] = {
    "          ",
    " >        ",
    "##########",
    "D",  // unreachable, keeps the level legal
  };
  Game g = LOAD(rows);
  game_walk(&g, 1);
  game_walk(&g, 1);
  CHECK(game_can_undo(&g), "should have history");
  game_undo(&g);
  CHECK(game_can_redo(&g), "should have a redo available");
  game_walk(&g, -1);
  CHECK(!game_can_redo(&g), "diverging must discard the redo tail");
  game_free(&g);
}

static void test_undo_on_empty_history(void) {
  printf("undo with nothing to undo\n");
  static const char *const rows[] = { "    ", " >  ", "####", "D" };
  Game g = LOAD(rows);
  CHECK(!game_can_undo(&g), "a fresh level has nothing to undo");
  CHECK(!game_undo(&g), "undo should report that it did nothing");
  CHECK(!game_can_redo(&g), "a fresh level has nothing to redo");
  CHECK(!game_redo(&g), "redo should report that it did nothing");
  game_free(&g);
}

//! Deep history: push well past the growth chunk to exercise reallocation.
static void test_long_history(void) {
  printf("long history\n");
  static const char *const rows[] = {
    "                    ",
    " >                  ",
    "####################",
    "D",  // unreachable, keeps the level legal
  };
  Game g = LOAD(rows);
  int n = 0;
  for (int i = 0; i < 600; i++) {
    if (game_walk(&g, (i & 1) ? 1 : -1)) n++;
  }
  CHECK(n > 400, "expected a long history, got %d records", n);
  int undone = 0;
  while (game_can_undo(&g)) { game_undo(&g); undone++; }
  CHECK(undone == n, "undid %d of %d records", undone, n);
  CHECK(g.px == 1 && g.dir == 1, "full undo should restore the starting pose");
  game_free(&g);
}


//! Restarting must put every crate back, not just zero the counters.
static void test_restart_rebuilds_the_board(void) {
  printf("restart rebuilds the board\n");
  Game g;
  memset(&g, 0, sizeof(g));
  CHECK(game_load(&g, levels_get(0)), "level 1 should load");
  g.auto_climb = false;

  uint8_t before[GAME_MAX_W * GAME_MAX_H];
  memcpy(before, g.map, (size_t)g.w * g.h);
  int px0 = g.px, py0 = g.py, dir0 = g.dir;

  // Shift a crate somewhere else entirely.
  int acted = 0;
  for (int i = 0; i < 60 && !g.won; i++) {
    if (game_act(&g)) { acted++; if (acted >= 2) break; }
    if (!game_walk(&g, -1)) game_climb(&g);
  }
  CHECK(acted > 0, "the walk should have moved at least one crate");
  CHECK(memcmp(before, g.map, (size_t)g.w * g.h) != 0, "the board should differ before restart");

  CHECK(game_restart(&g), "restart should succeed");
  CHECK(memcmp(before, g.map, (size_t)g.w * g.h) == 0, "restart must restore every crate");
  CHECK(g.px == px0 && g.py == py0 && g.dir == dir0, "restart must restore the starting pose");
  CHECK(g.moves == 0, "restart must clear the move count");
  CHECK(!g.carry, "restart must empty the dude's hands");
  CHECK(!game_can_undo(&g), "restart must clear the undo history");
  CHECK(!game_can_redo(&g), "restart must clear the redo history");
  CHECK(!g.auto_climb, "restart must not disturb the auto-climb setting");
  game_free(&g);

  Game empty;
  memset(&empty, 0, sizeof(empty));
  CHECK(!game_restart(&empty), "restarting with no level loaded should fail cleanly");
}

//! A malformed level must leave the game that is already loaded untouched.
static void test_failed_load_preserves_state(void) {
  printf("failed load leaves the game alone\n");
  Game g;
  memset(&g, 0, sizeof(g));
  CHECK(game_load(&g, levels_get(2)), "level 3 should load");
  g.auto_climb = false;
  game_walk(&g, -1);

  int px = g.px, py = g.py;
  uint16_t moves = g.moves;
  const Level *lv = g.lv;

  static const char *const junk[] = { "#####", "#<X_#", "#####" };
  Level bad = { junk, 3 };
  CHECK(!game_load(&g, &bad), "the malformed level must be rejected");
  CHECK(g.map != NULL, "the previous board must survive");
  CHECK(g.px == px && g.py == py, "the dude must not move");
  CHECK(g.moves == moves, "the move count must not change");
  CHECK(g.lv == lv, "the loaded level must not change");
  CHECK(!g.auto_climb, "a failed load must not disturb the auto-climb setting");
  game_free(&g);
}

//! Gravity must not carry the dude past the door. The door is walkable, so a
//! naive fall runs straight through the only winning cell.
static void test_falling_into_the_door_wins(void) {
  printf("falling into the door\n");
  static const char *const rows[] = {
    "  >D  ",
    "###   ",
    "######",
  };
  Game g = LOAD(rows);
  CHECK(!g.won, "should not start won");
  CHECK(game_walk(&g, 1), "stepping onto the door should register");
  CHECK(g.py == 0, "the dude must stop in the doorway, not fall through it");
  CHECK(g.won, "stepping into the door should win even with a drop below it");
  game_free(&g);
}

//! A refused move must leave the history exactly as it was.
static void test_refused_move_records_nothing(void) {
  printf("refused moves are not recorded\n");
  static const char *const rows[] = {
    "        ",
    "    #   ",
    "  >o    ",
    "########",
    "D",
  };
  Game g = LOAD(rows);
  CHECK(game_act(&g), "pick up should succeed");
  CHECK(game_walk(&g, 1), "the first step has headroom");
  const uint16_t moves = g.moves;

  CHECK(!game_walk(&g, 1), "the low ceiling must refuse the step");
  CHECK(g.moves == moves, "a refused move must not score");
  CHECK(game_undo(&g), "undo should step back to before the walk");
  CHECK(g.px == 2, "undo must reverse the walk, not the refused move");
  game_free(&g);
}

//! The undo stack is bounded, so a marathon session cannot exhaust the heap.
//! Past the ceiling the oldest records are forgotten, never the whole stack.
static void test_history_is_bounded(void) {
  printf("history ceiling\n");
  static const char *const rows[] = {
    "                    ",
    " >                  ",
    "####################",
    "D",
  };
  Game g = LOAD(rows);
  for (int i = 0; i < 6000; i++) game_walk(&g, (i & 1) ? 1 : -1);

  int undone = 0;
  while (game_can_undo(&g)) { game_undo(&g); undone++; }
  CHECK(undone > 1000, "a deep undo stack should survive, got %d", undone);
  CHECK(undone <= 2000, "the stack must stay bounded, got %d", undone);
  CHECK(g.px >= 1, "the dude should still be somewhere legal, at %d", g.px);
  game_free(&g);
}

//! An input the rules refuse must cost nothing at all, including at the history
//! ceiling where making room means forgetting the oldest records.
static void test_refused_move_costs_no_history(void) {
  printf("refused moves cost no history\n");
  static const char *const rows[] = {
    "                    ",
    " >                  ",
    "####################",
    "D",
  };
  Game g = LOAD(rows);
  g.auto_climb = false;

  // Fill the stack to its ceiling.
  for (int i = 0; i < 6000; i++) game_walk(&g, (i & 1) ? 1 : -1);

  int depth = 0;
  while (game_can_undo(&g)) { game_undo(&g); depth++; }
  while (game_can_redo(&g)) game_redo(&g);
  CHECK(depth > 1000, "expected a deep stack, got %d", depth);

  // Face the left wall, then keep pushing into it. Each of these is refused.
  while (game_walk(&g, -1)) { }
  int after = 0;
  while (game_can_undo(&g)) { game_undo(&g); after++; }
  while (game_can_redo(&g)) game_redo(&g);

  const int before = after;
  for (int i = 0; i < 20; i++) CHECK(!game_walk(&g, -1), "walking into the wall must be refused");
  CHECK(!game_climb(&g), "climbing the wall must be refused");
  CHECK(!game_act(&g), "there is nothing to pick up");

  int now = 0;
  while (game_can_undo(&g)) { game_undo(&g); now++; }
  CHECK(now == before, "refused input must not trim history: %d then %d", before, now);
  game_free(&g);
}

// ---------------------------------------------------------------------------

int main(void) {
  test_bundled_levels();
  test_malformed_levels();
  test_turn_then_walk();
  test_turn_against_a_wall_is_free();
  test_gravity();
  test_auto_climb();
  test_climb_needs_headroom();
  test_pickup_and_drop();
  test_pickup_needs_clearance();
  test_carry_refuses_low_ceiling();
  test_block_never_plugs_the_door();
  test_win_on_door();
  test_falling_into_the_door_wins();
  test_refused_move_records_nothing();
  test_history_is_bounded();
  test_refused_move_costs_no_history();
  test_undo_redo_roundtrip();
  test_new_move_drops_redo();
  test_undo_on_empty_history();
  test_long_history();
  test_restart_rebuilds_the_board();
  test_failed_load_preserves_state();

  printf("\n%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
