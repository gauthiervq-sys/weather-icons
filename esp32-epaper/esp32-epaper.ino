/*
 * ESP32 E-Paper Calendar & Weather Display
 *
 * Hardware
 *   Board  : Seeed XIAO ESP32-S3
 *   Display: Waveshare 7" e-paper 800×480 (GxEPD2_750_T7)
 *   Buttons: D4 = cal2, D5 = cal7, D6 = weather1, D7 = weather7
 *   EPD pwr: D3
 *
 * Libraries (install via Arduino Library Manager)
 *   - GxEPD2           (Jean-Marc Zingg)
 *   - Adafruit GFX
 *   - WiFi              (ESP32 core)
 *   - HTTPClient        (ESP32 core)
 *
 * Board package (Arduino IDE → Boards Manager)
 *   "esp32" by Espressif  ≥ 2.0.x
 *   Select board: "XIAO_ESP32S3"
 */

#include <GxEPD2_BW.h>
#include <WiFi.h>
#include <HTTPClient.h>

// FreeSans fonts from Adafruit GFX
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

// ===================== USER CONFIG =====================
const char* ssid      = "YOUR_WIFI_SSID";
const char* password  = "YOUR_WIFI_PASSWORD";
const char* scriptUrl = "https://script.google.com/macros/s/YOUR_DEPLOY_ID/exec";
// =======================================================

// ----- Pin definitions (XIAO ESP32-S3) -----
#define EPD_PWR_PIN   D3
#define BTN_CAL2      D4
#define BTN_CAL7      D5
#define BTN_WEATHER1  D6
#define BTN_WEATHER7  D7

// ----- Display wiring (SPI – XIAO defaults) -----
//  CS=D1  DC=D0  RST=D2  BUSY=D8   (adjust to your wiring)
#define EPD_CS    D1
#define EPD_DC    D0
#define EPD_RST   D2
#define EPD_BUSY  D8

GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT>
    display(GxEPD2_750_T7(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// ----- Display constants -----
#define SCREEN_W  800
#define SCREEN_H  480

// ----- Max array sizes (no std::vector) -----
#define MAX_LINES   120
#define MAX_LINE_LEN 200

// ========================================================
//  32×32 weather bitmap icons  (PROGMEM, 1-bit, MSB first)
// ========================================================

// Sunny – simple sun
static const unsigned char PROGMEM icon_sunny[] = {
  0x00,0x01,0x80,0x00, 0x00,0x01,0x80,0x00, 0x00,0x01,0x80,0x00, 0x04,0x00,0x00,0x20,
  0x06,0x00,0x00,0x60, 0x03,0x00,0x00,0xC0, 0x01,0x07,0xE0,0x80, 0x00,0x1F,0xF8,0x00,
  0x00,0x3F,0xFC,0x00, 0x00,0x7F,0xFE,0x00, 0x00,0x7F,0xFE,0x00, 0x00,0xFF,0xFF,0x00,
  0x00,0xFF,0xFF,0x00, 0x00,0xFF,0xFF,0x00, 0x70,0xFF,0xFF,0x0E, 0xF8,0xFF,0xFF,0x1F,
  0xF8,0xFF,0xFF,0x1F, 0x70,0xFF,0xFF,0x0E, 0x00,0xFF,0xFF,0x00, 0x00,0xFF,0xFF,0x00,
  0x00,0xFF,0xFF,0x00, 0x00,0x7F,0xFE,0x00, 0x00,0x7F,0xFE,0x00, 0x00,0x3F,0xFC,0x00,
  0x00,0x1F,0xF8,0x00, 0x01,0x07,0xE0,0x80, 0x03,0x00,0x00,0xC0, 0x06,0x00,0x00,0x60,
  0x04,0x00,0x00,0x20, 0x00,0x01,0x80,0x00, 0x00,0x01,0x80,0x00, 0x00,0x01,0x80,0x00
};

// Cloudy – large cloud
static const unsigned char PROGMEM icon_cloudy[] = {
  0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00, 0x00,0x03,0xC0,0x00, 0x00,0x0F,0xF0,0x00, 0x00,0x1F,0xF8,0x00,
  0x00,0x3F,0xFC,0x00, 0x00,0x3F,0xFE,0x00, 0x00,0x7F,0xFF,0x00, 0x00,0xFF,0xFF,0x00,
  0x01,0xFF,0xFF,0x80, 0x03,0xFF,0xFF,0xC0, 0x07,0xFF,0xFF,0xE0, 0x07,0xFF,0xFF,0xE0,
  0x0F,0xFF,0xFF,0xF0, 0x0F,0xFF,0xFF,0xF0, 0x1F,0xFF,0xFF,0xF8, 0x1F,0xFF,0xFF,0xF8,
  0x1F,0xFF,0xFF,0xF8, 0x1F,0xFF,0xFF,0xF8, 0x1F,0xFF,0xFF,0xF8, 0x0F,0xFF,0xFF,0xF0,
  0x0F,0xFF,0xFF,0xF0, 0x07,0xFF,0xFF,0xE0, 0x03,0xFF,0xFF,0xC0, 0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
};

// Rain – cloud with rain drops
static const unsigned char PROGMEM icon_rain[] = {
  0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x03,0xC0,0x00, 0x00,0x0F,0xF0,0x00,
  0x00,0x1F,0xF8,0x00, 0x00,0x3F,0xFC,0x00, 0x00,0x7F,0xFE,0x00, 0x01,0xFF,0xFF,0x80,
  0x03,0xFF,0xFF,0xC0, 0x07,0xFF,0xFF,0xE0, 0x0F,0xFF,0xFF,0xF0, 0x0F,0xFF,0xFF,0xF0,
  0x1F,0xFF,0xFF,0xF8, 0x1F,0xFF,0xFF,0xF8, 0x1F,0xFF,0xFF,0xF8, 0x0F,0xFF,0xFF,0xF0,
  0x07,0xFF,0xFF,0xE0, 0x03,0xFF,0xFF,0xC0, 0x00,0x00,0x00,0x00, 0x00,0x80,0x10,0x02,
  0x01,0xC0,0x38,0x07, 0x01,0xC0,0x38,0x07, 0x00,0x80,0x10,0x02, 0x00,0x00,0x00,0x00,
  0x02,0x00,0x40,0x08, 0x07,0x00,0xE0,0x1C, 0x07,0x00,0xE0,0x1C, 0x02,0x00,0x40,0x08,
  0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
};

// Snow – cloud with snowflakes
static const unsigned char PROGMEM icon_snow[] = {
  0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x03,0xC0,0x00, 0x00,0x0F,0xF0,0x00,
  0x00,0x1F,0xF8,0x00, 0x00,0x3F,0xFC,0x00, 0x00,0x7F,0xFE,0x00, 0x01,0xFF,0xFF,0x80,
  0x03,0xFF,0xFF,0xC0, 0x07,0xFF,0xFF,0xE0, 0x0F,0xFF,0xFF,0xF0, 0x0F,0xFF,0xFF,0xF0,
  0x1F,0xFF,0xFF,0xF8, 0x1F,0xFF,0xFF,0xF8, 0x1F,0xFF,0xFF,0xF8, 0x0F,0xFF,0xFF,0xF0,
  0x07,0xFF,0xFF,0xE0, 0x03,0xFF,0xFF,0xC0, 0x00,0x00,0x00,0x00, 0x00,0x49,0x24,0x00,
  0x00,0x2A,0x54,0x00, 0x00,0x1C,0x38,0x00, 0x00,0x2A,0x54,0x00, 0x00,0x49,0x24,0x00,
  0x00,0x00,0x00,0x00, 0x00,0x92,0x49,0x00, 0x00,0x54,0xAA,0x00, 0x00,0x38,0x1C,0x00,
  0x00,0x54,0xAA,0x00, 0x00,0x92,0x49,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
};

// Thunder – cloud with lightning bolt
static const unsigned char PROGMEM icon_thunder[] = {
  0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x03,0xC0,0x00, 0x00,0x0F,0xF0,0x00,
  0x00,0x1F,0xF8,0x00, 0x00,0x3F,0xFC,0x00, 0x00,0x7F,0xFE,0x00, 0x01,0xFF,0xFF,0x80,
  0x03,0xFF,0xFF,0xC0, 0x07,0xFF,0xFF,0xE0, 0x0F,0xFF,0xFF,0xF0, 0x0F,0xFF,0xFF,0xF0,
  0x1F,0xFF,0xFF,0xF8, 0x1F,0xFF,0xFF,0xF8, 0x1F,0xFF,0xFF,0xF8, 0x0F,0xFF,0xFF,0xF0,
  0x07,0xFF,0xFF,0xE0, 0x03,0xFF,0xFF,0xC0, 0x00,0x01,0x80,0x00, 0x00,0x03,0x80,0x00,
  0x00,0x07,0x00,0x00, 0x00,0x0F,0x00,0x00, 0x00,0x1F,0xF0,0x00, 0x00,0x07,0xE0,0x00,
  0x00,0x03,0xC0,0x00, 0x00,0x03,0x80,0x00, 0x00,0x07,0x00,0x00, 0x00,0x06,0x00,0x00,
  0x00,0x04,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
};

// Drizzle – cloud with fine drops
static const unsigned char PROGMEM icon_drizzle[] = {
  0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x03,0xC0,0x00, 0x00,0x0F,0xF0,0x00,
  0x00,0x1F,0xF8,0x00, 0x00,0x3F,0xFC,0x00, 0x00,0x7F,0xFE,0x00, 0x01,0xFF,0xFF,0x80,
  0x03,0xFF,0xFF,0xC0, 0x07,0xFF,0xFF,0xE0, 0x0F,0xFF,0xFF,0xF0, 0x0F,0xFF,0xFF,0xF0,
  0x1F,0xFF,0xFF,0xF8, 0x1F,0xFF,0xFF,0xF8, 0x1F,0xFF,0xFF,0xF8, 0x0F,0xFF,0xFF,0xF0,
  0x07,0xFF,0xFF,0xE0, 0x03,0xFF,0xFF,0xC0, 0x00,0x00,0x00,0x00, 0x00,0x40,0x08,0x01,
  0x00,0x40,0x08,0x01, 0x00,0x00,0x00,0x00, 0x01,0x00,0x20,0x04, 0x01,0x00,0x20,0x04,
  0x00,0x00,0x00,0x00, 0x04,0x00,0x80,0x10, 0x04,0x00,0x80,0x10, 0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
};

// Fog – horizontal lines
static const unsigned char PROGMEM icon_fog[] = {
  0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
  0x1F,0xFF,0xFF,0xF8, 0x1F,0xFF,0xFF,0xF8, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
  0x07,0xFF,0xFF,0xE0, 0x07,0xFF,0xFF,0xE0, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
  0x1F,0xFF,0xFF,0xF8, 0x1F,0xFF,0xFF,0xF8, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
  0x07,0xFF,0xFF,0xE0, 0x07,0xFF,0xFF,0xE0, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
  0x1F,0xFF,0xFF,0xF8, 0x1F,0xFF,0xFF,0xF8, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
};

// ========================================================
//  WMO code → icon mapping
// ========================================================
const unsigned char* wmoIcon(int code) {
  if (code <= 1)                   return icon_sunny;
  if (code <= 3)                   return icon_cloudy;
  if (code == 45 || code == 48)    return icon_fog;
  if (code >= 51 && code <= 57)    return icon_drizzle;
  if (code >= 61 && code <= 67)    return icon_rain;
  if (code >= 71 && code <= 77)    return icon_snow;
  if (code >= 80 && code <= 82)    return icon_rain;
  if (code >= 85 && code <= 86)    return icon_snow;
  if (code >= 95)                  return icon_thunder;
  return icon_cloudy;
}

// ========================================================
//  Parsed data structures  (flat arrays, no STL)
// ========================================================
char lines[MAX_LINES][MAX_LINE_LEN];
int  lineCount = 0;

// ========================================================
//  Arduino setup / loop
// ========================================================

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== E-Paper Calendar & Weather ===");

  // EPD power
  pinMode(EPD_PWR_PIN, OUTPUT);
  digitalWrite(EPD_PWR_PIN, HIGH);
  delay(100);

  // Buttons (active LOW with internal pull-up)
  pinMode(BTN_CAL2,     INPUT_PULLUP);
  pinMode(BTN_CAL7,     INPUT_PULLUP);
  pinMode(BTN_WEATHER1, INPUT_PULLUP);
  pinMode(BTN_WEATHER7, INPUT_PULLUP);

  // Display init
  display.init(115200, true, 2, false);
  display.setRotation(0);
  display.setTextWrap(false);
  display.setTextColor(GxEPD_BLACK);

  // WiFi
  connectWiFi();

  // Default view: 2-day calendar
  fetchAndRender("cal2");
}

void loop() {
  // Simple polling with debounce
  if (digitalRead(BTN_CAL2) == LOW)     { delay(200); fetchAndRender("cal2");     }
  if (digitalRead(BTN_CAL7) == LOW)     { delay(200); fetchAndRender("cal7");     }
  if (digitalRead(BTN_WEATHER1) == LOW) { delay(200); fetchAndRender("weather1"); }
  if (digitalRead(BTN_WEATHER7) == LOW) { delay(200); fetchAndRender("weather7"); }
  delay(100);
}

// ========================================================
//  Network
// ========================================================

void connectWiFi() {
  Serial.print("WiFi connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 40) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nConnected – IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi FAILED");
  }
}

