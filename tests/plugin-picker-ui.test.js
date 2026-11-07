const test = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const install = require('../src/pkjs/plugin-picker-ui');
function fixture(saved, twoSlots = false, alreadyReady = false) {
  function element(tag) {
    return { tag, style: {}, children: [], value: '', appendChild(child) { child.parentNode = this; this.children.push(child); },
      set textContent(value) { this.text = value; this.children = []; }, get textContent() { return this.text; } };
  }
  const fields = { showTopLeft: { value: '200' }, PluginSourceTopLeft: { value: encodeURIComponent(JSON.stringify(saved || null)) },
    sourceControlsTopLeft: element('div') };
  if (twoSlots) {
    fields.showTopRight = { value: '200' };
    fields.PluginSourceTopRight = { value: encodeURIComponent(JSON.stringify(saved || null)) };
    fields.sourceControlsTopRight = element('div');
  }
  let ready;
  const requests = [];
  const catalog = [
    { plugin: 'weather', category: 'weather', item: 'location', multiple: true,
      readings: [{ value: 'name', label: 'Name' }, { value: 'temperature', label: 'Temperature' }, { value: 'humidity', label: 'Humidity' }] },
    { plugin: 'stocks', category: 'finance', item: 'stock', multiple: true,
      readings: [{ value: 'price', label: 'Price' }, { value: 'change', label: 'Change' }] }
  ];
  const context = vm.createContext({ document: { getElementById: id => fields[id], createElement: element },
    Pebble: { addEventListener(type, fn) { ready = fn; if (alreadyReady) fn({ target: 'pkjs' }); }, sendMessage(target, message) {
      return new Promise(resolve => requests.push({ target, message, resolve }));
    } } });
  const picker = vm.runInContext('(' + install.toString() + ')', context)(catalog, twoSlots ? ['TopLeft', 'TopRight'] : ['TopLeft']);
  return { picker, fields, requests, catalog, ready: () => ready({ target: 'pkjs' }),
    binding: () => JSON.parse(decodeURIComponent(fields.PluginSourceTopLeft.value)),
    container: fields.sourceControlsTopLeft };
}
test('weather selects temperature; reading and item changes remain independent', async () => {
  const f = fixture(); f.picker.sync('TopLeft');
  assert.equal(f.binding().property, 'temperature'); assert.equal(f.requests.length, 0);
  const reading = f.container.children[0].children[0]; reading.value = 'humidity'; reading.onchange();
  f.ready(); assert.equal(f.requests.length, 1);
  f.requests[0].resolve({ items: [{ value: 'home', label: 'Home' }, { value: 'work', label: 'Work' }] });
  await Promise.resolve();
  const item = f.container.children[1].children[0].children[0]; item.value = 'work'; item.onchange();
  assert.equal(f.binding().property, 'humidity'); assert.equal(f.binding().instanceId, 'work');
  f.fields.showTopLeft.value = '201'; f.picker.sync('TopLeft');
  assert.equal(f.binding().property, 'price'); assert.equal(f.binding().instanceId, null);
  f.fields.showTopLeft.value = '101'; assert.equal(f.picker.sync('TopLeft'), false); assert.equal(f.binding(), null);
});
test('late items cannot replace another source; removed items remain saved', async () => {
  const saved = { plugin: 'weather', category: 'weather', item: 'location', property: 'humidity', instanceId: 'gone' };
  const f = fixture(saved); f.ready(); f.picker.sync('TopLeft');
  f.requests[0].resolve({ items: [{ value: 'home', label: 'Home' }] }); await Promise.resolve();
  assert.equal(f.binding().instanceId, 'gone');
  const item = f.container.children[1].children[0].children[0];
  assert.ok(item.children.some(o => o.value === 'gone'));
  f.container.children.at(-1).onclick();
  f.fields.showTopLeft.value = '201'; f.picker.sync('TopLeft');
  f.requests[1].resolve({ items: [{ value: 'wrong', label: 'Old weather response' }] }); await Promise.resolve();
  assert.equal(f.binding().plugin, 'stocks'); assert.equal(f.container.children[1].children.length, 0);
});
test('failed lookups retain choices and can be retried', async () => {
  const saved = { plugin: 'weather', category: 'weather', item: 'location', property: 'humidity', instanceId: 'home' };
  const f = fixture(saved); f.ready(); f.picker.sync('TopLeft');
  f.requests[0].resolve({ error: 'Permission denied' }); await Promise.resolve();
  assert.deepEqual(f.binding(), saved); assert.equal(f.container.children[2].textContent, 'Permission denied');
  f.container.children.at(-1).onclick(); assert.equal(f.requests.length, 2);
});

