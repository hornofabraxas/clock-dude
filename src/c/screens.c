// SPDX-License-Identifier: MIT

#include "screens.h"

#include <pebble.h>

#include "app.h"
#include "levels.h"
#include "list.h"
#include "play.h"
#include "render.h"
#include "store.h"
#include "ui.h"

static ListWindow s_main, s_levels, s_settings, s_pause, s_win;
static Window *s_help;

// ---------------------------------------------------------------------------
// Main menu
// ---------------------------------------------------------------------------

enum { MAIN_PLAY = 0, MAIN_LEVELS, MAIN_HELP, MAIN_SETTINGS, MAIN_COUNT };

static int main_count(void) { return MAIN_COUNT; }

static RowStyle main_style(int i) { (void)i; return ROW_NORMAL; }

static void main_row(int i, char *label, size_t lcap, char *value, size_t vcap) {
  switch (i) {
    case MAIN_PLAY:
      // Nothing to continue until a level has actually been finished, and a
      // fresh watch offering CONTINUE LVL 1 reads as a game already in progress.
      if (store_has_progress()) {
        snprintf(label, lcap, "CONTINUE");
        snprintf(value, vcap, "LVL %d", store_continue_level() + 1);
      } else {
        snprintf(label, lcap, "NEW GAME");
      }
      break;
    case MAIN_LEVELS:   snprintf(label, lcap, "LEVEL SELECT"); break;
    case MAIN_HELP:     snprintf(label, lcap, "HOW TO PLAY");  break;
    case MAIN_SETTINGS: snprintf(label, lcap, "SETTINGS");     break;
    default: break;
  }
}

static void main_select(int i) {
  switch (i) {
    case MAIN_PLAY:
      // Fall back rather than leaving the menu inert if the stored level no
      // longer resolves, for instance after a build with fewer levels.
      if (!play_push(store_continue_level())) play_push(0);
      break;
    case MAIN_LEVELS:    screens_push_levels();            break;
    case MAIN_HELP:      screens_push_help();              break;
    case MAIN_SETTINGS:  screens_push_settings();          break;
    default: break;
  }
}

//! The dude and a crate standing on a brick floor, so the menu is built from
//! the same vocabulary as the board it leads to.
static void main_footer(GContext *ctx, GRect box) {
  const int tile = PBL_IF_ROUND_ELSE(20, 22);
  const int floor_top = box.size.h - tile;
  render_brick_band(ctx, GRect(0, box.origin.y + floor_top, box.size.w, tile), tile);
  const int cx = box.size.w / 2;
  const int stand = box.origin.y + floor_top - tile;
  render_dude(ctx, GRect(cx - tile - tile / 2, stand, tile, tile), 1);
  render_block(ctx, GRect(cx + tile / 2, stand, tile, tile));
}

static const ListSpec MAIN_SPEC = {
  .title = "CLOCK DUDE",
  .count = main_count,
  .style = main_style,
  .row = main_row,
  .select = main_select,
  .footer_height = PBL_IF_ROUND_ELSE(64, 46),
  .draw_footer = main_footer,
};

static void drop(Window *w) {
  if (w && window_stack_contains_window(w)) window_stack_remove(w, false);
}

//! Unwind to the main menu from anywhere. Removing one window at a time lands
//! on whatever pushed the board, which for a level started from the picker is
//! the picker rather than the menu the row promises.
//!
//! Everything that can sit above the menu is removed by name, top down, and the
//! menu itself is left in place. Emptying the stack instead would leave the app
//! with no window at all, even momentarily, and a drained window stack is how
//! the firmware is told an app has finished.
void screens_go_main(void) {
  drop(s_win.window);
  drop(s_pause.window);
  play_leave();
  drop(s_help);
  drop(s_settings.window);
  drop(s_levels.window);
  if (!s_main.window || !window_stack_contains_window(s_main.window))
    list_push(&s_main, &MAIN_SPEC);
  else
    list_reload(&s_main);
}

void screens_push_main(void) {
  list_push(&s_main, &MAIN_SPEC);
}

// ---------------------------------------------------------------------------
// Level select
// ---------------------------------------------------------------------------

static int levels_count_rows(void) { return levels_count(); }

static RowStyle levels_style(int i) {
  if (i < 0 || i >= levels_count()) return ROW_LOCKED;
  return (i > store_unlocked()) ? ROW_LOCKED : ROW_NORMAL;
}

