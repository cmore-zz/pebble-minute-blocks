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
  puts("Plugin reading validation, expiry and built-in fallback checks passed.");
}
