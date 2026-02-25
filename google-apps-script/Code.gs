/**
 * Google Apps Script – Calendar + Open-Meteo weather endpoint
 *
 * Deploy as:  Web App  →  Execute as ME  →  Anyone (even anonymous)
 *
 * Query parameters:
 *   ?action=cal2      → 2-day calendar
 *   ?action=cal7      → 7-day calendar
 *   ?action=weather1  → current weather + 24-hour forecast
 *   ?action=weather7  → 7-day weather forecast
 *
 * Response: plain-text lines prefixed with tags:
 *   [SYS], [DAY], [ALLDAY], [EV], [WEATHERDAY], [NOW], [H],
 *   [SUMMARY], [W7], [END]
 */

// ---------- configuration ----------
var CAL_ID    = 'primary';
var LAT       = 51.0447;   // Gentbrugge
var LON       = 3.7588;
var TZ        = 'Europe/Brussels';

// ---------- entry point (GET) ----------
function doGet(e) {
  var action = (e && e.parameter && e.parameter.action) ? e.parameter.action : 'cal2';
  var out    = '';

  try {
    // Week number (ISO)
    var now  = new Date();
    var week = isoWeek(now);
    out += '[SYS]|WEEK ' + week + '|\n';

    if (action === 'cal2' || action === 'cal7') {
      var days = (action === 'cal2') ? 2 : 7;
      out += buildCalendar(days);
    } else if (action === 'weather1') {
      out += buildWeather1();
    } else if (action === 'weather7') {
      out += buildWeather7();
    } else {
      out += '[SYS]|ERROR|Unknown action: ' + action + '\n';
    }

    out += '[END]\n';
  } catch (err) {
    out = '[SYS]|ERROR|' + err.message + '\n[END]\n';
  }

  return ContentService.createTextOutput(out).setMimeType(ContentService.MimeType.TEXT);
}

// ===================================================================
//  CALENDAR
// ===================================================================

function buildCalendar(days) {
  var result = '';
  var now    = new Date();

  for (var d = 0; d < days; d++) {
    var dayStart = new Date(now.getFullYear(), now.getMonth(), now.getDate() + d, 0, 0, 0);
    var dayEnd   = new Date(now.getFullYear(), now.getMonth(), now.getDate() + d, 23, 59, 59);

    var label = dayLabel(d, dayStart);
    result += '[DAY]|' + d + '|' + label + '\n';

    var events = CalendarApp.getCalendarById(CAL_ID).getEvents(dayStart, dayEnd);
    for (var i = 0; i < events.length; i++) {
      var ev = events[i];
      if (ev.isAllDayEvent()) {
        result += '[ALLDAY]|' + sanitize(ev.getTitle()) + '|' + sanitize(ev.getLocation()) + '\n';
      } else {
        result += '[EV]|' + sanitize(ev.getTitle()) + '|' +
                  hhmm(ev.getStartTime()) + '|' +
                  hhmm(ev.getEndTime())   + '|' +
                  sanitize(ev.getLocation()) + '\n';
      }
    }
  }
  return result;
}

// ===================================================================
//  WEATHER  –  Open-Meteo (no API key)
// ===================================================================

function buildWeather1() {
  var url = 'https://api.open-meteo.com/v1/forecast?' +
    'latitude='  + LAT +
    '&longitude=' + LON +
    '&current=temperature_2m,relative_humidity_2m,apparent_temperature,' +
      'weather_code,wind_speed_10m,wind_direction_10m,uv_index,precipitation' +
    '&hourly=temperature_2m,weather_code,precipitation,wind_speed_10m' +
    '&daily=temperature_2m_min,temperature_2m_max,weather_code,precipitation_sum,wind_speed_10m_max' +
    '&timezone=' + encodeURIComponent(TZ) +
    '&forecast_days=2';

  var data = JSON.parse(UrlFetchApp.fetch(url).getContentText());
  var result = '';

  // Today summary line
  var daily = data.daily;
  result += '[WEATHERDAY]|' + daily.time[0] + '|Today|' +
    daily.temperature_2m_min[0] + '|' + daily.temperature_2m_max[0] + '|' +
    daily.weather_code[0] + '|' + daily.precipitation_sum[0] + '|' +
    daily.wind_speed_10m_max[0] + '\n';

  // Current conditions
  var c = data.current;
  result += '[NOW]|' + c.temperature_2m + '|' + c.weather_code + '|' +
    wmoDescription(c.weather_code) + '|' +
    c.apparent_temperature + '|' + c.relative_humidity_2m + '|' +
    c.wind_speed_10m + '|' + windDir(c.wind_direction_10m) + '|' +
    c.uv_index + '|' + c.precipitation + '\n';

  // Next 24 hours (hourly)
  var h = data.hourly;
  var nowHour = new Date().getHours();
  var count = 0;
  for (var i = 0; i < h.time.length && count < 24; i++) {
    var hour = parseInt(h.time[i].substring(11, 13), 10);
    var dayIdx = h.time[i].substring(0, 10) === daily.time[0] ? 0 : 1;
    if (dayIdx === 0 && hour < nowHour) continue;
    result += '[H]|' + h.time[i].substring(11, 16) + '|' +
      h.temperature_2m[i] + '|' + h.weather_code[i] + '|' +
      h.precipitation[i] + '|' + h.wind_speed_10m[i] + '\n';
    count++;
  }

  // Short summary
  result += '[SUMMARY]|' + wmoDescription(c.weather_code) +
    ', ' + c.temperature_2m + '°C, wind ' + c.wind_speed_10m + ' km/h\n';

  return result;
}

