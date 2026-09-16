// SPDX-License-Identifier: MIT
//
// A list screen: fixed brick title band with a MenuLayer under it.
//
// The lists are MenuLayers rather than hand-drawn canvases so they inherit the
// firmware's own list behaviour, including drag to scroll, tap a row to pick
// it, and swipe right to go back. The play screen holds a raw touch
// subscription while it is up, which suppresses that bridge exactly where the
// game needs a rightward swipe to mean "walk right" instead of "back".
//
// Only a list that genuinely overflows its window scrolls, and the selection
// never animates. Both fall out of list.c: the bottom pad is off, and the
// highlight is drawn from the selected index rather than from the cell's
// animation state.

#pragma once

#include <pebble.h>

#include "ui.h"

typedef struct {
  const char *title;

  //! Number of rows. Called whenever the list reloads.
  int (*count)(void);

  //! How a row reads and whether it can be picked. Kept separate from row()
  //! because the list asks this on every selection change and on every push,
  //! where formatting the text would be wasted work.
  RowStyle (*style)(int index);

  //! Fill in one row's text. Only called when the row is about to be drawn.
  void (*row)(int index, char *label, size_t label_cap, char *value, size_t value_cap);

  //! A row was picked. Only ROW_NORMAL rows get here. Call list_reload() from
  //! here if the row's text changed.
  void (*select)(int index);

  //! Optional strip along the bottom, reserved from the scrolling area. Used by
  //! the main menu for the brick floor the dude stands on.
  int footer_height;
  void (*draw_footer)(GContext *ctx, GRect box);
} ListSpec;

//! A list window owned by the calling screen, so two lists can be on the stack
//! at once without sharing state.
typedef struct {
  Window *window;
  MenuLayer *menu;
  Layer *title;
  Layer *footer;
  int menu_h;   //!< height of the list area, so it can be told whether it fits
  const ListSpec *spec;
} ListWindow;

//! Create if needed and push. Returns false only if allocation failed.
bool list_push(ListWindow *lw, const ListSpec *spec);

//! Redraw the list that is currently on top, after its data changed.
void list_reload(ListWindow *lw);

//! Release the window. Call at app exit, never from a window handler.
void list_deinit(ListWindow *lw);