test('a sole item can be pinned and remains pinned when another appears first', async () => {
  const f = fixture(); f.ready(); f.picker.sync('TopLeft');
  f.requests[0].resolve({ items: [{ value: 'home', label: 'Home' }] }); await Promise.resolve();
  const wrap = f.container.children[1]; assert.equal(wrap.style.display, '');
  const item = wrap.children[0].children[0];
  assert.deepEqual(item.children.map(o => o.value), ['', 'home']);
  item.value = 'home'; item.onchange(); assert.equal(f.binding().instanceId, 'home');
  f.container.children.at(-1).onclick();
  f.requests[1].resolve({ items: [{ value: 'away', label: 'Away' }, { value: 'home', label: 'Home' }] });
  await Promise.resolve();
  assert.equal(f.binding().instanceId, 'home'); assert.equal(wrap.children[0].children[0].value, 'home');
});
test('concurrent corners share item requests while retaining their own selection', async () => {
  const f = fixture(null, true); f.ready(); f.picker.sync('TopLeft'); f.picker.sync('TopRight');
  assert.equal(f.requests.length, 1);
  f.requests[0].resolve({ items: [{ value: 'home', label: 'Home' }] }); await Promise.resolve();
  assert.equal(f.fields.sourceControlsTopLeft.children[1].children.length, 1);
  assert.equal(f.fields.sourceControlsTopRight.children[1].children.length, 1);
  f.container.children.at(-1).onclick();
  f.fields.sourceControlsTopRight.children.at(-1).onclick();
  assert.equal(f.requests.length, 2, 'retry fetches fresh data, shared across corners');
  f.fields.showTopLeft.value = '201'; f.picker.sync('TopLeft');
  f.requests[1].resolve({ items: [{ value: 'home', label: 'Home' }] }); await Promise.resolve();
  assert.equal(f.binding().plugin, 'stocks');
  assert.equal(f.fields.sourceControlsTopRight.children[1].children[0].children[0].children[1].value, 'home');
});
test('shared errors release the lookup so both corners can retry', async () => {
  const f = fixture(null, true); f.ready(); f.picker.sync('TopLeft'); f.picker.sync('TopRight');
  f.requests[0].resolve({ error: 'Denied' }); await Promise.resolve();
  for (const key of ['sourceControlsTopLeft', 'sourceControlsTopRight']) {
    assert.equal(f.fields[key].children[2].textContent, 'Denied');
    assert.equal(f.fields[key].children.at(-1).style.display, '');
  }
  f.container.children.at(-1).onclick(); assert.equal(f.requests.length, 2);
});
test('truncated lists retain saved IDs without falsely calling them unavailable', async () => {
  const f = fixture({ plugin: 'weather', category: 'weather', item: 'location', property: 'humidity', instanceId: 'beyond-limit' });
  f.ready(); f.picker.sync('TopLeft');
  f.requests[0].resolve({ items: [{ value: 'home', label: 'Home' }], truncated: true }); await Promise.resolve();
  const options = f.container.children[1].children[0].children[0].children;
  assert.equal(options.at(-1).textContent, 'beyond-limit (not listed)');
  assert.equal(f.binding().instanceId, 'beyond-limit');
  assert.match(f.container.children[2].textContent, /first 50/);
});
test('the companion replaying ready during listener registration enables item loading', () => {
  const f = fixture(null, false, true); f.picker.sync('TopLeft'); assert.equal(f.requests.length, 1);
});
test('non-weather sources default to their first reading even if they advertise temperature', () => {
  const f = fixture(); f.catalog[1].readings.push({ value: 'temperature', label: 'Temperature' });
  f.fields.showTopLeft.value = '201'; f.picker.sync('TopLeft'); assert.equal(f.binding().property, 'price');
});

test('calendar defaults to event start time rather than an often-empty location', () => {
  const f = fixture();
  f.catalog[1] = { plugin: 'calendar', category: 'calendar', item: 'event', multiple: true,
    readings: ['name', 'location', 'starts_at', 'ends_at'].map(value => ({ value, label: value })) };
  f.fields.showTopLeft.value = '201'; f.picker.sync('TopLeft');
  assert.equal(f.binding().property, 'starts_at');
});
