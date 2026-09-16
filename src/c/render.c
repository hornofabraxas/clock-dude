// SPDX-License-Identifier: MIT

#include "render.h"

#include "app.h"

const uint8_t ZOOM_PX[ZOOM_COUNT] = { 18, 24, 30 };
const char *const ZOOM_NAME[ZOOM_COUNT] = { "FURTHER", "DEFAULT", "CLOSER" };

//! Fall back to the middle rung rather than trusting a stored index, so a
//! settings record written by a build with a different ladder cannot index off
//! the end of either table.
static uint8_t zoom_index(void) {
  return (g_settings.zoom < ZOOM_COUNT) ? g_settings.zoom : 1;
}

uint8_t app_zoom_px(void) { return ZOOM_PX[zoom_index()]; }
const char *app_zoom_name(void) { return ZOOM_NAME[zoom_index()]; }

#ifdef PBL_ROUND
  #define HUD_TOP 42
  #define HUD_BOTTOM 36
#else
  #define HUD_TOP 22
  #define HUD_BOTTOM 0
#endif

GRect render_board_view(GRect bounds) {
  return GRect(0, HUD_TOP, bounds.size.w, bounds.size.h - HUD_TOP - HUD_BOTTOM);
}

// ---------------------------------------------------------------------------
// Tiles
//
// Detail is a function of the tile size rather than a set of fixed sprites, so
// the same code reads correctly from the 12px peek up to the 30px maximum. The
// thresholds below are where each layer of detail starts to earn its pixels.
// ---------------------------------------------------------------------------

#define DETAIL_FINE 10  //!< legs, eyes, crate bracing
#define DETAIL_RICH 20  //!< mortar courses, plank rails, door arch

static void fill(GContext *ctx, GColor c, GRect r) {
  graphics_context_set_fill_color(ctx, c);
  graphics_fill_rect(ctx, r, 0, GCornerNone);
}

//! Border of thickness `b` drawn inside `r` on all four sides.
static void frame(GContext *ctx, GColor c, GRect r, int b) {
  fill(ctx, c, GRect(r.origin.x, r.origin.y, r.size.w, b));
  fill(ctx, c, GRect(r.origin.x, r.origin.y + r.size.h - b, r.size.w, b));
  fill(ctx, c, GRect(r.origin.x, r.origin.y, b, r.size.h));
  fill(ctx, c, GRect(r.origin.x + r.size.w - b, r.origin.y, b, r.size.h));
}

void render_wall(GContext *ctx, GRect r) {
  const int t = r.size.w;
  const int x = r.origin.x, y = r.origin.y;
  int edge = t / 12;
  if (edge < 1) edge = 1;

  fill(ctx, GColorDarkGray, r);
  fill(ctx, GColorLightGray, GRect(x, y, t, edge));
  // Black on the right and bottom keeps individual bricks countable even at the
  // peek size, where a flat fill would smear into one grey mass.
  fill(ctx, GColorBlack, GRect(x + t - edge, y, edge, t));
  fill(ctx, GColorBlack, GRect(x, y + t - edge, t, edge));

  if (t >= 12) {
    const int mid = y + t / 2;
    fill(ctx, GColorBlack, GRect(x, mid, t - edge, edge));
    fill(ctx, GColorBlack, GRect(x + t / 2, y, edge, t / 2));
    if (t >= DETAIL_RICH) {
      fill(ctx, GColorBlack, GRect(x + t / 4, mid, edge, t / 2));
      fill(ctx, GColorBlack, GRect(x + (3 * t) / 4, mid, edge, t / 2));
      fill(ctx, GColorLightGray, GRect(x, mid + edge, t - edge, edge));
    }
  }
}

