// SPDX-License-Identifier: MIT

#include "gesture.h"

static struct {
  GestureHandler handler;
  void *context;
  bool subscribed;

  bool tracking;    //!< a contact is down and still eligible to become a gesture
  GPoint down;
  GPoint last;      //!< last position update; liftoff coordinates are unusable
  time_t down_s;
  uint16_t down_ms;
  bool wandered;    //!< moved past the tap threshold at some point
  bool crooked;     //!< failed the straightness test, so it cannot be a swipe
} s;

// ---------------------------------------------------------------------------

//! Milliseconds since touchdown, or a value outside the accepted window when
//! the clock is not trustworthy. time_ms() is wall clock, and the watch's RTC is
//! corrected from the phone, so a step can land mid-contact. Rather than let a
//! five second drag be scored as a flick, a step in either direction reports a
//! duration that fails every check. The widening to 64 bits matters: a cold boot
//! with an unset RTC can produce a correction past 24 days, where the seconds
//! multiply would otherwise be signed overflow.
#define ELAPSED_BAD (-1)
#define ELAPSED_CAP (60 * 1000)

static int32_t elapsed_ms(void) {
  time_t now_s = 0;
  uint16_t now_ms = 0;
  time_ms(&now_s, &now_ms);
  const int64_t ms = (int64_t)(now_s - s.down_s) * 1000
                     + ((int32_t)now_ms - (int32_t)s.down_ms);
  if (ms < 0) return ELAPSED_BAD;
  if (ms > ELAPSED_CAP) return ELAPSED_CAP;
  return (int32_t)ms;
}

static int32_t iabs(int32_t v) { return v < 0 ? -v : v; }

//! Travel from touchdown to the last tracked point, split into the dominant
//! axis and the other one. Both the in-flight straightness test and the final
//! verdict measure the path this way, so they cannot drift apart.
static void path_so_far(int32_t *dx, int32_t *dy, int32_t *major, int32_t *minor) {
  const int32_t x = s.last.x - s.down.x;
  const int32_t y = s.last.y - s.down.y;
  const int32_t ax = iabs(x), ay = iabs(y);
  if (dx) *dx = x;
  if (dy) *dy = y;
  *major = (ax > ay) ? ax : ay;
  *minor = (ax > ay) ? ay : ax;
}

static void emit(Gesture g, GPoint at) {
  if (s.handler) s.handler(g, at, s.context);
}

//! Direction of the dominant axis. Screen y grows downward.
static Gesture direction_of(GPoint delta) {
  if (iabs(delta.x) >= iabs(delta.y))
    return (delta.x >= 0) ? GESTURE_SWIPE_RIGHT : GESTURE_SWIPE_LEFT;
  return (delta.y >= 0) ? GESTURE_SWIPE_DOWN : GESTURE_SWIPE_UP;
}

static void on_touchdown(const TouchEvent *e) {
  s.down = GPoint(e->x, e->y);
  s.last = s.down;
  s.down_s = 0;
  s.down_ms = 0;
  time_ms(&s.down_s, &s.down_ms);
  s.wandered = false;
  s.crooked = false;
  s.tracking = true;
}

static void on_move(const TouchEvent *e) {
  if (!s.tracking) return;
  s.last = GPoint(e->x, e->y);

  int32_t major, minor;
  path_so_far(NULL, NULL, &major, &minor);

  // A wandering contact is no longer a tap.
  if (major > GESTURE_MOVE_THRESHOLD_PX) s.wandered = true;
  // Once the path is committed, drifting off the major axis disqualifies it.
  if (major > GESTURE_STRAIGHT_MIN_PX && (minor * 2) > major) s.crooked = true;
}

static void on_liftoff(void) {
  if (!s.tracking) return;
  s.tracking = false;

  // Liftoff reports (0, 0) rather than the finger position, so the end of the
  // gesture is the last position update we saw.
  int32_t dx, dy, major, minor;
  path_so_far(&dx, &dy, &major, &minor);
  const int32_t took = elapsed_ms();

  // A contact that outlives the swipe window is neither a swipe nor a tap, so a
  // long press on the board is simply ignored. That is deliberate: resting a
  // finger on the screen to read the level must not move the dude.
  const bool quick = (took >= 0 && took <= GESTURE_MAX_DURATION_MS);  // ELAPSED_BAD fails this
  const bool long_enough = (major >= GESTURE_SWIPE_MIN_PX);
  const bool clear_major = (major > 0) && ((minor * 2) <= major);

  if (!s.crooked && quick && long_enough && clear_major) {
    emit(direction_of(GPoint((int16_t)dx, (int16_t)dy)), s.last);
    return;
  }
  if (quick && !s.wandered) emit(GESTURE_TAP, s.down);
}

static void touch_handler(const TouchEvent *e, void *context) {
  (void)context;
  if (!e) return;
  // A contact that began while the watch was not in an interaction session must
  // not drive navigation; the firmware latches this for the whole gesture, so
  // every event of that gesture arrives flagged. Dropping the contact here means
  // the flagged liftoff, which would otherwise never reach on_liftoff, cannot
  // leave a stale path behind to be measured against the next touchdown.
  if (e->non_navigational) {
    s.tracking = false;
    return;
  }

  switch (e->type) {
    case TouchEvent_Touchdown:      on_touchdown(e); break;
    case TouchEvent_PositionUpdate: on_move(e);      break;
    case TouchEvent_Liftoff:        on_liftoff();    break;
    default: break;
  }
}

// ---------------------------------------------------------------------------

void gesture_subscribe(GestureHandler handler, void *context) {
  if (s.subscribed) gesture_unsubscribe();
  s.handler = handler;
  s.context = context;
  s.tracking = false;
  s.wandered = false;
  s.crooked = false;
  s.subscribed = true;
  touch_service_subscribe(touch_handler, NULL);
}

void gesture_unsubscribe(void) {
  if (!s.subscribed) return;
  s.tracking = false;
  s.handler = NULL;
  s.context = NULL;
  s.subscribed = false;
  touch_service_unsubscribe();
}
