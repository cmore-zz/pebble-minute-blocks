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
  assert.equal(sub.stopped, false, 'unused favorites do not restart the active binding');
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

function liveFixture() {
  let clock = now;
  const subs = [], sent = [], timers = [];
  const manager = support.create({ subscribeToSource(config) {
    const sub = { ...config, stopped: false, unsubscribe() { this.stopped = true; } };
    subs.push(sub); return sub;
  } }, (message, key, options) => {
    sent.push(message); options.onDelivered(message);
  }, () => clock, (fn, delay) => {
    const timer = { fn, delay, cancelled: false }; timers.push(timer); return timer;
  }, timer => { timer.cancelled = true; });
  return { manager, subs, sent, timers, advance(ms) { clock += ms; } };
}

test('identical text and expiry skip sends; shortened or extended validity is delivered', () => {
  const f = liveFixture(); f.manager.start({ PluginFavoriteTopLeft: 1 });
  f.subs[0].onData({ instances: [preference()] });
  const sub = f.subs[1];
  sub.onData(data(shortText('72F')));
  const count = f.sent.length;
  f.advance(123); // Callback jitter must not turn the same absolute expiry into a new deadline.
  sub.onData(data(shortText('72F')));
  assert.equal(f.sent.length, count);
  f.advance(280000);
  sub.onData({ ...data(shortText('72F')), validUntilMs: now + 600000 });
  assert.equal(f.sent.length, count + 1);
  sub.onData({ ...data(shortText('72F')), validUntilMs: null });
  assert.equal(f.sent.at(-1).PluginTTL, 60, 'shortened validity is not suppressed');
});

test('binding comparison ignores metadata and changes only the affected slot', () => {
  const f = liveFixture(); f.manager.start({ PluginFavoriteTopLeft: 1, PluginFavoriteTopRight: 2 });
  f.subs[0].onData({ instances: [preference(), preference()] });
  const a = f.subs[1], b = f.subs[2], count = f.sent.length;
  const decorated = preference(); decorated.properties.timestamp = shortText('new');
  f.subs[0].onData({ instances: [decorated, preference()] });
  assert.equal(f.subs.length, 3); assert.equal(f.sent.length, count);
  f.subs[0].onData({ instances: [preference('humidity'), preference()] });
  assert.equal(a.stopped, true); assert.equal(b.stopped, false);
  const sent = f.sent.length;
  a.onData(data(shortText('OLD'))); assert.equal(f.sent.length, sent);
});

test('denied source retries at 1, 5, 15 then 30 minutes without disturbing healthy slots', () => {
  const f = liveFixture(); f.manager.start({ PluginFavoriteTopLeft: 1, PluginFavoriteTopRight: 2 });
  f.subs[0].onData({ instances: [preference(), preference()] });
  const healthy = f.subs[2];
  healthy.onData(data(shortText('73F')));
  let broken = f.subs[1];
  for (const delay of [60000, 300000, 900000, 1800000, 1800000]) {
    broken.onError({ code: 'PERMISSION_DENIED' });
    const timer = f.timers.at(-1); assert.equal(timer.delay, delay);
    timer.fn(); broken = f.subs.at(-1);
    assert.equal(healthy.stopped, false); assert.equal(f.subs[0].stopped, false);
  }
  broken.onError({ code: 'PERMISSION_DENIED' });
  assert.equal(f.timers.length, 6);
  broken.onData(data(shortText('72F')));
  assert.equal(f.sent.at(-1).PluginText, '72F', 'grant recovers existing subscription during slow recovery');
});

test('spontaneous recovery cancels pending retry and late callbacks cannot restart it', () => {
  const f = liveFixture(); f.manager.start({ PluginFavoriteTopLeft: 1 });
  f.subs[0].onData({ instances: [preference()] });
  f.subs[1].onError({}); const timer = f.timers[0];
  f.subs[1].onData(data(shortText('72F')));
  assert.equal(timer.cancelled, true); timer.fn(); assert.equal(f.subs.length, 2);
});

