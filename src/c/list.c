// SPDX-License-Identifier: MIT

#include "list.h"

#include "ui.h"

#define ROW_H PBL_IF_ROUND_ELSE(38, 34)
#define LABEL_CAP 24
#define VALUE_CAP 24  // fits the level picker's "65535  H:MM:SS"

static int row_count(const ListWindow *lw) {
  const int n = lw->spec->count ? lw->spec->count() : 0;
  return (n < 0) ? 0 : n;
}

static RowStyle row_style(const ListWindow *lw, int index) {
  if (index < 0 || index >= row_count(lw)) return ROW_LOCKED;
  return lw->spec->style ? lw->spec->style(index) : ROW_NORMAL;
}

// ---------------------------------------------------------------------------
// Layers
// ---------------------------------------------------------------------------

static void footer_update(Layer *layer, GContext *ctx) {
  ListWindow *lw = *(ListWindow **)layer_get_data(layer);
  if (lw->spec->draw_footer) lw->spec->draw_footer(ctx, layer_get_bounds(layer));
}

static void title_update(Layer *layer, GContext *ctx) {
  ListWindow *lw = *(ListWindow **)layer_get_data(layer);
  const GRect b = layer_get_bounds(layer);
  // The band paints its own full-width title, so give it the window width.
  ui_draw_title(ctx, GRect(0, 0, b.size.w, b.size.h), lw->spec->title);
}

// ---------------------------------------------------------------------------
// MenuLayer callbacks
// ---------------------------------------------------------------------------

static uint16_t get_num_rows(MenuLayer *ml, uint16_t section, void *ctx) {
  (void)ml; (void)section;
  return (uint16_t)row_count((ListWindow *)ctx);
}

static int16_t get_cell_height(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  (void)ml; (void)idx; (void)ctx;
  return ROW_H;
}

//! Whether this row is the selected one, asked of the MenuLayer rather than of
//! the cell.
//!
//! menu_cell_layer_is_highlighted() reports the state of the firmware's
//! selection ANIMATION: while it runs, the outgoing and incoming cells are both
//! drawn part-highlighted, which is what makes the plank look like it is being
//! stretched from one row to the next. The selected index, by contrast, is
//! already the new row the moment the press lands, so highlighting off it moves
//! the plank in one step and the animation has nothing left to show.
static bool row_is_selected(const ListWindow *lw, const MenuIndex *idx) {
  if (!lw->menu) return false;
  const MenuIndex sel = menu_layer_get_selected_index(lw->menu);
  return sel.section == idx->section && sel.row == idx->row;
}

static void draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *idx, void *context) {
  ListWindow *lw = (ListWindow *)context;
  char label[LABEL_CAP] = {0};
  char value[VALUE_CAP] = {0};
  if (lw->spec->row) lw->spec->row(idx->row, label, sizeof(label), value, sizeof(value));
  ui_draw_row(ctx, layer_get_bounds(cell_layer), label, value,
              row_is_selected(lw, idx), row_style(lw, idx->row));
}

static void pick_row(ListWindow *lw, int row) {
  // Readings and locked rows are not choices.
  if (row_style(lw, row) != ROW_NORMAL) return;
  if (lw->spec->select) lw->spec->select(row);
}

static void select_click(MenuLayer *ml, MenuIndex *idx, void *context) {
  (void)ml;
  pick_row((ListWindow *)context, idx->row);
}

//! Keep the cursor off rows that cannot be picked. Without this the player can
//! park the selection on a locked level or on the win screen's move count and
//! press SELECT into silence. The firmware offers this hook precisely so a list
//! can skip cells; it lets us rewrite the incoming index in place.
static void selection_will_change(MenuLayer *ml, MenuIndex *new_index, MenuIndex old_index,
                                  void *context) {
  (void)ml;
  ListWindow *lw = (ListWindow *)context;
  const int n = row_count(lw);
  if (n <= 0) return;

  const int want = new_index->row;
  if (want < 0 || want >= n) return;
  if (row_style(lw, want) == ROW_NORMAL) return;

  // Keep going the way the cursor was already travelling, and stop at the end
  // rather than wrapping. Wrapping here would answer UP with a jump downward:
  // on the win screen the two rows above NEXT LEVEL are readings, so a scan
  // that wraps runs off the top and comes back on MAIN MENU, below where the
  // player started.
  const int step = (want < (int)old_index.row) ? -1 : 1;
  for (int i = want; i >= 0 && i < n; i += step) {
    if (row_style(lw, i) == ROW_NORMAL) {
      new_index->row = (uint16_t)i;
      return;
    }
  }
  // Nothing pickable that way: stay put rather than moving somewhere arbitrary.
  new_index->row = old_index.row;
}

