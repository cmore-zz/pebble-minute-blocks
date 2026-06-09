#include <pebble.h>

#define MARKER_COUNT 12
#define DOT_COUNT 4
#define PIXEL_ROWS 5
#define PIXEL_COLS 3

#if defined(PBL_PLATFORM_EMERY)
#define FUTURE_MARKER_SIZE 4
#define ACTIVE_MARKER_SIZE 9
#define COMPLETE_MARKER_SIZE 18
#define FUTURE_MARKER_OFFSET 4
#define ACTIVE_MARKER_OFFSET 6
#define HOUR_PIXEL_SIZE 11
#define HOUR_Y_OFFSET 0
#else
#define FUTURE_MARKER_SIZE 3
#define ACTIVE_MARKER_SIZE 7
#define COMPLETE_MARKER_SIZE 14
#define FUTURE_MARKER_OFFSET 3
#define ACTIVE_MARKER_OFFSET 5
#define HOUR_PIXEL_SIZE PBL_IF_ROUND_ELSE(8, 9)
#define HOUR_Y_OFFSET 0
#endif

#define SETTINGS_VERSION 2
#define SETTINGS_DEFAULT_BACKGROUND 0x000000
#define SETTINGS_DEFAULT_RING 0xFFFFFF
#define SETTINGS_DEFAULT_COMPLICATION 0xFFFFFF
#define SETTINGS_DEFAULT_HOUR 0x00FFFF
#define USE_VISITOR_COMPLICATION_FONT 1
#define COMPLICATION_SLOT_COUNT 4
#define COMPLICATION_REVEAL_MS 7000
#define LIGHT_POLL_MS 1000

enum {
  TimeModeWatch = 0,
  TimeMode12Hour = 1,
  TimeMode24Hour = 2,
};

enum {
  ComplicationSizeNormal = 0,
  ComplicationSizeMedium = 1,
  ComplicationSizeLarge = 2,
};

enum {
  ComplicationVisibilityAlways = 0,
  ComplicationVisibilityOnTap = 1,
};

enum {
  ComplicationNone = 0,
  ComplicationDate = 1,
  ComplicationWeather = 2,
  ComplicationForecast = 3,
  ComplicationBattery = 4,
  ComplicationBluetooth = 5,
  ComplicationSteps = 6,
};

enum {
  ComplicationSlotTopLeft = 0,
  ComplicationSlotTopRight = 1,
  ComplicationSlotBottomRight = 2,
  ComplicationSlotBottomLeft = 3,
};

enum {
  ConfigKeyBackgroundColor = 10000,
  ConfigKeyRingColor = 10001,
  ConfigKeyComplicationColor = 10002,
  ConfigKeyHourColor = 10003,
  ConfigKeyTimeMode = 10004,
  ConfigKeyComplicationSize = 10005,
  ConfigKeyWeatherEnabled = 10006,
  ConfigKeyWeatherUnits = 10007,
  ConfigKeyWeatherTemp = 10008,
  ConfigKeyWeatherAvailable = 10009,
  ConfigKeyWeatherRequest = 10010,
  ConfigKeyWeatherHigh = 10011,
  ConfigKeyWeatherLow = 10012,
  ConfigKeyComplicationVisibility = 10013,
  ConfigKeyComplicationTopLeft = 10014,
  ConfigKeyComplicationTopRight = 10015,
  ConfigKeyComplicationBottomRight = 10016,
  ConfigKeyComplicationBottomLeft = 10017,
};

enum {
  PersistKeySettingsVersion = 1,
  PersistKeyBackgroundColor,
  PersistKeyRingColor,
  PersistKeyComplicationColor,
  PersistKeyHourColor,
  PersistKeyTimeMode,
  PersistKeyComplicationSize,
  PersistKeyWeatherEnabled,
  PersistKeyWeatherUnits,
  PersistKeyWeatherTemp,
  PersistKeyWeatherAvailable,
  PersistKeyWeatherHigh,
  PersistKeyWeatherLow,
  PersistKeyComplicationVisibility,
  PersistKeyComplicationTopLeft,
  PersistKeyComplicationTopRight,
  PersistKeyComplicationBottomRight,
  PersistKeyComplicationBottomLeft,
};

enum {
  WeatherUnitsF = 0,
  WeatherUnitsC = 1,
};

typedef struct {
  uint32_t background_color;
  uint32_t ring_color;
  uint32_t complication_color;
  uint32_t hour_color;
  int time_mode;
  int complication_size;
  bool weather_enabled;
  int weather_units;
  int weather_temp;
  int weather_high;
  int weather_low;
  bool weather_available;
  int complication_visibility;
  int complications[COMPLICATION_SLOT_COUNT];
} Settings;