void render_block(GContext *ctx, GRect r) {
  const int t = r.size.w;
  const int x = r.origin.x, y = r.origin.y;
  int bd = t / 14;
  if (bd < 1) bd = 1;

  fill(ctx, GColorChromeYellow, r);
  frame(ctx, GColorBlack, r, bd);

  if (t >= DETAIL_FINE) {
    const int in = bd + (t >= DETAIL_RICH ? 3 : 1);
    graphics_context_set_stroke_color(ctx, GColorWindsorTan);
    graphics_context_set_stroke_width(ctx, (t >= DETAIL_RICH) ? 3 : 1);
    graphics_draw_line(ctx, GPoint(x + in, y + in), GPoint(x + t - 1 - in, y + t - 1 - in));
    graphics_draw_line(ctx, GPoint(x + t - 1 - in, y + in), GPoint(x + in, y + t - 1 - in));
    graphics_context_set_stroke_width(ctx, 1);
  }
  if (t >= DETAIL_RICH) {
    int rail = t / 10;
    if (rail < 2) rail = 2;
    fill(ctx, GColorWindsorTan, GRect(x + bd, y + bd, t - 2 * bd, rail));
    fill(ctx, GColorWindsorTan, GRect(x + bd, y + t - bd - rail, t - 2 * bd, rail));
  }
}

void render_door(GContext *ctx, GRect r) {
  const int t = r.size.w;
  const int x = r.origin.x, y = r.origin.y;
  int bd = t / 12;
  if (bd < 1) bd = 1;

  fill(ctx, GColorDarkGreen, r);
  frame(ctx, GColorBrightGreen, r, bd);

  if (t >= DETAIL_FINE) {
    int knob = t / 6;
    if (knob < 2) knob = 2;
    fill(ctx, GColorBrightGreen,
         GRect(x + t - bd - knob - (t / 8), y + t / 2 - knob / 2, knob, knob));
  }
  if (t >= DETAIL_RICH) {
    fill(ctx, GColorBrightGreen, GRect(x + t / 4, y + bd, t / 2, bd));
  }
}

void render_dude(GContext *ctx, GRect r, int dir) {
  const int t = r.size.w;
  const int cx = r.origin.x + t / 2;

  int outline = t / 16;   if (outline < 1) outline = 1;
  int head_w  = t / 2;    if (head_w  < 2) head_w  = 2;
  int head_h  = (t * 2) / 5; if (head_h < 2) head_h = 2;
  const int leg_h = (t >= DETAIL_FINE) ? t / 5 : 1;
  const int head_x = cx - head_w / 2;
  const int head_y = r.origin.y + ((t >= 12) ? t / 12 : 0);

  const GRect head = GRect(head_x, head_y, head_w, head_h);
  fill(ctx, GColorMelon, head);
  if (t >= DETAIL_FINE) {
    frame(ctx, GColorBlack, head, outline);

    int eye = t / 8;
    if (eye < 1) eye = 1;
    const int eye_x = (dir > 0) ? head_x + head_w - outline - eye - (t / 10)
                                : head_x + outline + (t / 10);
    fill(ctx, GColorBlack, GRect(eye_x, head_y + head_h / 3, eye, eye));
  }

  int body_w = t / 2;
  if (body_w < 2) body_w = 2;
  const int body_y = head_y + head_h;
  int body_h = r.origin.y + t - body_y - leg_h;
  if (body_h < 1) body_h = 1;
  const int body_x = cx - body_w / 2;

  fill(ctx, GColorBlue, GRect(body_x, body_y, body_w, body_h));
  if (t >= 14) {
    fill(ctx, GColorBlack, GRect(body_x, body_y, outline, body_h));
    fill(ctx, GColorBlack, GRect(body_x + body_w - outline, body_y, outline, body_h));
    fill(ctx, GColorBlack, GRect(body_x, body_y, body_w, outline));
  }

  // The forward arm is the one facing cue that survives at the peek size.
  int arm_w = t / 7;
  if (arm_w < 1) arm_w = 1;
  const int arm_h = (body_h > 2) ? body_h - body_h / 3 : 1;
  const int arm_x = (dir > 0) ? body_x + body_w : body_x - arm_w;
  const int arm_y = body_y + (body_h - arm_h) / 2;
  fill(ctx, GColorMelon, GRect(arm_x, arm_y, arm_w, arm_h));
  if (t >= 14) {
    fill(ctx, GColorBlack, GRect(arm_x, arm_y, arm_w, outline));
    fill(ctx, GColorBlack, GRect(arm_x, arm_y + arm_h - outline, arm_w, outline));
  }

  if (t >= DETAIL_FINE) {
    const int leg_y = body_y + body_h;
    const int leg_bottom = r.origin.y + t - leg_y;
    int leg_w = t / 5;
    if (leg_w < 1) leg_w = 1;
    fill(ctx, GColorBlack, GRect(body_x, leg_y, leg_w, leg_bottom));
    fill(ctx, GColorBlack, GRect(body_x + body_w - leg_w, leg_y, leg_w, leg_bottom));
  }
}

