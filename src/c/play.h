// SPDX-License-Identifier: MIT
#pragma once

#include <stdbool.h>
#include <stdint.h>

//! Push the play window for a zero-based level index. Returns false if the
//! level does not exist or will not load, in which case nothing is pushed.
bool play_push(int level_index);

//! Seconds spent on the level currently loaded, counting only the time its
//! window was on screen.
uint32_t play_elapsed(void);

//! Rebuild the current level from scratch.
void play_restart(void);

//! Take the board off the stack, landing on whatever is beneath it.
void play_leave(void);

//! Release the play window and the loaded level. Call once, at app exit.
void play_deinit(void);

#ifdef CD_SHOT
//! Screenshot harness only: drive the same path a swipe takes, so a capture can
//! show the loop running rather than a freshly loaded board.
void play_shot_walk(int dir, int times);
#endif
