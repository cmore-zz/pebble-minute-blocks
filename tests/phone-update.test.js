const test = require('node:test');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { execFileSync } = require('node:child_process');

test('watch reports clock changes without waking quiet configurations and retries failed reports', () => {
  const source = fs.readFileSync('src/c/box_minutes.c', 'utf8');
  const functions = source.slice(source.indexOf('static int s_reported_clock_style'), source.indexOf('// Plugin packets'));
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'minute-blocks-phone-'));
  try {
    fs.writeFileSync(path.join(dir, 'test.c'), `
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
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
  return 0;
}
`);
    execFileSync('cc', ['-std=c99', '-Wall', '-Wextra', '-Werror', path.join(dir, 'test.c'), '-o', path.join(dir, 'test')]);
    execFileSync(path.join(dir, 'test'));
  } finally { fs.rmSync(dir, { recursive: true, force: true }); }
});
