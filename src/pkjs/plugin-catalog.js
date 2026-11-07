// Discovery reads declarations only. Item names are fetched on demand, after
// the user chooses a source, so merely opening settings cannot ring a phone or
// request access to every installed plugin.
var weatherFormat = require('./plugin-weather');
var direct = require('./plugin-complications').direct;
var weatherLabels = { temperature: 'Temperature', feels_like: 'Feels like (FL)', high: 'Today’s high (H)',
  low: 'Today’s low (L)', precipitation: 'Rain chance (R)', uv_index: 'UV index (UV)',
  humidity: 'Humidity (RH)', wind_speed: 'Wind speed (W)', sunrise: 'Sunrise (SR)', sunset: 'Sunset (SS)' };
var MAX_ITEMS = 50;
function readable(value) {
  return String(value).replace(/([a-z])([A-Z])/g, '$1 $2').replace(/[_-]+/g, ' ')
    .replace(/\b[a-z]/g, function(c) { return c.toUpperCase(); });
}
function catalog(Pebble) {
  if (typeof Pebble.enumeratePlugins !== 'function') return [];
  try {
    var plugins = Pebble.enumeratePlugins(), result = [];
    if (!Array.isArray(plugins)) return result;
    plugins.forEach(function(plugin) {
      if (!plugin || typeof plugin.uuid !== 'string' || !plugin.uuid || !Array.isArray(plugin.sources)) return;
      plugin.sources.forEach(function(source) {
        if (!source || !Array.isArray(source.items) || !source.properties) return;
        var readings = Object.keys(source.properties).filter(function(key) {
          var shapes = source.properties[key];
          return key.length > 0 && key.length <= 128 && Array.isArray(shapes) && shapes.some(function(shape) {
            return ['shortText', 'numericValue', 'timestamp', 'boolean'].indexOf(shape) >= 0;
          });
        });
        source.items.forEach(function(item) {
          // Subscribing to this source performs an action, not a passive read.
          if (source.category === 'phone' && item === 'find_my_phone') return;
          if (!readings.length || !direct({ plugin: plugin.uuid, category: source.category,
            item: item, property: readings[0] })) return;
          result.push({ plugin: plugin.uuid, category: source.category, item: item,
            label: String(plugin.name || readable(source.category)) + ' — ' + readable(item),
            readings: readings.map(function(key) {
              return { value: key, label: source.category === 'weather' && weatherLabels[key] || readable(key) };
            }),
            multiple: !!source.supportsMultiple });
          weatherFormat.addRange(source.category, item, source.properties, result[result.length - 1].readings);
        });
      });
    });
    var weather = result.filter(function(s) { return s.category === 'weather' && s.item === 'location'; });
    if (weather.length) {
      var readings = [];
      weather.forEach(function(s) { s.readings.forEach(function(r) {
        if (!readings.some(function(existing) { return existing.value === r.value; })) readings.push(r);
      }); });
      result.unshift({ plugin: null, category: 'weather', item: 'location',
        label: 'Weather (preferred provider)', readings: readings, multiple: true });
    }
    return result;
  } catch (e) { return []; }
}
function items(Pebble, request, schedule, cancel, callback) {
  var source = catalog(Pebble).filter(function(s) {
    return request && s.plugin === request.plugin && s.category === request.category && s.item === request.item;
  })[0];
  if (!source) { callback({ error: 'Source unavailable' }); return; }
  var finished = false, sub, timer;
  function finish(result) {
    if (finished) return;
    finished = true;
    if (timer != null) cancel(timer);
    if (sub) sub.unsubscribe();
    callback(result);
  }
  timer = schedule(function() { finish({ error: 'No items received. Your saved choice is retained.' }); }, 8000);
  try {
    sub = Pebble.subscribeToSource({ plugin: source.plugin, category: source.category, item: source.item,
      properties: ['name'], onData: function(data) {
        if (!data || !Array.isArray(data.instances)) { finish({ error: 'Items unavailable' }); return; }
        var result = [], seen = Object.create(null), truncated = false;
        for (var index = 0; index < data.instances.length; index++) {
          var instance = data.instances[index];
          if (!instance || typeof instance.instanceId !== 'string' || !instance.instanceId.length ||
              instance.instanceId.length > 128 || seen[instance.instanceId]) continue;
          if (result.length === MAX_ITEMS) { truncated = true; break; }
          seen[instance.instanceId] = true;
          var name = instance.properties && instance.properties.name;
          var text = name && ((name.shortText && name.shortText.text) || (name.longText && name.longText.text));
          result.push({ value: instance.instanceId,
            label: typeof text === 'string' ? text.slice(0, 128) : 'Item ' + (index + 1) });
        }
        finish({ items: result, truncated: truncated });
      }, onError: function() { finish({ error: 'Items unavailable. Check plugin permissions in Pebble.' }); }
    });
    if (finished && sub) sub.unsubscribe();
  } catch (e) { finish({ error: 'Source unavailable' }); }
}
module.exports = { catalog: catalog, items: items };
