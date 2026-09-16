// SPDX-License-Identifier: MIT
//
// Tile art and the playfield. Every sprite is drawn from primitives and scales
// its detail with the tile size, so one code path serves 12 through 30 pixels
// and neither watch needs its own artwork.

#pragma once

#include <pebble.h>

#include "game.h"

//! The part of the window the board draws into, once the status band has taken
//! its share. The band is split on the round watch, level in the top arc and
//! moves in the bottom, because one line across the top of a circle is clipped
//! at any readable font size; keeping that arithmetic here means the board and
//! the band cannot drift apart.
GRect render_board_view(GRect bounds);

//! Single tiles. Each fills a SQUARE of side `r.size.w` at the rect's origin;
//! `r.size.h` is not read, because a tile is square by definition and every
//! caller passes one. `dir` is -1 or +1.
void render_wall(GContext *ctx, GRect r);
void render_block(GContext *ctx, GRect r);
void render_door(GContext *ctx, GRect r);
void render_dude(GContext *ctx, GRect r, int dir);

//! Fill a rect with wall tiles, used as chrome behind titles and floors.
//!
//! Tiles are drawn whole, starting at the band's origin, so the last row and
//! column may overshoot the far edge by less than one tile. The caller is
//! expected to give the band its own layer, which clips that overshoot. What
//! the band will never do is paint above or to the left of its origin.
void render_brick_band(GContext *ctx, GRect band, int tile);

//! Draw the board into `view` at `tile` pixels per cell, following the dude
//! when the level is larger than the view and centring it when it fits.
void render_playfield(GContext *ctx, GRect view, const Game *g, int tile);

//! Draw the status band(s) over the whole window.
void render_hud(GContext *ctx, GRect bounds, int level_number, uint16_t moves);
