#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

#include "../src/c/complication_value.h"

static void expect_value(int source, const ComplicationInputs *inputs,
                         ComplicationDisplay display, bool available,
                         const char *text, const char *detail, const char *suffix,
                         const char *label) {
  ComplicationValue value;
  complication_builtin_value(source, inputs, &value);
  assert(value.display == display);
  assert(value.available == available);
  assert(strcmp(value.text, text) == 0);
  assert(strcmp(value.detail, detail) == 0);
  assert(strcmp(value.suffix, suffix) == 0);
  char actual[48];
  assert(complication_value_label(&value, actual, sizeof(actual)));
  assert(strcmp(actual, label) == 0);
}

int main(void) {
  setlocale(LC_TIME, "C");
  const struct tm date = {.tm_wday = 3, .tm_mday = 4};
  ComplicationInputs inputs = {
    .date = &date,
    .now = 1793052000,
    .weather_enabled = true,
    .weather_available = true,
    .weather_updated_at = 1793052000,
    .temperature = 72,
    .low = 61,
    .high = 81,
    .battery_percent = 100,
    .bluetooth_connected = true,
    .steps = 999,
  };

  expect_value(ComplicationDate, &inputs, ComplicationDisplayStacked, true,
               "Wed", "04", "", "Wed 04");
  expect_value(ComplicationWeather, &inputs, ComplicationDisplayNumber, true,
               "72", "", "F", "72F");
  expect_value(ComplicationForecast, &inputs, ComplicationDisplayNumber, true,
               "61-81", "", "F", "61-81F");
  inputs.celsius = true;
  inputs.temperature = -12;
  inputs.low = -20;
  inputs.high = -3;
  expect_value(ComplicationWeather, &inputs, ComplicationDisplayNumber, true,
               "-12", "", "C", "-12C");
  expect_value(ComplicationForecast, &inputs, ComplicationDisplayNumber, true,
               "-20--3", "", "C", "-20--3C");
  expect_value(ComplicationBattery, &inputs, ComplicationDisplayNumber, true,
               "100", "", "%", "100%");
  inputs.battery_percent = 0;
  expect_value(ComplicationBattery, &inputs, ComplicationDisplayNumber, true,
               "0", "", "%", "0%");
  expect_value(ComplicationBluetooth, &inputs, ComplicationDisplayText, true,
               "BT", "", "", "BT");
  inputs.bluetooth_connected = false;
  expect_value(ComplicationBluetooth, &inputs, ComplicationDisplayText, false,
               "--", "", "", "--");

  // Preserve the original rounding thresholds, including unknown Health data.
  const int steps[] = {-1, 0, 999, 1000, 1099, 9999, 10000, 10499, 10500};
  const char *labels[] = {"--", "0", "999", "1000", "1000", "9900", "10K", "10K", "11K"};
  for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
    inputs.steps = steps[i];
    expect_value(ComplicationSteps, &inputs, ComplicationDisplaySteps, steps[i] >= 0,
                 labels[i], "", "", labels[i]);
  }

  // Weather becomes plain placeholder text (no stale unit or number) when
  // disabled, missing, expired, undated, or dated in the future.
  for (int source = ComplicationWeather; source <= ComplicationForecast; source++) {
    for (int reason = 0; reason < 5; reason++) {
      ComplicationInputs missing = inputs;
      if (reason == 0) missing.weather_enabled = false;
      if (reason == 1) missing.weather_available = false;
      if (reason == 2) missing.now += WEATHER_CACHE_MAX_AGE_SECONDS;
      if (reason == 3) missing.weather_updated_at = 0;
      if (reason == 4) missing.weather_updated_at = missing.now + 1;
      expect_value(source, &missing, ComplicationDisplayText, false,
                   "--", "", "", "--");
    }
  }
  inputs.now += WEATHER_CACHE_MAX_AGE_SECONDS - 1;
  expect_value(ComplicationWeather, &inputs, ComplicationDisplayNumber, true,
               "-12", "", "C", "-12C");

  // The same width hints feed drawing and collision detection at every size.
  const int widths[][3] = {{0, 0, 0}, {44, 34, 54}, {54, 46, 66}, {86, 70, 98},
                           {62, 52, 66}, {44, 38, 48}, {70, 58, 82}};
  ComplicationValue value;
  for (int source = ComplicationNone; source <= ComplicationSteps; source++) {
    complication_builtin_value(source, &inputs, &value);
    for (int size = 0; size < 3; size++) {
      assert(value.widths[size] == widths[source][size]);
    }
  }

  // Reusing storage for an empty/invalid slot clears the previous reading.
  const int empty[] = {ComplicationNone, -1, 99};
  for (size_t i = 0; i < sizeof(empty) / sizeof(empty[0]); i++) {
    complication_builtin_value(ComplicationForecast, &inputs, &value);
    complication_builtin_value(empty[i], &inputs, &value);
    char label[8] = "old";
    assert(value.display == ComplicationDisplayNone);
    assert(!value.available && !value.suffix[0] && !value.detail[0]);
    assert(!complication_value_label(&value, label, sizeof(label)));
    assert(label[0] == '\0');
  }

  // A display value can be supplied without a built-in source ID.
  value = (ComplicationValue) {.display = ComplicationDisplayText, .available = true};
  snprintf(value.text, sizeof(value.text), "Next bus");
  char label[5];
  assert(complication_value_label(&value, label, sizeof(label)));
  assert(strcmp(label, "Next") == 0);
  char tiny[2] = {'x', 'y'};
  assert(complication_value_label(&value, tiny, 0));
  assert(tiny[0] == 'x' && tiny[1] == 'y');
  assert(complication_value_label(&value, tiny, 1));
  assert(tiny[0] == '\0' && tiny[1] == 'y');

  // Refreshing an expired value must remove the unit from the previous value.
  complication_builtin_value(ComplicationWeather, &inputs, &value);
  assert(value.suffix[0] == 'C');
  inputs.now++;
  complication_builtin_value(ComplicationWeather, &inputs, &value);
  assert(!value.available && value.display == ComplicationDisplayText);
  assert(value.suffix[0] == '\0');
  assert(strcmp(value.text, "--") == 0);

  puts("Complication values, placeholders, and formatting checks passed.");
  return 0;
}