void render_brick_band(GContext *ctx, GRect band, int tile) {
  if (tile <= 0 || band.size.w <= 0 || band.size.h <= 0) return;
  const int right = band.origin.x + band.size.w;
  const int bottom = band.origin.y + band.size.h;

  // Same pixels render_wall() would lay down tile by tile, drawn as spans
  // instead. A band is the one place the same wall repeats across a whole rect,
  // and every horizontal in the pattern runs the full width of its tile, so one
  // fill per course does the work of one per tile. It matters because a band is
  // redrawn on every frame of a window transition, where a few hundred fills is
  // the difference between a slide that reads as motion and one that stutters.
  //
  // Order is load bearing. The spans run edge to edge, including over the black
  // right-hand edge of each tile; the verticals go on afterwards and put that
  // edge back, which is what keeps the result identical to the per-tile version.
  int edge = tile / 12;
  if (edge < 1) edge = 1;

  int mid_off = 0;
  if (tile >= 12) mid_off = tile / 2;

  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, band, 0, GCornerNone);

  for (int y = band.origin.y; y < bottom; y += tile) {
    fill(ctx, GColorLightGray, GRect(band.origin.x, y, band.size.w, edge));
    fill(ctx, GColorBlack, GRect(band.origin.x, y + tile - edge, band.size.w, edge));
    if (tile < 12) continue;
    fill(ctx, GColorBlack, GRect(band.origin.x, y + mid_off, band.size.w, edge));
    if (tile >= DETAIL_RICH)
      fill(ctx, GColorLightGray, GRect(band.origin.x, y + mid_off + edge, band.size.w, edge));
  }

  // One fill per column for the tile edges: they line up across every course, so
  // the whole band's worth is a single full-height stripe.
  graphics_context_set_fill_color(ctx, GColorBlack);
  for (int x = band.origin.x; x < right; x += tile)
    graphics_fill_rect(ctx, GRect(x + tile - edge, band.origin.y, edge, band.size.h),
                       0, GCornerNone);

  if (tile < 12) return;
  for (int y = band.origin.y; y < bottom; y += tile) {
    for (int x = band.origin.x; x < right; x += tile) {
      graphics_fill_rect(ctx, GRect(x + mid_off, y, edge, tile / 2), 0, GCornerNone);
      if (tile < DETAIL_RICH) continue;
      graphics_fill_rect(ctx, GRect(x + tile / 4, y + mid_off, edge, tile / 2), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(x + (3 * tile) / 4, y + mid_off, edge, tile / 2),
                         0, GCornerNone);
    }
  }
}

// ---------------------------------------------------------------------------
// Playfield
// ---------------------------------------------------------------------------

//! Integer division that rounds toward negative infinity, so a camera origin to
//! the left of the view does not truncate toward zero and skip a column.
static int floor_div(int a, int b) { return (a >= 0) ? (a / b) : -(((-a) + b - 1) / b); }
static int ceil_div(int a, int b) { return (a >= 0) ? ((a + b - 1) / b) : -((-a) / b); }
static int clamp_cell(int v, int n) { return (v < 0) ? 0 : ((v > n) ? n : v); }

