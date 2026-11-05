#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "complication_layout.h"

static inline bool hour_draw_tens(int hour, bool is_24h, bool leading_zero) {
  return hour >= 10 || (is_24h && leading_zero);
}

// The 1 glyph occupies only the middle column; all other digits span all
// three columns. Balance the visible outer edges of any two-digit hour.
// 11 already balances because its empty columns are symmetric.
static inline int16_t hour_visible_centering_offset(int hour, bool draw_tens,
                                                 int16_t pixel, int16_t gap) {
  if (!draw_tens) {
    return 0;
  }
  int16_t left_inset = hour / 10 == 1 ? pixel + gap : 0;
  int16_t right_inset = hour % 10 == 1 ? pixel + gap : 0;
  return (right_inset - left_inset) / 2;
}

#define HOUR_PIXEL_ROWS 5
#define HOUR_PIXEL_COLS 3

static const uint8_t DIGITS[10][HOUR_PIXEL_ROWS] = {
  {0x7, 0x5, 0x5, 0x5, 0x7},
  {0x2, 0x2, 0x2, 0x2, 0x2},
  {0x7, 0x1, 0x7, 0x4, 0x7},
  {0x7, 0x1, 0x7, 0x1, 0x7},
  {0x5, 0x5, 0x7, 0x1, 0x1},
  {0x7, 0x4, 0x7, 0x1, 0x7},
  {0x7, 0x4, 0x7, 0x5, 0x7},
  {0x7, 0x1, 0x2, 0x2, 0x2},
  {0x7, 0x5, 0x7, 0x5, 0x7},
  {0x7, 0x5, 0x7, 0x1, 0x7},
};

typedef struct {
  int tens, ones;
  bool draw_tens;
  int16_t pixel, x, y, digit_width, digit_gap;
} HourGeometry;

typedef bool (*HourPixelVisitor)(ComplicationRect pixel, void *context);

// Drawing and collision detection walk exactly the same lit pixel rectangles.
static inline bool hour_visit_pixels(const HourGeometry *hour, HourPixelVisitor visit, void *context) {
  for (int digit = hour->draw_tens ? 0 : 1; digit < 2; digit++) {
    int number = digit == 0 ? hour->tens : hour->ones;
    int16_t x = hour->x + (digit == 1 && hour->draw_tens ? hour->digit_width + hour->digit_gap : 0);
    for (int row = 0; row < HOUR_PIXEL_ROWS; row++) {
      for (int col = 0; col < HOUR_PIXEL_COLS; col++) {
        if (DIGITS[number][row] & (1 << (HOUR_PIXEL_COLS - col - 1))) {
          ComplicationRect pixel = {x + col * (hour->pixel + 2),
                                    hour->y + row * (hour->pixel + 2), hour->pixel, hour->pixel};
          if (visit(pixel, context)) return true;
        }
      }
    }
  }
  return false;
}