String httpGet(const char* action) {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    if (WiFi.status() != WL_CONNECTED) return "";
  }

  String url = String(scriptUrl) + "?action=" + action;
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  http.begin(url);

  int code = http.GET();
  String body = "";
  if (code == 200) {
    body = http.getString();
  } else {
    Serial.print("HTTP error: ");
    Serial.println(code);
  }
  http.end();
  return body;
}

// ========================================================
//  Parsing
// ========================================================

void parseLines(const String& body) {
  lineCount = 0;
  int start = 0;
  int len   = body.length();
  while (start < len && lineCount < MAX_LINES) {
    int nl = body.indexOf('\n', start);
    if (nl < 0) nl = len;
    int segLen = nl - start;
    if (segLen >= MAX_LINE_LEN) segLen = MAX_LINE_LEN - 1;
    body.substring(start, start + segLen).toCharArray(lines[lineCount], MAX_LINE_LEN);
    lineCount++;
    start = nl + 1;
  }
}

// Return the value of field at 'index' within a '|'-delimited line.
// Index 0 = the tag itself (e.g. "[DAY]").
String field(int lineIdx, int fieldIdx) {
  String s = String(lines[lineIdx]);
  int pos = 0;
  for (int f = 0; f < fieldIdx; f++) {
    pos = s.indexOf('|', pos);
    if (pos < 0) return "";
    pos++;
  }
  int end = s.indexOf('|', pos);
  if (end < 0) end = s.length();
  return s.substring(pos, end);
}

