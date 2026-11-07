// Optional consumer of the phone's preferred complications. Built-in readings
// stay selected on the watch and are used whenever a plugin reading is absent.
var weatherFormat = require('./plugin-weather');
var SLOT_KEYS = ['TopLeft', 'TopRight', 'BottomRight', 'BottomLeft'];
var MAX_AGE_SEC = 900;

// Phone-only binding. A null instanceId follows the first item; explicit IDs
// survive reordering and never silently switch to a different location/account.
function direct(value) {
  if (!value || typeof value !== 'object') return null;
  function id(v) { return typeof v === 'string' && v.length > 0 && v.length <= 128; }
  if (![value.category, value.item, value.property].every(id) ||
      (value.plugin != null && !id(value.plugin)) ||
      (value.instanceId != null && !id(value.instanceId))) return null;
  return { plugin: value.plugin || null, category: value.category, item: value.item,
    property: value.property, instanceId: value.instanceId == null ? null : value.instanceId };
}
function selection(settings, slot) {
  return direct(settings['PluginSource' + SLOT_KEYS[slot]]) ? 6 : favorite(settings, slot);
}

function textFor(property) {
  if (!property || typeof property !== 'object') return '';
  var text = property.shortText && property.shortText.text;
  if (typeof text !== 'string') {
    var stamp = property.timestamp && property.timestamp.value;
    if (typeof stamp === 'number' && isFinite(stamp)) {
      var date = new Date(stamp * 1000);
      if (!isFinite(date.getTime())) return '';
      return ('0' + date.getHours()).slice(-2) + ':' + ('0' + date.getMinutes()).slice(-2);
    }
    if (property.boolean && typeof property.boolean.value === 'boolean') return property.boolean.value ? 'ON' : 'OFF';
    var number = property.numericValue;
    if (!number || typeof number.value !== 'number' || !isFinite(number.value)) return '';
    var rounded = Math.round(number.value * 10) / 10;
    if (!isFinite(rounded)) return '';
    text = String(rounded) +
      (typeof number.unit === 'string' ? number.unit : '');
  }
  // Visitor is an ASCII font. Remove degree signs, normalize dashes and spaces,
  // and never split UTF-8 bytes in the watch's bounded string buffer.
  text = text.replace(/\u00b0/g, '').replace(/[\u2010-\u2015\u2212]/g, '-')
    .replace(/\s+/g, ' ').replace(/[^\x20-\x7e]/g, '').trim();
  // A clipped number could silently change its magnitude. Fall back instead.
  return text.length <= 7 ? text : '';
}

function favorite(settings, slot) {
  var value = settings['PluginFavorite' + SLOT_KEYS[slot]];
  return typeof value === 'number' && value >= 1 && value <= 5 && value % 1 === 0 ? value : 0;
}

function binding(entry) {
  var p = entry && entry.properties;
  function text(key) { return p && p[key] && p[key].shortText && p[key].shortText.text; }
  var result = [text('category'), text('item'), text('property'),
    p && p.index && p.index.numericValue && p.index.numericValue.value];
  if (!result.slice(0, 3).every(function(s) {
    return typeof s === 'string' && s.length > 0 && s.length <= 128;
  }) || typeof result[3] !== 'number' || result[3] < 0 || result[3] % 1 !== 0) return null;
  return result;
}