//! Whether the rows need more room than the window gives them.
static bool list_overflows(const ListWindow *lw) {
  return row_count(lw) * ROW_H > lw->menu_h;
}

//! How the MenuLayer may move the view when the selection changes.
//!
//! A list whose rows all fit gets MenuRowAlignNone, which is documented as
//! leaving the scroll offset alone. That is the whole of the fix for a menu that
//! would otherwise creep: the firmware likes to pull the selected row towards a
//! comfortable zone, and on a four-row menu that is exactly the height of its
//! window there is nothing to reveal by doing so, only a list that will not sit
//! still. Only the level picker genuinely runs off the bottom, and it scrolls.
static MenuRowAlign list_align(const ListWindow *lw) {
  return list_overflows(lw) ? MenuRowAlignCenter : MenuRowAlignNone;
}

//! Open on the first row that can be picked, so SELECT is never pointing at a
//! reading when the screen appears.
static void select_first_pickable(ListWindow *lw) {
  if (!lw->menu) return;
  const int n = row_count(lw);
  for (int i = 0; i < n; i++) {
    if (row_style(lw, i) != ROW_NORMAL) continue;
    menu_layer_set_selected_index(lw->menu, MenuIndex(0, (uint16_t)i), list_align(lw), false);
    return;
  }
}

// ---------------------------------------------------------------------------
// Clicks
//
// The MenuLayer's own click config is not installed. Its handlers move the
// selection with animation switched on, and that animation is what drags the
// highlight from one row to the next; there is no knob to turn it off. Driving
// the same MenuLayer calls ourselves with `animated` false leaves every other
// list behaviour, including drag to scroll and tap to pick, exactly as it was:
// those belong to the widget, not to the click config.
// ---------------------------------------------------------------------------

static void move_selection(ListWindow *lw, bool up) {
  if (!lw->menu) return;
  menu_layer_set_selected_next(lw->menu, up, list_align(lw), false);
}

static void up_click(ClickRecognizerRef r, void *context) {
  (void)r;
  move_selection((ListWindow *)context, true);
}

static void down_click(ClickRecognizerRef r, void *context) {
  (void)r;
  move_selection((ListWindow *)context, false);
}

static void select_button(ClickRecognizerRef r, void *context) {
  (void)r;
  ListWindow *lw = (ListWindow *)context;
  if (!lw->menu) return;
  pick_row(lw, menu_layer_get_selected_index(lw->menu).row);
}

//! BACK, taken over only so the screen can leave the way it arrived.
//!
//! The firmware's default back handler pops with the slide switched on, and a
//! screen that appears instantly but leaves on a slide is worse than either
//! being consistent. Removing the window is what the default does, including on
//! the last window: draining the stack is how the firmware is told an app has
//! finished, which is what BACK on the main menu is supposed to mean.
static void back_click(ClickRecognizerRef r, void *context) {
  (void)r;
  ListWindow *lw = (ListWindow *)context;
  if (lw->window) window_stack_remove(lw->window, false);
}

//! The window's click context is the ListWindow, so every handler above gets it
//! without this provider having to pass anything along.
static void click_config(void *context) {
  (void)context;
  // Repeating, so a hold still runs down the level picker.
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, up_click);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_button);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click);
}

// ---------------------------------------------------------------------------
// Window
// ---------------------------------------------------------------------------

