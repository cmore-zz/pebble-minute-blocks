const test = require('node:test');
const assert = require('node:assert/strict');
const calendar = require('../src/pkjs/plugin-calendar');
const { textFor } = require('../src/pkjs/plugin-complications');
const stamp = hour => ({ timestamp: { value: new Date(2026, 10, 6, hour, 0).getTime() / 1000 }, shortText: { text: '08:00' } });

test('calendar timestamps override provider text and respect 12/24-hour style', () => {
  for (const [hour, twelve, twentyFour] of [[0, '12:00A', '00:00'], [8, '8:00A', '08:00'], [12, '12:00P', '12:00'], [20, '8:00P', '20:00']]) {
    for (const property of ['starts_at', 'ends_at']) {
      assert.equal(calendar.text(property, { [property]: stamp(hour) }, false, textFor), twelve);
      assert.equal(calendar.text(property, { [property]: stamp(hour) }, true, textFor), twentyFour);
    }
  }
});
test('all-day events remain all day and missing timestamps retain provider text', () => {
  assert.deepEqual(calendar.properties('starts_at'), ['starts_at', 'all_day']);
  assert.deepEqual(calendar.properties('ends_at'), ['ends_at', 'all_day']);
  assert.equal(calendar.text('starts_at', { starts_at: stamp(0), all_day: { boolean: { value: true } } }, false, textFor), 'All day');
  assert.equal(calendar.text('starts_at', { starts_at: { shortText: { text: 'Soon' } } }, false, textFor), 'Soon');
  assert.equal(calendar.text('starts_at', { starts_at: stamp(8) }, undefined, textFor), '');
  assert.equal(calendar.text('starts_at', {}, false, textFor), '');
});
test('calendar titles keep six characters and a continuation marker, without clipping numbers elsewhere', () => {
  assert.equal(calendar.text('name', { name: { shortText: { text: 'Meeting with Sam' } } }, false, textFor), 'Meetin>');
  assert.equal(calendar.text('name', { name: { longText: { text: 'Meeting' } } }, false, textFor), 'Meeting');
  assert.equal(calendar.text('name', { name: { shortText: { text: '  Team\nmeeting 🎉' } } }, false, textFor), 'Team m>');
  assert.equal(textFor({ numericValue: { value: 12345678 } }), '');
});
