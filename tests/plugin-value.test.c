#include <assert.h>
#include <stdio.h>
#include "../src/c/plugin_value.h"

int main(void) {
  PluginReading reading = {0};
  const time_t now = 1800000000;
  ComplicationInputs inputs = {.battery_percent = 80};
  ComplicationValue value;
  complication_builtin_value(ComplicationBattery, &inputs, &value);
  assert(!plugin_value_apply(&value, &reading, 1, now));
  assert(!strcmp(value.text, "80"));
  assert(plugin_reading_set(&reading, 1, "72F", 60, now));
  assert(!plugin_value_apply(&value, &reading, 2, now));
  assert(plugin_value_apply(&value, &reading, 1, now));
  assert(!strcmp(value.text, "72F"));
  assert(value.display == ComplicationDisplayText && value.available);
  complication_builtin_value(ComplicationBattery, &inputs, &value);
  assert(!plugin_value_apply(&value, &reading, 1, now + 60));
  assert(!plugin_value_apply(&value, &reading, 1, now - PLUGIN_MAX_AGE_SEC));
  assert(!strcmp(value.text, "80"));
  assert(plugin_reading_set(&reading, 1, "", 0, now));
  assert(!plugin_value_apply(&value, &reading, 1, now));
  assert(!plugin_reading_set(&reading, 0, "x", 60, now));
  assert(!plugin_reading_set(&reading, 6, "x", 60, now));
  assert(!plugin_reading_set(&reading, 1, "12345678", 60, now));
  assert(!plugin_reading_set(&reading, 1, "bad\n", 60, now));
  assert(!plugin_reading_set(&reading, 1, "x", 901, now));
  assert(!plugin_reading_set(&reading, 1, "x", -1, now));
  PluginReading before;
  assert(plugin_reading_set(&reading, 1, "72F", 60, now));
  before = reading;
  assert(plugin_reading_set(&reading, 1, "72F", 120, now));
  assert(!plugin_reading_display_changed(&before, &reading, now));
  assert(plugin_reading_display_changed(&before, &reading, now + 70));
  before = reading;
  assert(plugin_reading_set(&reading, 1, "73F", 120, now));
  assert(plugin_reading_display_changed(&before, &reading, now));
  before = reading;
  assert(plugin_reading_set(&reading, 1, "", 0, now));
  assert(plugin_reading_display_changed(&before, &reading, now));
  ComplicationRect regions[] = {{4, 0, 78, 20}, {62, 0, 78, 20}, {4, 140, 30, 20}};
  uint8_t slots[] = {0, 1, 3};
  assert(plugin_overlap_mask(regions, slots, 3, 3) == 2);
  assert(plugin_overlap_mask(regions, slots, 3, 1) == 1); // plugin vs built-in
  assert(plugin_overlap_mask(regions, slots, 3, 0) == 0);
  regions[1].w = 70;
  assert(plugin_overlap_mask(regions, slots, 3, 3) == 1); // wider left goes first
  regions[1].w = 78;
  ComplicationRect fallback[] = {{4, 0, 78, 20}, {100, 0, 40, 20}};
  assert(plugin_overlap_mask(fallback, slots, 2, 1) == 0); // keep the left plugin
  regions[1].x = 83; // Exactly one pixel between the ink bounds fits.
  assert(plugin_overlap_mask(regions, slots, 3, 3) == 0);
  regions[1].x = 82;
  assert(plugin_overlap_mask(regions, slots, 3, 3) == 2);
  slots[1] = 0; // stacked lines within a slot don't conflict
  assert(plugin_overlap_mask(regions, slots, 3, 3) == 0);
  puts("Plugin reading validation, expiry and built-in fallback checks passed.");
}
