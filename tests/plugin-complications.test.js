const test = require('node:test');
const assert = require('node:assert/strict');
const support = require('../src/pkjs/plugin-complications');
const queue = require('../src/pkjs/watch-messages');

const now = 1800000000000;
const shortText = text => ({ shortText: { text } });
const preference = (property = 'temperature') => ({ properties: {
  category: shortText('weather'), item: shortText('location'), property: shortText(property),
  index: { numericValue: { value: 0 } }
} });
const data = property => ({ validUntilMs: now + 300000, instances: [{
  instanceId: 'home', properties: { temperature: property }
}] });
function fixture() {
  const subs = [], sent = [];
  const api = { subscribeToSource(config) {
    const sub = { ...config, stopped: false, unsubscribe() { this.stopped = true; } };
    subs.push(sub); return sub;
  } };
  return { subs, sent, manager: support.create(api, message => sent.push(message), () => now) };
}

test('normalizes text and finite numeric readings for the bounded ASCII display', () => {
  assert.equal(support.textFor(shortText('72°F')), '72F');
  assert.equal(support.textFor(shortText('−12 – 3')), '-12 - 3');
  assert.equal(support.textFor(shortText('123456789')), '');
  assert.equal(support.textFor({ numericValue: { value: 12.34, unit: '%' } }), '12.3%');
  for (const value of [null, {}, { numericValue: { value: null } },
    { numericValue: { value: NaN } }, { numericValue: { value: Infinity } }, { numericValue: { value: 1e308 } }, { numericValue: { value: 12345678 } }]) {
    assert.equal(support.textFor(value), '');
  }
});

test('starts no subscriptions unless a slot opts in', () => {
  const f = fixture(); f.manager.start({}); assert.equal(f.subs.length, 0);
});

test('routes favorite readings to slots and clears failures for built-in fallback', () => {
  const f = fixture();
  f.manager.start({ PluginFavoriteTopLeft: 1 });
  assert.equal(f.sent[0].PluginTTL, 0);
  f.subs[0].onData({ instances: [preference()] });
  f.subs[0].onData({ instances: [preference()] });
  assert.equal(f.subs.length, 2, 'unchanged preferences retain existing subscriptions');
  assert.equal(f.subs[1].category, 'weather');
  assert.deepEqual(f.subs[1].properties, ['temperature']);
  f.subs[1].onData(data(shortText('72°F')));
  assert.deepEqual(f.sent.at(-1), { PluginSlot: 0, PluginFavorite: 1, PluginText: '72F', PluginTTL: 300 });
  f.subs[1].onError({ code: 'PERMISSION_DENIED' });
  assert.equal(f.sent.at(-1).PluginTTL, 0);
  // Source errors don't end the subscription; a later grant can recover.
  f.subs[1].onData(data(shortText('73°F')));
  assert.equal(f.sent.at(-1).PluginText, '73F');
});

test('missing, malformed and expired readings fall back; lifetime is bounded', () => {
  const f = fixture(); f.manager.start({ PluginFavoriteTopLeft: 1 });
  f.subs[0].onData({ instances: [preference()] });
  const sub = f.subs[1];
  for (const envelope of [null, {}, { instances: [] }, data({}),
    { ...data(shortText('72F')), validUntilMs: now - 1 },
    { ...data(shortText('72F')), validUntilMs: 'not a timestamp' }]) {
    sub.onData(envelope); assert.equal(f.sent.at(-1).PluginTTL, 0);
  }
  sub.onData({ ...data(shortText('72F')), validUntilMs: now + 1e9 });
  assert.equal(f.sent.at(-1).PluginTTL, 900);
  f.subs[0].onData({ instances: [preference(), preference()] });
  assert.equal(sub.stopped, true);
});

test('preference removal and settings changes cancel old callbacks', () => {
  const f = fixture(); f.manager.start({ PluginFavoriteTopLeft: 1 });
  f.subs[0].onData({ instances: [preference()] });
  const first = f.subs[1];
  f.subs[0].onData({ instances: [] });
  assert.equal(first.stopped, true);
  assert.equal(f.sent.at(-1).PluginTTL, 0);
  const count = f.sent.length;
  first.onData(data(shortText('STALE')));
  assert.equal(f.sent.length, count);
  f.manager.start({});
  f.subs[0].onData({ instances: [preference()] });
  assert.equal(f.subs.length, 2);
});

test('missing favorite entries and unsupported shapes do not replace built-ins', () => {
  const f = fixture(); f.manager.start({ PluginFavoriteTopLeft: 5 });
  f.subs[0].onData({ instances: [preference()] });
  assert.equal(f.subs.length, 1);
  assert.equal(f.sent.at(-1).PluginTTL, 0);
  assert.equal(support.textFor({ image: { pixels: 'x' } }), '');
});

test('watch queue serializes sends, coalesces pending readings and bounds retry', () => {
  const calls = [], timers = [];
  const send = queue({ sendAppMessage(message, ok, fail) { calls.push({ message, ok, fail }); } }, fn => timers.push(fn));
  send({ setting: 1 }, 'settings');
  send({ reading: 1 }, 'slot'); send({ reading: 2 }, 'slot');
  assert.equal(calls.length, 1);
  calls[0].ok(); assert.deepEqual(calls[1].message, { reading: 2 });
  for (let i = 0; i < 3; i++) {
    calls.at(-1).fail();
    if (timers.length) timers.shift()();
  }
  assert.equal(calls.length, 4);
  send({ setting: 2 }, 'settings'); assert.equal(calls.length, 5);
});


test('unavailable providers retry and old retry timers cannot revive changed choices', () => {
  const subs = [], timers = [], sent = [];
  const manager = support.create({ subscribeToSource(config) {
    subs.push(config); return { unsubscribe() {} };
  } }, m => sent.push(m), () => now, fn => { timers.push(fn); return timers.length; }, () => {});
  manager.start({ PluginFavoriteTopLeft: 1 });
  subs[0].onError({ code: 'PLUGIN_UNAVAILABLE' });
  assert.equal(timers.length, 1);
  timers[0]();
  assert.equal(subs.length, 2);
  subs[1].onError({ code: 'PERMISSION_DENIED' });
  manager.start({});
  timers[1]();
  assert.equal(subs.length, 2);
});
