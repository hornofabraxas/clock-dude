// SPDX-License-Identifier: MIT
//
// Swipe and tap recognition on top of the raw touch service.
//
// The thresholds and the decision procedure here mirror the firmware's own
// recognizers in PebbleOS (applib/ui/recognizer/swipe.c and tap.c) so gestures
// in the game feel the same as gestures anywhere else on the watch. We cannot
// simply use the firmware's recognizers: those reach apps only through the
// touch navigation bridge, which collapses every tap on a hand-drawn window to
// a plain SELECT and reserves a left-to-right swipe for BACK. This game needs a
// rightward swipe to mean "walk right", so it consumes raw touch events and
// reproduces the firmware's behaviour rather than inheriting it.

#pragma once

#include <pebble.h>

//! Minimum travel along the major axis for a flick to count as a swipe.
#define GESTURE_SWIPE_MIN_PX 30
//! Longest contact that can still be a swipe or a tap. A contact that outlives
//! this reports nothing, so resting a finger on the board never moves anything.
#define GESTURE_MAX_DURATION_MS 300
//! Once the major axis passes this, the path must stay straight.
#define GESTURE_STRAIGHT_MIN_PX 10
//! Contact that wanders more than this is no longer a tap.
#define GESTURE_MOVE_THRESHOLD_PX 10

typedef enum {
  GESTURE_NONE = 0,
  GESTURE_SWIPE_UP,
  GESTURE_SWIPE_DOWN,
  GESTURE_SWIPE_LEFT,
  GESTURE_SWIPE_RIGHT,
  GESTURE_TAP,
} Gesture;

//! Called for each recognized gesture. `at` is the touchdown point for a tap and
//! the last tracked point for a swipe.
typedef void (*GestureHandler)(Gesture gesture, GPoint at, void *context);

//! Start delivering gestures. Subscribing enables the touch sensor; pair every
//! call with gesture_unsubscribe() when the window disappears so the sensor is
//! released.
void gesture_subscribe(GestureHandler handler, void *context);
void gesture_unsubscribe(void);
