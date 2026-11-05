#pragma once

#include "complication_value.h"

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
