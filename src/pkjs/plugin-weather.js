// Display conventions owned by the watchface, not provider property names.
// This synthetic choice expands to two real properties in one subscription.
var RANGE = '__minute_blocks_temperature_range';
var MARKERS = { humidity: 'RH', precipitation: 'R', feels_like: 'FL',
  high: 'H', low: 'L', uv_index: 'UV', wind_speed: 'W', sunrise: 'SR', sunset: 'SS' };
function weather(category, item) { return category === 'weather' && item === 'location'; }
function properties(category, item, property) {
  return weather(category, item) && property === RANGE ? ['low', 'high'] : [property];
}
function range(values) {
  var low = values && values.low && values.low.numericValue;
  var high = values && values.high && values.high.numericValue;
  if (!low || !high || typeof low.value !== 'number' || typeof high.value !== 'number' ||
      !isFinite(low.value) || !isFinite(high.value) || low.value > high.value ||
      low.unit !== high.unit || ['°C', '°F', 'C', 'F'].indexOf(low.unit) < 0) return '';
  var text = Math.round(low.value) + '-' + Math.round(high.value) + low.unit.replace('°', '');
  return text.length <= 7 ? text : '';
}
function windText(property, textFor) {
  if (!property) return '';
  var raw = property.shortText && property.shortText.text;
  // Prefer the provider's display units: its numeric shape can use other units.
  var match = typeof raw === 'string' && raw.match(/^(-?\d+(?:\.\d+)?)\s*(mph|km\/h|m\/s|knots|kt|kts)$/i);
  var numeric = property.numericValue;
  var number = match ? Number(match[1]) : numeric && numeric.value;
  var unit = match ? match[2].toLowerCase() : numeric && numeric.unit;
  if (typeof number !== 'number' || !isFinite(number) ||
      ['mph', 'km/h', 'm/s', 'knots', 'kt', 'kts'].indexOf(unit) < 0) return textFor(property);
  if (unit === 'knots' || unit === 'kts') unit = 'kt';
  var value = String(Math.round(number * 10) / 10) + unit;
  if (value.length + 1 > 7) value = String(Math.round(number)) + unit;
  // A speed unit identifies the reading even if there is no room for W.
  return value.length + 1 <= 7 ? 'W' + value : value.length <= 7 ? value : '';
}
function text(category, item, property, values, textFor) {
  if (weather(category, item) && property === RANGE) return range(values);
  if (weather(category, item) && property === 'wind_speed') return windText(values && values[property], textFor);
  var value = textFor(values && values[property]);
  var marker = weather(category, item) && MARKERS[property];
  if (!value || !marker) return value;
  // Honor an already-marked provider string; never truncate a number to fit.
  var result = value.indexOf(marker) === 0 ? value : marker + value;
  return result.length <= 7 ? result : '';
}
function addRange(category, item, declared, readings) {
  if (!weather(category, item) || !['low', 'high'].every(function(key) {
    return Array.isArray(declared[key]) && declared[key].indexOf('numericValue') >= 0;
  })) return;
  readings.push({ value: RANGE, label: 'Today’s range (55-72F)' });
}
module.exports = { RANGE: RANGE, properties: properties, text: text, addRange: addRange };
