// SPDX-License-Identifier: MIT

#include "ui.h"

#include "render.h"

#define SAFE_INSET PBL_IF_ROUND_ELSE(38, 4)

int ui_side_inset(void) { return SAFE_INSET; }

//! How far a full-width band or plank is pulled in from the window edge. On the
//! flat watch that is nothing: a selection that stops short of the glass reads
//! as a floating box rather than as the selected row, and the whole row is what
//! is selected. On the round watch the edge of the window is behind the bezel,
//! so the plank has to keep the same gutter as everything else or its corners
//! are simply cut off.
int ui_row_inset(void) { return PBL_IF_ROUND_ELSE(SAFE_INSET, 0); }

GRect ui_safe_box(GRect bounds) {
  return GRect(SAFE_INSET, PBL_IF_ROUND_ELSE(SAFE_INSET, 0),
               bounds.size.w - 2 * SAFE_INSET,
               bounds.size.h - PBL_IF_ROUND_ELSE(2 * SAFE_INSET, 0));
}

int ui_title_tile(void) { return PBL_IF_ROUND_ELSE(22, 23); }
int ui_title_height(void) { return ui_title_tile() * 2; }

void ui_draw_title(GContext *ctx, GRect bounds, const char *title) {
  const int tile = ui_title_tile();
  const int h = ui_title_height();
  render_brick_band(ctx, GRect(0, 0, bounds.size.w, h), tile);
  if (!title) return;

  // The band is not a full window rect, so take the gutter directly rather than
  // asking ui_safe_box for a box whose height would be meaningless here.
  const int inset = ui_side_inset();
  const GRect box = GRect(inset, h / 2 - 19, bounds.size.w - 2 * inset, 34);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);

  // Hard one-pixel outline, so the title stays legible wherever it crosses a
  // mortar line. Four diagonal passes, not eight: a diagonal pass also lands ink
  // directly above and below wherever the glyph is two pixels wide at that row,
  // which for a bold face is everywhere, so the four corners cover all eight
  // neighbours. The four passes saved are four draws of a 28 pixel face, which
  // is the single most expensive thing on screen during a window transition.
  static const int OUTLINE[4][2] = { { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } };
  graphics_context_set_text_color(ctx, GColorBlack);
  for (int i = 0; i < 4; i++)
    graphics_draw_text(ctx, title, font,
                       GRect(box.origin.x + OUTLINE[i][0], box.origin.y + OUTLINE[i][1],
                             box.size.w, box.size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, title, font, box,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

//! Width one line of text wants, measured with no column to wrap into. Measuring
//! inside the column is the trap: a line too wide for it reports the height of a
//! wrapped paragraph and a width that fits, so the caller concludes it has room.
static int text_width(const char *text, GFont font) {
  const GSize size = graphics_text_layout_get_content_size(
      text, font, GRect(0, 0, 1000, 40), GTextOverflowModeWordWrap, GTextAlignmentLeft);
  return size.w;
}

//! Rect for one line of `h` pixel text, vertically centred in a row. The nudge
//! is the measured offset for the system Gothic faces, which sit low in their
//! line box, and lower still at 28.
static GRect centred_line(int x, int w, GRect row, int h) {
  const int dy = (h >= 28) ? -4 : -2;
  return GRect(x, row.origin.y + (row.size.h - h) / 2 + dy, w, h + 6);
}

#define ROW_PAD 8    //!< gutter between the plank edge and the text inside it
#define VALUE_GAP 8  //!< clear space kept between a label and the value after it

void ui_draw_row(GContext *ctx, GRect box, const char *label, const char *value,
                 bool highlighted, RowStyle style) {
  // The plank fills the row edge to edge on the flat watch and keeps the bezel
  // gutter on the round one; the text keeps the gutter on both, so a long label
  // never runs under the curve. GSize is signed, so an unguarded subtraction on
  // a narrow rect would produce a negative width and graphics_fill_rect would
  // normalise it into a plank drawn to the LEFT of the row rather than drawing
  // nothing.
  const int text_inset = ui_side_inset();
  const int plank_inset = ui_row_inset();
  if (box.size.w <= 2 * text_inset + 24) return;

  const GRect plank = GRect(box.origin.x + plank_inset, box.origin.y,
                            box.size.w - 2 * plank_inset, box.size.h);

  // Only a row that can be picked wears the crate plank. A reading or a locked
  // level can still take the cursor in some states, and dressing it as a choice
  // would promise a SELECT that does nothing.
  if (highlighted && style == ROW_NORMAL) {
    graphics_context_set_fill_color(ctx, GColorChromeYellow);
    graphics_fill_rect(ctx, plank, 0, GCornerNone);
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(plank.origin.x, plank.origin.y, plank.size.w, 2),
                       0, GCornerNone);
    graphics_fill_rect(ctx, GRect(plank.origin.x, plank.origin.y + plank.size.h - 2,
                                  plank.size.w, 2), 0, GCornerNone);
    graphics_context_set_fill_color(ctx, GColorWindsorTan);
    graphics_fill_rect(ctx, GRect(plank.origin.x, plank.origin.y + 3, plank.size.w, 2),
                       0, GCornerNone);
    graphics_fill_rect(ctx, GRect(plank.origin.x, plank.origin.y + plank.size.h - 5,
                                  plank.size.w, 2), 0, GCornerNone);
  }

  graphics_context_set_text_color(ctx, (style == ROW_LOCKED) ? GColorLightGray : GColorBlack);
  const int inner_x = box.origin.x + text_inset + ROW_PAD;
  const int inner_w = box.size.w - 2 * (text_inset + ROW_PAD);

  // A reading is a figure with a caption, so the figure takes the large face and
  // the label shrinks out of its way. Everywhere else the value is as big as the
  // label it belongs to: a value set two sizes down reads as a footnote, when it
  // is the half of the row the player is actually choosing between.
  const bool reading = (style == ROW_INFO);
  GFont label_font = fonts_get_system_font(reading ? FONT_KEY_GOTHIC_18_BOLD
                                                   : FONT_KEY_GOTHIC_24_BOLD);
  const int label_h = reading ? 18 : 24;
  GFont value_font = fonts_get_system_font(reading ? FONT_KEY_GOTHIC_28_BOLD
                                                   : FONT_KEY_GOTHIC_24_BOLD);
  int value_h = reading ? 28 : 24;

  int value_w = 0;
  if (value && value[0]) {
    value_w = text_width(value, value_font) + VALUE_GAP;
    // Drop to the small face rather than let the two collide. Every value used
    // to draw at this size, so the fallback is the old layout exactly: nothing
    // ends up narrower than it was, it only gets wider where there is room.
    if (text_width(label, label_font) + value_w > inner_w) {
      value_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
      value_h = 18;
      value_w = text_width(value, value_font) + VALUE_GAP;
    }
    if (value_w > inner_w) value_w = inner_w;
  }

  graphics_draw_text(ctx, label, label_font,
                     centred_line(inner_x, inner_w - value_w, box, label_h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  if (value_w > 0)
    graphics_draw_text(ctx, value, value_font,
                       centred_line(inner_x + inner_w - value_w, value_w, box, value_h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
}

void ui_format_time(uint32_t secs, char *out, size_t cap) {
  if (!out || cap == 0) return;
  if (secs >= 3600) {
    snprintf(out, cap, "%u:%02u:%02u", (unsigned)(secs / 3600),
             (unsigned)((secs / 60) % 60), (unsigned)(secs % 60));
  } else {
    snprintf(out, cap, "%u:%02u", (unsigned)(secs / 60), (unsigned)(secs % 60));
  }
}
