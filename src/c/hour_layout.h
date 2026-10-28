#pragma once

#include <stdbool.h>
#include <stdint.h>

static inline bool hour_draw_tens(int hour, bool is_24h, bool leading_zero) {
  return hour >= 10 || (is_24h && leading_zero);
}

// The 1 glyph occupies only the middle column. For 10–19, balance the
// visible outer edges rather than the two full three-column glyph cells.
// 11 already balances: its empty columns are symmetric.
static inline int16_t hour_large_centering_offset(int hour, bool draw_tens,
                                                 int16_t pixel, int16_t gap) {
  if (!draw_tens || hour / 10 != 1 || hour % 10 == 1) {
    return 0;
  }
  return -(pixel + gap) / 2;
}
