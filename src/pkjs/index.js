var SETTINGS_KEY = "pebble-minute-blocks-settings";

var DEFAULT_SETTINGS = {
  BackgroundColor: 0x000000,
  RingColor: 0xFFFFFF,
  ComplicationColor: 0xFFFFFF,
  HourColor: 0x00FFFF,
  TimeMode: 0,
  ComplicationSize: 0,
  ComplicationVisibility: 1,
  SecondsVisibility: 2,
  ComplicationTopLeft: 1,
  ComplicationTopRight: 2,
  ComplicationBottomRight: 4,
  ComplicationBottomLeft: 5,
  WeatherEnabled: 1,
  WeatherUnits: 0
};

var COLOR_PRESETS = [
  {
    name: "Default",
    BackgroundColor: 0x000000,
    RingColor: 0xFFFFFF,
    ComplicationColor: 0xFFFFFF,
    HourColor: 0x00FFFF
  },
  {
    name: "Mono",
    BackgroundColor: 0x000000,
    RingColor: 0xFFFFFF,
    ComplicationColor: 0xFFFFFF,
    HourColor: 0xFFFFFF
  },
  {
    name: "Inverted",
    BackgroundColor: 0xFFFFFF,
    RingColor: 0x000000,
    ComplicationColor: 0x000000,
    HourColor: 0x000000
  },
  {
    name: "Amber",
    BackgroundColor: 0x000000,
    RingColor: 0xFFFFFF,
    ComplicationColor: 0xFFFFFF,
    HourColor: 0xFFAA00
  },
  {
    name: "Green",
    BackgroundColor: 0x000000,
    RingColor: 0xFFFFFF,
    ComplicationColor: 0xFFFFFF,
    HourColor: 0x00FF00
  },
  {
    name: "Red",
    BackgroundColor: 0x000000,
    RingColor: 0xFFFFFF,
    ComplicationColor: 0xFFFFFF,
    HourColor: 0xFF0000
  },
  {
    name: "Blue",
    BackgroundColor: 0x000000,
    RingColor: 0xFFFFFF,
    ComplicationColor: 0xFFFFFF,
    HourColor: 0x0055FF
  }
];

var BW_COLOR_PRESETS = [
  COLOR_PRESETS[0],
  COLOR_PRESETS[2]
];

var COMPLICATION_OPTIONS = [
  { value: 0, label: "None" },
  { value: 1, label: "Date" },
  { value: 2, label: "Current temp" },
  { value: 3, label: "Forecast range" },
  { value: 4, label: "Battery" },
  { value: 5, label: "Bluetooth" },
  { value: 6, label: "Steps" }
];

