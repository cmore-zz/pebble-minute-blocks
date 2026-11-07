const test = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const install = require('../src/pkjs/plugin-picker-ui');
function fixture(saved) {
  function element(tag) {
    return { tag, style: {}, children: [], value: '', appendChild(child) { child.parentNode = this; this.children.push(child); },
      set textContent(value) { this.text = value; this.children = []; }, get textContent() { return this.text; } };
  }
  const fields = { showTopLeft: { value: '200' }, PluginSourceTopLeft: { value: encodeURIComponent(JSON.stringify(saved || null)) },
    sourceControlsTopLeft: element('div') };
  let ready;
  const requests = [];
  const catalog = [
    { plugin: 'weather', category: 'weather', item: 'location', multiple: true,
      readings: [{ value: 'name', label: 'Name' }, { value: 'temperature', label: 'Temperature' }, { value: 'humidity', label: 'Humidity' }] },
    { plugin: 'stocks', category: 'finance', item: 'stock', multiple: true,
      readings: [{ value: 'price', label: 'Price' }, { value: 'change', label: 'Change' }] }
  ];
  const context = vm.createContext({ document: { getElementById: id => fields[id], createElement: element },
    Pebble: { addEventListener(type, fn) { ready = fn; }, sendMessage(target, message) {
      return new Promise(resolve => requests.push({ target, message, resolve }));
    } } });
  const picker = vm.runInContext('(' + install.toString() + ')', context)(catalog, ['TopLeft']);
  return { picker, fields, requests, ready: () => ready({ target: 'pkjs' }),
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
