#pragma once

#include <stdbool.h>
#include <stdint.h>

#define COMPLICATION_LAYOUT_MAX_SHRINK 8
#define COMPLICATION_LAYOUT_CLEARANCE 1

typedef struct {
  int16_t x, y, w, h;
} ComplicationRect;

typedef struct {
  int16_t x, y;
} ComplicationPoint;

typedef struct {
  int16_t complete, active, future, active_offset, future_offset;
} ComplicationMarkers;

typedef struct {
  int ring, hour;
} ComplicationFit;

typedef bool (*ComplicationFitProbe)(int shrink, bool hour, void *context);

// Probe the final bounded candidate too, so callers can apply its geometry even
// when no candidate fits. Ring and hour searches progress independently.
static inline ComplicationFit complication_select_fit(ComplicationFitProbe probe, void *context) {
  ComplicationFit fit = {0};
  for (; fit.ring < COMPLICATION_LAYOUT_MAX_SHRINK; fit.ring++) {
    if (!probe(fit.ring, false, context)) break;
  }
  if (fit.ring == COMPLICATION_LAYOUT_MAX_SHRINK) probe(fit.ring, false, context);
  for (; fit.hour < COMPLICATION_LAYOUT_MAX_SHRINK; fit.hour++) {
    if (!probe(fit.hour, true, context)) break;
  }
  if (fit.hour == COMPLICATION_LAYOUT_MAX_SHRINK) probe(fit.hour, true, context);
  return fit;
}

static inline int complication_compact_hour_shrink(int shrink, bool already_compact) {
  return shrink > 0 && !already_compact ? shrink - 1 : shrink;
}

// These are the actual four filled rectangles, relative to the icon's drawing
// origin. Both painting and collision measurement use this geometry.
static inline void complication_steps_rects(int16_t x, int16_t y, bool small,
                                            ComplicationRect rects[4]) {
  int scale = small ? 1 : 2;
  int first_y = small ? 7 : 9;
  int second_x = small ? 5 : 7;
  int second_y = small ? 2 : 1;
  rects[0] = (ComplicationRect){x + 1 + scale, y + first_y, scale * 2, scale * 3};
  rects[1] = (ComplicationRect){x + 1, y + first_y + scale * 2, scale * 4, scale * 2};
  rects[2] = (ComplicationRect){x + second_x + scale, y + second_y, scale * 2, scale * 3};
  rects[3] = (ComplicationRect){x + second_x, y + second_y + scale * 2, scale * 4, scale * 2};
}

static inline ComplicationRect complication_steps_ink(int16_t x, int16_t y, bool small) {
  ComplicationRect rects[4];
  complication_steps_rects(x, y, small, rects);
  int left = rects[0].x, top = rects[0].y;
  int right = left + rects[0].w, bottom = top + rects[0].h;
  for (int i = 1; i < 4; i++) {
    if (rects[i].x < left) left = rects[i].x;
    if (rects[i].y < top) top = rects[i].y;
    if (rects[i].x + rects[i].w > right) right = rects[i].x + rects[i].w;
    if (rects[i].y + rects[i].h > bottom) bottom = rects[i].y + rects[i].h;
  }
  return (ComplicationRect){left, top, right - left, bottom - top};
}

static inline bool complication_rects_overlap(ComplicationRect a, ComplicationRect b,
                                              int16_t clearance) {
  return a.w > 0 && a.h > 0 && b.w > 0 && b.h > 0 &&
         a.x < b.x + b.w + clearance && a.x + a.w > b.x - clearance &&
         a.y < b.y + b.h + clearance && a.y + a.h > b.y - clearance;
}

static inline int16_t complication_shrink_size(int16_t size, int shrink, int16_t minimum) {
  return size - shrink > minimum ? size - shrink : minimum;
}

static inline int16_t complication_scale_size(int16_t size, int scale, int16_t minimum) {
  int16_t scaled = (size * scale) / 1000;
  return scaled > minimum ? scaled : minimum;
}

// Marker footprints follow the reduced radius. On round faces the radius stays
// fixed, but conflicts can still require smaller squares and dots inside it.
static inline int complication_marker_scale(int16_t base_radius, int16_t radius, int shrink) {
  int radial = base_radius > 0 ? radius * 1000 / base_radius : 1000;
  int footprint = 1000 - shrink * 50;
  return radial < footprint ? radial : footprint;
}

// Corner text needs the ring farther inward. Round faces put their text inside
// the ring instead, so retain its radius and reduce the marker footprints.
static inline int16_t complication_ring_inset(int16_t base, int shrink,
                                              bool round) {
  if (round || shrink == 0) {
    return base;
  }
  return base + shrink * 2;
}

// Check all resting states, independently of the current minute.
static inline bool complication_rect_hits_marker(ComplicationRect text,
                                                  int16_t x, int16_t y,
                                                  int16_t complete, int16_t active,
                                                  int16_t future, int16_t active_offset,
                                                  int16_t future_offset) {
  ComplicationRect block = {x - complete / 2, y - complete / 2, complete, complete};
  if (complication_rects_overlap(text, block, COMPLICATION_LAYOUT_CLEARANCE)) {
    return true;
  }
  for (int state = 0; state < 2; state++) {
    int16_t size = state == 0 ? active : future;
    int16_t offset = state == 0 ? active_offset : future_offset;
    for (int dot = 0; dot < 4; dot++) {
      block = (ComplicationRect) {
        x + (dot % 2 == 0 ? -offset : offset) - size / 2,
        y + (dot < 2 ? -offset : offset) - size / 2,
        size, size,
      };
      if (complication_rects_overlap(text, block, COMPLICATION_LAYOUT_CLEARANCE)) {
        return true;
      }
    }
  }
  return false;
}

static inline bool complication_rect_hits_ring(ComplicationRect text,
                                               const ComplicationPoint *centers, int count,
                                               ComplicationMarkers markers) {
  for (int i = 0; i < count; i++) {
    if (complication_rect_hits_marker(text, centers[i].x, centers[i].y,
          markers.complete, markers.active, markers.future,
          markers.active_offset, markers.future_offset)) return true;
  }
  return false;
}
