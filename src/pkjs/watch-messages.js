// AppMessage permits one outstanding send. Coalesce pending readings by slot,
// but preserve settings-before-data ordering and bound retries on disconnect.
module.exports = function(Pebble, schedule) {
  var pending = [];
  var busy = false;
  function pump() {
    if (busy || !pending.length) return;
    busy = true;
    var entry = pending.shift();
    function done() { busy = false; pump(); }
    try {
      Pebble.sendAppMessage(entry.message, done, function() {
        if (entry.attempts++ < 2 && !pending.some(function(item) { return item.key === entry.key; })) {
          pending.unshift(entry);
          schedule(function() { busy = false; pump(); }, 1000);
        } else {
          done();
        }
      });
    } catch (e) { done(); }
  }
  return function(message, key) {
    pending = pending.filter(function(entry) { return entry.key !== key; });
    pending.push({ message: message, key: key, attempts: 0 });
    pump();
  };
};
