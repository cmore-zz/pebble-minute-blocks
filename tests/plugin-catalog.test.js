const test = require('node:test');
const assert = require('node:assert/strict');
const { catalog, items } = require('../src/pkjs/plugin-catalog');
const declaration = [{ uuid: 'weather', name: 'Weather', sources: [{ category: 'weather', items: ['location'],
  supportsMultiple: true, properties: { temperature: ['shortText', 'numericValue'], humidity: ['numericValue'],
    sunrise: ['timestamp'], icon: ['image'], hourly_temperature: ['series.numericValue'] } }] }];
test('discovery filters unsupported shapes without subscribing to sources', () => {
  const result = catalog({ enumeratePlugins: () => declaration, subscribeToSource() { assert.fail('passive discovery'); } });
  assert.equal(result.length, 2); assert.equal(result[0].plugin, null);
  assert.deepEqual(result[1].readings.map(r => r.value), ['temperature', 'humidity', 'sunrise']);
  assert.deepEqual(catalog({ enumeratePlugins() { throw Error('disabled'); } }), []);
  assert.deepEqual(catalog({}), []);
});
test('settings cannot subscribe to action-like phone finder or arbitrary unadvertised sources', () => {
  const api = { enumeratePlugins: () => [{ uuid: 'phone', sources: [{ category: 'phone', items: ['find_my_phone'],
    properties: { ringing: ['boolean'] } }] }], subscribeToSource() { assert.fail('must not ring'); } };
  assert.deepEqual(catalog(api), []);
  items(api, { plugin: 'phone', category: 'phone', item: 'find_my_phone' }, setTimeout, clearTimeout,
    reply => assert.ok(reply.error));
});
test('item lookup preserves stable IDs, bounds names, and cleans up synchronous replies', () => {
  let stopped = 0, cancelled = 0, reply;
  items({ enumeratePlugins: () => declaration, subscribeToSource(config) {
    assert.deepEqual(config.properties, ['name']);
    config.onData({ instances: [{ instanceId: 'home', properties: { name: { longText: { text: 'Home' } } } }] });
    return { unsubscribe() { stopped++; } };
  } }, { plugin: 'weather', category: 'weather', item: 'location' }, () => 1, () => cancelled++, r => reply = r);
  assert.deepEqual(reply.items, [{ value: 'home', label: 'Home' }]);
  assert.equal(stopped, 1); assert.equal(cancelled, 1);
});
test('item lookup timeout cancels subscriptions and ignores late data', () => {
  let timeout, request, stopped = 0, replies = [];
  items({ enumeratePlugins: () => declaration, subscribeToSource(config) {
    request = config; return { unsubscribe() { stopped++; } };
  } }, { plugin: 'weather', category: 'weather', item: 'location' }, fn => { timeout = fn; return 1; }, () => {}, r => replies.push(r));
  timeout(); request.onData({ instances: [] });
  assert.equal(replies.length, 1); assert.ok(replies[0].error); assert.equal(stopped, 1);
});

test('item replies are bounded and deduplicated, with an explicit truncation signal', () => {
  for (const count of [50, 51, 1000]) {
    let reply;
    const instances = Array.from({ length: count }, (_, i) => ({ instanceId: 'item-' + i,
      properties: { name: { shortText: { text: 'x'.repeat(200) } } } }));
    items({ enumeratePlugins: () => declaration, subscribeToSource(config) {
      config.onData({ instances: [instances[0], instances[0], { instanceId: '' }, ...instances] });
      return { unsubscribe() {} };
    } }, { plugin: 'weather', category: 'weather', item: 'location' }, () => 1, () => {}, r => reply = r);
    assert.equal(reply.items.length, 50); assert.equal(new Set(reply.items.map(i => i.value)).size, 50);
    assert.equal(reply.truncated, count > 50); assert.ok(reply.items.every(i => i.label.length === 128));
  }
});