static Window *s_window;
static Layer *s_canvas_layer;
static struct tm s_time;
static BatteryChargeState s_battery_state;
static bool s_bluetooth_connected;
static bool s_complications_revealed;
static AppTimer *s_complication_reveal_timer;
static AppTimer *s_light_poll_timer;
static bool s_light_poll_active;
static bool s_was_light_on;
static Settings s_settings;
static GFont s_visitor_font_15;
static GFont s_visitor_font_20;
static GFont s_visitor_font_25;

static void update_light_polling(void);

static const uint8_t DIGITS[10][PIXEL_ROWS] = {
  {0x7, 0x5, 0x5, 0x5, 0x7},
  {0x2, 0x2, 0x2, 0x2, 0x2},
  {0x7, 0x1, 0x7, 0x4, 0x7},
  {0x7, 0x1, 0x7, 0x1, 0x7},
  {0x5, 0x5, 0x7, 0x1, 0x1},
  {0x7, 0x4, 0x7, 0x1, 0x7},
  {0x7, 0x4, 0x7, 0x5, 0x7},
  {0x7, 0x1, 0x2, 0x2, 0x2},
  {0x7, 0x5, 0x7, 0x5, 0x7},
  {0x7, 0x5, 0x7, 0x1, 0x7},
};

static int16_t scale_component(int32_t trig_value, int16_t radius) {
  return (int16_t)((trig_value * radius) / TRIG_MAX_RATIO);
}

static GPoint point_on_circle(GPoint center, int16_t radius, int32_t angle) {
  return GPoint(
    center.x + scale_component(sin_lookup(angle), radius),
    center.y - scale_component(cos_lookup(angle), radius)
  );
}

static void fill_centered_rect(GContext *ctx, GPoint center, int16_t size) {
  graphics_fill_rect(ctx, GRect(center.x - size / 2, center.y - size / 2, size, size), 0, GCornerNone);
}

static void draw_marker_pixel(GContext *ctx, GPoint center, int16_t size) {
  fill_centered_rect(ctx, center, size);
}

static void draw_marker(GContext *ctx, GPoint center, int16_t radius, int marker_index) {
  int minute = s_time.tm_min;
  int completed_markers = minute / 5;
  int active_progress = minute % 5;
  int32_t angle = DEG_TO_TRIGANGLE((marker_index + 1) * 30);
  GPoint marker_center = point_on_circle(center, radius, angle);

  if (marker_index < completed_markers) {
    fill_centered_rect(ctx, marker_center, COMPLETE_MARKER_SIZE);
    return;
  }

  bool is_active = marker_index == completed_markers && active_progress > 0;
  for (int i = 0; i < DOT_COUNT; i++) {
    bool is_active_pixel = is_active && i < active_progress;
    int16_t offset = is_active_pixel ? ACTIVE_MARKER_OFFSET : FUTURE_MARKER_OFFSET;
    int16_t row = i < 2 ? -offset : offset;
    int16_t col = i % 2 == 0 ? -offset : offset;
    GPoint dot = GPoint(marker_center.x + col, marker_center.y + row);
    draw_marker_pixel(ctx, dot, is_active_pixel ? ACTIVE_MARKER_SIZE : FUTURE_MARKER_SIZE);
  }
}

static void draw_pixel_digit(GContext *ctx, int digit, GPoint origin, int16_t pixel_size, int16_t gap) {
  for (int row = 0; row < PIXEL_ROWS; row++) {
    for (int col = 0; col < PIXEL_COLS; col++) {
      if (DIGITS[digit][row] & (1 << (PIXEL_COLS - col - 1))) {
        GRect rect = GRect(
          origin.x + col * (pixel_size + gap),
          origin.y + row * (pixel_size + gap),
          pixel_size,
          pixel_size
        );
        graphics_fill_rect(ctx, rect, 0, GCornerNone);
      }
    }
  }
}

static void draw_pixel_hour(GContext *ctx, GRect bounds) {
  bool is_24h = s_settings.time_mode == TimeModeWatch ? clock_is_24h_style() :
                                                     s_settings.time_mode == TimeMode24Hour;
  int hour = s_time.tm_hour;

  if (!is_24h) {
    hour %= 12;
    if (hour == 0) {
      hour = 12;
    }
  }

  int tens = hour / 10;
  int ones = hour % 10;
  bool draw_tens = is_24h || tens > 0;

  const int16_t pixel = HOUR_PIXEL_SIZE;
  const int16_t gap = 2;
  const int16_t digit_width = PIXEL_COLS * pixel + (PIXEL_COLS - 1) * gap;
  const int16_t digit_height = PIXEL_ROWS * pixel + (PIXEL_ROWS - 1) * gap;
  const int16_t digit_gap = pixel;
  const int16_t total_width = draw_tens ? digit_width * 2 + digit_gap : digit_width;
  int16_t x = bounds.origin.x + (bounds.size.w - total_width) / 2;
  int16_t y = bounds.origin.y + (bounds.size.h - digit_height) / 2 + HOUR_Y_OFFSET;

  if (draw_tens) {
    draw_pixel_digit(ctx, tens, GPoint(x, y), pixel, gap);
    x += digit_width + digit_gap;
  }

  draw_pixel_digit(ctx, ones, GPoint(x, y), pixel, gap);
}

