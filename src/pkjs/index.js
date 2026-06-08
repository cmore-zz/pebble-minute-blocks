var SETTINGS_KEY = "pebble-minute-blocks-settings";

var DEFAULT_SETTINGS = {
  BackgroundColor: 0x000000,
  RingColor: 0xFFFFFF,
  ComplicationColor: 0xFFFFFF,
  HourColor: 0xFFFFFF,
  TimeMode: 0,
  WeatherEnabled: 1,
  WeatherUnits: 0
};

function cloneDefaults() {
  return {
    BackgroundColor: DEFAULT_SETTINGS.BackgroundColor,
    RingColor: DEFAULT_SETTINGS.RingColor,
    ComplicationColor: DEFAULT_SETTINGS.ComplicationColor,
    HourColor: DEFAULT_SETTINGS.HourColor,
    TimeMode: DEFAULT_SETTINGS.TimeMode,
    WeatherEnabled: DEFAULT_SETTINGS.WeatherEnabled,
    WeatherUnits: DEFAULT_SETTINGS.WeatherUnits
  };
}

function loadSettings() {
  var settings = cloneDefaults();
  var saved = localStorage.getItem(SETTINGS_KEY);

  if (saved) {
    try {
      var parsed = JSON.parse(saved);
      Object.keys(settings).forEach(function(key) {
        if (parsed[key] !== undefined) {
          settings[key] = parsed[key];
        }
      });
    } catch (e) {
      console.log("Could not parse saved settings: " + e.message);
    }
  }

  return settings;
}

function saveSettings(settings) {
  localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings));
}

function hexFromNumber(value) {
  var hex = Number(value).toString(16);
  while (hex.length < 6) {
    hex = "0" + hex;
  }
  return "#" + hex.slice(-6);
}

function numberFromHex(value) {
  return parseInt(String(value).replace("#", ""), 16);
}

function sendSettings(settings) {
  Pebble.sendAppMessage(settings, function() {
    console.log("Settings sent");
  }, function() {
    console.log("Settings send failed");
  });
}

function sendWeatherUnavailable() {
  Pebble.sendAppMessage({
    WeatherAvailable: 0
  });
}

function getWeather(settings) {
  if (!settings.WeatherEnabled) {
    return;
  }

  navigator.geolocation.getCurrentPosition(function(pos) {
    var unit = settings.WeatherUnits === 1 ? "celsius" : "fahrenheit";
    var url = "https://api.open-meteo.com/v1/forecast" +
      "?latitude=" + encodeURIComponent(pos.coords.latitude) +
      "&longitude=" + encodeURIComponent(pos.coords.longitude) +
      "&current=temperature_2m" +
      "&temperature_unit=" + unit;
    var req = new XMLHttpRequest();

    req.onload = function() {
      try {
        var data = JSON.parse(req.responseText);
        var temp = Math.round(data.current.temperature_2m);
        Pebble.sendAppMessage({
          WeatherTemp: temp,
          WeatherAvailable: 1
        });
      } catch (e) {
        console.log("Weather parse failed: " + e.message);
        sendWeatherUnavailable();
      }
    };

    req.onerror = function() {
      console.log("Weather request failed");
      sendWeatherUnavailable();
    };

    req.open("GET", url);
    req.send();
  }, function(err) {
    console.log("Location failed: " + err.message);
    sendWeatherUnavailable();
  }, {
    timeout: 15000,
    maximumAge: 30 * 60 * 1000
  });
}

function field(label, id, value) {
  return [
    "<label for=\"" + id + "\">" + label + "</label>",
    "<div class=\"color-row\">",
    "<span class=\"color-preview\" id=\"" + id + "Preview\" style=\"background:" + hexFromNumber(value) + "\"></span>",
    "<input id=\"" + id + "\" type=\"color\" value=\"" + hexFromNumber(value) + "\">",
    "</div>",
  ].join("");
}

