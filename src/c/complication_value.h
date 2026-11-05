#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "weather_cache.h"

// These IDs are saved on the watch and sent by the existing phone settings page.
enum {
  ComplicationNone = 0,
  ComplicationDate = 1,
  ComplicationWeather = 2,
  ComplicationForecast = 3,
  ComplicationBattery = 4,
  ComplicationBluetooth = 5,
  ComplicationSteps = 6,
};

typedef enum {
  ComplicationDisplayNone = 0,
  ComplicationDisplayText,
  ComplicationDisplayNumber,
  ComplicationDisplayStacked,
  ComplicationDisplaySteps,
} ComplicationDisplay;

// A renderer needs presentation data, not a source ID or access to its services.
// Unavailable readings still have a display (usually "--"); None has no display.
typedef struct {
  ComplicationDisplay display;
  bool available;
  char text[32];
  char detail[8];
  char suffix[2];
  int16_t widths[3]; // normal, medium, large
} ComplicationValue;

// Compare meaning, not struct padding or unused bytes after string terminators.
static inline bool complication_values_equal(const ComplicationValue *a,
                                              const ComplicationValue *b, size_t count) {
  for (size_t i = 0; i < count; i++) {
    if (a[i].display != b[i].display || a[i].available != b[i].available ||
        strcmp(a[i].text, b[i].text) || strcmp(a[i].detail, b[i].detail) ||
        strcmp(a[i].suffix, b[i].suffix)) {
      return false;
    }
    for (int width = 0; width < 3; width++) {
      if (a[i].widths[width] != b[i].widths[width]) return false;
    }
  }
  return true;
}

// Snapshot only: preparing a value never queries Health, the clock, or the phone.
typedef struct {
  const struct tm *date;
  time_t now;
  bool weather_enabled;
  bool weather_available;
  time_t weather_updated_at;
  bool celsius;
  int temperature;
  int low;
  int high;
  int battery_percent;
  bool bluetooth_connected;
  int steps;
} ComplicationInputs;

static inline void complication_value_widths(ComplicationValue *value,
                                             int16_t normal, int16_t medium,
                                             int16_t large) {
  value->widths[0] = normal;
  value->widths[1] = medium;
  value->widths[2] = large;
}

static inline void complication_builtin_value(int source, const ComplicationInputs *inputs,
                                              ComplicationValue *value) {
  *value = (ComplicationValue) {0};
  value->available = true;
  switch (source) {
    case ComplicationDate:
      value->display = ComplicationDisplayStacked;
      strftime(value->text, sizeof(value->text), "%a", inputs->date);
      snprintf(value->detail, sizeof(value->detail), "%02d", inputs->date->tm_mday);
      complication_value_widths(value, 44, 34, 54);
      break;
    case ComplicationWeather:
    case ComplicationForecast:
      value->available = inputs->weather_enabled && inputs->weather_available &&
                        weather_cache_is_fresh(inputs->now, inputs->weather_updated_at);
      value->display = value->available ? ComplicationDisplayNumber : ComplicationDisplayText;
      if (!value->available) {
        snprintf(value->text, sizeof(value->text), "--");
      } else {
        if (source == ComplicationWeather) {
          snprintf(value->text, sizeof(value->text), "%d", inputs->temperature);
        } else {
          snprintf(value->text, sizeof(value->text), "%d-%d", inputs->low, inputs->high);
        }
        value->suffix[0] = inputs->celsius ? 'C' : 'F';
      }
      if (source == ComplicationWeather) {
        complication_value_widths(value, 54, 46, 66);
      } else {
        complication_value_widths(value, 86, 70, 98);
      }
      break;
    case ComplicationBattery:
      value->display = ComplicationDisplayNumber;
      snprintf(value->text, sizeof(value->text), "%d", inputs->battery_percent);
      value->suffix[0] = '%';
      complication_value_widths(value, 62, 52, 66);
      break;
    case ComplicationBluetooth:
      value->display = ComplicationDisplayText;
      value->available = inputs->bluetooth_connected;
      snprintf(value->text, sizeof(value->text), "%s", value->available ? "BT" : "--");
      complication_value_widths(value, 44, 38, 48);
      break;
    case ComplicationSteps:
      value->display = ComplicationDisplaySteps;
      value->available = inputs->steps >= 0;
      if (!value->available) {
        snprintf(value->text, sizeof(value->text), "--");
      } else if (inputs->steps >= 10000) {
        snprintf(value->text, sizeof(value->text), "%dK", (inputs->steps + 500) / 1000);
      } else if (inputs->steps >= 1000) {
        snprintf(value->text, sizeof(value->text), "%d", (inputs->steps / 100) * 100);
      } else {
        snprintf(value->text, sizeof(value->text), "%d", inputs->steps);
      }
      complication_value_widths(value, 70, 58, 82);
      break;
    default:
      value->available = false;
      break;
  }
}

// Round screens use a single line, while rectangular screens can draw the
// same value's number/suffix or stacked lines with their existing spacing.
static inline bool complication_value_label(const ComplicationValue *value,
                                             char *buffer, size_t size) {
  if (value->display == ComplicationDisplayNone) {
    if (size > 0) {
      buffer[0] = '\0';
    }
    return false;
  }
  const bool stacked = value->display == ComplicationDisplayStacked;
  const char *parts[] = {value->text, stacked ? " " : value->suffix,
                        stacked ? value->detail : ""};
  size_t used = 0;
  if (size > 0) {
    for (size_t part = 0; part < sizeof(parts) / sizeof(parts[0]); part++) {
      for (const char *p = parts[part]; *p && used < size - 1; p++) {
        buffer[used++] = *p;
      }
    }
    buffer[used] = '\0';
  }
  return true;
}
