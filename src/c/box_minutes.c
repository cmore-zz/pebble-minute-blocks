#include <pebble.h>
#include "weather_cache.h"
#include "hour_layout.h"
#include "complication_value.h"
#include "complication_layout.h"

#define MARKER_COUNT 12
#define DOT_COUNT 4
#define PIXEL_ROWS HOUR_PIXEL_ROWS
#define PIXEL_COLS HOUR_PIXEL_COLS

#if defined(PBL_PLATFORM_EMERY)
// Large mode adds ~25% to the blocks. When corner details risk overlap, pull
// the ring in and reduce the hour to leave room for long forecasts. Otherwise keep
// the blocks near the edges; breathe inward on launch to avoid clipping.
#define BASE_FUTURE_MARKER_SIZE (s_settings.large_blocks ? 5 : 4)
#define BASE_ACTIVE_MARKER_SIZE (s_settings.large_blocks ? 11 : 9)
#define BASE_COMPLETE_MARKER_SIZE (s_settings.large_blocks ? 20 : 16)
#define BASE_FUTURE_MARKER_OFFSET (s_settings.large_blocks ? 5 : 4)
#define BASE_ACTIVE_MARKER_OFFSET (s_settings.large_blocks ? 7 : 6)
#define BASE_HOUR_PIXEL_SIZE (s_settings.large_blocks ? 14 : 11)
#define BASE_HOUR_COMPACT_PIXEL_SIZE (s_settings.large_blocks ? 12 : 8)
#define BASE_HOUR_OVERLAY_PIXEL_SIZE (s_settings.large_blocks ? 9 : 7)
#define HOUR_Y_OFFSET 0
#define BASE_RING_INSET (s_settings.large_blocks ? 14 : 12)
#define INTRO_PULSE_AMPLITUDE (s_settings.large_blocks ? -4 : 4)
#elif defined(PBL_PLATFORM_CHALK)
#define BASE_FUTURE_MARKER_SIZE 3
#define BASE_ACTIVE_MARKER_SIZE 7
#define BASE_COMPLETE_MARKER_SIZE 12
#define BASE_FUTURE_MARKER_OFFSET 3
#define BASE_ACTIVE_MARKER_OFFSET 5
#define BASE_HOUR_PIXEL_SIZE 8
#define BASE_HOUR_COMPACT_PIXEL_SIZE 6
#define BASE_HOUR_OVERLAY_PIXEL_SIZE 5
#define HOUR_Y_OFFSET 0
#elif defined(PBL_PLATFORM_GABBRO)
// Pebble Round 2: round like chalk, but 260x260 vs 180x180 -- chalk's sizes
// scaled ~1.44x. BASE_ACTIVE_MARKER_OFFSET 7 vs BASE_ACTIVE_MARKER_SIZE 10 keeps the four
// active dots separated (4px gap).
#define BASE_FUTURE_MARKER_SIZE 4
#define BASE_ACTIVE_MARKER_SIZE 10
#define BASE_COMPLETE_MARKER_SIZE 17
#define BASE_FUTURE_MARKER_OFFSET 4
#define BASE_ACTIVE_MARKER_OFFSET 7
#define BASE_HOUR_PIXEL_SIZE 12
#define BASE_HOUR_COMPACT_PIXEL_SIZE 9
#define BASE_HOUR_OVERLAY_PIXEL_SIZE 7
#define HOUR_Y_OFFSET 0
// Bigger blocks on a round screen reach the bezel at the diagonals; pull the
// ring in further than the default 12px inset.
#define BASE_RING_INSET 22
#elif defined(PBL_PLATFORM_APLITE) || defined(PBL_PLATFORM_BASALT) || \
      defined(PBL_PLATFORM_DIORITE) || defined(PBL_PLATFORM_FLINT)
#define BASE_FUTURE_MARKER_SIZE 2
#define BASE_ACTIVE_MARKER_SIZE 6
#define BASE_COMPLETE_MARKER_SIZE 10
#define BASE_FUTURE_MARKER_OFFSET 2
#define BASE_ACTIVE_MARKER_OFFSET 4
#define BASE_HOUR_PIXEL_SIZE 8
#define BASE_HOUR_COMPACT_PIXEL_SIZE 7
#define BASE_HOUR_OVERLAY_PIXEL_SIZE 6
#define HOUR_Y_OFFSET 0
#else
#define BASE_FUTURE_MARKER_SIZE 1
#define BASE_ACTIVE_MARKER_SIZE 3
#define BASE_COMPLETE_MARKER_SIZE 7
#define BASE_FUTURE_MARKER_OFFSET 2
#define BASE_ACTIVE_MARKER_OFFSET 3
#define BASE_HOUR_PIXEL_SIZE 8
#define BASE_HOUR_COMPACT_PIXEL_SIZE 7
#define BASE_HOUR_OVERLAY_PIXEL_SIZE 6
#define HOUR_Y_OFFSET 0
#endif

// Distance the marker ring is inset from the nearest screen edge. Platforms can
// override above (e.g. gabbro pulls in further for its larger round blocks).
#ifndef BASE_RING_INSET
#define BASE_RING_INSET 12
#endif

// Every drawing and animation path uses the selected temporary sizes.
#define FUTURE_MARKER_SIZE complication_scale_size(BASE_FUTURE_MARKER_SIZE, s_marker_scale, 1)
#define ACTIVE_MARKER_SIZE complication_scale_size(BASE_ACTIVE_MARKER_SIZE, s_marker_scale, 2)
#define COMPLETE_MARKER_SIZE complication_scale_size(BASE_COMPLETE_MARKER_SIZE, s_marker_scale, 4)
#define FUTURE_MARKER_OFFSET \
  complication_scale_size(BASE_FUTURE_MARKER_OFFSET, s_marker_scale, FUTURE_MARKER_SIZE / 2 + 1)
#define ACTIVE_MARKER_OFFSET \
  complication_scale_size(BASE_ACTIVE_MARKER_OFFSET, s_marker_scale, ACTIVE_MARKER_SIZE / 2 + 1)
#define HOUR_PIXEL_SIZE complication_shrink_size(BASE_HOUR_PIXEL_SIZE, s_hour_shrink, 3)
#define HOUR_COMPACT_PIXEL_SIZE \
  complication_shrink_size(BASE_HOUR_COMPACT_PIXEL_SIZE, complication_compact_hour_shrink(s_hour_shrink, PBL_IF_ROUND_ELSE(true, false)), 3)
#define HOUR_OVERLAY_PIXEL_SIZE complication_shrink_size(BASE_HOUR_OVERLAY_PIXEL_SIZE, s_hour_shrink, 3)

#define SETTINGS_VERSION 2
#define SETTINGS_DEFAULT_BACKGROUND 0x000000
#define SETTINGS_DEFAULT_RING 0xFFFFFF
#define SETTINGS_DEFAULT_COMPLICATION 0xFFFFFF
#define SETTINGS_DEFAULT_HOUR 0x00FFFF
#define USE_VISITOR_COMPLICATION_FONT 1
#define COMPLICATION_SLOT_COUNT 4
#define COMPLICATION_REVEAL_MS 7000
#define KINETIC_FRAME_MS 33
#define KINETIC_FALL_PIXELS_PER_SECOND 220
#define KINETIC_SMASH_DURATION_MS 1500
#define KINETIC_FRAGMENT_DURATION_MS 900
#define KINETIC_TOP_HOUR_FALL_MS 1300
#define KINETIC_TOP_HOUR_STAGGER_MS 700
#define KINETIC_FALL_WINDOW_MS 1000
#define INTRO_FRAME_MS 33
#define INTRO_LEAD_MS 325
#define INTRO_DURATION_MS 1700
#define INTRO_HOUR_DURATION_MS 560
#define INTRO_HOUR_SWEEP_MS 280
#define INTRO_HOUR_PIECE_MS 200
#define INTRO_RING_START_MS 300
#define INTRO_MARKER_STAGGER_MS 42
#define INTRO_MARKER_DURATION_MS 240
// The whole ring pulses and returns once it has fully landed. Large PT2
// blocks breathe inward; other layouts breathe outward.
#define INTRO_RING_LANDED_MS (INTRO_RING_START_MS + \
                              (MARKER_COUNT - 1) * INTRO_MARKER_STAGGER_MS + \
                              INTRO_MARKER_DURATION_MS)
#define INTRO_PULSE_MS 260
#ifndef INTRO_PULSE_AMPLITUDE
#define INTRO_PULSE_AMPLITUDE 4
#endif
// A settle/charge beat after the ring lands, before it pulses (must be at least
// ~1 frame / 33ms to register).
#define INTRO_PULSE_DELAY_MS 100

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
  SecondsVisibilityNever = 0,
  SecondsVisibilityAlways = 1,
  SecondsVisibilityOnTap = 2,
};

