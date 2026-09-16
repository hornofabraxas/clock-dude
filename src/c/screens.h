// SPDX-License-Identifier: MIT
//
// The screens outside the board, and the navigation between them.

#pragma once

#include <stdbool.h>

//! Main menu. Pushed at startup.
void screens_push_main(void);

//! Unwind the whole stack back to the main menu.
void screens_go_main(void);

//! Level picker, showing each level's best move count and time.
void screens_push_levels(void);

//! Settings list.
void screens_push_settings(void);

//! Controls and credits.
void screens_push_help(void);

//! Pause list over the board. False if the window could not be built, which
//! matters because BACK is the board's only exit.
bool screens_push_pause(void);

//! Level solved. Shows this run against the stored best and offers the next
//! level. `moves` and `secs` are the finished run.
bool screens_push_win(int level_index, unsigned moves, unsigned secs, bool new_best);

//! Release every screen's window. Call once, at app exit.
void screens_deinit(void);
