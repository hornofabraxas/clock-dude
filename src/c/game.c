// SPDX-License-Identifier: MIT
//
// The rules, kept free of the Pebble SDK so test/host can exercise them.
//
// Behaviour follows Brandon Sterner's original, with one deliberate difference
// and one option:
//
//   * Walking while carrying a block is REFUSED when the block would not clear
//     the ceiling at the destination. Some ports instead drop the block behind
//     you, which loses blocks by accident and is hard to read on a small screen.
//   * auto_climb makes walking into a single step climb it, so one swipe does
//     what the calculator needed two keys for. Turning on the spot still costs
//     an input, so the first swipe toward a wall turns and the second climbs.

#include "game.h"

#include <stdlib.h>
#include <string.h>

#define HIST_CHUNK 128
//! Ceiling on the undo stack. At six bytes a record this is 12 KB of a 128 KB
//! app budget, and no level here is solvable in anything close to this many
//! moves, so in practice undo reaches the start of the level. Past the ceiling
//! the oldest quarter is forgotten rather than the whole stack being lost.
#define HIST_MAX 2000

// ---------------------------------------------------------------------------
// Cells
// ---------------------------------------------------------------------------

uint8_t game_at(const Game *g, int x, int y) {
  if (!g || !g->map) return TILE_WALL;
  if (x < 0 || y < 0 || x >= (int)g->w || y >= (int)g->h) return TILE_WALL;
  return g->map[(size_t)y * g->w + (size_t)x];
}

static void set_at(Game *g, int x, int y, uint8_t t) {
  if (x < 0 || y < 0 || x >= (int)g->w || y >= (int)g->h) return;
  g->map[(size_t)y * g->w + (size_t)x] = t;
}

//! Walkable: the door counts, so the dude can step into it and win.
static bool is_open(const Game *g, int x, int y) {
  uint8_t t = game_at(g, x, y);
  return t == TILE_EMPTY || t == TILE_DOOR;
}

//! Free for a block to occupy. The door does NOT count: a dropped block must
//! never plug the exit.
static bool is_vacant(const Game *g, int x, int y) {
  return game_at(g, x, y) == TILE_EMPTY;
}

// ---------------------------------------------------------------------------
// History
// ---------------------------------------------------------------------------

//! Can a record be taken? Called BEFORE a move touches the board, so a move
//! that cannot be recorded is refused outright rather than applied with no way
//! to undo it.
//!
//! Only growth can fail, so only growth happens here. Trimming at the ceiling
//! always succeeds and is left to hist_push, which matters: this runs on every
//! attempted input, including the ones the rules go on to refuse, and trimming
//! here would charge a player 500 undo records for walking into a wall.
static bool hist_can_record(Game *g) {
  if (g->hist_cur < g->hist_cap) return true;
  if (g->hist_cap >= HIST_MAX) return true;  // hist_push will make room by trimming

  int cap = g->hist_cap + HIST_CHUNK;
  if (cap > HIST_MAX) cap = HIST_MAX;
  Move *grown = (Move *)realloc(g->hist, (size_t)cap * sizeof(Move));
  if (!grown) return false;
  g->hist = grown;
  g->hist_cap = cap;
  return true;
}

//! Append a record at the cursor, dropping any redo tail, trimming the oldest
//! records if the stack has reached its ceiling.
static void hist_push(Game *g, const Move *m) {
  if (g->hist_cur < g->hist_len) g->hist_len = g->hist_cur;

  if (g->hist_cur >= g->hist_cap) {
    // At the ceiling. Forget the oldest quarter so a marathon session keeps a
    // deep undo stack rather than losing all of it.
    const int drop = HIST_MAX / 4;
    if (g->hist_len > drop) {
      memmove(g->hist, g->hist + drop, (size_t)(g->hist_len - drop) * sizeof(Move));
      g->hist_len -= drop;
      g->hist_cur -= drop;
    } else {
      g->hist_len = g->hist_cur = 0;
    }
  }

  g->hist[g->hist_len++] = *m;
  g->hist_cur = g->hist_len;
}

//! A record scores a move unless it was only a change of facing.
static bool scores(const Move *m) {
  return m->dx != 0 || m->dy != 0 || m->act != ACT_NONE;
}

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