enum {
  ComplicationSlotTopLeft = 0,
  ComplicationSlotTopRight = 1,
  ComplicationSlotBottomRight = 2,
  ComplicationSlotBottomLeft = 3,
};

typedef enum {
  HourModeNormal = 0,
  HourModeCompact = 1,
  HourModeOverlay = 2,
} HourMode;

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
  PersistKeySecondsVisibility,
  PersistKeyKineticEnabled,
  PersistKeyLargeBlocks,
  PersistKeyWeatherUpdatedAt,
  PersistKeyHourLeadingZero,
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
  bool hour_leading_zero;
  int complication_size;
  bool weather_enabled;
  int weather_units;
  int weather_temp;
  int weather_high;
  int weather_low;
  bool weather_available;
  time_t weather_updated_at;
  int complication_visibility;
  int seconds_visibility;
  bool kinetic_enabled;
  bool large_blocks;
  int complications[COMPLICATION_SLOT_COUNT];
} Settings;

static Window *s_window;
static Layer *s_canvas_layer;
static struct tm s_time;
static BatteryChargeState s_battery_state;
static bool s_bluetooth_connected;
static int s_steps = -1;
static bool s_complications_revealed;
static AppTimer *s_complication_reveal_timer;
static AppTimer *s_kinetic_frame_timer;
static AppTimer *s_kinetic_arm_timer;
static AppTimer *s_intro_frame_timer;
static bool s_intro_active;
static bool s_intro_started;
static time_t s_intro_start_time;
static uint16_t s_intro_start_ms;
static Settings s_settings;
static ComplicationValue s_complication_values[COMPLICATION_SLOT_COUNT];
static GFont s_visitor_font_15;
static GFont s_visitor_font_20;
static GFont s_visitor_font_25;

static uint8_t s_layout_shrink;
static uint8_t s_hour_shrink;
static int s_marker_scale = 1000;
static GRect s_layout_canvas_bounds;

static void refresh_time(void);
static void update_backlight_subscription(void);
static void update_tick_subscription(void);
static void kinetic_update_frame_timer(void);
static void kinetic_schedule_arm_timer(void);
static void intro_update_frame_timer(void);

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

static GPoint marker_center_for_index(GPoint center, int16_t radius, int marker_index) {
  int32_t angle = DEG_TO_TRIGANGLE((marker_index + 1) * 30);
  return point_on_circle(center, radius, angle);
}

static GPoint marker_dot_for_index(GPoint marker_center, int dot_index, int16_t offset) {
  int16_t row = dot_index < 2 ? -offset : offset;
  int16_t col = dot_index % 2 == 0 ? -offset : offset;
  return GPoint(marker_center.x + col, marker_center.y + row);
}

// Clock order for the four dots within a marker: top-right, bottom-right,
// bottom-left, top-left (clockwise from upper right, like an analog quadrant).
// Minute fill, the seconds sweep, and the kinetic incoming pixel all share this
// order so the dots always progress consistently.
static int ordered_dot_index(int progress_index) {
  static const uint8_t order[DOT_COUNT] = {1, 3, 2, 0};
  return order[progress_index % DOT_COUNT];
}

// Single wall-clock read for the kinetic timing helpers. Any of the out-pointers
// may be NULL. ms_until_minute and ms_after_minute are complementary within the
// minute (ms_until = 60000 - ms_after). Returns false if the clock is unavailable.
static bool kinetic_now(int *min, int32_t *ms_until_minute, int32_t *ms_after_minute) {
  time_t now;
  uint16_t milliseconds;
  time_ms(&now, &milliseconds);
  struct tm *time_now = localtime(&now);
  if (!time_now) {
    return false;
  }

  int32_t after = time_now->tm_sec * 1000 + milliseconds;
  if (min) {
    *min = time_now->tm_min;
  }
  if (ms_after_minute) {
    *ms_after_minute = after;
  }
  if (ms_until_minute) {
    *ms_until_minute = 60000 - after;
  }
  return true;
}

// Length of the pre-boundary animation: the smash on a completing marker
// (min % 5 == 4), otherwise the shorter falling-pixel window.
static int32_t kinetic_run_up_ms(int min) {
  return (min % 5 == 4) ? KINETIC_SMASH_DURATION_MS : KINETIC_FALL_WINDOW_MS;
}

static int16_t kinetic_lerp_int16(int16_t from, int16_t to, int32_t amount) {
  return from + ((to - from) * amount) / 1000;
}

static int32_t clamp_progress(int32_t elapsed, int32_t duration) {
  if (elapsed <= 0) {
    return 0;
  }
  if (elapsed >= duration) {
    return 1000;
  }
  return (elapsed * 1000) / duration;
}

static int32_t ease_out_progress(int32_t progress) {
  int32_t inverse = 1000 - progress;
  return 1000 - (inverse * inverse) / 1000;
}

static bool intro_elapsed_ms(int32_t *elapsed) {
  if (!s_intro_active) {
    return false;
  }

  if (!s_intro_started) {
    time_ms(&s_intro_start_time, &s_intro_start_ms);
    s_intro_started = true;
    *elapsed = 0;
    return true;
  }

  time_t now;
  uint16_t milliseconds;
  time_ms(&now, &milliseconds);
  *elapsed = (int32_t)(now - s_intro_start_time) * 1000 +
             (int32_t)milliseconds - (int32_t)s_intro_start_ms;
  if (*elapsed < 0) {
    *elapsed = 0;
  }
  return true;
}

static int32_t kinetic_smash_amount(int32_t elapsed) {
  // Choreography over KINETIC_SMASH_DURATION_MS (1500ms). amount is a lerp factor
  // toward the center: 0 = dots spread out, 1000 = collapsed onto the block,
  // negative = pulled outward. Think spring-loaded: a quick yank to full tension,
  // a brief held strain, then a single fast SNAP releasing straight to the center
  // with no bounce -- it hits a hard stop and sticks, and the completion-fragment
  // burst at :00 carries the released energy outward.
  if (elapsed < 400) {                 // quick yank outward
    return kinetic_lerp_int16(0, -1200, (elapsed * 1000) / 400);
  } else if (elapsed < 700) {          // ease into full stretch
    return kinetic_lerp_int16(-1200, -1500, ((elapsed - 400) * 1000) / 300);
  } else if (elapsed < 1380) {         // held taut, straining
    return kinetic_lerp_int16(-1500, -1550, ((elapsed - 700) * 1000) / 680);
  } else if (elapsed < 1490) {         // SNAP: fast release straight to center
    return kinetic_lerp_int16(-1550, 1000, ((elapsed - 1380) * 1000) / 110);
  }

  return 1000;  // landed: stick on the block as the fragments burst at :00
}

static bool kinetic_draw_smashing_marker(GContext *ctx, GPoint marker_center, int marker_index) {
  if (s_time.tm_min % 5 != 4 || marker_index != s_time.tm_min / 5) {
    return false;
  }

  int32_t ms_until_minute;
  if (!kinetic_now(NULL, &ms_until_minute, NULL) || ms_until_minute > KINETIC_SMASH_DURATION_MS) {
    return false;
  }

  int32_t elapsed = KINETIC_SMASH_DURATION_MS - ms_until_minute;
  int32_t amount = kinetic_smash_amount(elapsed);
  for (int i = 0; i < DOT_COUNT; i++) {
    GPoint start = marker_dot_for_index(marker_center, ordered_dot_index(i), ACTIVE_MARKER_OFFSET);
    GPoint position = GPoint(
      kinetic_lerp_int16(start.x, marker_center.x, amount),
      kinetic_lerp_int16(start.y, marker_center.y, amount)
    );
    draw_marker_pixel(ctx, position, ACTIVE_MARKER_SIZE);
  }

  return true;
}

static void draw_marker_at_rest(GContext *ctx, GPoint center, int16_t radius, int marker_index) {
  int minute = s_time.tm_min;
  int completed_markers = minute / 5;
  int active_progress = minute % 5;
  GPoint marker_center = marker_center_for_index(center, radius, marker_index);

  if (marker_index < completed_markers) {
    fill_centered_rect(ctx, marker_center, COMPLETE_MARKER_SIZE);
    return;
  }

  bool is_active = marker_index == completed_markers && active_progress > 0;
  for (int i = 0; i < DOT_COUNT; i++) {
    bool is_active_pixel = is_active && i < active_progress;
    int16_t offset = is_active_pixel ? ACTIVE_MARKER_OFFSET : FUTURE_MARKER_OFFSET;
    GPoint dot = marker_dot_for_index(marker_center, ordered_dot_index(i), offset);
    draw_marker_pixel(ctx, dot, is_active_pixel ? ACTIVE_MARKER_SIZE : FUTURE_MARKER_SIZE);
  }
}

