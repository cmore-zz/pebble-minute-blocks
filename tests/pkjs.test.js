const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const test = require('node:test');
const source = fs.readFileSync('src/pkjs/index.js', 'utf8');

function phone(platform = 'emery', saved, plugins = {}, storage = new Map()) {
  if (saved !== undefined) storage.set("pebble-minute-blocks-settings", saved);
  const events = {};
  const messages = [];
  const requests = [];
  let configUrl;
  const opened = [];
  const timers = [];
  const context = vm.createContext({
    console: { log() {} },
    require: name => require('../src/pkjs/' + name.slice(2)),
    setTimeout: (fn, delay) => { const timer = { fn, delay }; timers.push(timer); return timer; },
    clearTimeout(timer) { timer.cancelled = true; },
    localStorage: { getItem: key => storage.get(key), setItem: (key, value) => storage.set(key, value) },
    Pebble: {
      addEventListener: (name, callback) => { events[name] = callback; },
      sendAppMessage: (message, success) => { messages.push(message); if (success) success(); },
      ...plugins,
      getActiveWatchInfo: () => ({ platform }),
      openURL: (url) => { configUrl = url; opened.push(url); }
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
  return { context, events, messages, requests, timers, opened, storage,
    html: () => {
      timers.filter(t => t.delay === 750 && !t.cancelled).forEach(t => { t.cancelled = true; t.fn(); });
      return decodeURIComponent(configUrl.split(',')[1]);
    } };
}

test('existing settings gain normal blocks without losing saved preferences', () => {
  const { context } = phone('emery', JSON.stringify({ HourColor: 123, TimeMode: 2 }));
  const settings = context.loadSettings();
  assert.equal(settings.LargeBlocks, 0);
  assert.equal(settings.HourLeadingZero, 1);
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
  const app = phone();
  const settings = app.context.cloneDefaults();
  app.context.sendSettings(settings);
  assert.equal(app.messages.length, 2);
  for (const message of app.messages) assert.ok(1 + Object.keys(message).length * 11 <= 256);
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
    timeMode: '0', leadingZero: '0', largeBlocks: '1', complicationSize: '0', complicationVisibility: '1',
    secondsVisibility: '2', complicationTopLeft: '1', complicationTopRight: '2',
    complicationBottomRight: '4', complicationBottomLeft: '5', weatherUnits: '0',
    PluginFavoriteTopLeft: '0', PluginFavoriteTopRight: '2',
    PluginFavoriteBottomRight: '0', PluginFavoriteBottomLeft: '0'
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
  assert.equal(saved.HourLeadingZero, 0);
  assert.equal(saved.PluginFavoriteTopRight, 2);
  // Hidden fields can be restored by the webview independently of visible controls.
  for (const malformed of ['%broken', encodeURIComponent('{bad json')]) {
    fields.PluginSourceTopLeft = { value: malformed };
    fields.save.onclick();
    const recovered = JSON.parse(decodeURIComponent(result.split('#')[1]));
    assert.equal(recovered.PluginSourceTopLeft, null);
    assert.equal(recovered.LargeBlocks, 1);
    assert.equal(recovered.PluginFavoriteTopRight, 2);
  }
});


test('leading-zero preference is sent and survives loading saved settings', () => {
  const app = phone('emery', JSON.stringify({ HourLeadingZero: 0 }));
  assert.equal(app.context.loadSettings().HourLeadingZero, 0);
  app.events.webviewclosed({ response: encodeURIComponent(JSON.stringify(app.context.loadSettings())) });
  assert.equal(app.messages[0].HourLeadingZero, 0);
});

test('older settings pages keep the existing leading-zero behavior', () => {
  const app = phone();
  const settings = app.context.cloneDefaults();
  delete settings.HourLeadingZero;
  app.events.webviewclosed({ response: encodeURIComponent(JSON.stringify(settings)) });
  assert.equal(app.messages[0].HourLeadingZero, 1);
});


test('plugins absent: legacy settings, weather and configuration still work', () => {
  const app = phone('aplite');
  app.events.ready();
  assert.equal(app.messages[0].ComplicationTopLeft, 1);
  assert.equal(app.requests.length, 1);
  app.events.showConfiguration();
  assert.doesNotMatch(app.html(), /<label[^>]*>Top left plugin/);
  assert.match(app.html(), /id="PluginFavoriteTopLeft" type="hidden"/);
});

test('disabled or denied plugins retain built-in choices and saved favorites', () => {
  let attempted = 0;
  const app = phone('emery', JSON.stringify({ ComplicationTopLeft: 4, PluginFavoriteTopLeft: 2 }), {
    subscribeToSource() { attempted++; throw new Error('Plugins disabled'); }
  });
  app.events.ready();
  assert.equal(attempted, 1);
  assert.equal(app.messages[0].ComplicationTopLeft, 4);
  assert.equal(app.messages[1].PluginFavoriteTopLeft, 2);
  assert.equal(app.messages.at(-1).PluginTTL, 0);
  app.events.showConfiguration();
  assert.match(app.html(), /<h3>Top left<\/h3>/);
  assert.match(app.html(), /Fav 5<\/option>/);
});

test('old settings pages preserve saved favorite choices', () => {
  const app = phone('emery', JSON.stringify({ PluginFavoriteTopLeft: 3 }));
  const legacy = JSON.parse(JSON.stringify(app.context.cloneDefaults()));
  delete legacy.PluginFavoriteTopLeft;
  app.events.webviewclosed({ response: encodeURIComponent(JSON.stringify(legacy)) });
  assert.equal(app.messages[1].PluginFavoriteTopLeft, 3);
});


test('plugin controls use two slots on chalk and four elsewhere', () => {
  for (const platform of ['aplite', 'basalt', 'chalk', 'diorite', 'emery', 'flint', 'gabbro']) {
    const app = phone(platform, undefined, { subscribeToSource() {} });
    app.events.showConfiguration();
    const count = (app.html().match(/<label for="show/g) || []).length;
    assert.equal(count, platform === 'chalk' ? 2 : 4);
    new vm.Script(app.html().match(/<script>([\s\S]*)<\/script>/)[1]);
  }
});

test('each slot groups its Show and When unavailable controls', () => {
  const f = phone('aplite', undefined, { subscribeToSource() { return { unsubscribe() {} }; } });
  f.events.showConfiguration();
  const html = f.html();
  assert.ok(html.indexOf('id="showBottomRight"') < html.indexOf('id="complicationBottomRight"'));
  assert.ok(html.indexOf('id="complicationBottomRight"') < html.indexOf('id="showBottomLeft"'));
  assert.ok(html.indexOf('id="showBottomLeft"') < html.indexOf('id="complicationBottomLeft"'));
});

test('weather refresh does not restart unchanged plugin subscriptions or clear readings', () => {
  const subs = [];
  const f = phone('aplite', JSON.stringify({ PluginFavoriteTopLeft: 1, WeatherEnabled: 0 }), {
    subscribeToSource(config) {
      const sub = { ...config, stopped: false, unsubscribe() { this.stopped = true; } };
      subs.push(sub); return sub;
    }
  });
  f.events.ready();
  const text = text => ({ shortText: { text } });
  subs[0].onData({ instances: [{ properties: {
    category: text('demo'), item: text('reading'), property: text('value'),
    index: { numericValue: { value: 0 } }
  } }] });
  subs[1].onData({ validUntilMs: Date.now() + 600000,
    instances: [{ properties: { value: text('72F') } }] });
  const count = f.messages.filter(m => m.PluginSlot !== undefined).length;
  f.events.appmessage({ payload: { WeatherRequest: 1 } });
  assert.equal(subs.length, 2);
  assert.equal(subs[0].stopped, false); assert.equal(subs[1].stopped, false);
  assert.equal(f.messages.filter(m => m.PluginSlot !== undefined).length, count);
});

test('configuration shows named favorites safely and retains an unconfigured saved selection', () => {
  const requests = [];
  const f = phone('emery', JSON.stringify({ PluginFavoriteTopLeft: 5 }), {
    subscribeToSource(config) {
      requests.push(config);
      const text = text => ({ shortText: { text } });
      config.onData({ instances: [{ properties: {
        category: text('weather'), item: text('location'), property: text('<script>&temperature'),
        index: { numericValue: { value: 0 } }
      } }] });
      return { unsubscribe() {} };
    }
  });
  f.events.showConfiguration();
  const html = f.html();
  assert.match(html, />Show<\/label>/);
  assert.match(html, />When unavailable<\/label>/);
  assert.match(html, /Fav 1 — Weather \/ Location 1 \/ &lt;Script&gt;&amp;Temperature/);
  assert.match(html, /value="105" selected>Fav 5 — Not configured/);
  assert.match(html, /Settings → Plugins → Complications/);
  assert.equal(requests.length, 1);
  assert.equal(requests[0].category, 'preferences');
  new vm.Script(html.match(/<script>([\s\S]*)<\/script>/)[1]);
});

test('Show switches between built-in and plugin while preserving an explicit fallback', () => {
  const app = phone('emery', JSON.stringify({ ComplicationTopLeft: 4 }), {
    subscribeToSource(config) { config.onData({ instances: [] }); return { unsubscribe() {} }; }
  });
  app.events.showConfiguration();
  const html = app.html(), fields = {};
  for (const match of html.matchAll(/<input\b([^>]*)>/g)) {
    const id = match[1].match(/id="([^"]+)"/);
    if (id) fields[id[1]] = { value: (match[1].match(/value="([^"]*)"/) || [,''])[1],
      checked: /\schecked/.test(match[1]) };
  }
  for (const match of html.matchAll(/<select id="([^"]+)">([\s\S]*?)<\/select>/g)) {
    const options = [...match[2].matchAll(/<option value="([^"]+)"([^>]*)>/g)];
    fields[match[1]] = { value: (options.find(o => /selected/.test(o[2])) || options[0])[1] };
  }
  for (const slot of ['TopLeft', 'TopRight', 'BottomRight', 'BottomLeft']) {
    fields['fallback' + slot] = { style: {} };
  }
  fields.save = {};
  let saved;
  const context = vm.createContext({
    document: { getElementById: id => fields[id], querySelectorAll: () => [] },
    location: { search: '', hash: '', protocol: 'data:' },
    window: { location: { set href(url) { saved = JSON.parse(decodeURIComponent(url.split('#')[1])); } } }
  });
  vm.runInContext(html.match(/<script>([\s\S]*)<\/script>/)[1], context);
  const show = fields.showTopLeft, fallback = fields.complicationTopLeft;
  assert.equal(show.value, '4'); assert.equal(fields.fallbackTopLeft.style.display, 'none');
  show.value = '101'; show.onchange();
  assert.equal(fields.fallbackTopLeft.style.display, ''); assert.equal(Number(fallback.value), 4);
  fallback.value = '6'; fields.save.onclick();
  assert.equal(saved.PluginFavoriteTopLeft, 1); assert.equal(saved.ComplicationTopLeft, 6);
  app.events.webviewclosed({ response: encodeURIComponent(JSON.stringify(saved)) });
  assert.equal(app.messages[0].ComplicationTopLeft, 6);
  assert.equal(app.messages[1].PluginFavoriteTopLeft, 1);
  show.value = '2'; show.onchange(); fields.save.onclick();
  assert.equal(fields.fallbackTopLeft.style.display, 'none');
  assert.equal(saved.PluginFavoriteTopLeft, 0); assert.equal(saved.ComplicationTopLeft, 2);
  show.value = '105'; show.onchange(); fields.save.onclick();
  assert.equal(saved.PluginFavoriteTopLeft, 5); assert.equal(saved.ComplicationTopLeft, 2);
  show.value = '0'; show.onchange(); fields.save.onclick();
  assert.equal(saved.PluginFavoriteTopLeft, 0); assert.equal(saved.ComplicationTopLeft, 0);
});

test('async lookup shows fresh names on the first opening and bounds slow lookups', () => {
  const requests = [];
  let stopped = 0;
  const app = phone('emery', undefined, { subscribeToSource(request) {
    requests.push(request); return { unsubscribe() { stopped++; } };
  } });
  const envelope = property => {
    const text = value => ({ shortText: { text: value } });
    return { instances: [{ properties: { category: text('weather'), item: text('location'),
      property: text(property), index: { numericValue: { value: 0 } } } }] };
  };
  app.events.showConfiguration();
  assert.equal(app.opened.length, 0);
  assert.equal(app.timers[0].delay, 750);
  app.events.showConfiguration();
  assert.equal(requests.length, 1, 'concurrent openings share the pending lookup');
  requests[0].onData(envelope('temperature'));
  assert.equal(app.opened.length, 1);
  assert.match(app.html(), /Fav 1 — Weather \/ Location 1 \/ Temperature/);
  assert.equal(stopped, 1);
  // Fresh data must replace cached names in this opening, including after restart.
  const restarted = phone('emery', undefined, { subscribeToSource(request) {
    requests.push(request); return { unsubscribe() { stopped++; } };
  } }, app.storage);
  restarted.events.showConfiguration();
  requests[1].onData(envelope('humidity'));
  assert.match(restarted.html(), /Fav 1 — Weather \/ Location 1 \/ Humidity/);
  restarted.events.showConfiguration();
  assert.match(restarted.html(), /Fav 1 — Weather \/ Location 1 \/ Humidity/, 'timeout uses cache');
  const opens = restarted.opened.length;
  requests[2].onData({ instances: [] });
  assert.equal(restarted.opened.length, opens, 'late reply cannot reopen settings');
  restarted.events.showConfiguration();
  requests[3].onError({});
  assert.match(restarted.html(), /Fav 1 — Weather \/ Location 1 \/ Humidity/, 'errors retain cache');
  restarted.events.showConfiguration();
  requests[4].onData({ instances: [] });
  assert.match(restarted.html(), /Fav 1 — Not configured/, 'empty list replaces cache immediately');
});

test('unknown or invalid cache uses neutral names when the live lookup fails', () => {
  for (const cached of [undefined, 'broken JSON', '{}', '[1,2,3,4,5]']) {
    const storage = new Map([['pebble-minute-blocks-favorite-labels', cached]]);
    const app = phone('emery', undefined, { subscribeToSource() {
      return { unsubscribe() {} };
    } }, storage);
    app.events.showConfiguration();
    assert.equal(app.opened.length, 0);
    assert.match(app.html(), /value="101">Fav 1<\/option>/);
    assert.equal(app.opened.length, 1);
  }
});

test('fresh labels display even when cache storage fails', () => {
  const app = phone('emery', undefined, { subscribeToSource(request) {
    request.onData({ instances: [{}] }); return { unsubscribe() {} };
  } });
  app.context.localStorage.setItem = () => { throw new Error('storage full'); };
  app.events.showConfiguration();
  assert.equal(app.opened.length, 1);
  assert.match(app.html(), /Fav 1 — Unavailable/);
  assert.match(app.html(), /Fav 2 — Not configured/);
});

test('direct bindings survive restart and legacy saves without entering the watch dictionary', () => {
  const source = { plugin: null, category: 'weather', item: 'location', property: 'humidity', instanceId: 'home' };
  const app = phone('emery', JSON.stringify({ PluginSourceTopLeft: source }));
  assert.deepEqual(JSON.parse(JSON.stringify(app.context.loadSettings().PluginSourceTopLeft)), source);
  app.events.ready();
  assert.equal(app.messages[1].PluginFavoriteTopLeft, 6);
  assert.ok(app.messages.every(m => !('PluginSourceTopLeft' in m)));
  app.events.webviewclosed({ response: encodeURIComponent(JSON.stringify(app.context.cloneDefaults())) });
  assert.deepEqual(JSON.parse(app.storage.get('pebble-minute-blocks-settings')).PluginSourceTopLeft, source);
});
test('catalog and saved bindings are safely embedded, including unavailable sources', () => {
  const saved = { plugin: 'missing', category: 'weather', item: 'location', property: '</script>', instanceId: 'home' };
  const app = phone('emery', JSON.stringify({ PluginSourceTopLeft: saved }), {
    subscribeToSource() {}, enumeratePlugins: () => []
  });
  app.events.showConfiguration(); const html = app.html();
  assert.match(html, /value="200" selected>weather \/ location \(unavailable\)/);
  assert.equal((html.match(/<\/script>/g) || []).length, 1);
  new vm.Script(html.match(/<script>([\s\S]*)<\/script>/)[1]);
});
test('direct controls hide on old companions and hidden chalk slots preserve choices', () => {
  const source = { plugin: 'weather', category: 'weather', item: 'location', property: 'humidity', instanceId: 'home' };
  const old = phone('aplite', JSON.stringify({ PluginSourceTopLeft: source }));
  old.events.showConfiguration(); assert.doesNotMatch(old.html(), /id="sourceControlsTopLeft"/);
  assert.match(old.html(), /id="PluginSourceTopLeft" type="hidden"/);
  const round = phone('chalk', JSON.stringify({ PluginSourceBottomRight: source }), { subscribeToSource() {} });
  round.events.showConfiguration(); assert.doesNotMatch(round.html(), /id="sourceControlsBottomRight"/);
  assert.match(round.html(), /id="PluginSourceBottomRight" type="hidden"/);
});

test('saving a direct reading routes live data and switching back to a favorite removes it', () => {
  const subscriptions = [];
  const app = phone('emery', undefined, { subscribeToSource(config) {
    subscriptions.push(config); return { unsubscribe() {} };
  } });
  const settings = app.context.cloneDefaults();
  settings.PluginSourceTopLeft = { plugin: 'stock-provider', category: 'finance', item: 'stock', property: 'price', instanceId: 'AAPL' };
  settings.PluginFavoriteTopLeft = 3;
  app.events.webviewclosed({ response: encodeURIComponent(JSON.stringify(settings)) });
  assert.equal(subscriptions[0].plugin, 'stock-provider');
  assert.equal(app.messages[1].PluginFavoriteTopLeft, 6);
  subscriptions[0].onData({ pluginUuid: 'stock-provider', validUntilMs: Date.now() + 60000,
    instances: [{ instanceId: 'AAPL', properties: { price: { numericValue: { value: 123.4, unit: '$' } } } }] });
  assert.equal(app.messages.at(-1).PluginText, '123.4$');
  settings.PluginSourceTopLeft = null;
  settings.PluginFavoriteTopLeft = 2;
  app.events.webviewclosed({ response: encodeURIComponent(JSON.stringify(settings)) });
  assert.equal(app.context.loadSettings().PluginSourceTopLeft, null);
  assert.equal(app.messages.filter(m => m.PluginFavoriteTopLeft !== undefined).at(-1).PluginFavoriteTopLeft, 2);
});

test('watch clock reports reach calendar formatting even with weather disabled', () => {
  const subs = [];
  const app = phone('emery', JSON.stringify({ WeatherEnabled: 0, TimeMode: 0,
    PluginSourceTopLeft: { plugin: null, category: 'calendar', item: 'event', property: 'starts_at', instanceId: null }
  }), { subscribeToSource(config) { subs.push(config); return { unsubscribe() {} }; } });
  app.events.appmessage({ payload: { Watch24Hour: 0, WeatherRequest: 0 } });
  subs.at(-1).onData({ instances: [{ properties: { starts_at: {
    timestamp: { value: new Date(2026, 10, 6, 8, 0).getTime() / 1000 }, shortText: { text: '08:00' }
  } } }] });
  assert.equal(app.messages.at(-1).PluginText, '8:00A');
  assert.equal(app.requests.length, 0);
});