static void levels_row(int i, char *label, size_t lcap, char *value, size_t vcap) {
  if (i < 0 || i >= levels_count()) return;
  snprintf(label, lcap, "LEVEL %d", i + 1);

  if (i > store_unlocked()) {
    snprintf(value, vcap, "LOCKED");
  } else if (store_completed(i)) {
    char t[12];
    ui_format_time(store_best_secs(i), t, sizeof(t));
    snprintf(value, vcap, "%u  %s", (unsigned)store_best_moves(i), t);
  }
}

static void levels_select(int i) {
  if (i > store_unlocked()) return;
  play_push(i);
}

static const ListSpec LEVELS_SPEC = {
  .title = "LEVELS",
  .count = levels_count_rows,
  .style = levels_style,
  .row = levels_row,
  .select = levels_select,
};

void screens_push_levels(void) { list_push(&s_levels, &LEVELS_SPEC); }

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

enum { SET_ZOOM = 0, SET_CLIMB, SET_VIBRATE, SET_RESET, SET_COUNT };

//! Reset asks once. screens_push_settings() clears the flag on every entry, so
//! arming it and leaving cannot carry a live confirmation into a later visit.
//! That entry point is the only way onto this list.
static bool s_reset_armed;

static int settings_count(void) { return SET_COUNT; }

static RowStyle settings_style(int i) { (void)i; return ROW_NORMAL; }

static void settings_row(int i, char *label, size_t lcap, char *value, size_t vcap) {
  switch (i) {
    case SET_ZOOM:
      snprintf(label, lcap, "ZOOM");
      snprintf(value, vcap, "%s", app_zoom_name());
      break;
    case SET_CLIMB:
      snprintf(label, lcap, "AUTO-CLIMB");
      snprintf(value, vcap, g_settings.auto_climb ? "ON" : "OFF");
      break;
    case SET_VIBRATE:
      snprintf(label, lcap, "VIBRATE");
      snprintf(value, vcap, g_settings.vibrate ? "ON" : "OFF");
      break;
    case SET_RESET:
      snprintf(label, lcap, s_reset_armed ? "TAP TO CONFIRM" : "RESET PROGRESS");
      break;
    default: break;
  }
}

static void settings_select(int i) {
  if (i != SET_RESET) s_reset_armed = false;

  switch (i) {
    case SET_ZOOM:
      g_settings.zoom = (uint8_t)((g_settings.zoom + 1) % ZOOM_COUNT);
      store_save_settings();
      break;
    case SET_CLIMB:
      g_settings.auto_climb = !g_settings.auto_climb;
      store_save_settings();
      break;
    case SET_VIBRATE:
      g_settings.vibrate = !g_settings.vibrate;
      store_save_settings();
      if (g_settings.vibrate) vibes_short_pulse();
      break;
    case SET_RESET:
      if (!s_reset_armed) {
        s_reset_armed = true;
      } else {
        s_reset_armed = false;
        store_reset_progress();
        if (g_settings.vibrate) vibes_short_pulse();
      }
      break;
    default: break;
  }
  list_reload(&s_settings);
}

static const ListSpec SETTINGS_SPEC = {
  .title = "SETTINGS",
  .count = settings_count,
  .style = settings_style,
  .row = settings_row,
  .select = settings_select,
};

void screens_push_settings(void) {
  s_reset_armed = false;
  list_push(&s_settings, &SETTINGS_SPEC);
}

// ---------------------------------------------------------------------------
// Pause
// ---------------------------------------------------------------------------

enum { PAUSE_RESUME = 0, PAUSE_RESTART, PAUSE_QUIT, PAUSE_COUNT };

static int pause_count(void) { return PAUSE_COUNT; }

// Undo is deliberately absent here. This sheet covers the board, so acting on
// it from a menu would change something the player cannot see. Undo lives on a
// held UP instead, where the result is visible as it happens.
static RowStyle pause_style(int i) { (void)i; return ROW_NORMAL; }

static void pause_row(int i, char *label, size_t lcap, char *value, size_t vcap) {
  (void)value; (void)vcap;
  switch (i) {
    case PAUSE_RESUME:  snprintf(label, lcap, "RESUME");        break;
    case PAUSE_RESTART: snprintf(label, lcap, "RESTART LEVEL"); break;
    case PAUSE_QUIT:    snprintf(label, lcap, "MAIN MENU");     break;
    default: break;
  }
}

static void pause_select(int i) {
  switch (i) {
    case PAUSE_RESUME:
      window_stack_remove(s_pause.window, false);
      break;
    case PAUSE_RESTART:
      play_restart();
      window_stack_remove(s_pause.window, false);
      break;
    case PAUSE_QUIT:
      screens_go_main();
      break;
    default: break;
  }
}