void game_free(Game *g) {
  if (!g) return;
  free(g->map);
  free(g->hist);
  memset(g, 0, sizeof(*g));
}

//! Drop the dude straight down until something solid is underfoot, or until he
//! lands in the doorway. Falling THROUGH the door would be a missed win: the
//! door is walkable so gravity would carry him past it to the floor below, and
//! the win test only ever looks at where he came to rest.
static int settle(Game *g) {
  int fell = 0;
  while (game_at(g, g->px, g->py) != TILE_DOOR && is_open(g, g->px, g->py + 1)) {
    g->py++;
    fell++;
  }
  return fell;
}

//! Parse a level into a freshly zeroed game. On any failure the caller frees
//! `out`; nothing else is touched, which is what lets game_load leave the
//! caller's game alone when a level turns out to be malformed.
static bool parse_level(Game *out, const Level *lv) {
  if (!lv || !lv->rows || lv->nrows <= 0 || lv->nrows > GAME_MAX_H) return false;

  size_t w = 0;
  for (int r = 0; r < lv->nrows; r++) {
    if (!lv->rows[r]) return false;
    size_t len = strlen(lv->rows[r]);
    if (len > w) w = len;
  }
  if (w == 0 || w > GAME_MAX_W) return false;

  out->w = (uint8_t)w;
  out->h = (uint8_t)lv->nrows;
  out->map = (uint8_t *)calloc((size_t)out->w * out->h, 1);
  if (!out->map) return false;

  int found = 0;
  int doors = 0;
  for (int r = 0; r < lv->nrows; r++) {
    const char *row = lv->rows[r];
    for (size_t col = 0; row[col]; col++) {
      uint8_t t = TILE_EMPTY;
      switch (row[col]) {
        case '#': t = TILE_WALL;  break;
        case 'o': t = TILE_BLOCK; break;
        case 'D': t = TILE_DOOR; doors++; break;
        case '<':
        case '>':
          if (++found > 1) return false;
          out->px = (int8_t)col;
          out->py = (int8_t)r;
          out->dir = (row[col] == '>') ? 1 : -1;
          break;
        case ' ': break;
        default: return false;
      }
      out->map[(size_t)r * out->w + col] = t;
    }
  }
  // Exactly one dude and exactly one door. A level with no door cannot be won
  // and one with several is ambiguous; both are authoring mistakes worth
  // catching at load rather than leaving for the player to discover.
  if (found != 1 || doors != 1) return false;

  out->lv = lv;
  settle(out);
  // A level that is already solved before the first input is malformed.
  if (game_at(out, out->px, out->py) == TILE_DOOR) return false;
  out->won = false;
  return true;
}

bool game_load(Game *g, const Level *lv) {
  if (!g) return false;

  // Build the new board off to the side first. A malformed level must leave the
  // game exactly as it was rather than half-replaced.
  Game fresh;
  memset(&fresh, 0, sizeof(fresh));
  if (!parse_level(&fresh, lv)) {
    free(fresh.map);
    return false;
  }

  bool auto_climb = g->auto_climb;
  game_free(g);
  *g = fresh;
  g->auto_climb = auto_climb;
  return true;
}

bool game_restart(Game *g) {
  if (!g || !g->lv) return false;
  return game_load(g, g->lv);
}

// ---------------------------------------------------------------------------
// Moves
// ---------------------------------------------------------------------------

static void finish(Game *g, Move *m) {
  hist_push(g, m);
  // Saturate rather than wrap. Undo already saturates at zero, and a counter
  // that wraps in one direction only drifts permanently.
  if (scores(m) && g->moves < UINT16_MAX) g->moves++;
  if (game_at(g, g->px, g->py) == TILE_DOOR) g->won = true;
}

bool game_climb(Game *g) {
  if (!g || !g->map || g->won) return false;
  if (!hist_can_record(g)) return false;
  int d = g->dir;
  // Something to climb, headroom above the dude, a clear landing, and with a
  // carried block the cell above the landing must be clear too.
  if (is_open(g, g->px + d, g->py)) return false;
  if (!is_open(g, g->px, g->py - 1)) return false;
  if (!is_open(g, g->px + d, g->py - 1)) return false;
  if (g->carry && !is_open(g, g->px + d, g->py - 2)) return false;

  g->px = (int8_t)(g->px + d);
  g->py = (int8_t)(g->py - 1);

  Move m = {0};
  m.dx = (int8_t)d;
  m.dy = -1;
  m.act = ACT_NONE;
  finish(g, &m);
  return true;
}

