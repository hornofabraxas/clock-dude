// SPDX-License-Identifier: MIT
//
// Settings and progress, held in persistent storage.
//
// Pebble caps a persisted value at PERSIST_DATA_MAX_LENGTH (256 bytes), which
// is why nothing here tries to save a level in progress: the largest board is
// 551 cells. CONTINUE resumes at the level you were on, from the top, which is
// also how the calculator original behaved.

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! Room for more levels than are bundled, so adding one does not invalidate
//! everybody's saved progress.
#define STORE_MAX_LEVELS 16

//! Load settings and progress. Safe to call once at startup; missing or
//! unreadable records fall back to defaults.
void store_init(void);

//! Persist the current settings. Cheap and idempotent.
void store_save_settings(void);

//! Highest level the player has unlocked, zero-based. Level 0 is always open.
int store_unlocked(void);

//! Whether this watch has ever finished a level. The main menu offers a NEW
//! GAME rather than a CONTINUE until it has, because there is nothing to
//! continue: a level abandoned part way through is not saved.
bool store_has_progress(void);

//! Level CONTINUE resumes: the furthest one unlocked. Derived from progress
//! rather than tracked separately, so replaying an early level from the picker
//! cannot drag the player's place backwards.
int store_continue_level(void);

//! Whether a level has ever been finished. Best figures are only meaningful
//! for a completed level: a level solved in under a second legitimately records
//! a best time of zero, so zero cannot double as "no record".
bool store_completed(int index);

//! Best result for a level. Only meaningful when store_completed() is true.
uint16_t store_best_moves(int index);
uint16_t store_best_secs(int index);

//! Clamp a duration to what a stored best can hold, so the win screen compares
//! like with like instead of a raw elapsed against a clamped record.
uint16_t store_clamp_secs(uint32_t secs);

//! Record a finished level. Returns true only when this run BEAT a previous
//! record, so a first clear reads as solved rather than as a personal best.
//! Also unlocks the next level.
bool store_record_win(int index, uint16_t moves, uint32_t secs);

//! Wipe progress and best scores. Settings are left alone.
void store_reset_progress(void);
