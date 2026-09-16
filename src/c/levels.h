// SPDX-License-Identifier: MIT
#pragma once

//! A level is a NUL-terminated array of row strings plus its row count. The
//! text format and legend are documented at the top of levels.c.
typedef struct {
  const char *const *rows;
  int nrows;
} Level;

//! Number of bundled levels.
int levels_count(void);

//! Bundled level by zero-based index, or NULL when the index is out of range.
const Level *levels_get(int index);
