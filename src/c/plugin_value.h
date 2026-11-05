#pragma once

#include "complication_value.h"
#include "complication_layout.h"

#define PLUGIN_TEXT_LENGTH 7
#define PLUGIN_MAX_AGE_SEC 900

typedef struct {
  char text[PLUGIN_TEXT_LENGTH + 1];
  time_t expires_at;
  uint8_t favorite;
} PluginReading;

static inline bool plugin_reading_set(PluginReading *reading, int favorite,
                                      const char *text, int ttl, time_t now) {
  if (favorite < 1 || favorite > 5 || !text || ttl < 0 || ttl > PLUGIN_MAX_AGE_SEC) return false;
  size_t length = strlen(text);
  if (length > PLUGIN_TEXT_LENGTH) return false;
  for (size_t i = 0; i < length; i++) {
    if ((unsigned char)text[i] < 32 || (unsigned char)text[i] > 126) return false;
  }
  *reading = (PluginReading){.favorite = favorite, .expires_at = now + ttl};
  memcpy(reading->text, text, length + 1);
  return true;
}

// Leave the already-prepared built-in fallback intact when a plugin is absent,
// denied, expired, or from a previously selected favorite. Readings aren't saved.
static inline bool plugin_value_apply(ComplicationValue *value, const PluginReading *reading,
                                      int favorite, time_t now) {
  if (!favorite || reading->favorite != favorite || !reading->text[0] ||
      now >= reading->expires_at || reading->expires_at - now > PLUGIN_MAX_AGE_SEC) return false;
  *value = (ComplicationValue){.display = ComplicationDisplayText, .available = true};
  memcpy(value->text, reading->text, sizeof(reading->text));
  complication_value_widths(value, 84, 63, 112);
  return true;
}

// Return only plugin slots involved in an actual ink collision with another
// slot. Built-in values retain their existing layout and text size.
static inline uint8_t plugin_overlap_mask(const ComplicationRect *regions,
                                          const uint8_t *slots, int count,
                                          uint8_t plugin_mask) {
  uint8_t result = 0;
  for (int i = 0; i < count; i++) {
    for (int j = i + 1; j < count; j++) {
      if (slots[i] != slots[j] && complication_rects_overlap(regions[i], regions[j], 1)) {
        result |= ((1u << slots[i]) | (1u << slots[j])) & plugin_mask;
      }
    }
  }
  return result;
}

static inline bool plugin_reading_visible(const PluginReading *reading, time_t now) {
  return reading->text[0] && reading->expires_at > now &&
         reading->expires_at - now <= PLUGIN_MAX_AGE_SEC;
}

static inline bool plugin_reading_display_changed(const PluginReading *before,
                                                 const PluginReading *after, time_t now) {
  bool was_visible = plugin_reading_visible(before, now);
  bool is_visible = plugin_reading_visible(after, now);
  return was_visible != is_visible || (is_visible && strcmp(before->text, after->text) != 0);
}
