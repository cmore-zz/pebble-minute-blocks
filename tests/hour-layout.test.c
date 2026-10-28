#include <assert.h>
#include <stdio.h>
#include "../src/c/hour_layout.h"

int main(void) {
  for (int hour = 0; hour < 24; hour++) {
    assert(hour_draw_tens(hour, true, true));
    assert(hour_draw_tens(hour, true, false) == (hour >= 10));
    assert(hour_draw_tens(hour, false, true) == (hour >= 10));
    assert(hour_draw_tens(hour, false, false) == (hour >= 10));
  }
  for (int hour = 10; hour < 20; hour++) {
    int offset = hour_large_centering_offset(hour, true, 14, 2);
    // Actual occupied edges: the left 1 has a blank first column, and
    // only a right-hand 1 has a blank final column. Their midpoint must
    // coincide with the center of the original 106px hour cell.
    int left = offset + 16;
    int right_exclusive = offset + 106 - (hour == 11 ? 16 : 0);
    assert(left + right_exclusive == 106);
    assert(hour_large_centering_offset(hour, false, 14, 2) == 0);
  }
  assert(hour_large_centering_offset(1, false, 14, 2) == 0);
  assert(hour_large_centering_offset(9, true, 14, 2) == 0);
  assert(hour_large_centering_offset(20, true, 14, 2) == 0);
  assert(hour_large_centering_offset(23, true, 14, 2) == 0);
  assert(hour_large_centering_offset(12, true, 9, 2) == -5);
  puts("Hour layout and leading-zero checks passed.");
  return 0;
}
