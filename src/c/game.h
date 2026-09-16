// SPDX-License-Identifier: MIT
//
// Block-pushing rules, with no dependency on the Pebble SDK so the whole rule
// set can be exercised by the host test suite in test/host.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "levels.h"

//! Widest and tallest level the engine accepts. The bundled levels top out at
//! 29x19; the headroom leaves space for hand-edited levels.
#define GAME_MAX_W 40
#define GAME_MAX_H 32

typedef enum {
  TILE_EMPTY = 0,
  TILE_WALL,
  TILE_BLOCK,
  TILE_DOOR,
} Tile;

typedef enum {
  ACT_NONE = 0,
  ACT_PICKUP,
  ACT_DROP,
} MoveAct;

//! One reversible step of history. A record is either a movement (turn, step,
//! climb and any fall that followed) or a block action, never both: walking is
//! refused outright when a carried block would not clear the ceiling, so a step
//! can never displace a block.
typedef struct {
  int8_t dx;      //!< net horizontal displacement
  int8_t dy;      //!< net vertical displacement, negative climbing, positive falling
  uint8_t act;    //!< MoveAct
  uint8_t bx, by; //!< block cell: source for a pickup, landing site for a drop
  bool turned;    //!< the facing flipped as part of this record
} Move;

typedef struct {
  const Level *lv; //!< the level this board was built from, for game_restart
  uint8_t w, h;
  uint8_t *map;    //!< w*h cells, owned

  int8_t px, py;
  int8_t dir;      //!< -1 facing left, +1 facing right
  bool carry;
  bool won;

  uint16_t moves;  //!< scored moves; a turn on the spot is free, as on the calculator

  //! Undo/redo history. Entries [0, hist_len) are recorded and hist_cur is the
  //! position within them: everything below the cursor has been applied. A new
  //! record truncates at the cursor, so redo is dropped the moment you diverge.
  Move *hist;
  int hist_len, hist_cur, hist_cap;

  bool auto_climb; //!< walking into a single step climbs it without a separate input
} Game;

//! Load a level. Returns false and leaves the game unusable when the level is
//! malformed or allocation fails. Safe to call on a game that already holds a
//! level; the previous one is released first.
bool game_load(Game *g, const Level *lv);

//! Release everything the game owns and zero it.
void game_free(Game *g);

//! Tile at a cell. Cells outside the level read as TILE_WALL, which is what
//! stops falls and walks at the border.
uint8_t game_at(const Game *g, int x, int y);

//! Turn to face `dir` and walk one cell that way. With auto_climb set, walking
//! into a single step climbs it instead. Returns true when anything changed.
bool game_walk(Game *g, int dir);

//! Climb the step directly ahead. Returns true when the climb happened.
bool game_climb(Game *g);

//! Pick up the block ahead, or drop the carried one ahead. Returns true when
//! the game changed.
bool game_act(Game *g);

bool game_can_undo(const Game *g);
bool game_undo(Game *g);

//! Replay a move that was undone. No input on the watch is bound to this: undo
//! runs to the start of the level and that is the whole recovery story the game
//! offers. It stays because the host suite's round trip rewinds a level and
//! replays it, which is what proves that undo restores the board exactly rather
//! than merely plausibly.
bool game_can_redo(const Game *g);
bool game_redo(Game *g);

//! Rebuild the loaded level from its text, clearing the board, the move count
//! and the history. Returns false if no level is loaded.
bool game_restart(Game *g);
