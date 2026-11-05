// Serialize AppMessages and coalesce pending entries by key. Settings recovery
// runs independently of weather and never blocks unrelated traffic while waiting.
module.exports = function(Pebble, schedule, now) {
  now = now || Date.now;
  var pending = [], latest = {}, busy = false, settingsReady = true;
  function pump() {
    if (busy || !pending.length) return;
    var index = -1;
    for (var i = 0; i < pending.length; i++) {
      if (settingsReady || pending[i].key.indexOf('plugin-') !== 0 || pending[i].key === 'plugin-settings') {
        index = i; break;
      }
    }
    if (index < 0) return;
    busy = true;
    var entry = pending.splice(index, 1)[0], message = {};
    Object.keys(entry.message).forEach(function(key) { message[key] = entry.message[key]; });
    if (entry.options.expiresAt !== undefined) {
      message.PluginTTL = Math.max(0, Math.min(900, Math.floor((entry.options.expiresAt - now()) / 1000)));
      if (!message.PluginTTL) message.PluginText = '';
    }
    function done() { busy = false; pump(); }
    function failed() {
      entry.attempts++;
      if (latest[entry.key] !== entry) { done(); return; }
      if (entry.attempts <= 2) {
        pending.unshift(entry);
        schedule(function() { busy = false; pump(); }, 1000);
      } else {
        if (entry.key === 'plugin-settings') {
          var delay = [60000, 300000, 900000][Math.min(entry.attempts - 3, 2)];
          schedule(function() {
            if (latest[entry.key] !== entry) return;
            pending.push(entry); pump();
          }, delay);
        }
        done();
      }
    }
    try {
      Pebble.sendAppMessage(message, function() {
        if (entry.key === 'plugin-settings' && latest[entry.key] === entry) settingsReady = true;
        if (entry.options.onDelivered) entry.options.onDelivered(message);
        done();
      }, failed);
    } catch (e) { failed(); }
  }
  return function(message, key, options) {
    pending = pending.filter(function(entry) { return entry.key !== key; });
    var entry = { message: message, key: key, options: options || {}, attempts: 0 };
    latest[key] = entry;
    if (key === 'plugin-settings') settingsReady = false;
    pending.push(entry);
    pump();
  };
};