function buildConfigHtml(settings) {
  return [
    "<!doctype html>",
    "<html>",
    "<head>",
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">",
    "<style>",
    "body{margin:0;padding:18px;font-family:-apple-system,BlinkMacSystemFont,Helvetica,Arial,sans-serif;background:#111;color:#eee;}",
    "h1{font-size:20px;margin:0 0 18px;}",
    "label{display:block;font-weight:700;margin:14px 0 8px;}",
    "input,select,button{box-sizing:border-box;font-size:18px;}",
    "select{height:44px;background:#222;color:#eee;border:1px solid #555;padding:0 10px;}",
    ".color-row{position:relative;width:44px;height:44px;}",
    ".color-preview{display:block;width:40px;height:40px;border:2px solid #777;border-radius:0;}",
    "input[type=color]{position:absolute;inset:0;width:44px;height:44px;opacity:0;}",
    "#save{height:48px;margin-top:22px;background:#ddd;color:#111;border:0;font-weight:700;}",
    "</style>",
    "</head>",
    "<body>",
    "<h1>Minute Blocks</h1>",
    field("Background", "background", settings.BackgroundColor),
    field("Ring", "ring", settings.RingColor),
    field("Complications", "complication", settings.ComplicationColor),
    field("Center digits", "hour", settings.HourColor),
    "<label for=\"timeMode\">Time digits</label>",
    "<select id=\"timeMode\">",
    "<option value=\"0\"" + (settings.TimeMode === 0 ? " selected" : "") + ">Watch setting</option>",
    "<option value=\"1\"" + (settings.TimeMode === 1 ? " selected" : "") + ">12 hour</option>",
    "<option value=\"2\"" + (settings.TimeMode === 2 ? " selected" : "") + ">24 hour</option>",
    "</select>",
    "<label><input id=\"weatherEnabled\" type=\"checkbox\"" + (settings.WeatherEnabled ? " checked" : "") + "> Weather</label>",
    "<label for=\"weatherUnits\">Weather units</label>",
    "<select id=\"weatherUnits\">",
    "<option value=\"0\"" + (settings.WeatherUnits === 0 ? " selected" : "") + ">Fahrenheit</option>",
    "<option value=\"1\"" + (settings.WeatherUnits === 1 ? " selected" : "") + ">Celsius</option>",
    "</select>",
    "<button id=\"save\">Save</button>",
    "<script>",
    "['background','ring','complication','hour'].forEach(function(id){",
    "var input=document.getElementById(id);",
    "var preview=document.getElementById(id+'Preview');",
    "input.oninput=function(){preview.style.background=input.value;};",
    "input.onchange=input.oninput;",
    "});",
    "document.getElementById('save').onclick=function(){",
    "var settings={",
    "BackgroundColor:parseInt(document.getElementById('background').value.slice(1),16),",
    "RingColor:parseInt(document.getElementById('ring').value.slice(1),16),",
    "ComplicationColor:parseInt(document.getElementById('complication').value.slice(1),16),",
    "HourColor:parseInt(document.getElementById('hour').value.slice(1),16),",
    "TimeMode:parseInt(document.getElementById('timeMode').value,10),",
    "WeatherEnabled:document.getElementById('weatherEnabled').checked?1:0,",
    "WeatherUnits:parseInt(document.getElementById('weatherUnits').value,10)",
    "};",
    "document.location='pebblejs://close#'+encodeURIComponent(JSON.stringify(settings));",
    "};",
    "</script>",
    "</body>",
    "</html>"
  ].join("");
}

Pebble.addEventListener("ready", function() {
  var settings = loadSettings();
  sendSettings(settings);
  getWeather(settings);
});

Pebble.addEventListener("showConfiguration", function() {
  Pebble.openURL("data:text/html," + encodeURIComponent(buildConfigHtml(loadSettings())));
});

Pebble.addEventListener("webviewclosed", function(e) {
  if (!e.response) {
    return;
  }

  try {
    var settings = JSON.parse(decodeURIComponent(e.response));
    settings.BackgroundColor = numberFromHex(hexFromNumber(settings.BackgroundColor));
    settings.RingColor = numberFromHex(hexFromNumber(settings.RingColor));
    settings.ComplicationColor = numberFromHex(hexFromNumber(settings.ComplicationColor));
    settings.HourColor = numberFromHex(hexFromNumber(settings.HourColor));
    settings.TimeMode = Number(settings.TimeMode);
    settings.WeatherEnabled = settings.WeatherEnabled ? 1 : 0;
    settings.WeatherUnits = Number(settings.WeatherUnits);
    saveSettings(settings);
    sendSettings(settings);
    getWeather(settings);
  } catch (err) {
    console.log("Could not apply settings: " + err.message);
  }
});

Pebble.addEventListener("appmessage", function(e) {
  if (e.payload && e.payload.WeatherRequest) {
    getWeather(loadSettings());
  }
});
