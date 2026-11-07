// Embedded in the settings page; keep this function self-contained.
function install(catalog, slots) {
  var states = {}, pending = {}, ready = false;
  function el(id) { return document.getElementById(id); }
  function sourceKey(s) { return JSON.stringify([s.plugin, s.category, s.item]); }
  function read(slot) {
    try {
      var value = JSON.parse(decodeURIComponent(el('PluginSource' + slot).value));
      return value && typeof value === 'object' && !Array.isArray(value) &&
        typeof value.category === 'string' && typeof value.item === 'string' &&
        typeof value.property === 'string' &&
        (value.instanceId == null || typeof value.instanceId === 'string') ? value : null;
    }
    catch (e) { return null; }
  }
  function write(slot, value) { el('PluginSource' + slot).value = encodeURIComponent(JSON.stringify(value)); }
  function control(parent, label, options, value) {
    var wrap = document.createElement('label'); wrap.textContent = label;
    var select = document.createElement('select');
    options.forEach(function(option) {
      var node = document.createElement('option'); node.value = option.value; node.textContent = option.label;
      select.appendChild(node);
    });
    select.value = value;
    wrap.appendChild(select); parent.appendChild(wrap); return select;
  }
  // Concurrent corners share a lookup, but retries after it settles fetch anew.
  function loadItems(source, success, failure) {
    var key = sourceKey(source), listener = { success: success, failure: failure };
    if (pending[key]) { pending[key].push(listener); return; }
    pending[key] = [listener];
    function finish(reply, failed) {
      var listeners = pending[key]; delete pending[key];
      listeners.forEach(function(l) {
        try { if (failed) l.failure(); else l.success(reply); }
        catch (e) { l.failure(); }
      });
    }
    try {
      Pebble.sendMessage('pkjs', { type: 'plugin-items', plugin: source.plugin,
        category: source.category, item: source.item }).then(function(reply) {
        finish(reply, false);
      }, function() { finish(null, true); });
    } catch (e) { finish(null, true); }
  }
  function fetchItems(slot, state) {
    if (!ready || state.loading || !state.source.multiple || state.source.unavailable) return;
    state.loading = true;
    if (state.retry) state.retry.style.display = 'none';
    state.note.textContent = 'Loading items…';
    loadItems(state.source, function(reply) {
      if (states[slot] !== state) return;
      state.loading = false;
      if (!reply || !Array.isArray(reply.items)) {
        state.note.textContent = reply && reply.error || 'Items unavailable. Your choice is retained.';
        if (state.retry) state.retry.style.display = ''; return;
      }
      state.itemWrap.textContent = '';
      var current = read(slot), options = [{ value: '', label: 'First available item' }].concat(reply.items);
      if (!current || sourceKey(current) !== sourceKey(state.source)) {
        state.note.textContent = 'Selection could not be read. Retry or choose a source again.';
        if (state.retry) state.retry.style.display = '';
        return;
      }
      if (current.instanceId && !reply.items.some(function(i) { return i.value === current.instanceId; })) {
        options.push({ value: current.instanceId, label: current.instanceId + (reply.truncated ? ' (not listed)' : ' (unavailable)') });
      }
      var item = control(state.itemWrap, state.source.category === 'weather' ? 'Location' : 'Item',
        options, current.instanceId || '');
      state.itemWrap.style.display = reply.items.length === 0 && !current.instanceId ? 'none' : '';
      item.onchange = function() { var value = read(slot); if (!value) { states[slot] = null; sync(slot); return; } value.instanceId = item.value || null; write(slot, value); };
      state.note.textContent = reply.truncated ? 'Showing the first 50 items. Any saved item outside this list is retained.' :
        reply.items.length ? '' : 'No items available. The fallback will be used.';
      if (state.retry) state.retry.style.display = reply.items.length ? 'none' : '';
    }, function() {
      if (states[slot] !== state) return;
      state.loading = false; state.note.textContent = 'Items unavailable. Your choice is retained.';
      if (state.retry) state.retry.style.display = '';
    });
  }
  function sync(slot) {
    var field = el('PluginSource' + slot), container = el('sourceControls' + slot), show = el('show' + slot);
    if (!field || !container || !show) return false;
    var index = Number(show.value) - 200, source = catalog[index];
    if (index < 0 || !source) {
      container.style.display = 'none'; states[slot] = null; write(slot, null); return false;
    }
    container.style.display = '';
    var old = read(slot);
    if (!old || sourceKey(old) !== sourceKey(source)) old = { plugin: source.plugin,
      category: source.category, item: source.item, property: (source.readings.filter(function(r) {
        return source.category === 'weather' && r.value === 'temperature' ||
          source.category === 'calendar' && source.item === 'event' && r.value === 'starts_at';
      })[0] ||
        source.readings.filter(function(r) { return r.value !== 'name'; })[0] || source.readings[0]).value, instanceId: null };
    write(slot, old);
    if (states[slot] && states[slot].source === source) return true;
    container.textContent = '';
    var choices = source.readings.slice();
    if (!choices.some(function(r) { return r.value === old.property; })) {
      choices.push({ value: old.property, label: old.property + ' (unavailable)' });
    }
    var reading = control(container, 'Reading', choices, old.property);
    if (choices.length === 1) reading.parentNode.style.display = 'none';
    reading.onchange = function() { var value = read(slot); if (!value) { states[slot] = null; sync(slot); return; } value.property = reading.value; write(slot, value); };
    var itemWrap = document.createElement('div'), note = document.createElement('p');
    container.appendChild(itemWrap); container.appendChild(note);
    if (source.multiple && !source.unavailable) {
      var retry = document.createElement('button'); retry.type = 'button'; retry.textContent = 'Retry loading items'; retry.style.display = 'none';
      retry.onclick = function() {
        var current = read(slot);
        if (!current || sourceKey(current) !== sourceKey(state.source)) {
          states[slot] = null; sync(slot); return;
        }
        fetchItems(slot, state);
      }; container.appendChild(retry);
    }
    var state = { source: source, itemWrap: itemWrap, note: note, retry: retry }; states[slot] = state;
    if (old.instanceId) note.textContent = 'Selected item: ' + old.instanceId;
    if (source.unavailable) note.textContent = 'Source unavailable. Your choice is retained; the fallback will be used.';
    else if (source.multiple) {
      if (!ready) note.textContent = 'Connect to Pebble to load item names. Your choice is retained.';
      fetchItems(slot, state);
    }
    return true;
  }
  if (typeof Pebble !== 'undefined' && typeof Pebble.sendMessage === 'function') {
    Pebble.addEventListener('ready', function(event) {
      if (event.target !== 'pkjs') return;
      ready = true; slots.forEach(function(slot) { if (states[slot]) fetchItems(slot, states[slot]); });
    });
  }
  return { sync: sync };
}
module.exports = install;
