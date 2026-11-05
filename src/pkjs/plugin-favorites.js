// Read only preference metadata for the settings page; do not subscribe to
// favorite data sources or trigger their permissions just to populate labels.
var binding = require('./plugin-complications').binding;
function readable(identifier) {
  return identifier.replace(/([a-z])([A-Z])/g, '$1 $2').replace(/[_-]+/g, ' ')
    .replace(/[\x00-\x1f\x7f]/g, '').replace(/\s+/g, ' ').trim().slice(0, 48)
    .replace(/\b[a-z]/g, function(letter) { return letter.toUpperCase(); });
}
function labels(entries) {
  var result = [];
  for (var i = 0; i < 5; i++) {
    var target = entries && binding(entries[i]);
    var description = !entries ? '' : !entries[i] ? 'Not configured' :
      target ? readable(target[0]) + ' / ' + readable(target[1]) + ' ' + (target[3] + 1) +
        ' / ' + readable(target[2]) : 'Unavailable';
    result.push('Favorite ' + (i + 1) + (description ? ' — ' + description : ''));
  }
  return result;
}
function load(Pebble, schedule, cancel, callback) {
  var finished = false, subscription, timer;
  function finish(entries) {
    if (finished) return;
    finished = true;
    if (timer != null) cancel(timer);
    if (subscription) subscription.unsubscribe();
    callback(labels(entries), entries !== null);
  }
  if (typeof Pebble.subscribeToSource !== 'function') { finish(null); return; }
  timer = schedule(function() { finish(null); }, 750);
  try {
    subscription = Pebble.subscribeToSource({ category: 'preferences', item: 'complications',
      onData: function(envelope) {
        finish(envelope && Array.isArray(envelope.instances) ? envelope.instances : null);
      },
      onError: function() { finish(null); }
    });
    // Some companions answer synchronously, before the handle is assigned.
    if (finished && subscription) subscription.unsubscribe();
  } catch (e) { finish(null); }
}
module.exports = { load: load, labels: labels };