static const ListSpec PAUSE_SPEC = {
  .title = "PAUSED",
  .count = pause_count,
  .style = pause_style,
  .row = pause_row,
  .select = pause_select,
};

bool screens_push_pause(void) { return list_push(&s_pause, &PAUSE_SPEC); }

// ---------------------------------------------------------------------------
// Win
// ---------------------------------------------------------------------------

// The run's figures, the record they are measured against, and the two ways out.
// The record gets a row of its own rather than riding along on the end of the
// other two: sharing a row is what forced the figures down to a size the player
// has to squint at, and they are the whole reason this screen exists.
enum { WIN_MOVES = 0, WIN_TIME, WIN_BEST, WIN_NEXT, WIN_MENU, WIN_KINDS };

static int s_win_level;
static unsigned s_win_moves, s_win_secs;

static bool win_has_next(void) { return s_win_level + 1 < levels_count(); }

//! Whether there is a record to show that this run did not equal. A first clear
//! records itself, so the stored best matches the run and there is nothing to
//! compare; the same goes for a run that beat both halves of the old one.
static bool win_has_best(void) {
  if (!store_completed(s_win_level)) return false;
  return store_best_moves(s_win_level) != s_win_moves
      || store_best_secs(s_win_level) != s_win_secs;
}

static int win_count(void) { return win_has_best() ? WIN_KINDS : WIN_KINDS - 1; }

//! Which of the five kinds of row lives at this position. Only the record row
//! comes and goes, so everything below it shifts up by one when it is absent.
static int win_kind(int i) {
  if (i < WIN_BEST) return i;
  return win_has_best() ? i : i + 1;
}

static RowStyle win_style(int i) {
  switch (win_kind(i)) {
    case WIN_MOVES:
    case WIN_TIME:
    case WIN_BEST: return ROW_INFO;
    case WIN_NEXT: return win_has_next() ? ROW_NORMAL : ROW_INFO;
    case WIN_MENU: return ROW_NORMAL;
    default: return ROW_LOCKED;
  }
}

static void win_row(int i, char *label, size_t lcap, char *value, size_t vcap) {
  char t[12];
  switch (win_kind(i)) {
    case WIN_MOVES:
      snprintf(label, lcap, "MOVES");
      snprintf(value, vcap, "%u", s_win_moves);
      break;
    case WIN_TIME:
      snprintf(label, lcap, "TIME");
      ui_format_time(s_win_secs, t, sizeof(t));
      snprintf(value, vcap, "%s", t);
      break;
    case WIN_BEST:
      snprintf(label, lcap, "BEST");
      ui_format_time(store_best_secs(s_win_level), t, sizeof(t));
      snprintf(value, vcap, "%u  %s", (unsigned)store_best_moves(s_win_level), t);
      break;
    case WIN_NEXT:
      snprintf(label, lcap, win_has_next() ? "NEXT LEVEL" : "ALL LEVELS DONE");
      break;
    case WIN_MENU:
      snprintf(label, lcap, "MAIN MENU");
      break;
    default: break;
  }
}

static void win_select(int i) {
  switch (win_kind(i)) {
    case WIN_NEXT: {
      if (!win_has_next()) return;
      const int next = s_win_level + 1;
      window_stack_remove(s_win.window, false);
      if (!play_push(next)) screens_go_main();
      break;
    }
    case WIN_MENU:
      screens_go_main();
      break;
    default: break;
  }
}

static const ListSpec WIN_SPEC = {
  .title = NULL,  // set per push: SOLVED or NEW BEST
  .count = win_count,
  .style = win_style,
  .row = win_row,
  .select = win_select,
};

static ListSpec s_win_spec;

bool screens_push_win(int level_index, unsigned moves, unsigned secs, bool new_best) {
  s_win_level = level_index;
  s_win_moves = moves;
  // Compare like with like: the stored best is clamped to what a record can
  // hold, so an unclamped run would look different from a best it just set.
  s_win_secs = store_clamp_secs(secs);

  s_win_spec = WIN_SPEC;
  s_win_spec.title = new_best ? "NEW BEST" : "SOLVED";
  return list_push(&s_win, &s_win_spec);
}

// ---------------------------------------------------------------------------
// Help
// ---------------------------------------------------------------------------

