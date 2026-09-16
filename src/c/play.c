// SPDX-License-Identifier: MIT
//
// The play screen: board, camera, gestures, buttons and the level clock.

#include "play.h"

#include <pebble.h>

#include "app.h"
#include "game.h"
#include "gesture.h"
#include "levels.h"
#include "render.h"
#include "screens.h"
#include "store.h"


static Window *s_window;
static Layer *s_layer;
static Game s_game;
static int s_level;

//! Peek is a transient zoom-out held only while SELECT is down. It opens on the
//! press itself, so SELECT can carry nothing else: a button that has to tell a
//! press from a hold cannot report either until the hold has been ruled out.
static bool s_peek;

//! True between appear and disappear. The clock may only run while this holds.
static bool s_showing;

// Elapsed seconds on this level, counted only while the board is on screen.
//
// Counted with a repeating app timer rather than by differencing time(NULL).
// The watch corrects its clock from the phone, and a correction landing mid
// level would either throw away the interval it interrupted or bank an hour the
// player never spent, and that figure is what gets saved as a personal best.
static AppTimer *s_clock;
static uint32_t s_elapsed;

static void clock_tick(void *context);

static void clock_start(void) {
  if (!s_showing || s_game.won || s_clock) return;
  s_clock = app_timer_register(1000, clock_tick, NULL);
}

static void clock_stop(void) {
  if (!s_clock) return;
  app_timer_cancel(s_clock);
  s_clock = NULL;
}

static void clock_tick(void *context) {
  (void)context;
  s_clock = NULL;
  if (!s_showing || s_game.won) return;
  if (s_elapsed < UINT32_MAX) s_elapsed++;
  s_clock = app_timer_register(1000, clock_tick, NULL);
}

//! Hold the clock to the current state. Winning banks the interval and stops
//! it; undoing back out of a win reopens it, which a one-shot stop on the
//! winning move would not do.
static void clock_sync(void) {
  if (s_game.won) clock_stop();
  else clock_start();
}

uint32_t play_elapsed(void) { return s_elapsed; }

// ---------------------------------------------------------------------------

static int tile_px(void) { return s_peek ? PEEK_PX : app_zoom_px(); }

//! Whether the win has already been announced, so undoing back through the door
//! and walking into it again does not re-fire the celebration.
static bool s_won_seen;

static void redraw(void) {
  if (s_layer) layer_mark_dirty(s_layer);
}

static void update_proc(Layer *layer, GContext *ctx) {
  const GRect b = layer_get_bounds(layer);
  render_playfield(ctx, render_board_view(b), &s_game, tile_px());
  render_hud(ctx, b, s_level + 1, s_game.moves);
}

//! Apply the result of an input. Refused input is deliberately silent: probing
//! the geometry by walking into walls is the core loop of this game, so a buzz
//! on every blocked swipe would be constant haptic noise.
static void apply(bool changed) {
  if (!changed) return;

  const bool won_before = s_won_seen;
  redraw();
  clock_sync();
  s_won_seen = s_game.won;
  if (!s_game.won || won_before) return;

  if (g_settings.vibrate) vibes_double_pulse();

  const uint32_t secs = play_elapsed();
  const uint16_t moves = s_game.moves;
  // Record before showing, so the win screen reads the updated best and can
  // simply omit the comparison when this run is the best.
  const bool best = store_record_win(s_level, moves, secs);

  // Take the solved board off the stack, but only once the win screen is
  // actually up: left underneath, BACK from the win screen would land the
  // player on a level that refuses every input, and dropping it before the push
  // would lose the result entirely if the push failed.
  if (screens_push_win(s_level, moves, (unsigned)secs, best)) {
    if (window_stack_contains_window(s_window)) window_stack_remove(s_window, false);
  }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

static void handle_gesture(Gesture g, GPoint at, void *context) {
  (void)at;
  (void)context;
  light_enable_interaction();

  // A downward swipe is deliberately unbound. Lift and drop is the input the
  // player reaches for constantly, and a tap is both quicker and easier to aim
  // than a flick on a screen this size, so the action lives there instead.
  switch (g) {
    case GESTURE_SWIPE_LEFT:  apply(game_walk(&s_game, -1)); break;
    case GESTURE_SWIPE_RIGHT: apply(game_walk(&s_game, 1));  break;
    case GESTURE_SWIPE_UP:    apply(game_climb(&s_game));    break;
    case GESTURE_TAP:         apply(game_act(&s_game));      break;
    default: break;
  }
}

static void zoom_in(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (g_settings.zoom + 1 < ZOOM_COUNT) {
    g_settings.zoom++;
    store_save_settings();
    redraw();
  }
}

static void zoom_out(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (g_settings.zoom > 0) {
    g_settings.zoom--;
    store_save_settings();
    redraw();
  }
}

// Holding UP rewinds rather than undoing a single move.
//
// A long click fires once and does not repeat, and undo went from a press to a
// hold when SELECT was given over to peek. Left at one move per hold, backing
// out of a botched stack would be a dozen separate holds. The timer below turns
// the hold into a rewind that runs until the history is spent or the button
// comes up, which is less work than the press it replaced rather than more.
#define REWIND_MS 150

static AppTimer *s_rewind;

static void rewind_stop(void) {
  if (!s_rewind) return;
  app_timer_cancel(s_rewind);
  s_rewind = NULL;
}

static void rewind_tick(void *context) {
  (void)context;
  s_rewind = NULL;
  // The board can go away under a held button, so check rather than trust that
  // the release handler will be the thing that stops this.
  if (!s_showing) return;
  const bool changed = game_undo(&s_game);
  apply(changed);
  if (!changed) return;   // nothing left to rewind
  s_rewind = app_timer_register(REWIND_MS, rewind_tick, NULL);
}

static void undo_begin(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  rewind_stop();
  const bool changed = game_undo(&s_game);
  apply(changed);
  if (changed) s_rewind = app_timer_register(REWIND_MS, rewind_tick, NULL);
}

static void undo_end(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  rewind_stop();
}

//! Peek runs off a button rather than a held finger. The screen is where every
//! movement input lives, so a contact that sat there to widen the view was one
//! slipped finger away from walking the dude off a stack.
static void peek_begin(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_peek) return;
  s_peek = true;
  redraw();
}

