#include <stdio.h>

#include "../src/c/complication_layout.h"

// Report a failing geometry case without triggering macOS crash notifications.
#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "Collision check failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
  } \
} while (0)

typedef struct {
  int width, height, inset, complete, active, future, active_offset, future_offset;
  bool round;
  ComplicationRect text;
} Profile;

static bool ring_hits_text(const Profile *profile, int shrink) {
  const int directions[12][2] = {
    {5000, -8660}, {8660, -5000}, {10000, 0}, {8660, 5000},
    {5000, 8660}, {0, 10000}, {-5000, 8660}, {-8660, 5000},
    {-10000, 0}, {-8660, -5000}, {-5000, -8660}, {0, -10000},
  };
  int base_radius = (profile->width < profile->height ? profile->width : profile->height) / 2 - profile->inset;
  int radius = (profile->width < profile->height ? profile->width : profile->height) / 2 -
               complication_ring_inset(profile->inset, shrink, profile->round);
  int scale = complication_marker_scale(base_radius, radius, shrink);
  int active = complication_scale_size(profile->active, scale, 2);
  int future = complication_scale_size(profile->future, scale, 1);
  for (int i = 0; i < 12; i++) {
    int x = profile->width / 2 + directions[i][0] * radius / 10000;
    int y = profile->height / 2 + directions[i][1] * radius / 10000;
    if (complication_rect_hits_marker(profile->text, x, y,
          complication_scale_size(profile->complete, scale, 4), active, future,
          complication_scale_size(profile->active_offset, scale, active / 2 + 1),
          complication_scale_size(profile->future_offset, scale, future / 2 + 1))) {
      return true;
    }
  }
  return false;
}

int main(void) {
  ComplicationRect a = {10, 10, 10, 10};
  CHECK(complication_rects_overlap(a, (ComplicationRect){19, 19, 5, 5}, 0));
  CHECK(!complication_rects_overlap(a, (ComplicationRect){20, 10, 5, 5}, 0));
  CHECK(complication_rects_overlap(a, (ComplicationRect){21, 10, 5, 5}, 2));
  CHECK(!complication_rects_overlap(a, (ComplicationRect){22, 10, 5, 5}, 2));
  CHECK(!complication_rects_overlap(a, (ComplicationRect){15, 15, 0, 5}, 2));

  // A completed block can fit while an active or future dot still hits text.
  ComplicationRect active_only = {38, 38, 2, 2};
  CHECK(!complication_rects_overlap(active_only, (ComplicationRect){45, 45, 10, 10}, 2));
  CHECK(complication_rect_hits_marker(active_only, 50, 50, 10, 8, 1, 9, 1));
  CHECK(complication_rect_hits_marker((ComplicationRect){37, 37, 1, 1},
                                       50, 50, 10, 4, 2, 3, 12));

  // Representative long-text conflicts for every target, including PT2 Normal
  // and Large. Round faces shrink footprints without moving the ring toward text.
  const Profile profiles[] = {
    {144, 168, 12, 10, 6, 2, 4, 2, false, {4, 8, 90, 14}}, // aplite
    {144, 168, 12, 10, 6, 2, 4, 2, false, {4, 8, 90, 14}}, // basalt
    {144, 168, 12, 10, 6, 2, 4, 2, false, {4, 8, 90, 14}}, // diorite
    {144, 168, 12, 10, 6, 2, 4, 2, false, {4, 8, 90, 14}}, // flint
    {144, 118, 15, 10, 6, 2, 4, 2, false, {4, 8, 90, 14}}, // banner + top forecast
    {200, 228, 12, 16, 9, 4, 6, 4, false, {4, 8, 120, 22}},
    {200, 228, 14, 20, 11, 5, 7, 5, false, {4, 8, 120, 22}},
    {180, 180, 12, 12, 7, 3, 5, 3, true, {30, 123, 120, 11}},
    {260, 260, 22, 17, 10, 4, 7, 4, true, {94, 179, 120, 11}},
  };
  for (size_t i = 0; i < sizeof(profiles) / sizeof(profiles[0]); i++) {
    CHECK(ring_hits_text(&profiles[i], 0));
    int shrink = 0;
    while (shrink < COMPLICATION_LAYOUT_MAX_SHRINK && ring_hits_text(&profiles[i], shrink)) {
      shrink++;
    }
    CHECK(shrink > 0);
    CHECK(!ring_hits_text(&profiles[i], shrink));
    Profile clear = profiles[i];
    clear.text = (ComplicationRect){0, 0, 1, 1};
    CHECK(!ring_hits_text(&clear, 0)); // restores the original sizes
  }
  for (int shrink = 0; shrink <= COMPLICATION_LAYOUT_MAX_SHRINK; shrink++) {
    CHECK(complication_shrink_size(2, shrink, 1) >= 1);
    CHECK(complication_shrink_size(6, shrink, 2) >= 2);
    CHECK(complication_shrink_size(7, shrink, 4) >= 4);
    CHECK(complication_scale_size(2, 1000 - shrink * 100, 1) >= 1);
    CHECK(complication_ring_inset(22, shrink, true) == 22);
  }
  CHECK(complication_ring_inset(14, 1, false) == 16);
  // PT2 banner compaction must scale every part of the ring, not just its radius.
  int scale = complication_marker_scale(72, 54, 1);
  CHECK(scale == 750);
  CHECK(complication_scale_size(20, scale, 4) == 15);
  CHECK(complication_scale_size(11, scale, 2) == 8);
  CHECK(complication_scale_size(5, scale, 1) == 3);
  CHECK(complication_scale_size(7, scale, 1) == 5);
  // No geometry can fit an obstruction covering the entire canvas. The search
  // must stop at its documented minimum rather than looping or using zero sizes.
  Profile blocked = profiles[0];
  blocked.text = (ComplicationRect){0, 0, blocked.width, blocked.height};
  int shrink = 0;
  while (shrink < COMPLICATION_LAYOUT_MAX_SHRINK && ring_hits_text(&blocked, shrink)) shrink++;
  CHECK(shrink == COMPLICATION_LAYOUT_MAX_SHRINK);
  CHECK(ring_hits_text(&blocked, shrink));
  puts("Cross-platform marker collision and sizing checks passed.");
  return 0;
}
