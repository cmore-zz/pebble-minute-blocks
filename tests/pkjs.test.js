const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const test = require('node:test');
const source = fs.readFileSync('src/pkjs/index.js', 'utf8');

function phone(platform = 'emery', saved) {
  const events = {};
  const messages = [];
  const requests = [];
  let configUrl;
  const context = vm.createContext({
    console: { log() {} },
    localStorage: { getItem: () => saved, setItem() {} },
    Pebble: {
      addEventListener: (name, callback) => { events[name] = callback; },
      sendAppMessage: (message) => { messages.push(message); },
      getActiveWatchInfo: () => ({ platform }),
      openURL: (url) => { configUrl = url; }
    },
    navigator: { geolocation: {
      getCurrentPosition: (success) => success({ coords: { latitude: 40, longitude: -74 } })
    } },
    XMLHttpRequest: function() {
      requests.push(this);
      this.open = () => {};
      this.send = () => {};
    }
  });
  vm.runInContext(source, context);
  return { context, events, messages, requests, html: () => decodeURIComponent(configUrl.split(',')[1]) };
}

test('existing settings gain normal blocks without losing saved preferences', () => {
  const { context } = phone('emery', JSON.stringify({ HourColor: 123, TimeMode: 2 }));
  const settings = context.loadSettings();
  assert.equal(settings.LargeBlocks, 0);
  assert.equal(settings.HourColor, 123);
  assert.equal(settings.TimeMode, 2);
});

test('large block selector is available only on PT2', () => {
  for (const platform of ['aplite', 'basalt', 'chalk', 'diorite', 'emery', 'flint', 'gabbro']) {
    const app = phone(platform);
    app.events.showConfiguration();
    assert.equal(app.html().includes('Block size (hour and minute ring)'), platform === 'emery');
    // Every platform still supplies the value to the generated Save handler.
    assert.match(app.html(), /id="largeBlocks"/);
    new vm.Script(app.html().match(/<script>([\s\S]*)<\/script>/)[1]);
  }
});

test('large blocks survive Save and are sent to the watch', () => {
  const app = phone();
  const settings = app.context.cloneDefaults();
  settings.LargeBlocks = 1;
  app.events.webviewclosed({ response: encodeURIComponent(JSON.stringify(settings)) });
  assert.equal(app.messages[0].LargeBlocks, 1);
});

test('complete settings dictionary fits the watch inbox', () => {
  const settings = phone().context.cloneDefaults();
  assert.ok(1 + Object.keys(settings).length * 11 <= 256);
  const keys = JSON.parse(fs.readFileSync('package.json')).pebble.messageKeys;
  assert.ok(Object.keys(settings).every(key => keys.includes(key)));
});

function weatherResponse(status, data) {
  const app = phone();
  app.context.getWeather(app.context.cloneDefaults());
  const request = app.requests[0];
  request.status = status;
  request.responseText = JSON.stringify(data);
  request.onload();
  return app;
}

const validWeather = {
  current: { temperature_2m: 72.2 },
  daily: { temperature_2m_max: [80.7], temperature_2m_min: [61.3] }
};

test('weather sends rounded current, high, and low values', () => {
  const app = weatherResponse(200, validWeather);
  assert.deepEqual(JSON.parse(JSON.stringify(app.messages[0])), {
    WeatherTemp: 72, WeatherHigh: 81, WeatherLow: 61, WeatherAvailable: 1
  });
});

test('weather rejects HTTP errors, null readings, and incomplete responses', () => {
  for (const [status, data] of [
    [503, validWeather],
    [200, { ...validWeather, current: { temperature_2m: null } }],
    [200, { ...validWeather, daily: { temperature_2m_max: [], temperature_2m_min: [] } }],
    [200, {}]
  ]) {
    const app = weatherResponse(status, data);
    assert.equal(app.messages[0].WeatherAvailable, 0);
    assert.equal(app.messages[0].WeatherTemp, undefined);
  }
});

test('weather requests time out and mark weather unavailable', () => {
  const app = phone();
  app.context.getWeather(app.context.cloneDefaults());
  assert.equal(app.requests[0].timeout, 30000);
  app.requests[0].ontimeout();
  assert.equal(app.messages[0].WeatherAvailable, 0);
});

test('generated config Save returns the selected block size', () => {
  const app = phone();
  app.events.showConfiguration();
  const values = {
    background: '#000000', ring: '#ffffff', complication: '#ffffff', hour: '#00ffff',
    timeMode: '0', largeBlocks: '1', complicationSize: '0', complicationVisibility: '1',
    secondsVisibility: '2', complicationTopLeft: '1', complicationTopRight: '2',
    complicationBottomRight: '4', complicationBottomLeft: '5', weatherUnits: '0'
  };
  const fields = {};
  for (const [key, value] of Object.entries(values)) fields[key] = { value };
  Object.assign(fields, {
    save: {}, kineticEnabled: { checked: false }, weatherEnabled: { checked: true }
  });
  let result;
  const context = vm.createContext({
    document: { getElementById: id => fields[id], querySelectorAll: () => [] },
    location: { search: '', hash: '', protocol: 'data:' },
    window: { location: { set href(url) { result = url; } } }
  });
  vm.runInContext(app.html().match(/<script>([\s\S]*)<\/script>/)[1], context);
  fields.save.onclick();
  const saved = JSON.parse(decodeURIComponent(result.split('#')[1]));
  assert.equal(saved.LargeBlocks, 1);
});