static void window_load(Window *w) {
  ListWindow *lw = (ListWindow *)window_get_user_data(w);
  const GRect b = layer_get_bounds(window_get_root_layer(w));
  const int title_h = ui_title_height();

  lw->title = layer_create_with_data(GRect(0, 0, b.size.w, title_h), sizeof(ListWindow *));
  if (lw->title) {
    *(ListWindow **)layer_get_data(lw->title) = lw;
    layer_set_update_proc(lw->title, title_update);
  }

  // On the round watch the bottom of the window is behind the bezel, so a list
  // long enough to scroll would put its last row where it cannot be read. Every
  // other surface reserves this margin; the list has to as well.
  const int bottom_inset = PBL_IF_ROUND_ELSE(ui_side_inset(), 0);
  const int footer_h = lw->spec->draw_footer ? lw->spec->footer_height : 0;
  if (footer_h > 0) {
    lw->footer = layer_create_with_data(GRect(0, b.size.h - footer_h, b.size.w, footer_h),
                                        sizeof(ListWindow *));
    if (lw->footer) {
      *(ListWindow **)layer_get_data(lw->footer) = lw;
      layer_set_update_proc(lw->footer, footer_update);
    }
  }

  const int menu_h = b.size.h - title_h - footer_h - (footer_h ? 0 : bottom_inset);
  lw->menu_h = menu_h;
  lw->menu = menu_layer_create(GRect(0, title_h, b.size.w, menu_h));
  if (lw->menu) {
    menu_layer_set_callbacks(lw->menu, lw, (MenuLayerCallbacks){
      .get_num_rows = get_num_rows,
      .get_cell_height = get_cell_height,
      .draw_row = draw_row,
      // The touch bridge turns a tap on a row into a SELECT click, which lands
      // in our click config rather than here, so this hook covers only the
      // firmware paths that still raise it directly.
      .select_click = select_click,
      .selection_will_change = selection_will_change,
    });
    menu_layer_set_normal_colors(lw->menu, GColorWhite, GColorBlack);
    // The highlight background stays white because ui_draw_row paints the crate
    // plank itself. Letting the MenuLayer fill the cell as well would show
    // through on the round watch, where the plank keeps the bezel gutter.
    menu_layer_set_highlight_colors(lw->menu, GColorWhite, GColorBlack);
    // The bottom pad is what makes a list that already fits scrollable, and a
    // list that can scroll drifts under the cursor even when there is nothing
    // below to reveal. Without it the four-row menus are exactly the height of
    // their window and stay put; the level picker, which genuinely overflows,
    // still scrolls.
    menu_layer_pad_bottom_enable(lw->menu, false);
    // Centre-focus is the round default and is wrong here: these lists sit under
    // a fixed title band, and centring the selection strands a dead white gap
    // between the band and the first row.
    menu_layer_set_center_focused(lw->menu, false);
    window_set_click_config_provider_with_context(w, click_config, lw);
    layer_add_child(window_get_root_layer(w), menu_layer_get_layer(lw->menu));
  }

  // Title and footer go on last so the list cannot scroll over them.
  if (lw->title) layer_add_child(window_get_root_layer(w), lw->title);
  if (lw->footer) layer_add_child(window_get_root_layer(w), lw->footer);
  select_first_pickable(lw);
}

static void window_unload(Window *w) {
  ListWindow *lw = (ListWindow *)window_get_user_data(w);
  if (lw->menu) { menu_layer_destroy(lw->menu); lw->menu = NULL; }
  if (lw->title) { layer_destroy(lw->title); lw->title = NULL; }
  if (lw->footer) { layer_destroy(lw->footer); lw->footer = NULL; }
}

//! True once the window has a usable list. window_load leaves menu NULL if the
//! MenuLayer could not be allocated, in which case the screen would show a title
//! band over nothing with no way to pick anything.
static bool list_ready(const ListWindow *lw) { return lw && lw->window && lw->menu; }

bool list_push(ListWindow *lw, const ListSpec *spec) {
  if (!lw || !spec) return false;
  lw->spec = spec;

  if (!lw->window) {
    lw->window = window_create();
    if (!lw->window) return false;
    window_set_user_data(lw->window, lw);
    window_set_window_handlers(lw->window, (WindowHandlers){
      .load = window_load,
      .unload = window_unload,
    });
  }
  if (window_stack_contains_window(lw->window)) {
    list_reload(lw);
    select_first_pickable(lw);
    return list_ready(lw);
  }

  // Remember whether anything is already on the stack. If this is the app's
  // first window and it fails to build, it must stay: removing it would drain
  // the stack, and a drained stack is how the firmware is told an app has
  // finished. A broken screen the player can back out of beats a silent exit.
  Window *below = window_stack_get_top_window();
  window_stack_push(lw->window, false);

  // The push runs window_load, so by here we know whether the list built.
  if (!list_ready(lw)) {
    if (below) window_stack_remove(lw->window, false);
    return false;
  }
  return true;
}

void list_reload(ListWindow *lw) {
  if (!lw) return;
  if (lw->menu) menu_layer_reload_data(lw->menu);
  if (lw->title) layer_mark_dirty(lw->title);
  if (lw->footer) layer_mark_dirty(lw->footer);
}

void list_deinit(ListWindow *lw) {
  if (!lw || !lw->window) return;
  window_destroy(lw->window);
  lw->window = NULL;
  lw->menu = NULL;
  lw->title = NULL;
  lw->footer = NULL;
}