static GFont complication_label_font(void) {
#if USE_VISITOR_COMPLICATION_FONT
  switch (s_settings.complication_size) {
    case ComplicationSizeLarge:
      return s_visitor_font_25;
    case ComplicationSizeMedium:
      return s_visitor_font_15;
    default:
      return s_visitor_font_20;
  }
#else
  switch (s_settings.complication_size) {
    case ComplicationSizeLarge:
      return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
    case ComplicationSizeMedium:
      return fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    default:
      return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  }
#endif
}

static GFont complication_number_font(void) {
#if USE_VISITOR_COMPLICATION_FONT
  return complication_label_font();
#else
  return fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS);
#endif
}

static int16_t complication_row_height(void) {
  switch (s_settings.complication_size) {
    case ComplicationSizeLarge:
      return 28;
    case ComplicationSizeMedium:
      return 22;
    default:
      return 26;
  }
}

static int16_t complication_box_height(void) {
  switch (s_settings.complication_size) {
    case ComplicationSizeLarge:
      return 54;
    case ComplicationSizeMedium:
      return 34;
    default:
      return 44;
  }
}

static int16_t complication_text_width(int16_t normal, int16_t medium, int16_t large) {
  switch (s_settings.complication_size) {
    case ComplicationSizeLarge:
      return large;
    case ComplicationSizeMedium:
      return medium;
    default:
      return normal;
  }
}

static GRect corner_rect(GRect bounds, bool right, bool bottom, int16_t width, int16_t height) {
  int16_t x = right ? bounds.size.w - width - 4 : 4;
  int16_t y = bottom ? bounds.size.h - height - 2 : 0;
  return GRect(x, y, width, height);
}

static GRect complication_slot_rect(GRect bounds, int slot, int16_t width, int16_t height) {
  bool right = slot == ComplicationSlotTopRight || slot == ComplicationSlotBottomRight;
  bool bottom = slot == ComplicationSlotBottomRight || slot == ComplicationSlotBottomLeft;
  return corner_rect(bounds, right, bottom, width, height);
}

static void draw_aligned_text(GContext *ctx, const char *text, GRect rect, GTextAlignment alignment) {
  graphics_draw_text(ctx, text, complication_label_font(), rect,
                     GTextOverflowModeTrailingEllipsis, alignment, NULL);
}

static GSize text_size(const char *text, GFont font, GRect rect) {
  if (!text || text[0] == '\0') {
    return GSize(0, 0);
  }

  return graphics_text_layout_get_content_size(text, font, rect,
                                               GTextOverflowModeTrailingEllipsis,
                                               GTextAlignmentLeft);
}