test('queue recomputes expiry on initial dispatch and every retry', () => {
  let clock = now;
  const calls = [], timers = [];
  const send = queue({ sendAppMessage(message, ok, fail) { calls.push({ message, ok, fail }); } },
    fn => timers.push(fn), () => clock);
  send({ setting: 1 }, 'settings');
  send({ PluginText: '72F', PluginTTL: 1 }, 'plugin-0', { expiresAt: now + 1000 });
  clock += 2000; calls[0].ok();
  assert.equal(calls[1].message.PluginTTL, 0); assert.equal(calls[1].message.PluginText, '');
  calls[1].ok();
  send({ PluginText: '73F', PluginTTL: 2 }, 'plugin-0', { expiresAt: clock + 2000 });
  calls[2].fail(); clock += 3000; timers.shift()();
  assert.equal(calls[3].message.PluginTTL, 0); assert.equal(calls[3].message.PluginText, '');
});

test('lost plugin settings recover independently and superseded retry cannot restore old choices', () => {
  const calls = [], timers = [];
  const send = queue({ sendAppMessage(message, ok, fail) { calls.push({ message, ok, fail }); } },
    (fn, delay) => timers.push({ fn, delay }));
  send({ PluginFavoriteTopLeft: 1 }, 'plugin-settings');
  calls.at(-1).fail(); timers.shift().fn();
  calls.at(-1).fail(); timers.shift().fn();
  calls.at(-1).fail();
  const recovery = timers.shift(); assert.equal(recovery.delay, 60000);
  send({ unrelated: 1 }, 'weather'); calls.at(-1).ok();
  recovery.fn(); assert.equal(calls.at(-1).message.PluginFavoriteTopLeft, 1);
  calls.at(-1).fail();
  const stale = timers.shift();
  send({ PluginFavoriteTopLeft: 0 }, 'plugin-settings'); calls.at(-1).ok();
  const count = calls.length; stale.fn(); assert.equal(calls.length, count);
});

test('plugin readings wait for settings acknowledgement, expire while waiting, and do not block weather', () => {
  let clock = now;
  const calls = [], timers = [];
  const send = queue({ sendAppMessage(message, ok, fail) { calls.push({ message, ok, fail }); } },
    fn => timers.push(fn), () => clock);
  send({ PluginFavoriteTopLeft: 1 }, 'plugin-settings');
  send({ PluginText: '72F', PluginTTL: 10 }, 'plugin-0', { expiresAt: now + 10000 });
  for (let attempt = 0; attempt < 3; attempt++) {
    calls.at(-1).fail();
    if (attempt < 2) timers.shift()();
  }
  assert.equal(calls.length, 3);
  send({ WeatherTemp: 72 }, 'weather');
  assert.equal(calls.at(-1).message.WeatherTemp, 72); calls.at(-1).ok();
  clock += 60000; timers.shift()();
  assert.equal(calls.at(-1).message.PluginFavoriteTopLeft, 1);
  calls.at(-1).ok();
  assert.equal(calls.at(-1).message.PluginTTL, 0);
  assert.equal(calls.at(-1).message.PluginText, '');
});

test('late delivery acknowledgements cannot suppress a newer recovery reading', () => {
  const subs = [], sent = [];
  const manager = support.create({ subscribeToSource(config) {
    subs.push(config); return { unsubscribe() {} };
  } }, (message, key, options) => sent.push({ message, options }), () => now);
  manager.start({ PluginFavoriteTopLeft: 1 });
  sent[0].options.onDelivered(sent[0].message);
  subs[0].onData({ instances: [preference()] });
  subs[1].onData(data(shortText('72F')));
  const older = sent.at(-1);
  subs[1].onError({});
  older.options.onDelivered(older.message);
  const count = sent.length;
  subs[1].onData(data(shortText('72F')));
  assert.equal(sent.length, count + 1);
  assert.equal(sent.at(-1).message.PluginText, '72F');
});

test('an early expiry extension is delivered without depending on another provider callback', () => {
  const f = liveFixture(); f.manager.start({ PluginFavoriteTopLeft: 1 });
  f.subs[0].onData({ instances: [preference()] });
  const envelope = expiry => ({ ...data(shortText('72F')), validUntilMs: expiry });
  f.subs[1].onData(envelope(now + 120000));
  const count = f.sent.length;
  f.subs[1].onData(envelope(now + 600000));
  assert.equal(f.sent.length, count);
  const renewal = f.timers.at(-1);
  assert.equal(renewal.delay, 90000);
  f.advance(90000); renewal.fn(); // No further provider callback.
  assert.equal(f.sent.length, count + 1);
  assert.equal(f.sent.at(-1).PluginTTL, 510); // Original deadline, not another 600 seconds.
  assert.equal(f.timers.length, 1, 'no renewal without another observed extension');
});

