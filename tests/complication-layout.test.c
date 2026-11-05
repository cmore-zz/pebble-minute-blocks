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
  ComplicationPoint centers[12];
  for (int i = 0; i < 12; i++) {
    centers[i] = (ComplicationPoint){profile->width / 2 + directions[i][0] * radius / 10000,
                                    profile->height / 2 + directions[i][1] * radius / 10000};
  }
  ComplicationMarkers markers = {complication_scale_size(profile->complete, scale, 4), active, future,
    complication_scale_size(profile->active_offset, scale, active / 2 + 1),
    complication_scale_size(profile->future_offset, scale, future / 2 + 1)};
  return complication_rect_hits_ring(profile->text, centers, 12, markers);
}

typedef struct {
  const Profile *profile;
  int ring_probes, hour_probes, last_ring, desired_hour;
} FitProbe;

static bool fit_probe(int shrink, bool hour, void *context) {
  FitProbe *probe = context;
  if (hour) {
    probe->hour_probes++;
    return shrink < probe->desired_hour;
  }
  probe->ring_probes++;
  probe->last_ring = shrink;
  return ring_hits_text(probe->profile, shrink);
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
    FitProbe probe = {.profile = &profiles[i], .desired_hour = 3};
    ComplicationFit fit = complication_select_fit(fit_probe, &probe);
    CHECK(fit.ring > 0 && fit.hour == 3);
    CHECK(probe.last_ring == fit.ring && probe.ring_probes == fit.ring + 1);
    CHECK(probe.hour_probes == 4);
    CHECK(!ring_hits_text(&profiles[i], fit.ring));
    Profile clear = profiles[i];
    clear.text = (ComplicationRect){0, 0, 1, 1};
    probe = (FitProbe){.profile = &clear};
    fit = complication_select_fit(fit_probe, &probe);
    CHECK(fit.ring == 0 && fit.hour == 0); // restores the original sizes
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
  // A banner scales markers even when no complication triggers an extra inset.
  scale = complication_marker_scale(86, 72, 0);
  CHECK(scale == 837);
  CHECK(complication_scale_size(20, scale, 4) == 16);
  CHECK(complication_scale_size(11, scale, 2) == 9);
  CHECK(complication_scale_size(5, scale, 1) == 4);
  CHECK(complication_scale_size(7, scale, 1) == 5);
  CHECK(complication_compact_hour_shrink(0, true) == 0);
  CHECK(complication_compact_hour_shrink(1, true) == 1);
  CHECK(complication_compact_hour_shrink(1, false) == 0); // Normal -> Compact
  CHECK(complication_compact_hour_shrink(2, false) == 1);

  // Fit checks contain the same four rectangles that the steps renderer paints.
  ComplicationRect ink = complication_steps_ink(0, 0, false), pieces[4];
  complication_steps_rects(0, 0, false, pieces);
  CHECK(ink.x == 1 && ink.y == 1 && ink.w == 14 && ink.h == 16);
  ComplicationRect right_pixel = {14, 8, 1, 1}, bottom_pixel = {2, 16, 1, 1};
  CHECK(complication_rects_overlap(right_pixel, pieces[3], 0));
  CHECK(complication_rects_overlap(bottom_pixel, pieces[1], 0));
  CHECK(!complication_rects_overlap(right_pixel, (ComplicationRect){0, 0, 13, 16}, 0));
  CHECK(!complication_rects_overlap(bottom_pixel, (ComplicationRect){0, 0, 13, 16}, 0));
  CHECK(complication_rects_overlap(right_pixel, ink, 0));
  CHECK(complication_rects_overlap(bottom_pixel, ink, 0));
  CHECK(!complication_rect_hits_marker((ComplicationRect){0, 0, 13, 16}, 17, 8, 4, 2, 1, 1, 1));
  CHECK(complication_rect_hits_marker(ink, 17, 8, 4, 2, 1, 1, 1));
  ink = complication_steps_ink(0, 0, true);
  CHECK(ink.x == 1 && ink.y == 2 && ink.w == 8 && ink.h == 9);
  // No geometry can fit an obstruction covering the entire canvas. The search
  // must stop at its documented minimum rather than looping or using zero sizes.
  Profile blocked = profiles[0];
  blocked.text = (ComplicationRect){0, 0, blocked.width, blocked.height};
  FitProbe probe = {.profile = &blocked, .desired_hour = 100};
  ComplicationFit fit = complication_select_fit(fit_probe, &probe);
  CHECK(fit.ring == COMPLICATION_LAYOUT_MAX_SHRINK && fit.hour == COMPLICATION_LAYOUT_MAX_SHRINK);
  CHECK(probe.last_ring == COMPLICATION_LAYOUT_MAX_SHRINK);
  CHECK(probe.ring_probes == COMPLICATION_LAYOUT_MAX_SHRINK + 1);
  CHECK(probe.hour_probes == COMPLICATION_LAYOUT_MAX_SHRINK + 1);
  CHECK(ring_hits_text(&blocked, fit.ring));
  puts("Cross-platform marker collision and sizing checks passed.");
  return 0;
}
