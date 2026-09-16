// SPDX-License-Identifier: MIT

#include "store.h"

#include <pebble.h>
#include <string.h>

#include "app.h"
#include "levels.h"

#define KEY_SETTINGS 1
#define KEY_PROGRESS 2

#define SETTINGS_VERSION 1
#define PROGRESS_VERSION 1

typedef struct {
  uint8_t version;
  uint8_t zoom;
  uint8_t auto_climb;
  uint8_t vibrate;
  //! Was "the controls screen has been shown". The game no longer opens on that
  //! screen, so nothing reads it. The byte stays so the record keeps its length
  //! and a player's existing settings still load under version 1.
  uint8_t reserved;
} StoredSettings;

typedef struct {
  uint8_t version;
  uint8_t unlocked;       //!< highest unlocked level, zero-based
  uint8_t reserved[2];    //!< explicit padding, so the layout is not left to the compiler
  uint16_t completed;     //!< bit per level; the best figures mean nothing without it
  uint16_t best_moves[STORE_MAX_LEVELS];
  uint16_t best_secs[STORE_MAX_LEVELS];
} StoredProgress;

// The completed bitmask has one bit per level, and the whole record has to fit
// in a single persisted value. Both ceilings are silent if crossed: a shift past
// the width evaluates to zero, and an oversized write simply fails.
_Static_assert(STORE_MAX_LEVELS <= 16, "completed is a uint16_t bitmask");
_Static_assert(sizeof(StoredProgress) <= PERSIST_DATA_MAX_LENGTH,
               "progress record must fit one persisted value");

static StoredProgress s_progress;

// ---------------------------------------------------------------------------
// Records
//
// Reads check the version byte before the length. Rejecting a record because it
// is the wrong size, as an earlier cut did, means a layout change can never be
// migrated: the data is discarded before anything gets to look at what version
// it is. A future version adds a branch here that reads the older layout and
// converts it, rather than widening the length test.
// ---------------------------------------------------------------------------

static bool read_record(uint32_t key, void *out, size_t want, uint8_t version) {
  if (!persist_exists(key)) return false;

  uint8_t buf[PERSIST_DATA_MAX_LENGTH];
  const int n = persist_read_data(key, buf, sizeof(buf));
  if (n < 1) return false;
  if (buf[0] != version) return false;   // no older layouts exist yet
  if (n != (int)want) return false;

  memcpy(out, buf, want);
  return true;
}

static void write_record(uint32_t key, const void *data, size_t len, const char *what) {
  const int n = persist_write_data(key, data, len);
  if (n != (int)len) {
    // The per-app persist quota is finite. Saying so beats a best score that
    // silently fails to survive the next launch.
    APP_LOG(APP_LOG_LEVEL_ERROR, "could not save %s (%d)", what, n);
  }
}

static void progress_defaults(void) {
  memset(&s_progress, 0, sizeof(s_progress));
  s_progress.version = PROGRESS_VERSION;
}

static void save_progress(void) {
  write_record(KEY_PROGRESS, &s_progress, sizeof(s_progress), "progress");
}

void store_save_settings(void) {
  const StoredSettings st = {
    .version = SETTINGS_VERSION,
    .zoom = g_settings.zoom,
    .auto_climb = g_settings.auto_climb ? 1 : 0,
    .vibrate = g_settings.vibrate ? 1 : 0,
    .reserved = 0,
  };
  write_record(KEY_SETTINGS, &st, sizeof(st), "settings");
}

void store_init(void) {
  StoredSettings st;
  if (read_record(KEY_SETTINGS, &st, sizeof(st), SETTINGS_VERSION)) {
    g_settings.zoom = (st.zoom < ZOOM_COUNT) ? st.zoom : 1;
    g_settings.auto_climb = st.auto_climb != 0;
    g_settings.vibrate = st.vibrate != 0;
  }

  // Stage into a local and commit only once it is known good, so a short or
  // corrupt record cannot leave the live copy half-overwritten.
  StoredProgress p;
  if (!read_record(KEY_PROGRESS, &p, sizeof(p), PROGRESS_VERSION)) {
    progress_defaults();
    return;
  }
  s_progress = p;

  // Bound against the levels that actually exist, not the array capacity. A
  // record from a build with more levels, or one stray byte, would otherwise
  // open every level at once: the level list gates purely on this number.
  const int last = levels_count() - 1;
  if (last < 0) { progress_defaults(); return; }
  if (s_progress.unlocked > (uint8_t)last) s_progress.unlocked = (uint8_t)last;
}

// ---------------------------------------------------------------------------

int store_unlocked(void) { return s_progress.unlocked; }

int store_continue_level(void) { return s_progress.unlocked; }

bool store_has_progress(void) {
  return s_progress.unlocked > 0 || s_progress.completed != 0;
}

bool store_completed(int index) {
  if (index < 0 || index >= STORE_MAX_LEVELS) return false;
  return (s_progress.completed & (uint16_t)(1u << index)) != 0;
}

uint16_t store_best_moves(int index) {
  if (index < 0 || index >= STORE_MAX_LEVELS) return 0;
  return s_progress.best_moves[index];
}

uint16_t store_best_secs(int index) {
  if (index < 0 || index >= STORE_MAX_LEVELS) return 0;
  return s_progress.best_secs[index];
}

uint16_t store_clamp_secs(uint32_t secs) {
  return (secs > UINT16_MAX) ? (uint16_t)UINT16_MAX : (uint16_t)secs;
}

bool store_record_win(int index, uint16_t moves, uint32_t secs) {
  if (index < 0 || index >= STORE_MAX_LEVELS) return false;

  // A level can only run past 18 hours if it was left open; clamp rather than
  // wrapping into an absurdly good time.
  const uint16_t capped = store_clamp_secs(secs);

  const bool first = !store_completed(index);
  // "Best" means beaten, not merely recorded. Calling a first clear a personal
  // best would make the title meaningless on a first playthrough, and the win
  // screen would announce a record with nothing to compare it against.
  bool beaten = false;
  if (first || moves < s_progress.best_moves[index]) {
    beaten = !first && moves < s_progress.best_moves[index];
    s_progress.best_moves[index] = moves;
  }
  if (first || capped < s_progress.best_secs[index]) {
    beaten = beaten || (!first && capped < s_progress.best_secs[index]);
    s_progress.best_secs[index] = capped;
  }
  s_progress.completed |= (uint16_t)(1u << index);

  if (index + 1 < levels_count() && s_progress.unlocked < (uint8_t)(index + 1))
    s_progress.unlocked = (uint8_t)(index + 1);

  save_progress();
  return beaten;
}

void store_reset_progress(void) {
  progress_defaults();
  save_progress();
}