bool game_walk(Game *g, int dir) {
  if (!g || !g->map || g->won) return false;
  if (dir != -1 && dir != 1) return false;
  if (!hist_can_record(g)) return false;

  Move m = {0};
  if (g->dir != dir) {
    g->dir = (int8_t)dir;
    m.turned = true;
  }

  bool stepped = false;
  if (is_open(g, g->px + dir, g->py)) {
    int nx = g->px + dir;
    // A carried block rides one cell above the dude's head; refuse the step
    // rather than crushing or silently dropping it.
    if (!g->carry || is_open(g, nx, g->py - 1)) {
      g->px = (int8_t)nx;
      m.dx = (int8_t)dir;
      stepped = true;
      m.dy = (int8_t)settle(g);
    }
  }

  if (m.turned || stepped) {
    finish(g, &m);
    return true;
  }
  // Nothing moved and the facing was already right: try the step ahead.
  if (g->auto_climb) return game_climb(g);
  return false;
}

bool game_act(Game *g) {
  if (!g || !g->map || g->won) return false;
  if (!hist_can_record(g)) return false;
  int d = g->dir;

  if (g->carry) {
    // Drop ahead at head height, then let it fall.
    if (!is_vacant(g, g->px + d, g->py - 1)) return false;
    int bx = g->px + d;
    int by = g->py - 1;
    while (is_vacant(g, bx, by + 1)) by++;
    set_at(g, bx, by, TILE_BLOCK);
    g->carry = false;

    Move m = {0};
    m.act = ACT_DROP;
    m.bx = (uint8_t)bx;
    m.by = (uint8_t)by;
    finish(g, &m);
    return true;
  }

  // Pick up: a block directly ahead, with clearance above both the dude and it.
  if (game_at(g, g->px + d, g->py) != TILE_BLOCK) return false;
  if (!is_open(g, g->px, g->py - 1)) return false;
  if (!is_open(g, g->px + d, g->py - 1)) return false;

  int bx = g->px + d;
  int by = g->py;
  set_at(g, bx, by, TILE_EMPTY);
  g->carry = true;

  Move m = {0};
  m.act = ACT_PICKUP;
  m.bx = (uint8_t)bx;
  m.by = (uint8_t)by;
  finish(g, &m);
  return true;
}

// ---------------------------------------------------------------------------
// Undo and redo
// ---------------------------------------------------------------------------

bool game_can_undo(const Game *g) { return g && g->hist && g->hist_cur > 0; }
bool game_can_redo(const Game *g) { return g && g->hist && g->hist_cur < g->hist_len; }

bool game_undo(Game *g) {
  if (!game_can_undo(g)) return false;
  const Move *m = &g->hist[--g->hist_cur];

  switch (m->act) {
    case ACT_PICKUP:
      set_at(g, m->bx, m->by, TILE_BLOCK);
      g->carry = false;
      break;
    case ACT_DROP:
      set_at(g, m->bx, m->by, TILE_EMPTY);
      g->carry = true;
      break;
    default: break;
  }
  g->px = (int8_t)(g->px - m->dx);
  g->py = (int8_t)(g->py - m->dy);
  if (m->turned) g->dir = (int8_t)(-g->dir);

  if (scores(m) && g->moves > 0) g->moves--;
  g->won = (game_at(g, g->px, g->py) == TILE_DOOR);
  return true;
}

bool game_redo(Game *g) {
  if (!game_can_redo(g)) return false;
  const Move *m = &g->hist[g->hist_cur++];

  if (m->turned) g->dir = (int8_t)(-g->dir);
  g->px = (int8_t)(g->px + m->dx);
  g->py = (int8_t)(g->py + m->dy);
  switch (m->act) {
    case ACT_PICKUP:
      set_at(g, m->bx, m->by, TILE_EMPTY);
      g->carry = true;
      break;
    case ACT_DROP:
      set_at(g, m->bx, m->by, TILE_BLOCK);
      g->carry = false;
      break;
    default: break;
  }

  if (scores(m) && g->moves < UINT16_MAX) g->moves++;
  g->won = (game_at(g, g->px, g->py) == TILE_DOOR);
  return true;
}