function create(Pebble, send, now, schedule, cancel) {
  var selections = [], directs = [], slots = [], preferences, session = 0, apiAvailable;
  var delivered = [], revisions = [], renewals = [], diagnostics = [];
  function report(slot, reason) {
    if (diagnostics[slot] === reason) return;
    diagnostics[slot] = reason;
    // No readings, event names, location IDs or provider error messages in logs.
    console.log('Plugin slot ' + slot + ': ' + reason);
  }
  var delays = [60000, 300000, 900000];
  function stop(state) {
    if (!state) return;
    state.active = false;
    if (state.timer != null && cancel) cancel(state.timer);
    if (state.sub) state.sub.unsubscribe();
  }
  // Errors do not end SDK subscriptions. Keep listening for recovery, while
  // retrying only this source. After the initial attempts, probe every half hour
  // independently of watch weather requests, without resetting healthy sources.
  function subscribe(options, onData, onError) {
    var state = { active: true, attempts: 0, timer: null, generation: 0 };
    function recovered() {
      if (state.timer != null && cancel) cancel(state.timer);
      state.timer = null;
      state.attempts = 0;
    }
    function connect() {
      var generation = ++state.generation;
      if (state.sub) state.sub.unsubscribe();
      state.sub = null;
      function current() { return state.active && generation === state.generation; }
      function failed(error) {
        if (!current()) return;
        onError(error);
        if (!schedule || state.timer != null) return;
        var attempt = ++state.attempts;
        state.timer = schedule(function() {
          if (!current() || state.timer == null || state.attempts !== attempt) return;
          state.timer = null;
          connect();
        }, delays[attempt - 1] || 1800000);
      }
      var config = {};
      Object.keys(options).forEach(function(key) { config[key] = options[key]; });
      config.onData = function(data) { if (current()) { recovered(); onData(data); } };
      config.onError = failed;
      try { state.sub = Pebble.subscribeToSource(config); } catch (e) { failed(); }
    }
    connect();
    return state;
  }
  function cancelRenewal(slot) {
    var renewal = renewals[slot];
    if (renewal && cancel) cancel(renewal.timer);
    renewals[slot] = null;
  }
  function publish(slot, text, ttl, deadline) {
    if (ttl <= 0) text = '';
    var previous = delivered[slot];
    var expires = deadline === undefined ? now() + ttl * 1000 : deadline;
    var generation = session;
    // Coalesce extensions until the acknowledged lifetime is nearly over.
    // Retain the latest absolute deadline: a quiet provider must still renew,
    // and firing the timer must never grant a fresh lifetime to old data.
    if (schedule && previous && text && previous.text === text &&
        expires > previous.expires && previous.expires - now() > 30000) {
      var renewal = renewals[slot];
      if (renewal && renewal.base === previous.expires) {
        renewal.expires = expires;
        return;
      }
      cancelRenewal(slot);
      renewal = { base: previous.expires, expires: expires };
      renewals[slot] = renewal;
      renewal.timer = schedule(function() {
        if (generation !== session || renewals[slot] !== renewal) return;
        renewals[slot] = null;
        var remaining = Math.max(0, Math.floor((renewal.expires - now()) / 1000));
        publish(slot, text, remaining, renewal.expires);
      }, previous.expires - now() - 30000);
      return;
    }
    // Changes, shortened validity, failures and removals supersede a renewal.
    cancelRenewal(slot);
    if (previous && previous.text === text && (!text || previous.expires === expires)) return;
    var revision = (revisions[slot] || 0) + 1;
    revisions[slot] = revision; delivered[slot] = null;
    send({ PluginSlot: slot, PluginFavorite: selections[slot], PluginText: text,
      PluginTTL: ttl }, 'plugin-' + slot, {
      expiresAt: expires,
      onDelivered: function(message) {
        if (generation === session && revisions[slot] === revision) delivered[slot] = {
          text: message.PluginText, expires: expires
        };
      }
    });
  }
  function update(entries) {
    selections.forEach(function(selected, slot) {
      var chosen = directs[slot];
      var target = chosen ? [chosen.category, chosen.item, chosen.property, chosen.instanceId, chosen.plugin] :
        selected ? binding(entries[selected - 1]) : null;
      var signature = JSON.stringify(target);
      if (slots[slot] && slots[slot].signature === signature) return;
      if (slots[slot]) stop(slots[slot].subscription);
      slots[slot] = { signature: signature };
      if (!selected) return;
      publish(slot, '', 0);
      if (!target) return;
      slots[slot].subscription = subscribe({ category: target[0], item: target[1],
        plugin: chosen ? chosen.plugin : undefined, properties: weatherFormat.properties(target[0], target[1], target[2]), maxStalenessMs: 60000 }, function(data) {
        if (chosen && chosen.plugin && data && data.pluginUuid && data.pluginUuid !== chosen.plugin) {
          publish(slot, '', 0); return;
        }
        var instances = data && Array.isArray(data.instances) ? data.instances : [];
        var instance = chosen ? (chosen.instanceId == null ? instances[0] : instances.filter(function(i) {
          return i && i.instanceId === chosen.instanceId;
        })[0]) : instances[target[3]];
        var text = weatherFormat.text(target[0], target[1], target[2], instance && instance.properties, textFor);
        var expires = data && data.validUntilMs;
        var observedAt = now();
        var deadline = typeof expires === 'number' && isFinite(expires) ?
          Math.min(expires, observedAt + MAX_AGE_SEC * 1000) :
          expires == null ? observedAt + 60000 : observedAt;
        var ttl = Math.max(0, Math.floor((deadline - observedAt) / 1000));
        report(slot, !instance ? 'item unavailable' : !text ? 'reading missing or too long' :
          ttl <= 0 ? 'reading expired' : 'reading ready');
        publish(slot, text, text ? ttl : 0, text && ttl > 0 ? deadline : undefined);
      }, function(error) {
        var code = error && error.code;
        var known = ['PERMISSION_DENIED', 'PLUGIN_UNAVAILABLE', 'TIMEOUT', 'INVALID_REQUEST', 'RATE_LIMITED'];
        report(slot, known.indexOf(code) >= 0 ? code : 'source error');
        publish(slot, '', 0);
      });
    });
  }
  function available() { return typeof Pebble.subscribeToSource === 'function'; }
  function start(settings) {
    var next = SLOT_KEYS.map(function(_, slot) { return selection(settings, slot); });
    var nextDirects = SLOT_KEYS.map(function(slot) { return direct(settings['PluginSource' + slot]); });
    var supported = available();
    // Weather refreshes and unrelated settings saves must not blank live slots.
    // Failed sources have their own timers, including after the fast retry budget.
    if (apiAvailable === supported && next.every(function(value, slot) {
      return value === selections[slot] && JSON.stringify(nextDirects[slot]) === JSON.stringify(directs[slot]);
    })) return;
    apiAvailable = supported;
    session++;
    renewals.forEach(function(_, slot) { cancelRenewal(slot); });
    stop(preferences);
    slots.forEach(function(slot) { stop(slot.subscription); });
    slots = []; delivered = []; diagnostics = [];
    selections = next; directs = nextDirects;
    selections.forEach(function(value, slot) { if (value) publish(slot, '', 0); });
    if (!available() || !selections.some(function(value) { return value > 0; })) return;
    if (directs.some(function(value) { return !!value; })) update([]);
    if (!selections.some(function(value) { return value >= 1 && value <= 5; })) return;
    preferences = subscribe({ category: 'preferences', item: 'complications', maxStalenessMs: 60000 },
      function(envelope) { update(envelope && Array.isArray(envelope.instances) ? envelope.instances : []); },
      function() { update([]); });
  }
  return { available: available, start: start };
}

module.exports = { create: create, textFor: textFor, favorite: favorite, binding: binding, direct: direct, selection: selection };