static void peek_end(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (!s_peek) return;
  s_peek = false;
  redraw();
}

static void pause_click(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  // BACK is the board's only exit, so if the sheet cannot be built, leave
  // rather than trapping the player on the level.
  if (!screens_push_pause()) screens_go_main();
}

static void click_config(void *ctx) {
  (void)ctx;
  window_single_click_subscribe(BUTTON_ID_UP, zoom_in);
  // Undo is a hold because SELECT has been given over entirely to peek. The cost
  // lands on UP: a button carrying a long click cannot report a short one until
  // the hold has been ruled out, so zooming in now acts on release. The delay is
  // the same line the touch layer draws between a press and a hold rather than
  // the firmware's 500ms default, which would be a visible pause on every zoom.
  window_long_click_subscribe(BUTTON_ID_UP, GESTURE_MAX_DURATION_MS, undo_begin, undo_end);
  window_single_click_subscribe(BUTTON_ID_DOWN, zoom_out);
  // Raw, so the view widens on the press itself. A long click would hold the
  // board still for a third of a second first, which is exactly long enough to
  // read as the button having been missed.
  window_raw_click_subscribe(BUTTON_ID_SELECT, peek_begin, peek_end, NULL);
  // BACK pauses instead of leaving; the pause sheet owns the way out.
  window_single_click_subscribe(BUTTON_ID_BACK, pause_click);
}

// ---------------------------------------------------------------------------
// Window
// ---------------------------------------------------------------------------

static void window_appear(Window *w) {
  (void)w;
  s_peek = false;
  s_showing = true;
  // Re-read the setting so a change made in the pause menu reaches a level that
  // is already loaded, rather than waiting until it is next pushed.
  s_game.auto_climb = g_settings.auto_climb;
  gesture_subscribe(handle_gesture, NULL);
  if (!touch_service_is_enabled()) {
    // Every movement input is a gesture, so without touch the board cannot be
    // played at all. Both declared platforms have a touchscreen; this is here so
    // a future target fails loudly instead of looking broken.
    APP_LOG(APP_LOG_LEVEL_ERROR, "touch unavailable: the board has no movement input");
  }
  clock_start();
}

static void window_disappear(Window *w) {
  (void)w;
  gesture_unsubscribe();
  rewind_stop();
  clock_stop();
  s_showing = false;
  s_peek = false;
}

static void window_load(Window *w) {
  const GRect b = layer_get_bounds(window_get_root_layer(w));
  s_layer = layer_create(b);
  if (!s_layer) return;
  layer_set_update_proc(s_layer, update_proc);
  layer_add_child(window_get_root_layer(w), s_layer);
}

//! Unload frees only what load allocated. The window itself belongs to
//! play_deinit(): destroying it from here would re-enter this same handler.
static void window_unload(Window *w) {
  (void)w;
  layer_destroy(s_layer);
  s_layer = NULL;
}

//! One play window is created lazily and reused for every level, so advancing
//! a level cannot orphan a stacked window that still points at shared state.
static bool ensure_window(void) {
  if (s_window) return true;
  s_window = window_create();
  if (!s_window) return false;
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .appear = window_appear,
    .disappear = window_disappear,
    .unload = window_unload,
  });
  window_set_click_config_provider(s_window, click_config);
  return true;
}

bool play_push(int level_index) {
  const Level *lv = levels_get(level_index);
  if (!lv) return false;
  if (!ensure_window()) return false;

  s_game.auto_climb = g_settings.auto_climb;
  if (!game_load(&s_game, lv)) return false;

  s_level = level_index;
  s_elapsed = 0;
  s_peek = false;
  s_won_seen = false;
  rewind_stop();

  if (window_stack_contains_window(s_window)) {
    // Already showing, so no appear is coming to start the clock for us.
    clock_sync();
    redraw();
  } else {
    window_stack_push(s_window, false);
  }

  // window_load builds the board layer. Without it the level is invisible but
  // fully live: gestures still move the dude, the clock still runs, and the
  // player can win a level they cannot see. Refuse rather than offer that.
  if (!s_layer) {
    if (window_stack_contains_window(s_window)) window_stack_remove(s_window, false);
    return false;
  }
  return true;
}

void play_restart(void) {
  rewind_stop();
  if (!game_restart(&s_game)) return;
  s_elapsed = 0;
  s_won_seen = false;
  clock_sync();
  redraw();
}

void play_leave(void) {
  if (s_window && window_stack_contains_window(s_window))
    window_stack_remove(s_window, false);
}

#ifdef CD_SHOT
void play_shot_walk(int dir, int times) {
  for (int i = 0; i < times; i++) apply(game_walk(&s_game, dir));
}
#endif

void play_deinit(void) {
  rewind_stop();
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
  game_free(&s_game);
}