test('unchanged starts preserve healthy subscriptions and pending failure recovery', () => {
  const f = liveFixture();
  const settings = { PluginFavoriteTopLeft: 1, PluginFavoriteTopRight: 2, WeatherEnabled: 0 };
  f.manager.start(settings);
  f.subs[0].onData({ instances: [preference(), preference()] });
  f.subs[1].onData(data(shortText('72F')));
  f.subs[2].onError({});
  const count = f.sent.length, timer = f.timers[0];
  f.manager.start(settings);
  f.manager.start({ ...settings, WeatherEnabled: 1, HourColor: 123 });
  assert.equal(f.subs.length, 3); assert.equal(f.sent.length, count);
  assert.equal(timer.cancelled, false); timer.fn();
  assert.equal(f.subs.length, 4); assert.equal(f.subs[1].stopped, false);
});

test('preferences recover after the fast retry budget with weather disabled', () => {
  const f = liveFixture();
  f.manager.start({ PluginFavoriteTopLeft: 1, WeatherEnabled: 0 });
  for (const delay of [60000, 300000, 900000, 1800000]) {
    f.subs.at(-1).onError({});
    assert.equal(f.timers.at(-1).delay, delay);
    f.timers.at(-1).fn();
  }
  f.subs.at(-1).onData({ instances: [preference()] });
  f.subs.at(-1).onData(data(shortText('72F')));
  assert.equal(f.sent.at(-1).PluginText, '72F');
  f.subs.at(-1).onError({});
  const late = f.timers.at(-1);
  f.manager.start({});
  const count = f.subs.length;
  late.fn(); assert.equal(f.subs.length, count);
});


test('rolling extensions share one renewal timer and use the latest observed deadline', () => {
  const f = liveFixture(); f.manager.start({ PluginFavoriteTopLeft: 1 });
  f.subs[0].onData({ instances: [preference()] });
  const sub = f.subs[1]; sub.onData(data(shortText('72F')));
  const count = f.sent.length;
  for (let second = 1; second <= 100; second++) {
    f.advance(1000);
    sub.onData({ ...data(shortText('72F')), validUntilMs: now + 300000 + second * 1000 });
  }
  assert.equal(f.sent.length, count);
  assert.equal(f.timers.length, 1);
  f.advance(170000); f.timers[0].fn();
  assert.equal(f.sent.at(-1).PluginTTL, 130);
  assert.equal(f.sent.length, count + 1);
});

test('omitted validity coalesces frequent updates without inventing a longer lifetime', () => {
  const f = liveFixture(); f.manager.start({ PluginFavoriteTopLeft: 1 });
  f.subs[0].onData({ instances: [preference()] });
  const reading = { ...data(shortText('72F')), validUntilMs: undefined };
  f.subs[1].onData(reading);
  const count = f.sent.length;
  f.advance(10000); f.subs[1].onData(reading);
  f.advance(10000); f.subs[1].onData(reading);
  assert.equal(f.sent.length, count); assert.equal(f.timers.length, 1);
  f.advance(10000); f.timers[0].fn();
  assert.equal(f.sent.at(-1).PluginTTL, 50); // Latest update at t=20 expires at t=80.
});

test('shorter validity and changed text cancel a pending extension immediately', () => {
  for (const [text, expiry] of [['72F', now + 40000], ['73F', now + 600000]]) {
    const f = liveFixture(); f.manager.start({ PluginFavoriteTopLeft: 1 });
    f.subs[0].onData({ instances: [preference()] });
    f.subs[1].onData(data(shortText('72F')));
    f.subs[1].onData({ ...data(shortText('72F')), validUntilMs: now + 600000 });
    const timer = f.timers[0];
    f.subs[1].onData({ ...data(shortText(text)), validUntilMs: expiry });
    assert.equal(timer.cancelled, true);
    assert.equal(f.sent.at(-1).PluginText, text);
    assert.equal(f.sent.at(-1).PluginTTL, (expiry - now) / 1000);
    const count = f.sent.length; timer.fn(); assert.equal(f.sent.length, count);
  }
});