static void draw_marker(GContext *ctx, GPoint center, int16_t radius, int marker_index) {
  GPoint marker_center = marker_center_for_index(center, radius, marker_index);
  if (s_settings.kinetic_enabled &&
      kinetic_draw_smashing_marker(ctx, marker_center, marker_index)) {
    return;
  }

  draw_marker_at_rest(ctx, center, radius, marker_index);
}

static void draw_intro_marker_piece(GContext *ctx, GPoint origin, GPoint target,
                                    int16_t size, int32_t progress) {
  int32_t eased = ease_out_progress(progress);
  GPoint position = GPoint(
    kinetic_lerp_int16(origin.x, target.x, eased),
    kinetic_lerp_int16(origin.y, target.y, eased)
  );
  draw_marker_pixel(ctx, position, size);
}

// Signed radial offset for the whole ring once it has landed: 0 ->
// INTRO_PULSE_AMPLITUDE -> 0 over INTRO_PULSE_MS (a half-sine bump).
static int16_t intro_ring_pulse_delta(int32_t intro_elapsed) {
  int32_t t = intro_elapsed - INTRO_RING_LANDED_MS - INTRO_PULSE_DELAY_MS;
  if (t <= 0 || t >= INTRO_PULSE_MS) {
    return 0;
  }
  int32_t angle = (t * (TRIG_MAX_ANGLE / 2)) / INTRO_PULSE_MS;
  return (int16_t)((INTRO_PULSE_AMPLITUDE * sin_lookup(angle)) / TRIG_MAX_RATIO);
}

static void draw_intro_marker(GContext *ctx, GPoint center, int16_t radius,
                              int marker_index, int32_t intro_elapsed) {
  int32_t marker_elapsed = intro_elapsed - INTRO_RING_START_MS -
                           marker_index * INTRO_MARKER_STAGGER_MS;
  if (marker_elapsed <= 0) {
    return;
  }
  if (marker_elapsed >= INTRO_MARKER_DURATION_MS) {
    // Landed: apply the ring-wide pulse (0 until every marker is down).
    draw_marker_at_rest(ctx, center, radius + intro_ring_pulse_delta(intro_elapsed),
                        marker_index);
    return;
  }

  int32_t progress = clamp_progress(marker_elapsed, INTRO_MARKER_DURATION_MS);
  int minute = s_time.tm_min;
  int completed_markers = minute / 5;
  int active_progress = minute % 5;
  GPoint marker_center = marker_center_for_index(center, radius, marker_index);

  if (marker_index < completed_markers) {
    draw_intro_marker_piece(ctx, center, marker_center, COMPLETE_MARKER_SIZE, progress);
    return;
  }

  bool is_active = marker_index == completed_markers && active_progress > 0;
  for (int i = 0; i < DOT_COUNT; i++) {
    bool is_active_pixel = is_active && i < active_progress;
    int16_t offset = is_active_pixel ? ACTIVE_MARKER_OFFSET : FUTURE_MARKER_OFFSET;
    int16_t size = is_active_pixel ? ACTIVE_MARKER_SIZE : FUTURE_MARKER_SIZE;
    GPoint target = marker_dot_for_index(marker_center, ordered_dot_index(i), offset);
    draw_intro_marker_piece(ctx, center, target, size, progress);
  }
}

static bool kinetic_next_pixel_target(GPoint center, int16_t radius, GPoint *target) {
  int next_minute = (s_time.tm_min + 1) % 60;
  int next_active_progress = next_minute % 5;

  if (next_active_progress == 0) {
    return false;
  }

  int marker_index = next_minute / 5;
  int dot_index = ordered_dot_index(next_active_progress - 1);
  GPoint marker_center = marker_center_for_index(center, radius, marker_index);
  *target = marker_dot_for_index(marker_center, dot_index, ACTIVE_MARKER_OFFSET);
  return true;
}

static bool kinetic_incoming_pixel_position(GPoint center, int16_t radius, GPoint *position) {
  GPoint target;
  if (!kinetic_next_pixel_target(center, radius, &target)) {
    return false;
  }

  int32_t milliseconds_until_minute;
  if (!kinetic_now(NULL, &milliseconds_until_minute, NULL)) {
    return false;
  }

  int16_t start_y = -ACTIVE_MARKER_SIZE;
  int16_t travel_distance = target.y - start_y;
  int32_t fall_duration = ((int32_t)travel_distance * 1000) / KINETIC_FALL_PIXELS_PER_SECOND;

  if (fall_duration <= 0 || milliseconds_until_minute > fall_duration) {
    return false;
  }

  int32_t elapsed = fall_duration - milliseconds_until_minute;
  position->x = target.x;
  position->y = start_y + (travel_distance * elapsed) / fall_duration;
  return true;
}

static void draw_kinetic_incoming_pixel(GContext *ctx, GPoint center, int16_t radius) {
  GPoint position;
  if (kinetic_incoming_pixel_position(center, radius, &position)) {
    draw_marker_pixel(ctx, position, ACTIVE_MARKER_SIZE);
  }
}

static void draw_kinetic_completion_fragments(GContext *ctx, GPoint center, int16_t radius) {
  if (s_time.tm_min % 5 != 0) {
    return;
  }

  int32_t elapsed;
  if (!kinetic_now(NULL, NULL, &elapsed) || elapsed > KINETIC_FRAGMENT_DURATION_MS) {
    return;
  }

  int marker_index = (s_time.tm_min / 5 + MARKER_COUNT - 1) % MARKER_COUNT;
  GPoint origin = marker_center_for_index(center, radius, marker_index);
  int16_t fragment_size = ACTIVE_MARKER_SIZE / 3;
  if (fragment_size < 1) {
    fragment_size = 1;
  }

  const int16_t velocity_x[4] = { -70, 50, -35, 75 };
  const int16_t velocity_y[4] = { -95, -75, -120, -60 };
  const int16_t gravity = 240;

  for (int i = 0; i < 4; i++) {
    int16_t piece_size = fragment_size;
#if defined(PBL_PLATFORM_EMERY)
    if (s_settings.large_blocks) {
      // Mix two larger chips into the burst so it reads against the big blocks.
      piece_size += i == 0 ? 2 : i == 2 ? 1 : 0;
    }
#endif
    int32_t x = origin.x + (velocity_x[i] * elapsed) / 1000;
    int32_t y = origin.y + (velocity_y[i] * elapsed) / 1000 +
                (gravity * elapsed * elapsed) / 1000000;
    draw_marker_pixel(ctx, GPoint((int16_t)x, (int16_t)y), piece_size);
  }
}

static int32_t kinetic_top_hour_start_offset(int marker_index) {
  static const int16_t offsets[MARKER_COUNT] = {
    40, 430, 160, 650, 280, 520, 80, 360, 700, 220, 580, 120
  };
  return offsets[marker_index] % KINETIC_TOP_HOUR_STAGGER_MS;
}

static void draw_kinetic_top_hour_falling_blocks(GContext *ctx, GRect bounds,
                                                 GPoint center, int16_t radius) {
  if (s_time.tm_min != 0) {
    return;
  }

  int32_t elapsed;
  if (!kinetic_now(NULL, NULL, &elapsed) ||
      elapsed > KINETIC_TOP_HOUR_FALL_MS + KINETIC_TOP_HOUR_STAGGER_MS) {
    return;
  }

  int16_t end_y = bounds.origin.y + bounds.size.h + COMPLETE_MARKER_SIZE;
  for (int marker = 0; marker < MARKER_COUNT; marker++) {
    int32_t marker_elapsed = elapsed - kinetic_top_hour_start_offset(marker);
    if (marker_elapsed < 0) {
      marker_elapsed = 0;
    }
    if (marker_elapsed > KINETIC_TOP_HOUR_FALL_MS) {
      continue;
    }

    GPoint start = marker_center_for_index(center, radius, marker);
    int32_t amount = (marker_elapsed * 1000) / KINETIC_TOP_HOUR_FALL_MS;
    int16_t y = kinetic_lerp_int16(start.y, end_y, amount);
    fill_centered_rect(ctx, GPoint(start.x, y), COMPLETE_MARKER_SIZE);
  }
}

// Per-pixel start delay that sweeps the lit hour pixels in clockwise from 12
// o'clock: a pixel's angle around the center maps to how late it flies in.
static int32_t intro_hour_pixel_delay(GPoint target, GPoint center) {
  // atan2_lookup(sin-axis, cos-axis) matches the sin/cos convention used
  // elsewhere (0 = up, increasing clockwise). If the sweep runs the wrong way,
  // flip with (TRIG_MAX_ANGLE - angle).
  int32_t angle = atan2_lookup((int16_t)(target.x - center.x),
                               (int16_t)(center.y - target.y)) & 0xFFFF;
  return (angle * INTRO_HOUR_SWEEP_MS) / TRIG_MAX_ANGLE;
}

