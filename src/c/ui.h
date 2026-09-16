// SPDX-License-Identifier: MIT
//
// Shared chrome. Everything outside the board is built from the same brick and
// crate vocabulary as the game itself, so the menus look like they belong to it.

#pragma once

#include <pebble.h>

//! Horizontal gutter that keeps TEXT clear of the round watch's bezel. Use this
//! for anything that is not a full-window rect, such as a band or a row.
int ui_side_inset(void);

//! Gutter for a filled band that is meant to read as the full width of the row
//! it belongs to. Zero on a flat screen, the bezel gutter on a round one.
int ui_row_inset(void);

//! Content rect that stays clear of the round watch's clipped corners. Takes
//! the WINDOW bounds; passing a shorter rect yields a negative height.
GRect ui_safe_box(GRect bounds);

//! Height of the brick title band, and the tile size it is built from.
int ui_title_height(void);
int ui_title_tile(void);

//! Brick band with a title across it, drawn in white with a hard black shadow
//! so it stays readable over the mortar.
void ui_draw_title(GContext *ctx, GRect bounds, const char *title);

//! How a list row reads and behaves.
typedef enum {
  ROW_NORMAL = 0, //!< pick it
  ROW_INFO,       //!< a reading, not a choice: normal ink, not selectable
  ROW_LOCKED,     //!< greyed and not selectable
} RowStyle;

//! One list row. Highlighted rows are drawn as a crate plank. `value` may be
//! NULL or empty.
void ui_draw_row(GContext *ctx, GRect box, const char *label, const char *value,
                 bool highlighted, RowStyle style);

//! Format a duration the way the win screen and level list show it: M:SS, or
//! H:MM:SS once it runs past an hour.
void ui_format_time(uint32_t secs, char *out, size_t cap);