function buildWeather7() {
  var url = 'https://api.open-meteo.com/v1/forecast?' +
    'latitude='  + LAT +
    '&longitude=' + LON +
    '&daily=temperature_2m_min,temperature_2m_max,weather_code,precipitation_sum,wind_speed_10m_max' +
    '&timezone=' + encodeURIComponent(TZ) +
    '&forecast_days=7';

  var data  = JSON.parse(UrlFetchApp.fetch(url).getContentText());
  var daily = data.daily;
  var result = '';

  for (var i = 0; i < daily.time.length; i++) {
    var lbl = dayLabelFromDate(daily.time[i], i);
    result += '[W7]|' + lbl + '|' +
      daily.temperature_2m_min[i] + '|' + daily.temperature_2m_max[i] + '|' +
      daily.weather_code[i] + '|' + daily.precipitation_sum[i] + '\n';
  }
  return result;
}

// ===================================================================
//  HELPERS
// ===================================================================

function isoWeek(d) {
  var tmp = new Date(Date.UTC(d.getFullYear(), d.getMonth(), d.getDate()));
  tmp.setUTCDate(tmp.getUTCDate() + 4 - (tmp.getUTCDay() || 7));
  var yearStart = new Date(Date.UTC(tmp.getUTCFullYear(), 0, 1));
  return Math.ceil((((tmp - yearStart) / 86400000) + 1) / 7);
}

function dayLabel(offset, dt) {
  if (offset === 0) return 'TODAY';
  if (offset === 1) return 'TOMORROW';
  var days = ['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
  var months = ['Jan','Feb','Mar','Apr','May','Jun',
                'Jul','Aug','Sep','Oct','Nov','Dec'];
  return days[dt.getDay()] + ' ' + dt.getDate() + ' ' + months[dt.getMonth()];
}

function dayLabelFromDate(isoStr, idx) {
  if (idx === 0) return 'Today';
  if (idx === 1) return 'Tomorrow';
  var dt = new Date(isoStr + 'T00:00:00');
  var days = ['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
  return days[dt.getDay()];
}

function hhmm(d) {
  return ('0' + d.getHours()).slice(-2) + ':' + ('0' + d.getMinutes()).slice(-2);
}

function sanitize(s) {
  if (!s) return '';
  return s.replace(/\|/g, '/').replace(/\n/g, ' ').replace(/\r/g, '');
}

function windDir(deg) {
  var dirs = ['N','NNE','NE','ENE','E','ESE','SE','SSE',
              'S','SSW','SW','WSW','W','WNW','NW','NNW'];
  return dirs[Math.round(deg / 22.5) % 16];
}

/**
 * WMO Weather interpretation codes → human-readable description.
 * Reference: https://open-meteo.com/en/docs#weathervariables
 */
function wmoDescription(code) {
  var map = {
    0:  'Clear sky',
    1:  'Mainly clear',     2: 'Partly cloudy',   3: 'Overcast',
    45: 'Fog',              48: 'Rime fog',
    51: 'Light drizzle',    53: 'Moderate drizzle', 55: 'Dense drizzle',
    56: 'Freezing drizzle', 57: 'Dense freezing drizzle',
    61: 'Slight rain',      63: 'Moderate rain',    65: 'Heavy rain',
    66: 'Freezing rain',    67: 'Heavy freezing rain',
    71: 'Slight snow',      73: 'Moderate snow',    75: 'Heavy snow',
    77: 'Snow grains',
    80: 'Slight showers',   81: 'Moderate showers', 82: 'Violent showers',
    85: 'Slight snow showers', 86: 'Heavy snow showers',
    95: 'Thunderstorm',     96: 'Thunderstorm + hail', 99: 'Thunderstorm + heavy hail'
  };
  return map[code] || ('WMO ' + code);
}