static void draw_intro_hour_pixel(GContext *ctx, GPoint center, GPoint target,
                                  int16_t pixel, int32_t intro_elapsed) {
  int32_t pixel_elapsed = intro_elapsed - intro_hour_pixel_delay(target, center);
  if (pixel_elapsed <= 0) {
    return;
  }
  if (pixel_elapsed >= INTRO_HOUR_PIECE_MS) {
    fill_centered_rect(ctx, target, pixel);
    return;
  }

  // Fly out from the center to the target while growing to full size.
  int32_t eased = ease_out_progress(clamp_progress(pixel_elapsed, INTRO_HOUR_PIECE_MS));
  GPoint position = GPoint(
    kinetic_lerp_int16(center.x, target.x, eased),
    kinetic_lerp_int16(center.y, target.y, eased)
  );
  int16_t size = (pixel * eased) / 1000;
  if (size < 1) {
    size = 1;
  }
  fill_centered_rect(ctx, position, size);
}

static HourGeometry hour_geometry(GRect bounds, HourMode mode) {
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
  bool draw_tens = hour_draw_tens(hour, is_24h, s_settings.hour_leading_zero);

  const int16_t pixel = mode == HourModeOverlay ? HOUR_OVERLAY_PIXEL_SIZE :
                        mode == HourModeCompact ? HOUR_COMPACT_PIXEL_SIZE :
                        HOUR_PIXEL_SIZE;
  const int16_t gap = 2;
  const int16_t digit_width = PIXEL_COLS * pixel + (PIXEL_COLS - 1) * gap;
  const int16_t digit_height = PIXEL_ROWS * pixel + (PIXEL_ROWS - 1) * gap;
  const int16_t digit_gap = pixel;
  const int16_t total_width = draw_tens ? digit_width * 2 + digit_gap : digit_width;
  int16_t x = bounds.origin.x + (bounds.size.w - total_width) / 2;
#if defined(PBL_PLATFORM_EMERY)
  if (s_settings.large_blocks) {
    x += hour_large_centering_offset(hour, draw_tens, pixel, gap);
  }
#endif
  // Compact mode makes room for complications. chalk nudges the
  // digits up to fit two lines below; gabbro's taller screen keeps them centered
  // so complications can sit both above and below. Inlined into the round branch
  // so it isn't an unused variable on rectangular builds.
  int16_t y = bounds.origin.y + (bounds.size.h - digit_height) / 2 +
              PBL_IF_ROUND_ELSE(mode == HourModeCompact ? (bounds.size.h >= 220 ? 0 : -20)
                                                        : HOUR_Y_OFFSET,
                                HOUR_Y_OFFSET);

  return (HourGeometry) {tens, ones, draw_tens, pixel, x, y, digit_width, digit_gap};
}

typedef struct {
  GContext *ctx;
  GPoint center;
  int32_t elapsed;
} IntroHourContext;

static bool draw_hour_pixel(ComplicationRect pixel, void *context) {
  const IntroHourContext *intro = context;
  draw_intro_hour_pixel(intro->ctx, intro->center,
                        GPoint(pixel.x + pixel.w / 2, pixel.y + pixel.h / 2), pixel.w, intro->elapsed);
  return false;
}

static void draw_pixel_hour_with_intro(GContext *ctx, GRect bounds, HourMode mode,
                                       int32_t intro_elapsed) {
  const HourGeometry hour = hour_geometry(bounds, mode);
  IntroHourContext intro = {.ctx = ctx, .center = grect_center_point(&bounds), .elapsed = intro_elapsed};
  hour_visit_pixels(&hour, draw_hour_pixel, &intro);
}

