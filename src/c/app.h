// SPDX-License-Identifier: MIT
#pragma once

#include <pebble.h>

//! Zoom ladder, in pixels per tile. Anything below 18 stops being readable on a
//! 1.5 inch screen; the peek level below is reached only while SELECT is held,
//! where losing sprite detail buys context.
#define ZOOM_COUNT 3
#define PEEK_PX 12
extern const uint8_t ZOOM_PX[ZOOM_COUNT];

//! What each rung is called in settings. A pixel count is a number the player
//! has no way to picture before they have seen it; the names say which way the
//! camera moves, which is the only thing the setting actually does.
extern const char *const ZOOM_NAME[ZOOM_COUNT];

typedef struct {
  uint8_t zoom;      //!< index into ZOOM_PX
  bool auto_climb;
  bool vibrate;
} Settings;

extern Settings g_settings;

//! Current zoom in pixels per tile, and the name that goes with it. Always read
//! the ladder through these: a settings record from a build with a different
//! ladder can carry an index this one does not have.
uint8_t app_zoom_px(void);
const char *app_zoom_name(void);
