#pragma once

#include <stdbool.h>
#include <stdint.h>

static inline bool hour_draw_tens(int hour, bool is_24h, bool leading_zero) {
  return hour >= 10 || (is_24h && leading_zero);
}

// The 1 glyph occupies only the middle column; all other digits span all
// three columns. Balance the visible outer edges of any two-digit hour.
// 11 already balances because its empty columns are symmetric.
static inline int16_t hour_large_centering_offset(int hour, bool draw_tens,
                                                 int16_t pixel, int16_t gap) {
  if (!draw_tens) {
    return 0;
  }
  int16_t left_inset = hour / 10 == 1 ? pixel + gap : 0;
  int16_t right_inset = hour % 10 == 1 ? pixel + gap : 0;
  return (right_inset - left_inset) / 2;
}