static void draw_pixel_hour(GContext *ctx, GRect bounds, HourMode mode) {
  draw_pixel_hour_with_intro(ctx, bounds, mode, INTRO_HOUR_DURATION_MS);
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

static GRect corner_rect(GRect bounds, bool right, bool bottom, int16_t width, int16_t height,
                         bool tight) {
  int16_t horizontal_margin = tight ? 0 : 4;
  int16_t top_offset = -4;
  int16_t bottom_margin = tight ? 0 : 2;
  int16_t x = bounds.origin.x + (right ? bounds.size.w - width - horizontal_margin :
                                          horizontal_margin);
  int16_t y = bounds.origin.y + (bottom ? bounds.size.h - height - bottom_margin : top_offset);
  return GRect(x, y, width, height);
}

static GRect complication_slot_rect(GRect bounds, int slot, int16_t width, int16_t height,
                                    bool tight) {
  bool right = slot == ComplicationSlotTopRight || slot == ComplicationSlotBottomRight;
  bool bottom = slot == ComplicationSlotBottomRight || slot == ComplicationSlotBottomLeft;
  return corner_rect(bounds, right, bottom, width, height, tight);
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

static void draw_steps_icon(GContext *ctx, GRect rect) {
  int16_t y = rect.origin.y + (rect.size.h - steps_icon_height()) / 2;
  ComplicationRect pieces[4];
  complication_steps_rects(rect.origin.x, y,
                           s_settings.complication_size == ComplicationSizeMedium, pieces);
  for (int i = 0; i < 4; i++) {
    graphics_fill_rect(ctx, GRect(pieces[i].x, pieces[i].y, pieces[i].w, pieces[i].h),
                       0, GCornerNone);
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

static bool steps_complication_enabled(void) {
  for (int slot = 0; slot < COMPLICATION_SLOT_COUNT; slot++) {
    if (s_settings.complications[slot] == ComplicationSteps) {
      return true;
    }
  }
  return false;
}

// Querying Health is comparatively expensive, so cache the step count and only
// refresh it once a minute (and only when a steps complication is configured)
// rather than re-querying on every redraw / animation frame.
static void refresh_steps(void) {
  s_steps = steps_complication_enabled() ? steps_today() : -1;
}

static void prepare_complication_values(void) {
  const ComplicationInputs inputs = {
    .date = &s_time,
    .now = time(NULL),
    .weather_enabled = s_settings.weather_enabled,
    .weather_available = s_settings.weather_available,
    .weather_updated_at = s_settings.weather_updated_at,
    .celsius = s_settings.weather_units == WeatherUnitsC,
    .temperature = s_settings.weather_temp,
    .low = s_settings.weather_low,
    .high = s_settings.weather_high,
    .battery_percent = s_battery_state.charge_percent,
    .bluetooth_connected = s_bluetooth_connected,
    .steps = s_steps,
  };
  for (int slot = 0; slot < COMPLICATION_SLOT_COUNT; slot++) {
    complication_builtin_value(s_settings.complications[slot], &inputs,
                               &s_complication_values[slot]);
  }
}

static int16_t complication_value_width(const ComplicationValue *value) {
  return complication_text_width(value->widths[0], value->widths[1], value->widths[2]);
}

static bool complications_visible(void) {
  return s_settings.complication_visibility == ComplicationVisibilityAlways || s_complications_revealed;
}

typedef struct {
  char label[16];
  GRect rect;
} RoundComplication;

static RoundComplication s_round_complications[COMPLICATION_SLOT_COUNT];
static int s_round_complication_count;

static void prepare_round_complications(GRect bounds) {
  s_round_complication_count = 0;
  if (!PBL_IF_ROUND_ELSE(true, false) || !complications_visible()) {
    return;
  }
  int16_t mid = bounds.origin.y + bounds.size.h / 2;
  if (bounds.size.h >= 220) {
    const int groups[2][2] = {
      {ComplicationSlotTopLeft, ComplicationSlotTopRight},
      {ComplicationSlotBottomLeft, ComplicationSlotBottomRight},
    };
    for (int group = 0; group < 2; group++) {
      int first = s_round_complication_count;
      for (int i = 0; i < 2; i++) {
        RoundComplication *row = &s_round_complications[s_round_complication_count];
        if (complication_value_label(&s_complication_values[groups[group][i]],
                                     row->label, sizeof(row->label))) {
          s_round_complication_count++;
        }
      }
      int count = s_round_complication_count - first;
      for (int i = 0; i < count; i++) {
        RoundComplication *row = &s_round_complications[first + i];
        GSize size = text_size(row->label, s_visitor_font_15, GRect(0, 0, 120, 20));
        int16_t y = group == 0 ? mid - 30 - 22 * (count - i) : mid + 30 + 22 * i;
        row->rect = GRect(bounds.origin.x + (bounds.size.w - size.w) / 2, y, size.w, 20);
      }
    }
  } else {
    for (int slot = 0; slot < COMPLICATION_SLOT_COUNT && s_round_complication_count < 2; slot++) {
      RoundComplication *row = &s_round_complications[s_round_complication_count];
      if (complication_value_label(&s_complication_values[slot], row->label, sizeof(row->label))) {
        s_round_complication_count++;
      }
    }
    for (int i = 0; i < s_round_complication_count; i++) {
      RoundComplication *row = &s_round_complications[i];
      GSize size = text_size(row->label, s_visitor_font_15, GRect(0, 0, 80, 20));
      int16_t y = mid + (s_round_complication_count > 1 ? 12 : 18) + 14 * i;
      row->rect = GRect(bounds.origin.x + (bounds.size.w - size.w) / 2, y, size.w, 18);
    }
  }
}

// Visitor's generated glyphs leave blank rows above the ink. Exclude those
// rows so a short temperature isn't treated as touching a nearby lower block.
static GRect complication_text_ink_bounds(GRect box) {
#if USE_VISITOR_COMPLICATION_FONT
  int16_t top = PBL_IF_ROUND_ELSE(true, false) ? 7 :
                s_settings.complication_size == ComplicationSizeLarge ? 12 :
                s_settings.complication_size == ComplicationSizeMedium ? 7 : 10;
  box.origin.y += top;
  box.size.h = box.size.h > top ? box.size.h - top : 0;
#endif
  return box;
}

static ComplicationRect collision_rect(GRect rect) {
  return (ComplicationRect) {rect.origin.x, rect.origin.y, rect.size.w, rect.size.h};
}

static bool hour_pixel_hits_region(ComplicationRect pixel, void *context) {
  const ComplicationRect *region = context;
  return complication_rects_overlap(*region, pixel, COMPLICATION_LAYOUT_CLEARANCE);
}

static bool region_hits_hour(GRect region, GRect bounds, HourMode mode) {
  const HourGeometry hour = hour_geometry(bounds, mode);
  ComplicationRect text = collision_rect(region);
  return hour_visit_pixels(&hour, hour_pixel_hits_region, &text);
}

static int16_t layout_ring_radius(GRect bounds, bool tight) {
  int16_t inset = complication_ring_inset(BASE_RING_INSET, s_layout_shrink,
                                          PBL_IF_ROUND_ELSE(true, false));
  int16_t radius = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 2 - inset;
  if (tight && PBL_IF_ROUND_ELSE(false, complications_visible())) {
    radius -= 3;
  }
  return radius > 0 ? radius : 1;
}

static HourMode layout_hour_mode(bool tight) {
  return tight ? HourModeOverlay :
         (s_hour_shrink > 0 || PBL_IF_ROUND_ELSE(complications_visible(), false)) ?
         HourModeCompact : HourModeNormal;
}

static ComplicationPoint s_ring_centers[MARKER_COUNT];
static ComplicationMarkers s_ring_markers;

static bool region_hits_ring(GRect region) {
  return complication_rect_hits_ring(collision_rect(region), s_ring_centers, MARKER_COUNT,
                                     s_ring_markers);
}

static bool region_hits_layout(GRect region, GRect bounds, bool tight, bool check_hour) {
  return check_hour ? region_hits_hour(region, bounds, layout_hour_mode(tight)) :
                      region_hits_ring(region);
}

typedef struct {
  GRect bounds;
  bool tight;
} LayoutProbeContext;

static bool hour_pixel_hits_ring(ComplicationRect pixel, void *context) {
  (void)context;
  return region_hits_ring(GRect(pixel.x, pixel.y, pixel.w, pixel.h));
}

static bool hour_hits_ring(GRect bounds, bool tight) {
  const HourGeometry hour = hour_geometry(bounds, layout_hour_mode(tight));
  return hour_visit_pixels(&hour, hour_pixel_hits_ring, NULL);
}

// At most two ink regions per slot (stacked date or steps icon + number).
static GRect s_complication_regions[COMPLICATION_SLOT_COUNT * 2];
static int s_complication_region_count;

static void add_complication_region(GRect region) {
  if (region.size.w > 0 && region.size.h > 0 &&
      s_complication_region_count < COMPLICATION_SLOT_COUNT * 2) {
    s_complication_regions[s_complication_region_count++] = region;
  }
}

// Text positions do not depend on ring/hour shrink levels. Measure them once
// per changed layout, then only test integer geometry in the fit loops.
static void measure_complication_regions(GRect bounds, bool tight) {
  s_complication_region_count = 0;
  if (PBL_IF_ROUND_ELSE(true, false)) {
    for (int i = 0; i < s_round_complication_count; i++) {
      add_complication_region(complication_text_ink_bounds(s_round_complications[i].rect));
    }
    return;
  }
  GFont font = complication_label_font();
  for (int slot = 0; slot < COMPLICATION_SLOT_COUNT; slot++) {
    const ComplicationValue *value = &s_complication_values[slot];
    if (value->display == ComplicationDisplayNone) continue;
    bool right = slot == ComplicationSlotTopRight || slot == ComplicationSlotBottomRight;
    int16_t height = value->display == ComplicationDisplayStacked ?
                     complication_box_height() : complication_row_height();
    GRect rect = complication_slot_rect(bounds, slot, complication_value_width(value), height, tight);
    if (value->display == ComplicationDisplayStacked) {
      GSize top = text_size(value->text, font, rect);
      GSize bottom = text_size(value->detail, font, rect);
      int16_t y = rect.origin.y + (rect.size.h - top.h - bottom.h) / 2;
      add_complication_region(complication_text_ink_bounds(
          GRect(right ? rect.origin.x + rect.size.w - top.w : rect.origin.x, y, top.w, top.h)));
      add_complication_region(complication_text_ink_bounds(
          GRect(right ? rect.origin.x + rect.size.w - bottom.w : rect.origin.x,
                y + top.h, bottom.w, bottom.h)));
      continue;
    }
    GSize size = text_size(value->text, font, rect);
    if (value->suffix[0] != '\0') {
      GSize unit = text_size(value->suffix, font, rect);
      size.w += 1 + unit.w;
      if (unit.h > size.h) size.h = unit.h;
    }
    if (value->display == ComplicationDisplaySteps) {
      int16_t width = steps_icon_width() + 3 + size.w;
      int16_t x = right ? rect.origin.x + rect.size.w - width : rect.origin.x;
      int16_t y = rect.origin.y + (rect.size.h - steps_icon_height()) / 2;
      ComplicationRect ink = complication_steps_ink(x, y,
                               s_settings.complication_size == ComplicationSizeMedium);
      add_complication_region(GRect(ink.x, ink.y, ink.w, ink.h));
      add_complication_region(complication_text_ink_bounds(
          GRect(x + steps_icon_width() + 3, rect.origin.y + (rect.size.h - size.h) / 2,
                size.w, size.h)));
      continue;
    }
    int16_t x = right ? rect.origin.x + rect.size.w - size.w : rect.origin.x;
    int16_t y = value->display == ComplicationDisplayText ?
                rect.origin.y : rect.origin.y + (rect.size.h - size.h) / 2;
    add_complication_region(complication_text_ink_bounds(GRect(x, y, size.w, size.h)));
  }
}

static bool layout_needs_compaction(GRect bounds, bool tight, bool check_hour) {
  for (int i = 0; i < s_complication_region_count; i++) {
    if (region_hits_layout(s_complication_regions[i], bounds, tight, check_hour)) return true;
  }
  return false;
}

static void update_marker_scale(GRect bounds, bool tight) {
  // A banner is itself a reduction of the ring, even at fit level zero.
  // Compare with the full canvas, not an already-compressed content rectangle.
  int16_t base_radius = (s_layout_canvas_bounds.size.w < s_layout_canvas_bounds.size.h ?
                         s_layout_canvas_bounds.size.w : s_layout_canvas_bounds.size.h) / 2 -
                        BASE_RING_INSET;
  s_marker_scale = complication_marker_scale(base_radius, layout_ring_radius(bounds, tight),
                                             s_layout_shrink);
  GPoint center = grect_center_point(&bounds);
  int16_t radius = layout_ring_radius(bounds, tight);
  for (int i = 0; i < MARKER_COUNT; i++) {
    GPoint point = marker_center_for_index(center, radius, i);
    s_ring_centers[i] = (ComplicationPoint){point.x, point.y};
  }
  s_ring_markers = (ComplicationMarkers){COMPLETE_MARKER_SIZE, ACTIVE_MARKER_SIZE,
                                        FUTURE_MARKER_SIZE, ACTIVE_MARKER_OFFSET,
                                        FUTURE_MARKER_OFFSET};
}

static bool probe_complication_fit(int shrink, bool check_hour, void *context) {
  const LayoutProbeContext *probe = context;
  if (check_hour) {
    s_hour_shrink = shrink;
    return layout_needs_compaction(probe->bounds, probe->tight, true) ||
           hour_hits_ring(probe->bounds, probe->tight);
  }
  s_layout_shrink = shrink;
  update_marker_scale(probe->bounds, probe->tight);
  return layout_needs_compaction(probe->bounds, probe->tight, false);
}

static void select_complication_layout(GRect canvas, GRect bounds, bool tight) {
  // Animation frames and seconds ticks usually have identical geometry. Avoid
  // repeating font measurements and the bounded fit search on those frames.
  static bool valid;
  static GRect previous_bounds, previous_canvas;
  static bool previous_tight, previous_large, previous_zero, previous_24h;
  static int previous_hour, previous_size, previous_time_mode;
  static ComplicationValue previous_values[COMPLICATION_SLOT_COUNT];

  s_layout_canvas_bounds = canvas;
  if (!complications_visible()) {
    s_layout_shrink = 0;
    s_hour_shrink = 0;
    update_marker_scale(bounds, tight);
    s_round_complication_count = 0;
    valid = false;
    return;
  }
  bool is_24h = clock_is_24h_style();
  if (valid && grect_equal(&bounds, &previous_bounds) && grect_equal(&canvas, &previous_canvas) &&
      tight == previous_tight &&
      s_settings.large_blocks == previous_large && s_settings.hour_leading_zero == previous_zero &&
      is_24h == previous_24h && s_time.tm_hour == previous_hour &&
      s_settings.complication_size == previous_size && s_settings.time_mode == previous_time_mode &&
      complication_values_equal(s_complication_values, previous_values, COMPLICATION_SLOT_COUNT)) {
    return;
  }
  prepare_round_complications(bounds);
  measure_complication_regions(bounds, tight);
  // Start from the original sizes for every changed input, so hiding the banner
  // or replacing a long value restores the largest layout that fits.
  s_layout_shrink = 0;
  s_hour_shrink = 0;
  LayoutProbeContext probe = {.bounds = bounds, .tight = tight};
  ComplicationFit fit = complication_select_fit(probe_complication_fit, &probe);
  s_layout_shrink = fit.ring;
  s_hour_shrink = fit.hour;
  previous_canvas = canvas;
  previous_bounds = bounds;
  previous_tight = tight;
  previous_large = s_settings.large_blocks;
  previous_zero = s_settings.hour_leading_zero;
  previous_24h = is_24h;
  previous_hour = s_time.tm_hour;
  previous_size = s_settings.complication_size;
  previous_time_mode = s_settings.time_mode;
  memcpy(previous_values, s_complication_values, sizeof(previous_values));
  valid = true;
}

static bool seconds_visible(void) {
  return s_settings.seconds_visibility == SecondsVisibilityAlways ||
         (s_settings.seconds_visibility == SecondsVisibilityOnTap && s_complications_revealed);
}

static void draw_seconds_overlay(GContext *ctx, GPoint center, int16_t radius) {
  if (!seconds_visible()) {
    return;
  }

  int second = s_time.tm_sec == 0 ? 60 : s_time.tm_sec;
  int marker_index = (second - 1) / 5;
  int step = (second - 1) % 5;
  GPoint marker_center = marker_center_for_index(center, radius, marker_index);
  int completed_markers = s_time.tm_min / 5;
  bool is_completed = marker_index < completed_markers;

  if (step < DOT_COUNT) {
    graphics_context_set_fill_color(ctx, GColorFromHEX(s_settings.background_color));
    draw_marker_pixel(ctx, marker_dot_for_index(marker_center, ordered_dot_index(step),
                                                FUTURE_MARKER_OFFSET),
                      FUTURE_MARKER_SIZE);
    return;
  }

  graphics_context_set_fill_color(ctx, GColorFromHEX(is_completed ? s_settings.background_color :
                                                     s_settings.ring_color));
  draw_marker_pixel(ctx, marker_center, FUTURE_MARKER_SIZE);
}

static void draw_complication(GContext *ctx, GRect bounds, int slot,
                              const ComplicationValue *value, bool tight) {
  int16_t box_height = value->display == ComplicationDisplayStacked ?
                       complication_box_height() : complication_row_height();
  int16_t width = complication_value_width(value);
  if (value->display == ComplicationDisplayNone || width == 0) {
    return;
  }
  GRect rect = complication_slot_rect(bounds, slot, width, box_height, tight);
  GTextAlignment alignment = (slot == ComplicationSlotTopRight ||
                              slot == ComplicationSlotBottomRight) ?
                             GTextAlignmentRight : GTextAlignmentLeft;

  switch (value->display) {
    case ComplicationDisplayStacked:
      draw_stacked_text(ctx, value->text, value->detail, rect, alignment);
      break;
    case ComplicationDisplayNumber:
      draw_aligned_number_with_suffix(ctx, value->text, value->suffix, rect, alignment);
      break;
    case ComplicationDisplaySteps:
      draw_aligned_steps(ctx, value->text, rect, alignment);
      break;
    case ComplicationDisplayText:
      draw_aligned_text(ctx, value->text, rect, alignment);
      break;
    case ComplicationDisplayNone:
      break;
  }
}

static void draw_center_complications(GContext *ctx, GRect bounds, bool tight) {
  if (PBL_IF_ROUND_ELSE(true, false)) {
    return;
  }

  if (!complications_visible()) {
    return;
  }

  graphics_context_set_text_color(ctx, GColorFromHEX(s_settings.complication_color));

  for (int slot = 0; slot < COMPLICATION_SLOT_COUNT; slot++) {
    draw_complication(ctx, bounds, slot, &s_complication_values[slot], tight);
  }
}

static void draw_round_center_complications(GContext *ctx, GRect bounds) {
  (void)bounds;
  if (!PBL_IF_ROUND_ELSE(true, false) || !complications_visible()) {
    return;
  }
  graphics_context_set_text_color(ctx, GColorFromHEX(s_settings.complication_color));
  for (int i = 0; i < s_round_complication_count; i++) {
    RoundComplication *row = &s_round_complications[i];
    graphics_draw_text(ctx, row->label, s_visitor_font_15, row->rect,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  refresh_time();
  if (complications_visible()) {
    prepare_complication_values();
  }

  GRect bounds = layer_get_bounds(layer);
  GRect content_bounds = layer_get_unobstructed_bounds(layer);
  bool is_obstructed = !grect_equal(&bounds, &content_bounds);
  select_complication_layout(bounds, content_bounds, is_obstructed);
  HourMode hour_mode = layout_hour_mode(is_obstructed);
  GPoint center = grect_center_point(&content_bounds);
  int16_t radius = layout_ring_radius(content_bounds, is_obstructed);

  graphics_context_set_fill_color(ctx, GColorFromHEX(s_settings.background_color));
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  int32_t intro_elapsed = 0;
  bool intro_active = intro_elapsed_ms(&intro_elapsed);
  if (intro_active && intro_elapsed >= INTRO_DURATION_MS) {
    s_intro_active = false;
    intro_active = false;
  }

  if (intro_active) {
    // Hold blank for INTRO_LEAD_MS so the launch/display lag doesn't eat the
    // start of the sweep; the hour->ring choreography plays after the lead.
    int32_t anim_elapsed = intro_elapsed - INTRO_LEAD_MS;
    graphics_context_set_fill_color(ctx, GColorFromHEX(s_settings.hour_color));
    draw_pixel_hour_with_intro(ctx, content_bounds, hour_mode, anim_elapsed);

    graphics_context_set_fill_color(ctx, GColorFromHEX(s_settings.ring_color));
    for (int i = 0; i < MARKER_COUNT; i++) {
      draw_intro_marker(ctx, center, radius, i, anim_elapsed);
    }
    intro_update_frame_timer();
    return;
  }

  graphics_context_set_fill_color(ctx, GColorFromHEX(s_settings.ring_color));
  for (int i = 0; i < MARKER_COUNT; i++) {
    draw_marker(ctx, center, radius, i);
  }
  draw_seconds_overlay(ctx, center, radius);

  graphics_context_set_fill_color(ctx, GColorFromHEX(s_settings.hour_color));
  draw_pixel_hour(ctx, content_bounds, hour_mode);

  if (s_settings.kinetic_enabled) {
    // Draw the kinetic blocks on top of the hour digits: a block falling to a
    // center-column marker travels straight through the digits, so drawing it
    // underneath would hide it behind the "10". The smash and fragments sit at
    // the ring edges and are unaffected by the order. Reset the fill to the ring
    // color first (the hour draw left it on the hour color).
    graphics_context_set_fill_color(ctx, GColorFromHEX(s_settings.ring_color));
    draw_kinetic_incoming_pixel(ctx, center, radius);
    draw_kinetic_completion_fragments(ctx, center, radius);
    draw_kinetic_top_hour_falling_blocks(ctx, content_bounds, center, radius);
  }

  draw_round_center_complications(ctx, content_bounds);
  draw_center_complications(ctx, content_bounds, is_obstructed);
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
    .hour_leading_zero = true,
    .complication_size = ComplicationSizeNormal,
    .weather_enabled = true,
    .weather_units = WeatherUnitsF,
    .weather_temp = 0,
    .weather_high = 0,
    .weather_low = 0,
    .weather_available = false,
    .complication_visibility = ComplicationVisibilityOnTap,
    .seconds_visibility = SecondsVisibilityOnTap,
    .kinetic_enabled = false,
    .large_blocks = false,
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
  s_settings.hour_leading_zero = !persist_exists(PersistKeyHourLeadingZero) ||
                                 persist_read_bool(PersistKeyHourLeadingZero);
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
  s_settings.weather_updated_at = persist_exists(PersistKeyWeatherUpdatedAt) ?
                                 persist_read_int(PersistKeyWeatherUpdatedAt) : 0;
  s_settings.complication_visibility = persist_exists(PersistKeyComplicationVisibility) ?
                                       persist_read_int(PersistKeyComplicationVisibility) :
                                       ComplicationVisibilityOnTap;
  if (s_settings.complication_visibility < ComplicationVisibilityAlways ||
      s_settings.complication_visibility > ComplicationVisibilityOnTap) {
    s_settings.complication_visibility = ComplicationVisibilityOnTap;
  }
  s_settings.seconds_visibility = persist_exists(PersistKeySecondsVisibility) ?
                                  persist_read_int(PersistKeySecondsVisibility) :
                                  SecondsVisibilityOnTap;
  if (s_settings.seconds_visibility < SecondsVisibilityNever ||
      s_settings.seconds_visibility > SecondsVisibilityOnTap) {
    s_settings.seconds_visibility = SecondsVisibilityNever;
  }
  s_settings.kinetic_enabled = persist_exists(PersistKeyKineticEnabled) &&
                               persist_read_bool(PersistKeyKineticEnabled);

  s_settings.large_blocks = persist_exists(PersistKeyLargeBlocks) &&
                            persist_read_bool(PersistKeyLargeBlocks);

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
  persist_write_bool(PersistKeyHourLeadingZero, s_settings.hour_leading_zero);
  persist_write_int(PersistKeyComplicationSize, s_settings.complication_size);
  persist_write_bool(PersistKeyWeatherEnabled, s_settings.weather_enabled);
  persist_write_int(PersistKeyWeatherUnits, s_settings.weather_units);
  persist_write_int(PersistKeyWeatherTemp, s_settings.weather_temp);
  persist_write_int(PersistKeyWeatherHigh, s_settings.weather_high);
  persist_write_int(PersistKeyWeatherLow, s_settings.weather_low);
  persist_write_bool(PersistKeyWeatherAvailable, s_settings.weather_available);
  persist_write_int(PersistKeyWeatherUpdatedAt, (int32_t)s_settings.weather_updated_at);
  persist_write_int(PersistKeyComplicationVisibility, s_settings.complication_visibility);
  persist_write_int(PersistKeyComplicationTopLeft, s_settings.complications[ComplicationSlotTopLeft]);
  persist_write_int(PersistKeyComplicationTopRight, s_settings.complications[ComplicationSlotTopRight]);
  persist_write_int(PersistKeyComplicationBottomRight, s_settings.complications[ComplicationSlotBottomRight]);
  persist_write_int(PersistKeyComplicationBottomLeft, s_settings.complications[ComplicationSlotBottomLeft]);
  persist_write_int(PersistKeySecondsVisibility, s_settings.seconds_visibility);
  persist_write_bool(PersistKeyKineticEnabled, s_settings.kinetic_enabled);
  persist_write_bool(PersistKeyLargeBlocks, s_settings.large_blocks);
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

  dict_write_uint8(iter, MESSAGE_KEY_WeatherRequest, 1);
  app_message_outbox_send();
}

static void request_weather_timer_handler(void *context) {
  (void)context;
  request_weather();
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *background = dict_find(iter, MESSAGE_KEY_BackgroundColor);
  Tuple *ring = dict_find(iter, MESSAGE_KEY_RingColor);
  Tuple *complication = dict_find(iter, MESSAGE_KEY_ComplicationColor);
  Tuple *hour = dict_find(iter, MESSAGE_KEY_HourColor);
  Tuple *time_mode = dict_find(iter, MESSAGE_KEY_TimeMode);
  Tuple *hour_leading_zero = dict_find(iter, MESSAGE_KEY_HourLeadingZero);
  Tuple *complication_size = dict_find(iter, MESSAGE_KEY_ComplicationSize);
  Tuple *weather_enabled = dict_find(iter, MESSAGE_KEY_WeatherEnabled);
  Tuple *weather_units = dict_find(iter, MESSAGE_KEY_WeatherUnits);
  Tuple *weather_temp = dict_find(iter, MESSAGE_KEY_WeatherTemp);
  Tuple *weather_available = dict_find(iter, MESSAGE_KEY_WeatherAvailable);
  Tuple *weather_high = dict_find(iter, MESSAGE_KEY_WeatherHigh);
  Tuple *weather_low = dict_find(iter, MESSAGE_KEY_WeatherLow);
  Tuple *complication_visibility = dict_find(iter, MESSAGE_KEY_ComplicationVisibility);
  Tuple *complication_top_left = dict_find(iter, MESSAGE_KEY_ComplicationTopLeft);
  Tuple *complication_top_right = dict_find(iter, MESSAGE_KEY_ComplicationTopRight);
  Tuple *complication_bottom_right = dict_find(iter, MESSAGE_KEY_ComplicationBottomRight);
  Tuple *complication_bottom_left = dict_find(iter, MESSAGE_KEY_ComplicationBottomLeft);
  Tuple *seconds_visibility = dict_find(iter, MESSAGE_KEY_SecondsVisibility);
  Tuple *kinetic_enabled = dict_find(iter, MESSAGE_KEY_KineticEnabled);
  Tuple *large_blocks = dict_find(iter, MESSAGE_KEY_LargeBlocks);

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
  if (hour_leading_zero) {
    s_settings.hour_leading_zero = hour_leading_zero->value->int32 != 0;
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
      if (units != s_settings.weather_units) {
        s_settings.weather_available = false;
      }
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
    // Only a complete successful response renews the cache. Settings changes
    // and partial/unavailable messages must never extend its lifetime.
    s_settings.weather_available = weather_available->value->int32 != 0 &&
                                   weather_temp && weather_high && weather_low;
    if (s_settings.weather_available) {
      s_settings.weather_updated_at = time(NULL);
    }
  }
  if (complication_visibility) {
    int visibility = (int)complication_visibility->value->int32;
    if (visibility >= ComplicationVisibilityAlways && visibility <= ComplicationVisibilityOnTap) {
      s_settings.complication_visibility = visibility;
    }
  }
  if (seconds_visibility) {
    int visibility = (int)seconds_visibility->value->int32;
    if (visibility >= SecondsVisibilityNever && visibility <= SecondsVisibilityOnTap) {
      s_settings.seconds_visibility = visibility;
    }
  }
  if (large_blocks) {
    s_settings.large_blocks = large_blocks->value->int32 != 0;
  }
  if (kinetic_enabled) {
    s_settings.kinetic_enabled = kinetic_enabled->value->int32 != 0;
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
  refresh_steps();
  update_backlight_subscription();
  update_tick_subscription();
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
  if ((units_changed & MINUTE_UNIT) || tick_time->tm_sec == 0 ||
      ((units_changed & SECOND_UNIT) && seconds_visible())) {
    layer_mark_dirty(s_canvas_layer);

    if ((units_changed & MINUTE_UNIT) || tick_time->tm_sec == 0) {
      refresh_steps();
      if (tick_time->tm_min % 30 == 0) {
        request_weather();
      }
    }
  }

  kinetic_update_frame_timer();
  kinetic_schedule_arm_timer();
}

static void battery_handler(BatteryChargeState state) {
  s_battery_state = state;
  layer_mark_dirty(s_canvas_layer);
}

static void bluetooth_handler(bool connected) {
  s_bluetooth_connected = connected;
  layer_mark_dirty(s_canvas_layer);
}

static void mark_canvas_dirty(void) {
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void app_did_focus_handler(bool in_focus) {
  if (in_focus) {
    // Recheck the banner's unobstructed bounds after the system menu's return
    // animation, so a transitional layout cannot linger until the next tick.
    mark_canvas_dirty();
  }
}

#ifdef _PBL_API_EXISTS_unobstructed_area_service_subscribe
static void unobstructed_will_change_handler(GRect final_unobstructed_screen_area, void *context) {
  (void)final_unobstructed_screen_area;
  (void)context;
  mark_canvas_dirty();
}

static void unobstructed_change_handler(AnimationProgress progress, void *context) {
  (void)progress;
  (void)context;
  mark_canvas_dirty();
}

static void unobstructed_did_change_handler(void *context) {
  (void)context;
  mark_canvas_dirty();
}
#endif

static void complication_reveal_timer_handler(void *context) {
  s_complication_reveal_timer = NULL;
  s_complications_revealed = false;
  update_tick_subscription();
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void reveal_details(void) {
  if (s_settings.complication_visibility != ComplicationVisibilityOnTap &&
      s_settings.seconds_visibility != SecondsVisibilityOnTap) {
    return;
  }

  s_complications_revealed = true;
  if (s_complication_reveal_timer) {
    app_timer_cancel(s_complication_reveal_timer);
  }
  s_complication_reveal_timer = app_timer_register(COMPLICATION_REVEAL_MS,
                                                   complication_reveal_timer_handler,
                                                   NULL);
  update_tick_subscription();
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  reveal_details();
}

#ifdef _PBL_API_EXISTS_backlight_service_subscribe
static void backlight_handler(bool on) {
  if (on) {
    reveal_details();
  }
}
#endif

static void update_backlight_subscription(void) {
#ifdef _PBL_API_EXISTS_backlight_service_subscribe
  if (s_settings.complication_visibility == ComplicationVisibilityOnTap ||
      s_settings.seconds_visibility == SecondsVisibilityOnTap) {
    backlight_service_subscribe(backlight_handler);
  } else {
    backlight_service_unsubscribe();
  }
#endif
}

static void update_tick_subscription(void) {
  tick_timer_service_subscribe(seconds_visible() ? SECOND_UNIT : MINUTE_UNIT, tick_handler);
  // Catch any window we are already inside, then arm a wake-up before the next one.
  // Both self-gate on kinetic_enabled (and cancel their timers when it is off).
  kinetic_update_frame_timer();
  kinetic_schedule_arm_timer();
}

// Frames are only needed in brief windows around the minute boundary: the falling
// pixel / smash in the run-up, and the completion fragments / top-of-hour cascade
// just after. Outside those the face sleeps at minute (or second, when the seconds
// sweep is on) granularity rather than waking 60x/min.
static bool kinetic_should_run_frame_timer(void) {
  if (!s_settings.kinetic_enabled) {
    return false;
  }

  int min;
  int32_t ms_until_minute;
  int32_t ms_after_minute;
  if (!kinetic_now(&min, &ms_until_minute, &ms_after_minute)) {
    return false;
  }

  // Run-up: smash or falling pixel before the boundary.
  if (ms_until_minute <= kinetic_run_up_ms(min)) {
    return true;
  }

  // Aftermath: fragment burst when a marker just completed, plus the longer cascade.
  if (min % 5 == 0 && ms_after_minute <= KINETIC_FRAGMENT_DURATION_MS) {
    return true;
  }
  if (min == 0 && ms_after_minute <= KINETIC_TOP_HOUR_FALL_MS + KINETIC_TOP_HOUR_STAGGER_MS) {
    return true;
  }

  return false;
}

static void kinetic_frame_timer_handler(void *context) {
  (void)context;
  s_kinetic_frame_timer = NULL;

  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }

  kinetic_update_frame_timer();
}

static void kinetic_update_frame_timer(void) {
  bool should_run = kinetic_should_run_frame_timer();

  if (should_run && !s_kinetic_frame_timer) {
    s_kinetic_frame_timer = app_timer_register(KINETIC_FRAME_MS, kinetic_frame_timer_handler, NULL);
  } else if (!should_run && s_kinetic_frame_timer) {
    app_timer_cancel(s_kinetic_frame_timer);
    s_kinetic_frame_timer = NULL;
  }
}

static void intro_frame_timer_handler(void *context) {
  (void)context;
  s_intro_frame_timer = NULL;

  int32_t elapsed;
  if (intro_elapsed_ms(&elapsed) && elapsed < INTRO_DURATION_MS) {
    if (s_canvas_layer) {
      layer_mark_dirty(s_canvas_layer);
    }
    intro_update_frame_timer();
    return;
  }

  s_intro_active = false;
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void intro_update_frame_timer(void) {
  int32_t elapsed;
  bool should_run = intro_elapsed_ms(&elapsed) && elapsed < INTRO_DURATION_MS;

  if (should_run && !s_intro_frame_timer) {
    s_intro_frame_timer = app_timer_register(INTRO_FRAME_MS, intro_frame_timer_handler, NULL);
  } else if (!should_run && s_intro_frame_timer) {
    app_timer_cancel(s_intro_frame_timer);
    s_intro_frame_timer = NULL;
  }
}

static void intro_start(void) {
  s_intro_active = true;
  s_intro_started = false;
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void kinetic_arm_timer_handler(void *context) {
  (void)context;
  s_kinetic_arm_timer = NULL;
  // Reached the run-up window: start the frame loop. It self-sustains across the
  // boundary (and through the aftermath), and the next minute tick reschedules us.
  kinetic_update_frame_timer();
}

// When ticking at minute granularity, nothing would wake the CPU between :00 and the
// pre-boundary run-up, so schedule a one-shot wake just before the next window starts.
static void kinetic_schedule_arm_timer(void) {
  if (s_kinetic_arm_timer) {
    app_timer_cancel(s_kinetic_arm_timer);
    s_kinetic_arm_timer = NULL;
  }

  // With per-second ticks the frame timer is armed straight from tick_handler.
  if (!s_settings.kinetic_enabled || seconds_visible()) {
    return;
  }

  int min;
  int32_t ms_until_minute;
  if (!kinetic_now(&min, &ms_until_minute, NULL)) {
    return;
  }

  int32_t delay = ms_until_minute - kinetic_run_up_ms(min);
  if (delay > 0) {
    s_kinetic_arm_timer = app_timer_register(delay, kinetic_arm_timer_handler, NULL);
  }
  // delay <= 0 means we are already inside the run-up; kinetic_update_frame_timer
  // (called alongside this) starts the loop and the minute tick will reschedule.
}

#if defined(PBL_TOUCH)
static void touch_handler(const TouchEvent *event, void *context) {
  if (event && event->type == TouchEvent_Touchdown) {
    reveal_details();
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
  s_canvas_layer = NULL;
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
  s_bluetooth_connected = connection_service_peek_pebble_app_connection();
  refresh_steps();

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  update_tick_subscription();
  battery_state_service_subscribe(battery_handler);
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bluetooth_handler,
  });
  accel_tap_service_subscribe(accel_tap_handler);
  app_focus_service_subscribe_handlers((AppFocusHandlers) {
    .did_focus = app_did_focus_handler,
  });
#ifdef _PBL_API_EXISTS_unobstructed_area_service_subscribe
  unobstructed_area_service_subscribe((UnobstructedAreaHandlers) {
    .will_change = unobstructed_will_change_handler,
    .change = unobstructed_change_handler,
    .did_change = unobstructed_did_change_handler,
  }, NULL);
#endif
#if defined(PBL_TOUCH)
  touch_service_subscribe(touch_handler, NULL);
#endif
  update_backlight_subscription();
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(256, 256);

  intro_start();
  window_stack_push(s_window, false);
  app_timer_register(1000, request_weather_timer_handler, NULL);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  connection_service_unsubscribe();
  accel_tap_service_unsubscribe();
  app_focus_service_unsubscribe();
#ifdef _PBL_API_EXISTS_unobstructed_area_service_unsubscribe
  unobstructed_area_service_unsubscribe();
#endif
#if defined(PBL_TOUCH)
  touch_service_unsubscribe();
#endif
  if (s_complication_reveal_timer) {
    app_timer_cancel(s_complication_reveal_timer);
    s_complication_reveal_timer = NULL;
  }
#ifdef _PBL_API_EXISTS_backlight_service_unsubscribe
  backlight_service_unsubscribe();
#endif
  if (s_kinetic_frame_timer) {
    app_timer_cancel(s_kinetic_frame_timer);
    s_kinetic_frame_timer = NULL;
  }
  if (s_kinetic_arm_timer) {
    app_timer_cancel(s_kinetic_arm_timer);
    s_kinetic_arm_timer = NULL;
  }
  s_intro_active = false;
  s_intro_started = false;
  if (s_intro_frame_timer) {
    app_timer_cancel(s_intro_frame_timer);
    s_intro_frame_timer = NULL;
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
