const test = require('node:test');
const assert = require('node:assert/strict');
const weather = require('../src/pkjs/plugin-weather');
const { textFor, create } = require('../src/pkjs/plugin-complications');
const { catalog } = require('../src/pkjs/plugin-catalog');
const numeric = (value, unit = '°F') => ({ numericValue: { value, unit } });
const render = values => weather.text('weather', 'location', weather.RANGE, values, textFor);
test('temperature ranges round both numeric values and include one unit', () => {
  assert.equal(render({ low: numeric(55.2), high: numeric(71.8) }), '55-72F');
  assert.equal(render({ low: numeric(-12, '°C'), high: numeric(-3, '°C') }), '-12/-3C');
  assert.equal(render({ low: numeric(-3, '°C'), high: numeric(4, '°C') }), '-3/4C');
  assert.equal(render({ low: numeric(0), high: numeric(0) }), '0-0F');
});
test('incomplete, incompatible and oversized ranges use the fallback rather than inventing values', () => {
  for (const values of [null, {}, { low: numeric(50) }, { low: numeric(NaN), high: numeric(72) },
    { low: numeric(55), high: numeric(Infinity) }, { low: numeric(55), high: numeric(20, '°C') },
    { low: numeric(80), high: numeric(55) }, { low: numeric(-20, '°C'), high: numeric(-10, '°C') },
    { low: numeric(50, ''), high: numeric(70, '') },
    { low: { shortText: { text: '55F' } }, high: { shortText: { text: '72F' } } }]) {
    assert.equal(render(values), '');
  }
});
test('markers distinguish weather readings and preserve other plugin formatting', () => {
  const expected = { humidity: ['40%', 'RH40%'], precipitation: ['40%', 'R40%'],
    feels_like: ['72°F', 'FL72F'], high: ['72°F', 'H72F'], low: ['55°F', 'L55F'],
    uv_index: ['3', 'UV3'], wind_speed: ['8mph', 'W8mph'], sunrise: ['06:42', 'SR06:42'],
    sunset: ['18:10', 'SS18:10'], temperature: ['72°F', '72F'] };
  for (const [property, [input, output]] of Object.entries(expected)) {
    assert.equal(weather.text('weather', 'location', property, { [property]: { shortText: { text: input } } }, textFor), output);
  }
  assert.equal(weather.text('weather', 'location', 'humidity', { humidity: { shortText: { text: 'RH40%' } } }, textFor), 'RH40%');
  assert.equal(weather.text('weather', 'location', 'humidity', {}, textFor), '');
  assert.equal(weather.text('weather', 'location', 'wind_speed', { wind_speed: numeric(123.4, 'mph') }, textFor), 'W123mph');
  assert.equal(weather.text('custom', 'location', 'humidity', { humidity: numeric(40, '%') }, textFor), '40%');
});
test('range is offered only when a single weather source declares both numeric endpoints', () => {
  const plugin = (uuid, category, properties) => ({ uuid, sources: [{ category, items: ['location'], properties }] });
  const hasRange = source => source.readings.some(r => r.value === weather.RANGE);
  assert.ok(catalog({ enumeratePlugins: () => [plugin('a', 'weather', { low: ['numericValue'], high: ['numericValue'] })] }).every(hasRange));
  const split = catalog({ enumeratePlugins: () => [plugin('a', 'weather', { low: ['numericValue'] }), plugin('b', 'weather', { high: ['numericValue'] })] });
  assert.ok(split.every(s => !hasRange(s)), 'must not combine declarations across providers');
  assert.ok(catalog({ enumeratePlugins: () => [plugin('a', 'custom', { low: ['numericValue'], high: ['numericValue'] })] }).every(s => !hasRange(s)));
  assert.ok(catalog({ enumeratePlugins: () => [plugin('a', 'weather', { low: ['shortText'], high: ['numericValue'] })] }).every(s => !hasRange(s)));
});
test('range subscribes to both endpoints in the selected item and clears when either disappears', () => {
  const requests = [], sent = [], now = 1800000000000;
  const manager = create({ subscribeToSource(config) { requests.push(config); return { unsubscribe() {} }; } },
    message => sent.push(message), () => now);
  manager.start({ PluginSourceTopLeft: { plugin: null, category: 'weather', item: 'location', property: weather.RANGE, instanceId: 'home' } });
  assert.deepEqual(requests[0].properties, ['low', 'high']);
  requests[0].onData({ validUntilMs: now + 60000, instances: [
    { instanceId: 'away', properties: { low: numeric(10), high: numeric(20) } },
    { instanceId: 'home', properties: { low: numeric(55), high: numeric(72) } }
  ] });
  assert.equal(sent.at(-1).PluginText, '55-72F'); assert.equal(sent.at(-1).PluginTTL, 60);
  requests[0].onData({ validUntilMs: now + 60000, instances: [{ instanceId: 'home', properties: { low: numeric(55) } }] });
  assert.equal(sent.at(-1).PluginText, ''); assert.equal(sent.at(-1).PluginTTL, 0);
});

test('wind fits using the display units, compact spacing and rounded speeds', () => {
  const wind = (short, numericUnit = 'mph') => weather.text('weather', 'location', 'wind_speed', {
    wind_speed: { shortText: { text: short }, numericValue: { value: 8, unit: numericUnit } }
  }, textFor);
  assert.equal(wind('12 km/h'), 'W12km/h');
  assert.equal(wind('12 knots'), 'W12kt');
  assert.equal(wind('12.4 mph'), 'W12mph');
  assert.equal(wind('120 km/h'), '120km/h');
  assert.equal(wind('8 mph'), 'W8mph');
});

test('solar readings prefer valid timestamps and retain text when timestamps are unusable', () => {
  for (const [property, marker] of [['sunrise', 'SR'], ['sunset', 'SS']]) {
    const renderSolar = timestamp => weather.text('weather', 'location', property, {
      [property]: { timestamp: { value: timestamp }, shortText: { text: '06:42' } }
    }, textFor);
    const timestamp = new Date(2026, 10, 6, 6, 42).getTime() / 1000;
    assert.equal(weather.text('weather', 'location', property, {
      [property]: { timestamp: { value: timestamp }, shortText: { text: '6:42 AM' } }
    }, textFor), marker + '06:42');
    for (const invalid of [undefined, NaN, Infinity, 1e20]) assert.equal(renderSolar(invalid), marker + '06:42');
  }
});
