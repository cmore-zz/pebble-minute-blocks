const test = require('node:test');
const assert = require('node:assert/strict');
const favorites = require('../src/pkjs/plugin-favorites');
const entry = (category, item, property, index = 0) => ({ properties: {
  category: { shortText: { text: category } }, item: { shortText: { text: item } },
  property: { shortText: { text: property } }, index: { numericValue: { value: index } }
} });

test('favorite labels describe the configured binding without fetching its data', () => {
  assert.deepEqual(favorites.labels([entry('weather', 'location', 'temperature'),
    entry('phone', 'state', 'battery_percent', 1)]), [
    'Fav 1 — Weather / Location 1 / Temperature',
    'Fav 2 — Phone / State 2 / Battery Percent',
    'Fav 3 — Not configured', 'Fav 4 — Not configured', 'Fav 5 — Not configured'
  ]);
  assert.equal(favorites.labels(null)[0], 'Fav 1');
  assert.equal(favorites.labels([{}])[0], 'Fav 1 — Unavailable');
});

test('synchronous preference reply unsubscribes and completes exactly once', () => {
  let stopped = 0, cleared = 0;
  const results = [], requests = [];
  favorites.load({ subscribeToSource(config) {
    requests.push(config);
    config.onData({ instances: [entry('weather', 'location', 'temperature')] });
    config.onData({ instances: [] });
    return { unsubscribe() { stopped++; } };
  } }, () => 1, () => { cleared++; }, labels => results.push(labels));
  assert.equal(stopped, 1); assert.equal(cleared, 1); assert.equal(results.length, 1);
  assert.equal(requests.length, 1); assert.equal(requests[0].category, 'preferences');
  assert.match(results[0][0], /Temperature/);
});

test('timeout returns unknown labels and ignores late metadata callbacks', () => {
  let timeout, request, stopped = 0;
  const results = [];
  favorites.load({ subscribeToSource(config) {
    request = config; return { unsubscribe() { stopped++; } };
  } }, (fn, delay) => { assert.equal(delay, 750); timeout = fn; return 1; }, () => {},
  labels => results.push(labels));
  timeout(); request.onData({ instances: [] }); request.onError({});
  assert.equal(stopped, 1); assert.equal(results.length, 1);
  assert.equal(results[0][0], 'Fav 1');
});

test('missing API, thrown errors and malformed payloads return neutral labels', () => {
  for (const api of [{}, { subscribeToSource() { throw new Error('disabled'); } },
    { subscribeToSource(config) { config.onData({}); return { unsubscribe() {} }; } }]) {
    const results = [];
    favorites.load(api, () => 1, () => {}, labels => results.push(labels));
    assert.equal(results.length, 1); assert.equal(results[0][0], 'Fav 1');
  }
});