bool lineTag(int lineIdx, const char* tag) {
  return strncmp(lines[lineIdx], tag, strlen(tag)) == 0;
}

// ========================================================
//  Main fetch + render dispatcher
// ========================================================

void fetchAndRender(const char* action) {
  Serial.print("Fetching action=");
  Serial.println(action);

  String body = httpGet(action);
  if (body.length() == 0) {
    showMessage("No data received.\nCheck WiFi & script URL.");
    return;
  }

  parseLines(body);

  if (strcmp(action, "cal2") == 0 || strcmp(action, "cal7") == 0) {
    renderCalendar();
  } else if (strcmp(action, "weather1") == 0) {
    renderWeather1();
  } else if (strcmp(action, "weather7") == 0) {
    renderWeather7();
  }
}

// ========================================================
//  RENDER: simple message
// ========================================================

void showMessage(const char* msg) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&FreeSans12pt7b);
    display.setCursor(20, 40);
    display.print(msg);
  } while (display.nextPage());
}

// ========================================================
//  RENDER: Calendar (2-day or 7-day)
// ========================================================

void renderCalendar() {
  // Collect header info
  String weekStr = "";
  for (int i = 0; i < lineCount; i++) {
    if (lineTag(i, "[SYS]")) {
      weekStr = field(i, 1);
    }
  }

  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    // Header bar
    display.fillRect(0, 0, SCREEN_W, 36, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(10, 27);
    display.print("CALENDAR  ");
    display.setFont(&FreeSans9pt7b);
    display.print(weekStr);
    display.setTextColor(GxEPD_BLACK);

    int y = 52;

    for (int i = 0; i < lineCount; i++) {
      if (lineTag(i, "[DAY]")) {
        // Day header
        String label = field(i, 2);
        display.fillRect(0, y - 2, SCREEN_W, 24, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(10, y + 16);
        display.print(label);
        display.setTextColor(GxEPD_BLACK);
        y += 30;
      }
      else if (lineTag(i, "[ALLDAY]")) {
        String title = field(i, 1);
        display.setFont(&FreeSans9pt7b);
        display.setCursor(14, y + 12);
        display.print("\xB7 ");   // middle dot
        display.print(title);
        String loc = field(i, 2);
        if (loc.length() > 0) {
          display.setFont(&FreeSans9pt7b);
          display.print("  @ ");
          display.print(loc);
        }
        y += 20;
      }
      else if (lineTag(i, "[EV]")) {
        String title = field(i, 1);
        String tStart = field(i, 2);
        String tEnd   = field(i, 3);
        String loc    = field(i, 4);

        display.setFont(&FreeSans9pt7b);
        display.setCursor(14, y + 12);
        display.print(tStart);
        display.print("-");
        display.print(tEnd);
        display.print("  ");
        display.setFont(&FreeSansBold12pt7b);
        display.print(title);
        if (loc.length() > 0) {
          display.setFont(&FreeSans9pt7b);
          display.print("  @ ");
          display.print(loc);
        }
        y += 22;
      }

      if (y > SCREEN_H - 20) break;  // prevent overflow
    }

    // Footer
    display.setFont(&FreeSans9pt7b);
    display.setCursor(10, SCREEN_H - 6);
    display.print("Buttons: [cal2] [cal7] [weather1] [weather7]");

  } while (display.nextPage());
}

// ========================================================
//  RENDER: Weather current + 24h forecast
// ========================================================

void renderWeather1() {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    // Header
    display.fillRect(0, 0, SCREEN_W, 36, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(10, 27);
    display.print("WEATHER NOW");
    display.setTextColor(GxEPD_BLACK);

    int y = 50;

    for (int i = 0; i < lineCount; i++) {
      if (lineTag(i, "[WEATHERDAY]")) {
        // Day summary bar
        String dayLbl  = field(i, 2);
        String tempMin = field(i, 3);
        String tempMax = field(i, 4);
        int wmo        = field(i, 5).toInt();

        display.drawBitmap(10, y, wmoIcon(wmo), 32, 32, GxEPD_BLACK);
        display.setFont(&FreeSansBold18pt7b);
        display.setCursor(50, y + 26);
        display.print(dayLbl);
        display.setFont(&FreeSans12pt7b);
        display.print("  ");
        display.print(tempMin);
        display.print("/");
        display.print(tempMax);
        display.print("\xB0""C");
        y += 44;
      }
      else if (lineTag(i, "[NOW]")) {
        String temp    = field(i, 1);
        int wmo        = field(i, 2).toInt();
        String desc    = field(i, 3);
        String feels   = field(i, 4);
        String hum     = field(i, 5);
        String wind    = field(i, 6);
        String windD   = field(i, 7);
        String uv      = field(i, 8);
        String precip  = field(i, 9);

        // Large temperature
        display.drawBitmap(10, y, wmoIcon(wmo), 32, 32, GxEPD_BLACK);
        display.setFont(&FreeSansBold24pt7b);
        display.setCursor(50, y + 32);
        display.print(temp);
        display.print("\xB0""C");

        display.setFont(&FreeSans12pt7b);
        display.setCursor(250, y + 10);
        display.print(desc);
        display.setCursor(250, y + 32);
        display.print("Feels ");
        display.print(feels);
        display.print("\xB0  Hum ");
        display.print(hum);
        display.print("%");

        y += 44;
        display.setFont(&FreeSans9pt7b);
        display.setCursor(50, y);
        display.print("Wind ");
        display.print(wind);
        display.print(" km/h ");
        display.print(windD);
        display.print("   UV ");
        display.print(uv);
        display.print("   Precip ");
        display.print(precip);
        display.print(" mm");
        y += 22;
      }
      else if (lineTag(i, "[H]")) {
        // Hourly row
        String hh     = field(i, 1);
        String hTemp  = field(i, 2);
        int hWmo      = field(i, 3).toInt();
        String hPrec  = field(i, 4);

        // Compact: 6 columns across 800px
        int col  = 0;
        // Count preceding [H] lines to figure out column position
        int hIdx = 0;
        for (int j = 0; j < i; j++) {
          if (lineTag(j, "[H]")) hIdx++;
        }
        col = hIdx % 6;
        if (col == 0 && hIdx > 0) y += 52;
        int x = 10 + col * 130;

        display.drawBitmap(x, y, wmoIcon(hWmo), 32, 32, GxEPD_BLACK);
        display.setFont(&FreeSans9pt7b);
        display.setCursor(x + 34, y + 12);
        display.print(hh);
        display.setCursor(x + 34, y + 28);
        display.print(hTemp);
        display.print("\xB0 ");
        display.print(hPrec);
        display.print("mm");

        if (y > SCREEN_H - 60) break;
      }
      else if (lineTag(i, "[SUMMARY]")) {
        // Bottom summary line
        display.setFont(&FreeSans9pt7b);
        display.setCursor(10, SCREEN_H - 20);
        display.print(field(i, 1));
      }
    }

    // Footer
    display.setFont(&FreeSans9pt7b);
    display.setCursor(10, SCREEN_H - 6);
    display.print("Buttons: [cal2] [cal7] [weather1] [weather7]");

  } while (display.nextPage());
}

// ========================================================
//  RENDER: 7-day weather forecast
// ========================================================

void renderWeather7() {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    // Header
    display.fillRect(0, 0, SCREEN_W, 36, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(10, 27);
    display.print("7-DAY FORECAST");
    display.setTextColor(GxEPD_BLACK);

    int y = 50;
    int rowH = 58;

    for (int i = 0; i < lineCount; i++) {
      if (lineTag(i, "[W7]")) {
        String dayLbl  = field(i, 1);
        String tempMin = field(i, 2);
        String tempMax = field(i, 3);
        int wmo        = field(i, 4).toInt();
        String precip  = field(i, 5);

        // Alternating background for readability
        int rowIdx = 0;
        for (int j = 0; j < i; j++) {
          if (lineTag(j, "[W7]")) rowIdx++;
        }
        if (rowIdx % 2 == 1) {
          display.fillRect(0, y - 4, SCREEN_W, rowH, GxEPD_BLACK);
          display.setTextColor(GxEPD_WHITE);
        } else {
          display.setTextColor(GxEPD_BLACK);
        }

        // Icon
        display.drawBitmap(10, y, wmoIcon(wmo), 32, 32,
          (rowIdx % 2 == 1)
            ? GxEPD_WHITE
            : GxEPD_BLACK);

        // Day label
        display.setFont(&FreeSansBold18pt7b);
        display.setCursor(55, y + 28);
        display.print(dayLbl);

        // Temps
        display.setFont(&FreeSans18pt7b);
        display.setCursor(250, y + 28);
        display.print(tempMin);
        display.print(" / ");
        display.print(tempMax);
        display.print("\xB0""C");

        // Precipitation
        display.setFont(&FreeSans12pt7b);
        display.setCursor(550, y + 28);
        display.print(precip);
        display.print(" mm");

        // Reset text color
        display.setTextColor(GxEPD_BLACK);

        y += rowH;
        if (y > SCREEN_H - 30) break;
      }
    }

    // Footer
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(10, SCREEN_H - 6);
    display.print("Buttons: [cal2] [cal7] [weather1] [weather7]");

  } while (display.nextPage());
}
