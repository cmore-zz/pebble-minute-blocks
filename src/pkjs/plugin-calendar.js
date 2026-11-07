// Calendar timestamps use the phone's local timezone and the face's clock style.
function matches(category, item) { return category === 'calendar' && item === 'event'; }
function isTime(property) { return property === 'starts_at' || property === 'ends_at'; }
function properties(property) { return isTime(property) ? [property, 'all_day'] : [property]; }
function text(property, values, is24Hour, textFor) {
  var value = values && values[property];
  if (!value) return '';
  if (property === 'name') {
    var raw = value.shortText && value.shortText.text;
    if (typeof raw !== 'string') raw = value.longText && value.longText.text;
    if (typeof raw !== 'string') return '';
    var normalized = raw.replace(/[\u2010-\u2015\u2212]/g, '-')
      .replace(/\s+/g, ' ').replace(/[^\x20-\x7e]/g, '').trim();
    return normalized.length > 7 ? normalized.slice(0, 6) + '>' : normalized;
  }
  if (isTime(property)) {
    if (values.all_day && values.all_day.boolean && values.all_day.boolean.value === true) return 'All day';
    // Wait for the watch's authoritative clock style instead of flashing 24h.
    if (typeof is24Hour !== 'boolean') return '';
    var stamp = value.timestamp && value.timestamp.value;
    if (typeof is24Hour === 'boolean' && typeof stamp === 'number' && isFinite(stamp)) {
      var date = new Date(stamp * 1000);
      if (!isFinite(date.getTime())) return '';
      var hour = date.getHours();
      return (is24Hour ? ('0' + hour).slice(-2) : String(hour % 12 || 12)) + ':' +
        ('0' + date.getMinutes()).slice(-2) + (is24Hour ? '' : hour < 12 ? 'A' : 'P');
    }
  }
  return textFor(value);
}
module.exports = { matches: matches, properties: properties, text: text };