function cloneDefaults() {
  return {
    BackgroundColor: DEFAULT_SETTINGS.BackgroundColor,
    RingColor: DEFAULT_SETTINGS.RingColor,
    ComplicationColor: DEFAULT_SETTINGS.ComplicationColor,
    HourColor: DEFAULT_SETTINGS.HourColor,
    TimeMode: DEFAULT_SETTINGS.TimeMode,
    ComplicationSize: DEFAULT_SETTINGS.ComplicationSize,
    ComplicationVisibility: DEFAULT_SETTINGS.ComplicationVisibility,
    SecondsVisibility: DEFAULT_SETTINGS.SecondsVisibility,
    ComplicationTopLeft: DEFAULT_SETTINGS.ComplicationTopLeft,
    ComplicationTopRight: DEFAULT_SETTINGS.ComplicationTopRight,
    ComplicationBottomRight: DEFAULT_SETTINGS.ComplicationBottomRight,
    ComplicationBottomLeft: DEFAULT_SETTINGS.ComplicationBottomLeft,
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
      "&daily=temperature_2m_max,temperature_2m_min" +
      "&timezone=auto" +
      "&temperature_unit=" + unit;
    var req = new XMLHttpRequest();

    req.onload = function() {
      try {
        var data = JSON.parse(req.responseText);
        var temp = Math.round(data.current.temperature_2m);
        var high = Math.round(data.daily.temperature_2m_max[0]);
        var low = Math.round(data.daily.temperature_2m_min[0]);
        Pebble.sendAppMessage({
          WeatherTemp: temp,
          WeatherHigh: high,
          WeatherLow: low,
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

function selectField(label, id, value, options) {
  return [
    "<label for=\"" + id + "\">" + label + "</label>",
    "<select id=\"" + id + "\">",
    options.map(function(option) {
      return "<option value=\"" + option.value + "\"" +
        (Number(value) === option.value ? " selected" : "") + ">" +
        option.label + "</option>";
    }).join(""),
    "</select>"
  ].join("");
}

function presetButtons(presets, label) {
  return [
    "<label>" + label + "</label>",
    "<div class=\"preset-grid\">",
    presets.map(function(preset, index) {
      return [
        "<button class=\"preset\" type=\"button\" data-preset=\"" + index + "\">",
        "<span class=\"swatches\">",
        "<span style=\"background:" + hexFromNumber(preset.BackgroundColor) + "\"></span>",
        "<span style=\"background:" + hexFromNumber(preset.RingColor) + "\"></span>",
        "<span style=\"background:" + hexFromNumber(preset.ComplicationColor) + "\"></span>",
        "<span style=\"background:" + hexFromNumber(preset.HourColor) + "\"></span>",
        "</span>",
        "<span>" + preset.name + "</span>",
        "</button>"
      ].join("");
    }).join(""),
    "</div>"
  ].join("");
}

function hiddenField(id, value) {
  return "<input id=\"" + id + "\" type=\"hidden\" value=\"" + value + "\">";
}

function hiddenColorField(id, value) {
  return hiddenField(id, hexFromNumber(value));
}

function buildConfigHtml(settings, isRound, isBw) {
  var presets = isBw ? BW_COLOR_PRESETS : COLOR_PRESETS;

  return [
    "<!doctype html>",
    "<html>",
    "<head>",
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">",
    "<style>",
    "body{margin:0;padding:18px 18px 24px;font-family:-apple-system,BlinkMacSystemFont,Helvetica,Arial,sans-serif;background:#111;color:#eee;}",
    "h1{font-size:20px;margin:0 0 18px;}",
    "label{display:block;font-weight:700;margin:14px 0 8px;}",
    "input,select,button{box-sizing:border-box;font-size:18px;}",
    "select{display:block;min-width:164px;max-width:100%;height:44px;background:#222;color:#eee;border:1px solid #555;padding:0 10px;}",
    ".preset-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-bottom:4px;}",
    ".preset{height:46px;background:#222;color:#eee;border:1px solid #555;text-align:left;padding:6px 8px;}",
    ".preset:focus,.preset:active{border-color:#aaa;}",
    ".swatches{display:inline-flex;vertical-align:middle;margin-right:8px;border:1px solid #666;}",
    ".swatches span{display:block;width:10px;height:16px;}",
    ".color-row{position:relative;width:44px;height:44px;}",
    ".color-preview{display:block;width:40px;height:40px;border:2px solid #777;border-radius:0;}",
    "input[type=color]{position:absolute;inset:0;width:44px;height:44px;opacity:0;}",
    "#save{display:block;width:100%;height:48px;margin-top:24px;background:#ddd;color:#111;border:0;font-weight:700;}",
    "</style>",
    "</head>",
    "<body>",
    "<h1>Minute Blocks</h1>",
    presetButtons(presets, isBw ? "Display" : "Color preset"),
    isBw ? hiddenColorField("background", settings.BackgroundColor) :
      field("Background", "background", settings.BackgroundColor),
    isBw ? hiddenColorField("ring", settings.RingColor) :
      field("Ring", "ring", settings.RingColor),
    isBw ? hiddenColorField("complication", settings.ComplicationColor) :
      field("Complications", "complication", settings.ComplicationColor),
    isBw ? hiddenColorField("hour", settings.HourColor) :
      field("Center digits", "hour", settings.HourColor),
    selectField("Time digits", "timeMode", settings.TimeMode, [
      { value: 0, label: "Watch setting" },
      { value: 1, label: "12 hour" },
      { value: 2, label: "24 hour" }
    ]),
    isRound ? hiddenField("complicationSize", settings.ComplicationSize) :
      selectField("Complication size", "complicationSize", settings.ComplicationSize, [
        { value: 0, label: "Normal" },
        { value: 1, label: "Medium" },
        { value: 2, label: "Large" }
      ]),
    selectField("Complication visibility", "complicationVisibility", settings.ComplicationVisibility, [
      { value: 0, label: "Always shown" },
      { value: 1, label: "Tap to show" }
    ]),
    selectField("Seconds", "secondsVisibility", settings.SecondsVisibility, [
      { value: 0, label: "Never" },
      { value: 1, label: "Always" },
      { value: 2, label: "Tap to show" }
    ]),
    selectField(isRound ? "Detail 1" : "Top left", "complicationTopLeft",
                settings.ComplicationTopLeft, COMPLICATION_OPTIONS),
    selectField(isRound ? "Detail 2" : "Top right", "complicationTopRight",
                settings.ComplicationTopRight, COMPLICATION_OPTIONS),
    isRound ? hiddenField("complicationBottomRight", settings.ComplicationBottomRight) :
      selectField("Bottom right", "complicationBottomRight", settings.ComplicationBottomRight,
                  COMPLICATION_OPTIONS),
    isRound ? hiddenField("complicationBottomLeft", settings.ComplicationBottomLeft) :
      selectField("Bottom left", "complicationBottomLeft", settings.ComplicationBottomLeft,
                  COMPLICATION_OPTIONS),
    "<label><input id=\"weatherEnabled\" type=\"checkbox\"" + (settings.WeatherEnabled ? " checked" : "") + "> Weather</label>",
    "<label for=\"weatherUnits\">Weather units</label>",
    "<select id=\"weatherUnits\">",
    "<option value=\"0\"" + (settings.WeatherUnits === 0 ? " selected" : "") + ">Fahrenheit</option>",
    "<option value=\"1\"" + (settings.WeatherUnits === 1 ? " selected" : "") + ">Celsius</option>",
    "</select>",
    "<button id=\"save\">Save</button>",
    "<script>",
    "var presets=" + JSON.stringify(presets) + ";",
    "function setColor(id,value){",
    "var input=document.getElementById(id);",
    "var preview=document.getElementById(id+'Preview');",
    "var hex=('#'+('000000'+Number(value).toString(16)).slice(-6));",
    "input.value=hex;",
    "if(!preview){return;}",
    "preview.style.background=hex;",
    "}",
    "['background','ring','complication','hour'].forEach(function(id){",
    "var input=document.getElementById(id);",
    "var preview=document.getElementById(id+'Preview');",
    "input.oninput=function(){preview.style.background=input.value;};",
    "input.onchange=input.oninput;",
    "});",
    "[].forEach.call(document.querySelectorAll('.preset'),function(button){",
    "button.onclick=function(){",
    "var preset=presets[parseInt(button.getAttribute('data-preset'),10)];",
    "setColor('background',preset.BackgroundColor);",
    "setColor('ring',preset.RingColor);",
    "setColor('complication',preset.ComplicationColor);",
    "setColor('hour',preset.HourColor);",
    "};",
    "});",
    "document.getElementById('save').onclick=function(){",
    "var settings={",
    "BackgroundColor:parseInt(document.getElementById('background').value.slice(1),16),",
    "RingColor:parseInt(document.getElementById('ring').value.slice(1),16),",
    "ComplicationColor:parseInt(document.getElementById('complication').value.slice(1),16),",
    "HourColor:parseInt(document.getElementById('hour').value.slice(1),16),",
    "TimeMode:parseInt(document.getElementById('timeMode').value,10),",
    "ComplicationSize:parseInt(document.getElementById('complicationSize').value,10),",
    "ComplicationVisibility:parseInt(document.getElementById('complicationVisibility').value,10),",
    "SecondsVisibility:parseInt(document.getElementById('secondsVisibility').value,10),",
    "ComplicationTopLeft:parseInt(document.getElementById('complicationTopLeft').value,10),",
    "ComplicationTopRight:parseInt(document.getElementById('complicationTopRight').value,10),",
    "ComplicationBottomRight:parseInt(document.getElementById('complicationBottomRight').value,10),",
    "ComplicationBottomLeft:parseInt(document.getElementById('complicationBottomLeft').value,10),",
    "WeatherEnabled:document.getElementById('weatherEnabled').checked?1:0,",
    "WeatherUnits:parseInt(document.getElementById('weatherUnits').value,10)",
    "};",
    "var encoded=encodeURIComponent(JSON.stringify(settings));",
    "var match=(location.search+location.hash).match(/[?#&]return_to=([^&#]+)/);",
    "var target=match?decodeURIComponent(match[1])+encoded:'pebblejs://close#'+encoded;",
    "if(!match&&location.protocol==='file:'){alert('Open this with pebble emu-app-config --file so Save has an emulator return URL.');return;}",
    "document.getElementById('save').textContent='Saving...';",
    "window.location.href=target;",
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
  var info = typeof Pebble.getActiveWatchInfo === "function" ? Pebble.getActiveWatchInfo() : {};
  var platform = info && info.platform ? info.platform : "";
  var isBw = platform === "aplite" || platform === "diorite";
  Pebble.openURL("data:text/html," +
                 encodeURIComponent(buildConfigHtml(loadSettings(), platform === "chalk", isBw)));
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
    settings.ComplicationSize = Number(settings.ComplicationSize);
    settings.ComplicationVisibility = Number(settings.ComplicationVisibility);
    settings.SecondsVisibility = Number(settings.SecondsVisibility);
    settings.ComplicationTopLeft = Number(settings.ComplicationTopLeft);
    settings.ComplicationTopRight = Number(settings.ComplicationTopRight);
    settings.ComplicationBottomRight = Number(settings.ComplicationBottomRight);
    settings.ComplicationBottomLeft = Number(settings.ComplicationBottomLeft);
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
