// Optional consumer of the phone's preferred complications. Built-in readings
// stay selected on the watch and are used whenever a plugin reading is absent.
var SLOT_KEYS = ['TopLeft', 'TopRight', 'BottomRight', 'BottomLeft'];
var MAX_AGE_SEC = 900;

function textFor(property) {
  if (!property || typeof property !== 'object') return '';
  var text = property.shortText && property.shortText.text;
  if (typeof text !== 'string') {
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

function create(Pebble, send, now, schedule, cancel) {
  var retryTimer;
  var epoch = 0;
  var preferenceEpoch = 0;
  var preferenceSubscription;
  var dataSubscriptions = [];
  var selections = [];
  var bindingSignature;
  function stopData() {
    epoch++;
    dataSubscriptions.forEach(function(sub) { if (sub) sub.unsubscribe(); });
    dataSubscriptions = [];
  }
  function publish(slot, text, ttl) {
    send({ PluginSlot: slot, PluginFavorite: selections[slot], PluginText: text,
      PluginTTL: ttl }, 'plugin-' + slot);
  }
  function clear() {
    selections.forEach(function(value, slot) { if (value) publish(slot, '', 0); });
  }
  function available() { return typeof Pebble.subscribeToSource === 'function'; }
  function start(settings) {
    var session = ++preferenceEpoch;
    if (retryTimer && cancel) cancel(retryTimer);
    retryTimer = null;
    function retry() {
      if (!schedule || retryTimer) return;
      retryTimer = schedule(function() {
        retryTimer = null;
        if (session === preferenceEpoch) start(settings);
      }, 60000);
    }
    if (preferenceSubscription) preferenceSubscription.unsubscribe();
    preferenceSubscription = null;
    stopData();
    bindingSignature = null;
    selections = SLOT_KEYS.map(function(_, slot) { return favorite(settings, slot); });
    clear();
    if (!available() || !selections.some(function(value) { return value > 0; })) return;
    try {
      preferenceSubscription = Pebble.subscribeToSource({
        category: 'preferences', item: 'complications', maxStalenessMs: 60000,
        onData: function(envelope) {
          if (session !== preferenceEpoch) return;
          var entries = envelope && Array.isArray(envelope.instances) ? envelope.instances : [];
          var signature = JSON.stringify(entries);
          if (signature === bindingSignature) return;
          bindingSignature = signature;
          stopData();
          clear();
          var generation = epoch;
          selections.forEach(function(selected, slot) {
            if (!selected) return;
            var p = entries[selected - 1] && entries[selected - 1].properties;
            var category = p && p.category && p.category.shortText && p.category.shortText.text;
            var item = p && p.item && p.item.shortText && p.item.shortText.text;
            var property = p && p.property && p.property.shortText && p.property.shortText.text;
            var index = p && p.index && p.index.numericValue && p.index.numericValue.value;
            if (![category, item, property].every(function(s) {
              return typeof s === 'string' && s.length > 0 && s.length <= 128;
            }) || typeof index !== 'number' || index < 0 || index % 1 !== 0) return;
            try {
              var sub = Pebble.subscribeToSource({
                category: category, item: item, properties: [property], maxStalenessMs: 60000,
                onData: function(data) {
                  if (generation !== epoch) return;
                  var instance = data && Array.isArray(data.instances) && data.instances[index];
                  var value = instance && instance.properties && instance.properties[property];
                  var text = textFor(value);
                  var expires = data && data.validUntilMs;
                  var ttl = typeof expires === 'number' && isFinite(expires) ?
                    Math.max(0, Math.min(MAX_AGE_SEC, Math.floor((expires - now()) / 1000))) :
                    expires === undefined ? 60 : 0;
                  publish(slot, text, text ? ttl : 0);
                },
                onError: function() {
                  if (generation === epoch) { publish(slot, '', 0); retry(); }
                }
              });
              dataSubscriptions.push(sub);
            } catch (e) { publish(slot, '', 0); retry(); }
          });
        },
        onError: function() {
          if (session !== preferenceEpoch) return;
          stopData(); bindingSignature = null; clear(); retry();
        }
      });
    } catch (e) { clear(); retry(); }
  }
  // A preferences update replaces data subscriptions but belongs to the same
  // preferences session. A settings change invalidates both kinds of callback.
  return { available: available, start: start };
}

module.exports = { create: create, textFor: textFor, favorite: favorite };
