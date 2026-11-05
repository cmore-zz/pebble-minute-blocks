#include <assert.h>
#include <stdio.h>
#include "../src/c/hour_layout.h"

static bool count_pixel(ComplicationRect pixel, void *context) {
  (void)pixel;
  (*(int *)context)++;
  return false;
}

static bool hit_pixel(ComplicationRect pixel, void *context) {
  return complication_rects_overlap(pixel, *(ComplicationRect *)context, 0);
}

int main(void) {
  for (int hour = 0; hour < 24; hour++) {
    assert(hour_draw_tens(hour, true, true));
    assert(hour_draw_tens(hour, true, false) == (hour >= 10));
    assert(hour_draw_tens(hour, false, true) == (hour >= 10));
    assert(hour_draw_tens(hour, false, false) == (hour >= 10));
  }
  for (int hour = 0; hour < 24; hour++) {
    int offset = hour_visible_centering_offset(hour, true, 14, 2);
    // Actual occupied edges: the left 1 has a blank first column, and
    // only a right-hand 1 has a blank final column. Their midpoint must
    // coincide with the center of the original 106px hour cell.
    int left = offset + (hour / 10 == 1 ? 16 : 0);
    int right_exclusive = offset + 106 - (hour % 10 == 1 ? 16 : 0);
    assert(left + right_exclusive == 106);
    assert(hour_visible_centering_offset(hour, false, 14, 2) == 0);
  }
  assert(hour_visible_centering_offset(1, false, 14, 2) == 0);
  assert(hour_visible_centering_offset(1, true, 14, 2) == 8);
  assert(hour_visible_centering_offset(11, true, 14, 2) == 0);
  assert(hour_visible_centering_offset(21, true, 14, 2) == 8);
  assert(hour_visible_centering_offset(9, true, 14, 2) == 0);
  assert(hour_visible_centering_offset(20, true, 14, 2) == 0);
  assert(hour_visible_centering_offset(23, true, 14, 2) == 0);
  assert(hour_visible_centering_offset(12, true, 9, 2) == -5);
  // All platforms and fitted sizes balance the visible edges, including odd
  // pixel pitches where integer positioning can differ by at most one pixel.
  for (int pixel = 3; pixel <= 14; pixel++) {
    int pitch = pixel + 2;
    int total_width = 7 * pixel + 8;
    for (int hour = 0; hour < 24; hour++) {
      int offset = hour_visible_centering_offset(hour, true, pixel, 2);
      int left = offset + (hour / 10 == 1 ? pitch : 0);
      int right = offset + total_width - (hour % 10 == 1 ? pitch : 0);
      int error = left + right - total_width;
      assert(error >= -1 && error <= 1);
      assert(hour_visible_centering_offset(hour, false, pixel, 2) == 0);
    }
  }
  // Exercise the production pixel visitor shared by painting and collision checks.
  const int expected_pixels[10] = {12, 5, 11, 11, 9, 11, 12, 7, 13, 12};
  HourGeometry geometry = {.pixel = 4, .x = 10, .y = 20, .digit_width = 16, .digit_gap = 4};
  for (int digit = 0; digit < 10; digit++) {
    geometry.ones = digit;
    int count = 0;
    assert(!hour_visit_pixels(&geometry, count_pixel, &count));
    assert(count == expected_pixels[digit]);
  }
  geometry.ones = 1;
  ComplicationRect empty_column = {10, 20, 4, 4}, lit_column = {16, 20, 4, 4};
  ComplicationRect gap = {14, 20, 2, 4};
  assert(!hour_visit_pixels(&geometry, hit_pixel, &empty_column));
  assert(!hour_visit_pixels(&geometry, hit_pixel, &gap));
  assert(hour_visit_pixels(&geometry, hit_pixel, &lit_column));
  geometry.draw_tens = true;
  geometry.tens = 1;
  geometry.ones = 2;
  int count = 0;
  assert(!hour_visit_pixels(&geometry, count_pixel, &count));
  assert(count == 16);
  ComplicationRect ones_origin = {30, 20, 4, 4};
  assert(hour_visit_pixels(&geometry, hit_pixel, &ones_origin));
  puts("Hour layout and leading-zero checks passed.");
  return 0;
}