static void draw_aligned_label_number_suffix(GContext *ctx, const char *label, const char *number,
                                             const char *suffix, GRect rect,
                                             GTextAlignment alignment) {
  GFont label_font = complication_label_font();
  GFont number_font = complication_number_font();
  GSize label_size = text_size(label, label_font, rect);
  GSize number_size = text_size(number, number_font, rect);
  GSize suffix_size = text_size(suffix, label_font, rect);
  int16_t label_gap = label_size.w > 0 && number_size.w > 0 ? 3 : 0;
  int16_t suffix_gap = number_size.w > 0 && suffix_size.w > 0 ? 1 : 0;
  int16_t total_width = label_size.w + label_gap + number_size.w + suffix_gap + suffix_size.w;
  int16_t x = alignment == GTextAlignmentRight ? rect.origin.x + rect.size.w - total_width : rect.origin.x;
#if USE_VISITOR_COMPLICATION_FONT
  int16_t label_offset = 0;
#else
  int16_t label_offset = s_settings.complication_size == ComplicationSizeLarge ? -1 :
                         s_settings.complication_size == ComplicationSizeMedium ? 1 : -1;
#endif
  int16_t label_y = rect.origin.y + (rect.size.h - label_size.h) / 2 + label_offset;
  int16_t number_y = rect.origin.y + (rect.size.h - number_size.h) / 2;
  int16_t suffix_y = rect.origin.y + (rect.size.h - suffix_size.h) / 2 + label_offset;

  if (label_size.w > 0) {
    graphics_draw_text(ctx, label, label_font, GRect(x, label_y, label_size.w, label_size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    x += label_size.w + label_gap;
  }
  if (number_size.w > 0) {
    graphics_draw_text(ctx, number, number_font, GRect(x, number_y, number_size.w, number_size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    x += number_size.w + suffix_gap;
  }
  if (suffix_size.w > 0) {
    graphics_draw_text(ctx, suffix, label_font, GRect(x, suffix_y, suffix_size.w, suffix_size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static void draw_aligned_number_with_suffix(GContext *ctx, const char *number, const char *suffix,
                                            GRect rect, GTextAlignment alignment) {
  if (!number || number[0] == '\0') {
    draw_aligned_text(ctx, suffix, rect, alignment);
    return;
  }

  draw_aligned_label_number_suffix(ctx, "", number, suffix, rect, alignment);
}

static int16_t steps_icon_width(void) {
  switch (s_settings.complication_size) {
    case ComplicationSizeLarge:
      return 15;
    case ComplicationSizeMedium:
      return 10;
    default:
      return 13;
  }
}

static int16_t steps_icon_height(void) {
  switch (s_settings.complication_size) {
    case ComplicationSizeLarge:
      return 18;
    case ComplicationSizeMedium:
      return 14;
    default:
      return 16;
  }
}

static void draw_step_footprint(GContext *ctx, GPoint origin, int16_t scale) {
  graphics_fill_rect(ctx, GRect(origin.x + scale, origin.y, scale * 2, scale * 3), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(origin.x, origin.y + scale * 2, scale * 4, scale * 2), 0, GCornerNone);
}

static void draw_steps_icon(GContext *ctx, GRect rect) {
  int16_t scale = s_settings.complication_size == ComplicationSizeMedium ? 1 : 2;
  int16_t y = rect.origin.y + (rect.size.h - steps_icon_height()) / 2;
  if (s_settings.complication_size == ComplicationSizeMedium) {
    draw_step_footprint(ctx, GPoint(rect.origin.x + 1, y + 7), scale);
    draw_step_footprint(ctx, GPoint(rect.origin.x + 5, y + 2), scale);
  } else {
    draw_step_footprint(ctx, GPoint(rect.origin.x + 1, y + 9), scale);
    draw_step_footprint(ctx, GPoint(rect.origin.x + 7, y + 1), scale);
  }
}

static void draw_aligned_steps(GContext *ctx, const char *number, GRect rect,
                               GTextAlignment alignment) {
  GFont number_font = complication_number_font();
  GSize number_size = text_size(number, number_font, rect);
  int16_t icon_width = steps_icon_width();
  int16_t gap = number_size.w > 0 ? 3 : 0;
  int16_t total_width = icon_width + gap + number_size.w;
  int16_t x = alignment == GTextAlignmentRight ? rect.origin.x + rect.size.w - total_width : rect.origin.x;
  int16_t number_y = rect.origin.y + (rect.size.h - number_size.h) / 2;

  draw_steps_icon(ctx, GRect(x, rect.origin.y, icon_width, rect.size.h));
  x += icon_width + gap;

  if (number_size.w > 0) {
    graphics_draw_text(ctx, number, number_font, GRect(x, number_y, number_size.w, number_size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static void draw_stacked_text(GContext *ctx, const char *top, const char *bottom,
                              GRect rect, GTextAlignment alignment) {
  GFont font = complication_label_font();
  GSize top_size = text_size(top, font, rect);
  GSize bottom_size = text_size(bottom, font, rect);
  int16_t gap = 0;
  int16_t total_height = top_size.h + gap + bottom_size.h;
  int16_t y = rect.origin.y + (rect.size.h - total_height) / 2;
  int16_t top_x = alignment == GTextAlignmentRight ?
                  rect.origin.x + rect.size.w - top_size.w : rect.origin.x;
  int16_t bottom_x = alignment == GTextAlignmentRight ?
                     rect.origin.x + rect.size.w - bottom_size.w : rect.origin.x;

  if (top_size.w > 0) {
    graphics_draw_text(ctx, top, font, GRect(top_x, y, top_size.w, top_size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
  if (bottom_size.w > 0) {
    graphics_draw_text(ctx, bottom, font,
                       GRect(bottom_x, y + top_size.h + gap, bottom_size.w, bottom_size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static int steps_today(void) {
#if defined(PBL_HEALTH)
  HealthServiceAccessibilityMask mask = health_service_metric_accessible(HealthMetricStepCount,
                                                                         time_start_of_today(),
                                                                         time(NULL));
  if (mask & HealthServiceAccessibilityMaskAvailable) {
    return (int)health_service_sum_today(HealthMetricStepCount);
  }
#endif
  return -1;
}

static void format_steps(char *buffer, size_t size) {
  int steps = steps_today();

  if (steps < 0) {
    snprintf(buffer, size, "--");
  } else if (steps >= 10000) {
    snprintf(buffer, size, "%dK", (steps + 500) / 1000);
  } else if (steps >= 1000) {
    snprintf(buffer, size, "%d", (steps / 100) * 100);
  } else {
    snprintf(buffer, size, "%d", steps);
  }
}

static int16_t complication_width(int type) {
  switch (type) {
    case ComplicationDate:
      return complication_text_width(44, 34, 54);
    case ComplicationForecast:
      return complication_text_width(86, 70, 98);
    case ComplicationWeather:
      return complication_text_width(54, 46, 66);
    case ComplicationBattery:
      return complication_text_width(62, 52, 66);
    case ComplicationBluetooth:
      return complication_text_width(44, 38, 48);
    case ComplicationSteps:
      return complication_text_width(70, 58, 82);
    default:
      return 0;
  }
}

static bool complications_visible(void) {
  return s_settings.complication_visibility == ComplicationVisibilityAlways || s_complications_revealed;
}

static void draw_complication(GContext *ctx, GRect bounds, int slot, int type) {
  char date_label[8];
  char date_number[4];
  char battery_number[4];
  char bluetooth_text[4];
  char weather_number[8];
  char forecast_text[12];
  char steps_text[12];
  const char *weather_suffix = s_settings.weather_units == WeatherUnitsC ? "C" : "F";
  int16_t row_height = complication_row_height();
  int16_t box_height = type == ComplicationDate ? complication_box_height() : row_height;
  int16_t width = complication_width(type);
  GRect rect = complication_slot_rect(bounds, slot, width, box_height);
  GTextAlignment alignment = (slot == ComplicationSlotTopRight ||
                              slot == ComplicationSlotBottomRight) ?
                             GTextAlignmentRight : GTextAlignmentLeft;

  if (type == ComplicationNone || width == 0) {
    return;
  }

  strftime(date_label, sizeof(date_label), "%a", &s_time);
  snprintf(date_number, sizeof(date_number), "%02d", s_time.tm_mday);
  snprintf(battery_number, sizeof(battery_number), "%d", s_battery_state.charge_percent);
  snprintf(bluetooth_text, sizeof(bluetooth_text), "%s", s_bluetooth_connected ? "BT" : "--");
  snprintf(weather_number, sizeof(weather_number), "%d", s_settings.weather_available ? s_settings.weather_temp : 0);
  snprintf(forecast_text, sizeof(forecast_text), "%d-%d",
           s_settings.weather_low, s_settings.weather_high);
  format_steps(steps_text, sizeof(steps_text));

  switch (type) {
    case ComplicationDate:
      draw_stacked_text(ctx, date_label, date_number, rect, alignment);
      break;
    case ComplicationWeather:
      if (s_settings.weather_enabled && s_settings.weather_available) {
        draw_aligned_number_with_suffix(ctx, weather_number, weather_suffix, rect, alignment);
      } else {
        draw_aligned_text(ctx, "--", rect, alignment);
      }
      break;
    case ComplicationForecast:
      if (s_settings.weather_enabled && s_settings.weather_available) {
        draw_aligned_number_with_suffix(ctx, forecast_text, weather_suffix, rect, alignment);
      } else {
        draw_aligned_text(ctx, "--", rect, alignment);
      }
      break;
    case ComplicationBattery:
      draw_aligned_number_with_suffix(ctx, battery_number, "%", rect, alignment);
      break;
    case ComplicationBluetooth:
      draw_aligned_text(ctx, bluetooth_text, rect, alignment);
      break;
    case ComplicationSteps:
      draw_aligned_steps(ctx, steps_text, rect, alignment);
      break;
  }
}

static void draw_center_complications(GContext *ctx, GRect bounds) {
  if (!complications_visible()) {
    return;
  }

  graphics_context_set_text_color(ctx, GColorFromHEX(s_settings.complication_color));

  for (int slot = 0; slot < COMPLICATION_SLOT_COUNT; slot++) {
    draw_complication(ctx, bounds, slot, s_settings.complications[slot]);
  }
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  int16_t radius = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 2 - 12;

  graphics_context_set_fill_color(ctx, GColorFromHEX(s_settings.background_color));
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  graphics_context_set_fill_color(ctx, GColorFromHEX(s_settings.ring_color));
  for (int i = 0; i < MARKER_COUNT; i++) {
    draw_marker(ctx, center, radius, i);
  }

  graphics_context_set_fill_color(ctx, GColorFromHEX(s_settings.hour_color));
  draw_pixel_hour(ctx, bounds);
  draw_center_complications(ctx, bounds);
}

static uint32_t persist_read_color(int key, uint32_t fallback) {
  return persist_exists(key) ? (uint32_t)persist_read_int(key) : fallback;
}

static bool valid_complication(int type) {
  return type >= ComplicationNone && type <= ComplicationSteps;
}

static void settings_set_defaults(void) {
  s_settings = (Settings) {
    .background_color = SETTINGS_DEFAULT_BACKGROUND,
    .ring_color = SETTINGS_DEFAULT_RING,
    .complication_color = SETTINGS_DEFAULT_COMPLICATION,
    .hour_color = SETTINGS_DEFAULT_HOUR,
    .time_mode = TimeModeWatch,
    .complication_size = ComplicationSizeNormal,
    .weather_enabled = true,
    .weather_units = WeatherUnitsF,
    .weather_temp = 0,
    .weather_high = 0,
    .weather_low = 0,
    .weather_available = false,
    .complication_visibility = ComplicationVisibilityAlways,
    .complications = {
      ComplicationDate,
      ComplicationWeather,
      ComplicationBattery,
      ComplicationBluetooth,
    },
  };
}

static void settings_load(void) {
  int version = persist_exists(PersistKeySettingsVersion) ?
                persist_read_int(PersistKeySettingsVersion) : 0;

  settings_set_defaults();

  if (version == 0) {
    return;
  }

  s_settings.background_color = persist_read_color(PersistKeyBackgroundColor, SETTINGS_DEFAULT_BACKGROUND);
  s_settings.ring_color = persist_read_color(PersistKeyRingColor, SETTINGS_DEFAULT_RING);
  s_settings.complication_color = persist_read_color(PersistKeyComplicationColor,
                                                     SETTINGS_DEFAULT_COMPLICATION);
  s_settings.hour_color = persist_read_color(PersistKeyHourColor, SETTINGS_DEFAULT_HOUR);
  s_settings.time_mode = persist_exists(PersistKeyTimeMode) ?
                         persist_read_int(PersistKeyTimeMode) : TimeModeWatch;
  s_settings.complication_size = persist_exists(PersistKeyComplicationSize) ?
                                 persist_read_int(PersistKeyComplicationSize) :
                                 ComplicationSizeNormal;
  s_settings.weather_enabled = !persist_exists(PersistKeyWeatherEnabled) ||
                               persist_read_bool(PersistKeyWeatherEnabled);
  s_settings.weather_units = persist_exists(PersistKeyWeatherUnits) ?
                             persist_read_int(PersistKeyWeatherUnits) : WeatherUnitsF;
  s_settings.weather_temp = persist_exists(PersistKeyWeatherTemp) ?
                            persist_read_int(PersistKeyWeatherTemp) : 0;
  s_settings.weather_high = persist_exists(PersistKeyWeatherHigh) ?
                            persist_read_int(PersistKeyWeatherHigh) : 0;
  s_settings.weather_low = persist_exists(PersistKeyWeatherLow) ?
                           persist_read_int(PersistKeyWeatherLow) : 0;
  s_settings.weather_available = persist_exists(PersistKeyWeatherAvailable) &&
                                 persist_read_bool(PersistKeyWeatherAvailable);
  s_settings.complication_visibility = persist_exists(PersistKeyComplicationVisibility) ?
                                       persist_read_int(PersistKeyComplicationVisibility) :
                                       ComplicationVisibilityAlways;
  if (s_settings.complication_visibility < ComplicationVisibilityAlways ||
      s_settings.complication_visibility > ComplicationVisibilityOnTap) {
    s_settings.complication_visibility = ComplicationVisibilityAlways;
  }

  int persist_keys[COMPLICATION_SLOT_COUNT] = {
    PersistKeyComplicationTopLeft,
    PersistKeyComplicationTopRight,
    PersistKeyComplicationBottomRight,
    PersistKeyComplicationBottomLeft,
  };
  int defaults[COMPLICATION_SLOT_COUNT] = {
    ComplicationDate,
    ComplicationWeather,
    ComplicationBattery,
    ComplicationBluetooth,
  };
  for (int slot = 0; slot < COMPLICATION_SLOT_COUNT; slot++) {
    int type = persist_exists(persist_keys[slot]) ? persist_read_int(persist_keys[slot]) : defaults[slot];
    s_settings.complications[slot] = valid_complication(type) ? type : defaults[slot];
  }
}

static void settings_save(void) {
  persist_write_int(PersistKeySettingsVersion, SETTINGS_VERSION);
  persist_write_int(PersistKeyBackgroundColor, (int)s_settings.background_color);
  persist_write_int(PersistKeyRingColor, (int)s_settings.ring_color);
  persist_write_int(PersistKeyComplicationColor, (int)s_settings.complication_color);
  persist_write_int(PersistKeyHourColor, (int)s_settings.hour_color);
  persist_write_int(PersistKeyTimeMode, s_settings.time_mode);
  persist_write_int(PersistKeyComplicationSize, s_settings.complication_size);
  persist_write_bool(PersistKeyWeatherEnabled, s_settings.weather_enabled);
  persist_write_int(PersistKeyWeatherUnits, s_settings.weather_units);
  persist_write_int(PersistKeyWeatherTemp, s_settings.weather_temp);
  persist_write_int(PersistKeyWeatherHigh, s_settings.weather_high);
  persist_write_int(PersistKeyWeatherLow, s_settings.weather_low);
  persist_write_bool(PersistKeyWeatherAvailable, s_settings.weather_available);
  persist_write_int(PersistKeyComplicationVisibility, s_settings.complication_visibility);
  persist_write_int(PersistKeyComplicationTopLeft, s_settings.complications[ComplicationSlotTopLeft]);
  persist_write_int(PersistKeyComplicationTopRight, s_settings.complications[ComplicationSlotTopRight]);
  persist_write_int(PersistKeyComplicationBottomRight, s_settings.complications[ComplicationSlotBottomRight]);
  persist_write_int(PersistKeyComplicationBottomLeft, s_settings.complications[ComplicationSlotBottomLeft]);
}

static void request_weather(void) {
  if (!s_settings.weather_enabled) {
    return;
  }

  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result != APP_MSG_OK || !iter) {
    return;
  }

  dict_write_uint8(iter, ConfigKeyWeatherRequest, 1);
  app_message_outbox_send();
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *background = dict_find(iter, ConfigKeyBackgroundColor);
  Tuple *ring = dict_find(iter, ConfigKeyRingColor);
  Tuple *complication = dict_find(iter, ConfigKeyComplicationColor);
  Tuple *hour = dict_find(iter, ConfigKeyHourColor);
  Tuple *time_mode = dict_find(iter, ConfigKeyTimeMode);
  Tuple *complication_size = dict_find(iter, ConfigKeyComplicationSize);
  Tuple *weather_enabled = dict_find(iter, ConfigKeyWeatherEnabled);
  Tuple *weather_units = dict_find(iter, ConfigKeyWeatherUnits);
  Tuple *weather_temp = dict_find(iter, ConfigKeyWeatherTemp);
  Tuple *weather_available = dict_find(iter, ConfigKeyWeatherAvailable);
  Tuple *weather_high = dict_find(iter, ConfigKeyWeatherHigh);
  Tuple *weather_low = dict_find(iter, ConfigKeyWeatherLow);
  Tuple *complication_visibility = dict_find(iter, ConfigKeyComplicationVisibility);
  Tuple *complication_top_left = dict_find(iter, ConfigKeyComplicationTopLeft);
  Tuple *complication_top_right = dict_find(iter, ConfigKeyComplicationTopRight);
  Tuple *complication_bottom_right = dict_find(iter, ConfigKeyComplicationBottomRight);
  Tuple *complication_bottom_left = dict_find(iter, ConfigKeyComplicationBottomLeft);

  if (background) {
    s_settings.background_color = background->value->uint32;
  }
  if (ring) {
    s_settings.ring_color = ring->value->uint32;
  }
  if (complication) {
    s_settings.complication_color = complication->value->uint32;
  }
  if (hour) {
    s_settings.hour_color = hour->value->uint32;
  }
  if (time_mode) {
    int mode = (int)time_mode->value->int32;
    if (mode >= TimeModeWatch && mode <= TimeMode24Hour) {
      s_settings.time_mode = mode;
    }
  }
  if (complication_size) {
    int size = (int)complication_size->value->int32;
    if (size >= ComplicationSizeNormal && size <= ComplicationSizeLarge) {
      s_settings.complication_size = size;
    }
  }
  if (weather_enabled) {
    s_settings.weather_enabled = weather_enabled->value->int32 != 0;
  }
  if (weather_units) {
    int units = (int)weather_units->value->int32;
    if (units >= WeatherUnitsF && units <= WeatherUnitsC) {
      s_settings.weather_units = units;
    }
  }
  if (weather_temp) {
    s_settings.weather_temp = (int)weather_temp->value->int32;
  }
  if (weather_high) {
    s_settings.weather_high = (int)weather_high->value->int32;
  }
  if (weather_low) {
    s_settings.weather_low = (int)weather_low->value->int32;
  }
  if (weather_available) {
    s_settings.weather_available = weather_available->value->int32 != 0;
  }
  if (complication_visibility) {
    int visibility = (int)complication_visibility->value->int32;
    if (visibility >= ComplicationVisibilityAlways && visibility <= ComplicationVisibilityOnTap) {
      s_settings.complication_visibility = visibility;
    }
  }
  if (complication_top_left) {
    int type = (int)complication_top_left->value->int32;
    if (valid_complication(type)) {
      s_settings.complications[ComplicationSlotTopLeft] = type;
    }
  }
  if (complication_top_right) {
    int type = (int)complication_top_right->value->int32;
    if (valid_complication(type)) {
      s_settings.complications[ComplicationSlotTopRight] = type;
    }
  }
  if (complication_bottom_right) {
    int type = (int)complication_bottom_right->value->int32;
    if (valid_complication(type)) {
      s_settings.complications[ComplicationSlotBottomRight] = type;
    }
  }
  if (complication_bottom_left) {
    int type = (int)complication_bottom_left->value->int32;
    if (valid_complication(type)) {
      s_settings.complications[ComplicationSlotBottomLeft] = type;
    }
  }

  settings_save();
  update_light_polling();
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void refresh_time(void) {
  time_t now = time(NULL);
  struct tm *tick_time = localtime(&now);

  if (tick_time) {
    s_time = *tick_time;
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_time = *tick_time;
  layer_mark_dirty(s_canvas_layer);

  if (tick_time->tm_min % 30 == 0) {
    request_weather();
  }
}

static void battery_handler(BatteryChargeState state) {
  s_battery_state = state;
  layer_mark_dirty(s_canvas_layer);
}

static void bluetooth_handler(bool connected) {
  s_bluetooth_connected = connected;
  layer_mark_dirty(s_canvas_layer);
}

static void complication_reveal_timer_handler(void *context) {
  s_complication_reveal_timer = NULL;
  s_complications_revealed = false;
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void reveal_complications(void) {
  if (s_settings.complication_visibility != ComplicationVisibilityOnTap) {
    return;
  }

  s_complications_revealed = true;
  if (s_complication_reveal_timer) {
    app_timer_cancel(s_complication_reveal_timer);
  }
  s_complication_reveal_timer = app_timer_register(COMPLICATION_REVEAL_MS,
                                                   complication_reveal_timer_handler,
                                                   NULL);
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  reveal_complications();
}

static void light_poll_timer_handler(void *context) {
  s_light_poll_timer = NULL;

  if (!s_light_poll_active) {
    return;
  }

  bool is_light_on = light_is_on();
  if (is_light_on && !s_was_light_on) {
    reveal_complications();
  }
  s_was_light_on = is_light_on;

  s_light_poll_timer = app_timer_register(LIGHT_POLL_MS, light_poll_timer_handler, NULL);
}

static void update_light_polling(void) {
  bool should_poll = s_settings.complication_visibility == ComplicationVisibilityOnTap;

  if (should_poll && !s_light_poll_active) {
    s_light_poll_active = true;
    s_was_light_on = light_is_on();
    if (!s_light_poll_timer) {
      s_light_poll_timer = app_timer_register(LIGHT_POLL_MS, light_poll_timer_handler, NULL);
    }
  } else if (!should_poll && s_light_poll_active) {
    s_light_poll_active = false;
    if (s_light_poll_timer) {
      app_timer_cancel(s_light_poll_timer);
      s_light_poll_timer = NULL;
    }
    s_was_light_on = false;
  }
}

#if defined(PBL_TOUCH)
static void touch_handler(const TouchEvent *event, void *context) {
  if (event && event->type == TouchEvent_Touchdown) {
    reveal_complications();
  }
}
#endif

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
}

static void fonts_load(void) {
  s_visitor_font_15 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_VISITOR_15));
  s_visitor_font_20 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_VISITOR_20));
  s_visitor_font_25 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_VISITOR_25));
}

static void fonts_unload(void) {
  fonts_unload_custom_font(s_visitor_font_15);
  fonts_unload_custom_font(s_visitor_font_20);
  fonts_unload_custom_font(s_visitor_font_25);
}

static void init(void) {
  settings_load();
  fonts_load();
  refresh_time();
  s_battery_state = battery_state_service_peek();
  s_bluetooth_connected = bluetooth_connection_service_peek();

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
  bluetooth_connection_service_subscribe(bluetooth_handler);
  accel_tap_service_subscribe(accel_tap_handler);
#if defined(PBL_TOUCH)
  touch_service_subscribe(touch_handler, NULL);
#endif
  update_light_polling();
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(256, 256);

  window_stack_push(s_window, true);
  app_timer_register(1000, (AppTimerCallback)request_weather, NULL);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  accel_tap_service_unsubscribe();
#if defined(PBL_TOUCH)
  touch_service_unsubscribe();
#endif
  if (s_complication_reveal_timer) {
    app_timer_cancel(s_complication_reveal_timer);
    s_complication_reveal_timer = NULL;
  }
  s_light_poll_active = false;
  if (s_light_poll_timer) {
    app_timer_cancel(s_light_poll_timer);
    s_light_poll_timer = NULL;
  }
  app_message_deregister_callbacks();
  window_destroy(s_window);
  fonts_unload();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