//! Top-left pixel of cell (0,0). Centres an axis when the level fits, otherwise
//! follows the dude and clamps so the view never runs off the board.
static int camera_origin(int view_origin, int view_size, int level_size,
                         int focus_cell, int tile) {
  if (level_size <= view_size) return view_origin + (view_size - level_size) / 2;
  int o = view_origin + view_size / 2 - (focus_cell * tile + tile / 2);
  if (o > view_origin) o = view_origin;
  const int min_o = view_origin + view_size - level_size;
  if (o < min_o) o = min_o;
  return o;
}

void render_playfield(GContext *ctx, GRect view, const Game *g, int tile) {
  fill(ctx, GColorWhite, view);
  if (!g || !g->map || tile <= 0) return;

  const int ox = camera_origin(view.origin.x, view.size.w, g->w * tile, g->px, tile);
  const int oy = camera_origin(view.origin.y, view.size.h, g->h * tile, g->py, tile);
  // Clamp to the cells that can actually land in the view. The largest level is
  // 551 cells and at most 80 of them are ever on screen, so walking the whole
  // board to cull it would be almost all wasted work on every redraw.
  const int x0 = clamp_cell(floor_div(view.origin.x - ox, tile), g->w);
  const int x1 = clamp_cell(ceil_div(view.origin.x + view.size.w - ox, tile), g->w);
  const int y0 = clamp_cell(floor_div(view.origin.y - oy, tile), g->h);
  const int y1 = clamp_cell(ceil_div(view.origin.y + view.size.h - oy, tile), g->h);

  for (int y = y0; y < y1; y++) {
    // The loop bounds already guarantee these cells exist, so index the row
    // directly rather than paying game_at's out-of-bounds guard per tile.
    const uint8_t *row = &g->map[(size_t)y * g->w];
    const int py = oy + y * tile;
    for (int x = x0; x < x1; x++) {
      const GRect r = GRect(ox + x * tile, py, tile, tile);
      switch (row[x]) {
        case TILE_WALL:  render_wall(ctx, r);  break;
        case TILE_BLOCK: render_block(ctx, r); break;
        case TILE_DOOR:  render_door(ctx, r);  break;
        default: break;
      }
    }
  }

  if (g->carry)
    render_block(ctx, GRect(ox + g->px * tile, oy + (g->py - 1) * tile, tile, tile));
  render_dude(ctx, GRect(ox + g->px * tile, oy + g->py * tile, tile, tile), g->dir);
}

void render_hud(GContext *ctx, GRect bounds, int level_number, uint16_t moves) {
  char level_text[16];
  char moves_text[16];
  snprintf(level_text, sizeof(level_text), "LEVEL %d", level_number);
  snprintf(moves_text, sizeof(moves_text), "MOVES %u", (unsigned)moves);

  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  fill(ctx, GColorBlack, GRect(0, 0, bounds.size.w, HUD_TOP));
  graphics_context_set_text_color(ctx, GColorWhite);

#ifdef PBL_ROUND
  // A single line across the top of a circle is clipped at any readable size,
  // so the two readings take the top and bottom arcs instead.
  fill(ctx, GColorBlack, GRect(0, bounds.size.h - HUD_BOTTOM, bounds.size.w, HUD_BOTTOM));
  graphics_draw_text(ctx, level_text, font, GRect(0, HUD_TOP - 26, bounds.size.w, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, moves_text, font,
                     GRect(0, bounds.size.h - HUD_BOTTOM + 3, bounds.size.w, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
#else
  graphics_draw_text(ctx, level_text, font, GRect(6, -3, bounds.size.w - 12, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, moves_text, font, GRect(6, -3, bounds.size.w - 12, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
#endif
}
