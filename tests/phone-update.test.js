const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { execFileSync } = require('node:child_process');

test('watch reports clock changes without waking quiet configurations and retries failed reports', () => {
  const source = fs.readFileSync('src/c/box_minutes.c', 'utf8');
  const start = source.indexOf('static int s_reported_clock_style');
  const end = source.indexOf('// Plugin packets', start);
  assert.ok(start >= 0 && end > start, 'clock-report C test boundaries must exist');
  const functions = source.slice(start, end);
  const inboxStart = source.indexOf('static void inbox_received_handler(');
  const inboxEnd = source.indexOf('  if (background) {', inboxStart);
  assert.ok(inboxStart >= 0 && inboxEnd > inboxStart, 'inbox early-return boundaries must exist');
  const inbox = source.slice(inboxStart, inboxEnd) + '(void)context; }';
  const keys = [...new Set(inbox.match(/MESSAGE_KEY_\w+/g))];
  const keyDefines = keys.map((key, i) => '#define ' + key + ' ' + (i + 10)).join('\n');
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'minute-blocks-phone-'));
  try {
    fs.writeFileSync(path.join(dir, 'test.c'), `
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
static time_t test_now = 1000;
static time_t test_time(time_t *out) { if (out) *out = test_now; return test_now; }
#define time test_time
typedef int DictionaryIterator;
typedef int AppMessageResult;
#define APP_MSG_OK 0
#define COMPLICATION_SLOT_COUNT 4
#define TimeModeWatch 0
#define MESSAGE_KEY_WeatherRequest 1
#define MESSAGE_KEY_Watch24Hour 2
static struct { bool weather_enabled; int time_mode; } s_settings;
static uint8_t s_plugin_favorites[4];
static int style, sends, weather_flag, clock_flag, send_result, begin_result;
static DictionaryIterator dictionary;
static bool clock_is_24h_style(void) { return style; }
static int app_message_outbox_begin(DictionaryIterator **iter) { *iter = &dictionary; return begin_result; }
static void dict_write_uint8(DictionaryIterator *iter, int key, int value) {
  (void)iter; if (key == 1) weather_flag = value; else clock_flag = value;
}
static int app_message_outbox_send(void) { sends++; return send_result; }
${functions}
${keyDefines}
typedef int Tuple;
static Tuple *dict_find(DictionaryIterator *iter, int key) { (void)iter; (void)key; return NULL; }
static bool receive_plugin_message(DictionaryIterator *iter) { (void)iter; s_plugin_favorites[0] = 6; return true; }
${inbox}
int main(void) {
  initial_phone_update(NULL); update_phone(true); assert(sends == 0);
  s_plugin_favorites[0] = 6; update_phone(false);
  assert(sends == 1 && weather_flag == 0 && clock_flag == 0);
  update_phone(false); assert(sends == 1); // pending report is coalesced
  phone_update_sent(NULL, NULL);
  update_phone(true); assert(sends == 1); // unchanged plugins do not poll
  style = 1; update_phone(false); assert(sends == 2 && clock_flag == 1);
  phone_update_failed(NULL, 1, NULL); update_phone(false); assert(sends == 3);
  phone_update_sent(NULL, NULL);
  s_settings.time_mode = 1; style = 0; update_phone(false); assert(sends == 3);
  s_settings.weather_enabled = true; update_phone(true); assert(sends == 4 && weather_flag == 1);
  phone_update_sent(NULL, NULL);
  s_settings.weather_enabled = false; s_settings.time_mode = 0;
  s_reported_clock_style = -1; // phone restart / selection packet
  begin_result = 1; update_phone(false); assert(sends == 4);
  begin_result = 0; send_result = 1; update_phone(false); assert(sends == 5);
  assert(s_pending_clock_style == -1);
  send_result = 0; update_phone(false); assert(sends == 6);
  phone_update_sent(NULL, NULL);
  s_plugin_favorites[0] = 0; style = 1; update_phone(true); assert(sends == 6);
  // Pure selection packets must report immediately, before the built-in guard.
  inbox_received_handler(&dictionary, NULL); assert(sends == 7 && clock_flag == 1);
  // Simulate the SDK accepting a send but never delivering either callback.
  s_settings.weather_enabled = true;
  test_now += 59; update_phone(true); assert(sends == 7);
  test_now++; update_phone(true); assert(sends == 8 && weather_flag == 1);
  phone_update_sent(NULL, NULL);
  update_phone(false); assert(sends == 8);
  return 0;
}
`);
    execFileSync('cc', ['-std=c99', '-Wall', '-Wextra', '-Werror', path.join(dir, 'test.c'), '-o', path.join(dir, 'test')]);
    execFileSync(path.join(dir, 'test'));
  } finally { fs.rmSync(dir, { recursive: true, force: true }); }
});
