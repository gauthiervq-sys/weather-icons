# Deployment Guide – ESP32 E-Paper Calendar & Weather Display

## Overview

| Component | Technology |
|-----------|-----------|
| Backend | Google Apps Script (web app) |
| Firmware | Arduino sketch (C++) |
| Board | Seeed XIAO ESP32-S3 |
| Display | Waveshare 7″ e-paper 800×480 |
| Weather API | Open-Meteo (free, no key) |
| Calendar API | Google Calendar (via Apps Script) |

---

## 1. Google Apps Script Setup

### 1.1 Create the project
1. Open [Google Apps Script](https://script.google.com/) and click **New project**.
2. Delete the default `myFunction` code.
3. Paste the entire contents of `google-apps-script/Code.gs`.
4. **Rename** the project (e.g., *EPaper Backend*).

### 1.2 Configure (optional)
Edit the constants at the top of `Code.gs` if needed:

| Variable | Default | Description |
|----------|---------|-------------|
| `CAL_ID` | `'primary'` | Google Calendar ID |
| `LAT` | `51.0447` | Latitude (Gentbrugge) |
| `LON` | `3.7588` | Longitude (Gentbrugge) |
| `TZ` | `'Europe/Brussels'` | IANA timezone |

### 1.3 Deploy
1. Click **Deploy → New deployment**.
2. Type: **Web app**.
3. Execute as: **Me**.
4. Who has access: **Anyone** (so the ESP32 can call it without authentication).
5. Click **Deploy** and **Authorize** when prompted.
6. Copy the **Web app URL** — you will need it for the Arduino sketch.

### 1.4 Test
Open the URL in a browser with `?action=cal2`, `?action=weather1`, etc.:
```
https://script.google.com/macros/s/YOUR_DEPLOY_ID/exec?action=weather1
```
You should see plain text lines starting with `[SYS]`, `[WEATHERDAY]`, `[NOW]`, etc.

---

## 2. Arduino IDE Setup

### 2.1 Install board support
1. Open Arduino IDE → **File → Preferences**.
2. In *Additional boards manager URLs* add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. **Tools → Board → Boards Manager** → search `esp32` → install **esp32 by Espressif** (≥ 2.0.x).
4. Select board: **XIAO_ESP32S3**.

### 2.2 Install libraries
Via **Sketch → Include Library → Manage Libraries**:
- **GxEPD2** by Jean-Marc Zingg
- **Adafruit GFX Library** by Adafruit

### 2.3 Configure the sketch
Open `esp32-epaper/esp32-epaper.ino` and edit the three constants at the top:

```cpp
const char* ssid      = "YOUR_WIFI_SSID";
const char* password  = "YOUR_WIFI_PASSWORD";
const char* scriptUrl = "https://script.google.com/macros/s/YOUR_DEPLOY_ID/exec";
```

### 2.4 Wiring

| ESP32-S3 Pin | Connects to |
|-------------|-------------|
| D0 | EPD DC |
| D1 | EPD CS |
| D2 | EPD RST |
| D3 | EPD Power (VCC enable) |
| D4 | Button 1 – cal2 (to GND) |
| D5 | Button 2 – cal7 (to GND) |
| D6 | Button 3 – weather1 (to GND) |
| D7 | Button 4 – weather7 (to GND) |
| D8 | EPD BUSY |
| MOSI | EPD DIN |
| SCK | EPD CLK |

Buttons are active LOW with internal pull-ups enabled. Wire each button between the pin and GND.

### 2.5 Upload
1. Connect the XIAO ESP32-S3 via USB-C.
2. Select the correct port in **Tools → Port**.
3. Click **Upload** (→ icon).
4. Open **Serial Monitor** at 115200 baud to see connection status and debug output.

---

## 3. Data Contract

The Apps Script returns plain text, one line per record, fields separated by `|`.

### System line
```
[SYS]|WEEK <number>|
```

### Calendar lines (`cal2` / `cal7`)
```
[DAY]|<dayIndex>|<label>
[ALLDAY]|<title>|<location>
[EV]|<title>|<startHH:mm>|<endHH:mm>|<location>
```

### Weather current (`weather1`)
```
[WEATHERDAY]|<date>|<dayLabel>|<tempMin>|<tempMax>|<wmoCode>|<precipMM>|<windKmh>
[NOW]|<temp>|<wmoCode>|<description>|<feelsLike>|<humidity>|<windKmh>|<windDir>|<uv>|<precipMM>
[H]|<HH:mm>|<temp>|<wmoCode>|<precipMM>|<windKmh>
[SUMMARY]|<text>
```

### 7-day forecast (`weather7`)
```
[W7]|<dayLabel>|<tempMin>|<tempMax>|<wmoCode>|<precipMM>
```

### Terminator
```
[END]
```

---

## 4. Button Actions

| Button | Pin | Action | Description |
|--------|-----|--------|-------------|
| 1 | D4 | `cal2` | 2-day calendar view |
| 2 | D5 | `cal7` | 7-day calendar view |
| 3 | D6 | `weather1` | Current weather + 24h hourly |
| 4 | D7 | `weather7` | 7-day weather forecast |

---

## 5. Troubleshooting

| Symptom | Fix |
|---------|-----|
| WiFi fails | Check SSID/password; ensure 2.4 GHz network |
| HTTP error 302 | Google redirect — `setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS)` is set |
| Blank display | Check EPD wiring; verify D3 power pin is HIGH |
| "No data received" | Test the Apps Script URL in a browser first |
| Calendar empty | Ensure the Google account has events; check `CAL_ID` |
| Icons wrong | WMO code mapping in `wmoIcon()` — adjust thresholds if needed |

---

## 6. Customization

- **Location**: Change `LAT`, `LON`, and `TZ` in `Code.gs`.
- **Calendar**: Set `CAL_ID` to a specific calendar ID (find it in Google Calendar settings).
- **Display pins**: Adjust `EPD_CS`, `EPD_DC`, `EPD_RST`, `EPD_BUSY` pin defines in the sketch.
- **Fonts**: Replace `FreeSans*` includes with other Adafruit GFX fonts.
- **Icons**: Replace the `icon_*` PROGMEM arrays with your own 32×32 bitmaps.