test('errors, binding removal and settings changes invalidate queued renewal callbacks', () => {
  for (const change of ['error', 'binding', 'settings']) {
    const f = liveFixture(); f.manager.start({ PluginFavoriteTopLeft: 1 });
    f.subs[0].onData({ instances: [preference()] });
    f.subs[1].onData(data(shortText('72F')));
    f.subs[1].onData({ ...data(shortText('72F')), validUntilMs: now + 600000 });
    const timer = f.timers[0];
    if (change === 'error') f.subs[1].onError({});
    else if (change === 'binding') f.subs[0].onData({ instances: [] });
    else f.manager.start({});
    assert.equal(timer.cancelled, true);
    const count = f.sent.length; f.advance(300000); timer.fn();
    assert.equal(f.sent.length, count);
  }
});

test('a delayed renewal clears an already expired reading instead of reviving it', () => {
  const f = liveFixture(); f.manager.start({ PluginFavoriteTopLeft: 1 });
  f.subs[0].onData({ instances: [preference()] });
  f.subs[1].onData(data(shortText('72F')));
  f.subs[1].onData({ ...data(shortText('72F')), validUntilMs: now + 600000 });
  f.advance(601000); f.timers[0].fn();
  assert.equal(f.sent.at(-1).PluginText, ''); assert.equal(f.sent.at(-1).PluginTTL, 0);
});

const directChoice = (property = 'temperature', instanceId = 'home') => ({
  plugin: 'weather-provider', category: 'weather', item: 'location', property, instanceId
});
test('direct readings do not require favorites and bind by stable item ID', () => {
  const f = fixture();
  f.manager.start({ PluginSourceTopLeft: directChoice(), PluginSourceTopRight: directChoice('humidity') });
  assert.equal(f.subs.length, 2);
  assert.equal(f.subs[0].plugin, 'weather-provider');
  const envelope = { validUntilMs: now + 60000, instances: [
    { instanceId: 'work', properties: { temperature: shortText('99F') } },
    { instanceId: 'home', properties: { temperature: shortText('72F'), humidity: shortText('40%') } }
  ] };
  f.subs[0].onData(envelope); f.subs[1].onData(envelope);
  assert.equal(f.sent.at(-2).PluginText, '72F'); assert.equal(f.sent.at(-1).PluginText, 'RH40%');
  assert.equal(f.sent.at(-1).PluginFavorite, 6);
  f.subs[0].onData({ ...envelope, instances: [envelope.instances[0]] });
  assert.equal(f.sent.at(-1).PluginText, '', 'removed location must not switch to work');
});
test('changed direct reading cancels old callbacks even though watch mode stays direct', () => {
  const f = liveFixture();
  f.manager.start({ PluginSourceTopLeft: directChoice() }); const old = f.subs[0];
  old.onData(data(shortText('72F')));
  f.manager.start({ PluginSourceTopLeft: directChoice() }); assert.equal(f.subs.length, 1);
  f.manager.start({ PluginSourceTopLeft: directChoice('humidity') });
  assert.equal(old.stopped, true);
  const count = f.sent.length; old.onData(data(shortText('99F'))); assert.equal(f.sent.length, count);
  f.subs[1].onError({}); assert.equal(f.sent.at(-1).PluginTTL, 0);
});
test('favorite failures do not clear direct readings; first-item and preferred-provider choices work', () => {
  const f = fixture();
  f.manager.start({ PluginSourceTopLeft: { ...directChoice(), plugin: null, instanceId: null }, PluginFavoriteTopRight: 2 });
  assert.equal(f.subs[0].plugin, null); assert.equal(f.subs[1].category, 'preferences');
  f.subs[0].onData(data(shortText('72F')));
  const count = f.sent.length; f.subs[1].onError({});
  assert.equal(f.subs[0].stopped, false);
  assert.ok(f.sent.slice(count).every(m => m.PluginSlot !== 0));
});
test('direct bindings reject invalid identifiers and support compact timestamps and booleans', () => {
  for (const choice of [null, {}, { ...directChoice(), instanceId: 0 }, { ...directChoice(), property: '' }]) {
    assert.equal(support.direct(choice), null);
  }
  assert.equal(support.textFor({ boolean: { value: false } }), 'OFF');
  const stamp = new Date(2026, 10, 6, 6, 42).getTime() / 1000;
  assert.equal(support.textFor({ timestamp: { value: stamp } }), '06:42');
  assert.equal(support.textFor({ timestamp: { value: 1e300 } }), '');
});