//! One line per control, and the credit under a rule at the foot of the screen.
//! Both blocks are drawn line by line rather than as wrapped paragraphs, so the
//! line pitch is ours to set and six short lines fit in a column where the
//! font's own leading would have allowed five.
static const char *const HELP_LINES[] = {
  "Swipe to walk, climb.",
  "Tap to lift or drop.",
  "Hold SELECT to peek.",
  "Hold UP to undo.",
  "UP / DOWN to zoom.",
  "BACK to pause.",
};
#define HELP_LINE_COUNT ((int)(sizeof(HELP_LINES) / sizeof(HELP_LINES[0])))

static const char *const HELP_CREDIT[] = {
  "Original game and levels",
  "by Brandon Sterner",
};
#define HELP_CREDIT_COUNT ((int)(sizeof(HELP_CREDIT) / sizeof(HELP_CREDIT[0])))

//! Widest of a set of lines, measured unbounded. Measuring inside the column
//! would let a line that does not fit wrap instead of reporting its true width,
//! and the fit test below would then pass on a line it is about to ellipsise.
static int widest(const char *const *lines, int n, GFont font) {
  int w = 0;
  for (int i = 0; i < n; i++) {
    const GSize size = graphics_text_layout_get_content_size(
        lines[i], font, GRect(0, 0, 1000, 40), GTextOverflowModeWordWrap, GTextAlignmentLeft);
    if (size.w > w) w = size.w;
  }
  return w;
}

static void draw_lines(GContext *ctx, const char *const *lines, int n, GFont font,
                       int x, int y, int w, int pitch) {
  for (int i = 0; i < n; i++)
    graphics_draw_text(ctx, lines[i], font, GRect(x, y + i * pitch - 3, w, pitch + 6),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void help_update(Layer *layer, GContext *ctx) {
  const GRect b = layer_get_bounds(layer);
  const GRect safe = ui_safe_box(b);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  ui_draw_title(ctx, b, "HOW TO PLAY");

  const int x = safe.origin.x + 2;
  const int w = safe.size.w - 4;

  // The credit reads at the same size as the controls; only the weight differs,
  // so it still sits behind them. Drop both to the small size together rather
  // than separately, so the two blocks can never disagree about their size.
  GFont line_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GFont credit_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  int pitch = 20;
  if (widest(HELP_LINES, HELP_LINE_COUNT, line_font) > w ||
      widest(HELP_CREDIT, HELP_CREDIT_COUNT, credit_font) > w) {
    line_font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
    credit_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
    pitch = 16;
  }

  // Lay the credit out from the bottom and give the controls whatever is left.
  // Same gutter the rest of the chrome uses, rather than a second copy of the
  // bezel constant that would drift the moment ui.c is retuned.
  const int credit_top = b.size.h - ui_side_inset() - HELP_CREDIT_COUNT * pitch;
  const int rule_y = credit_top - 6;

  graphics_context_set_text_color(ctx, GColorBlack);
  draw_lines(ctx, HELP_LINES, HELP_LINE_COUNT, line_font,
             x, ui_title_height() + 4, w, pitch);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(safe.origin.x, rule_y, safe.size.w, 1), 0, GCornerNone);
  draw_lines(ctx, HELP_CREDIT, HELP_CREDIT_COUNT, credit_font, x, credit_top, w, pitch);
}

static Layer *s_help_layer;

//! BACK, for the same reason list.c takes it over: the default pop animates,
//! and this screen arrives without one.
static void help_back(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_help) window_stack_remove(s_help, false);
}

static void help_click_config(void *ctx) {
  (void)ctx;
  window_single_click_subscribe(BUTTON_ID_BACK, help_back);
}

static void help_load(Window *w) {
  const GRect b = layer_get_bounds(window_get_root_layer(w));
  s_help_layer = layer_create(b);
  if (!s_help_layer) return;
  layer_set_update_proc(s_help_layer, help_update);
  layer_add_child(window_get_root_layer(w), s_help_layer);
}

static void help_unload(Window *w) {
  (void)w;
  layer_destroy(s_help_layer);
  s_help_layer = NULL;
}

void screens_push_help(void) {
  if (!s_help) {
    s_help = window_create();
    if (!s_help) return;
    window_set_window_handlers(s_help, (WindowHandlers){ .load = help_load, .unload = help_unload });
    window_set_click_config_provider(s_help, help_click_config);
  }
  if (!window_stack_contains_window(s_help)) window_stack_push(s_help, false);
}

// ---------------------------------------------------------------------------

void screens_deinit(void) {
  list_deinit(&s_main);
  list_deinit(&s_levels);
  list_deinit(&s_settings);
  list_deinit(&s_pause);
  list_deinit(&s_win);
  if (s_help) {
    window_destroy(s_help);
    s_help = NULL;
  }
}
