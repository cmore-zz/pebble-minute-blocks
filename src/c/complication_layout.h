#pragma once

#include <stdbool.h>
#include <stdint.h>

#define COMPLICATION_LAYOUT_MAX_SHRINK 8
#define COMPLICATION_LAYOUT_CLEARANCE 1

typedef struct {
  int16_t x, y, w, h;
} ComplicationRect;

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
