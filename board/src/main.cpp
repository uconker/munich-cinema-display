// Munich cinema display for Heltec WiFi LoRa 32 V3 (ESP32-S3, 0.96" SSD1306 OLED, one PRG button).
//
// Modes (hold the button to switch):   KINO  -> pick a cinema, see its films + times
//                                      FILM  -> pick a film, see all cinemas + times
//                                      GLEICH -> what starts in the next 90 minutes, everywhere
// Button:  tap = next | double tap = previous | hold = next mode
// Long pages scroll by themselves. Past shows disappear; shows starting within 45 min get a '*'.
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <U8g2lib.h>
#include <time.h>
#include "secrets.h"
#include "data.h"

#define VEXT_PIN 36
#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_RST 21
#define BTN_PIN  0

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_RST, OLED_SCL, OLED_SDA);

static const uint32_t HOLD_MS = 700, DOUBLE_MS = 320, DEBOUNCE_MS = 30;
static const uint32_t SCROLL_MS = 3500;
static const uint32_t REFRESH_OK_MS = 3UL * 3600 * 1000;      // re-download every 3 h
static const uint32_t REFRESH_FAIL_MS = 10UL * 60 * 1000;     // retry after a failure
static const uint32_t DIM_AFTER_MS = 5UL * 60 * 1000;         // dim the OLED when nobody touches it (burn-in)
static const uint32_t WAKE_MS = 2UL * 60 * 1000;              // night: screen stays on this long after a press
static const int NIGHT_FROM = 1, NIGHT_TO = 7;                // 01:00-07:00 screen off
static const int BODY_LINES = 6;

static CinemaData gData, gTmp;
static Lines gLines;
static int gMode = MODE_CINEMA;
static int gPages[MAXF], gNPages = 0, gPos = 0;
static int gSelC = -1, gSelF = -1;
static int gScroll = 0;
static uint32_t gLastScroll = 0, gLastActivity = 0, gLastWake = 0, gLastView = 0;
static uint32_t gLastFetchOk = 0, gLastFetchTry = 0;
static bool gHaveData = false, gHaveTime = false, gScreenOn = true, gDimmed = false, gDirty = true, gFetchFailed = false;
static View gView = {-1, -1, 0};
static char gToday[12] = "";
static uint32_t gSplashUntil = 0; static char gSplash[24] = "";

// ---------------- display helpers ----------------
static void drawText(int x, int y, const char *s, bool bold = false) {
  u8g2.drawUTF8(x, y, s);
  if (bold) u8g2.drawUTF8(x + 1, y, s);
}
static void message(const char *a, const char *b = "") {
  u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawUTF8(0, 14, a); u8g2.drawUTF8(0, 30, b); u8g2.sendBuffer();
}

// ---------------- time ----------------
static bool updateClock() {
  time_t t = time(nullptr);
  if (t < 1700000000) return false;
  struct tm tmv; localtime_r(&t, &tmv);
  snprintf(gToday, sizeof(gToday), "%04d-%02d-%02d", tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
  gView.nowMin = tmv.tm_hour * 60 + tmv.tm_min;
  gHaveTime = true;
  return true;
}
static int nowHour() { time_t t = time(nullptr); struct tm tmv; localtime_r(&t, &tmv); return tmv.tm_hour; }

// ---------------- network + data ----------------
struct Net { const char *ssid; const char *pass; };
#ifdef WIFI_EXTRA
static const Net extraNets[] = WIFI_EXTRA;
#endif

static bool connectWiFi() {
  static WiFiMulti multi; static bool added = false;
  if (!added) {
    multi.addAP(WIFI_SSID, WIFI_PASSWORD);
#ifdef WIFI_EXTRA
    for (const Net &n : extraNets) multi.addAP(n.ssid, n.pass);
#endif
    added = true;
  }
  WiFi.mode(WIFI_STA);
  return multi.run(20000) == WL_CONNECTED;
}
static void wifiOff() { WiFi.disconnect(true); WiFi.mode(WIFI_OFF); }

static bool loadCache() {
  if (!LittleFS.begin(true)) return false;
  File f = LittleFS.open("/cinemas.txt", "r");
  if (!f) return false;
  size_t n = f.size();
  if (n == 0 || n > 60000) { f.close(); return false; }
  char *b = (char *)malloc(n + 1);
  if (!b) { f.close(); return false; }
  f.readBytes(b, n); b[n] = 0; f.close();
  bool ok = parseCinemaData(b, n, gTmp);
  free(b);
  if (ok) { memcpy(&gData, &gTmp, sizeof(gData)); gHaveData = true; }
  return ok;
}
static void saveCache(const String &body) {
  File f = LittleFS.open("/cinemas.txt", "w");
  if (f) { f.print(body); f.close(); }
}

static bool fetchData() {
  gLastFetchTry = millis();
  message("Kino-Daten laden...", "WLAN...");
  if (!connectWiFi()) { wifiOff(); gFetchFailed = true; return false; }
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.google.com");   // also resyncs the clock
  if (!gHaveTime) { struct tm t; getLocalTime(&t, 10000); }
  message("Kino-Daten laden...", "Download...");
  WiFiClientSecure client; client.setInsecure();     // public data; skips cert check (like the other projects)
  HTTPClient http; http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  bool ok = false;
  if (http.begin(client, DATA_URL)) {
    int code = http.GET();
    if (code == 200) {
      String body = http.getString();
      if (body.length() > 100 && body.length() < 60000 && parseCinemaData(body.c_str(), body.length(), gTmp)) {
        memcpy(&gData, &gTmp, sizeof(gData)); gHaveData = true; saveCache(body); ok = true;
      }
    }
    http.end();
  }
  wifiOff();
  gFetchFailed = !ok;
  if (ok) gLastFetchOk = millis();
  gDirty = true;
  return ok;
}

// ---------------- pages ----------------
static int findCinema(const char *needle) {
  if (!needle || !needle[0]) return -1;
  for (int i = 0; i < gData.nC; i++) if (strstr(gData.cin[i], needle)) return i;
  return -1;
}

static void rebuildPages() {
  if (!gHaveData || !gHaveTime) { gNPages = 0; return; }
  gView = makeView(gData, gToday, gView.nowMin);
  if (gView.viewDay < 0) { gNPages = 0; return; }
  gNPages = buildPages(gData, gView, gMode, gPages, MAXF);
  int sel = (gMode == MODE_CINEMA) ? gSelC : gSelF;
  int pos = 0;
  for (int i = 0; i < gNPages; i++) if (gPages[i] >= sel) { pos = i; break; }
  gPos = pos;
  if (gNPages > 0 && gMode != MODE_SOON) { if (gMode == MODE_CINEMA) gSelC = gPages[gPos]; else gSelF = gPages[gPos]; }
}

static void buildLines() {
  gLines.n = 0;
  if (gNPages == 0 || gView.viewDay < 0) return;
  if (gMode == MODE_CINEMA) buildCinemaLines(gLines, gData, gView, gPages[gPos]);
  else if (gMode == MODE_FILM) buildFilmLines(gLines, gData, gView, gPages[gPos]);
  else buildSoonLines(gLines, gData, gView);
}

static void go(int delta) {           // next / previous page (GLEICH mode: next / previous window)
  gLastScroll = millis();
  if (gMode == MODE_SOON) {
    if (gLines.n <= BODY_LINES) { gScroll = 0; }
    else if (delta > 0) { gScroll += BODY_LINES; if (gScroll >= gLines.n) gScroll = 0; }
    else { gScroll -= BODY_LINES; if (gScroll < 0) gScroll = ((gLines.n - 1) / BODY_LINES) * BODY_LINES; }
  } else if (gNPages > 0) {
    gScroll = 0;
    gPos = (gPos + delta + gNPages) % gNPages;
    if (gMode == MODE_CINEMA) gSelC = gPages[gPos]; else gSelF = gPages[gPos];
    buildLines();
  }
  gDirty = true;
}

static void nextMode() {
  gMode = (gMode + 1) % 3; gScroll = 0; gLastScroll = millis();
  static const char *names[] = {"KINO", "FILM", "GLEICH"};
  snprintf(gSplash, sizeof(gSplash), "%s", names[gMode]);
  gSplashUntil = millis() + 900;
  rebuildPages(); buildLines(); gDirty = true;
}

// ---------------- drawing ----------------
static void draw() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tf);

  if (millis() < gSplashUntil) {
    u8g2.setFont(u8g2_font_10x20_tf);
    u8g2.drawStr((128 - (int)strlen(gSplash) * 10) / 2, 40, gSplash);
    u8g2.sendBuffer(); return;
  }
  if (!gHaveTime) { message("Warte auf Uhrzeit...", "WLAN/NTP"); return; }
  if (!gHaveData) { message("Keine Daten", gFetchFailed ? "Download fehlgeschlagen" : "lade..."); return; }

  // header
  char head[48], right[16];
  if (gView.viewDay < 0) {
    drawText(0, 7, "Heute Schluss", true);
    drawText(0, 24, "Keine Vorstellungen mehr");
    drawText(0, 36, "Neue Daten kommen frueh");
    u8g2.sendBuffer(); return;
  }
  if (gMode == MODE_SOON) { snprintf(head, sizeof(head), "GLEICH %02d:%02d", gView.nowMin / 60, gView.nowMin % 60); right[0] = 0; }
  else {
    int id = gPages[gPos];
    clipChars(head, sizeof(head), gMode == MODE_CINEMA ? gData.cin[id] : gData.film[id], COLS - 5);
    snprintf(right, sizeof(right), "%d/%d", gPos + 1, gNPages);
  }
  drawText(0, 7, head, true);
  if (right[0]) drawText(128 - (int)strlen(right) * 5, 7, right);
  bool stale = gFetchFailed && (millis() - gLastFetchOk > 26UL * 3600 * 1000);
  if (stale) drawText(128 - (int)strlen(right) * 5 - 7, 7, "!");
  u8g2.drawHLine(0, 9, 128);

  if (gLines.n == 0) { drawText(0, 24, "Nichts mehr heute"); u8g2.sendBuffer(); return; }
  for (int i = 0; i < BODY_LINES; i++) {
    int li = gScroll + i;
    if (li >= gLines.n) break;
    drawText(0, 18 + i * 9, gLines.l[li].t, gLines.l[li].style == 1);
  }
  // scroll hint: small dot column on the right edge
  if (gLines.n > BODY_LINES) {
    int total = (gLines.n + BODY_LINES - 1) / BODY_LINES, cur = gScroll / BODY_LINES;
    for (int i = 0; i < total && i < 10; i++) { if (i == cur) u8g2.drawBox(126, 12 + i * 5, 2, 3); else u8g2.drawPixel(127, 13 + i * 5); }
  }
  u8g2.sendBuffer();
}

// ---------------- button gestures ----------------
enum Gesture { G_NONE, G_TAP, G_DOUBLE, G_HOLD };
static Gesture pollButton() {
  static bool down = false, holdFired = false; static uint32_t tDown = 0, tUp = 0; static int taps = 0;
  bool p = digitalRead(BTN_PIN) == LOW; uint32_t now = millis();
  if (p) gLastActivity = now;
  if (p && !down) { down = true; tDown = now; holdFired = false; }
  else if (p && down && !holdFired && now - tDown >= HOLD_MS) { holdFired = true; taps = 0; return G_HOLD; }
  else if (!p && down) {
    down = false;
    if (!holdFired && now - tDown >= DEBOUNCE_MS) { taps++; tUp = now; }
  }
  if (!down && taps > 0 && now - tUp >= DOUBLE_MS) { Gesture g = taps >= 2 ? G_DOUBLE : G_TAP; taps = 0; return g; }
  return G_NONE;
}

// ---------------- Arduino ----------------
void setup() {
  Serial.begin(115200);
  pinMode(VEXT_PIN, OUTPUT); digitalWrite(VEXT_PIN, LOW);
  pinMode(BTN_PIN, INPUT_PULLUP);
  delay(100);
  u8g2.begin(); u8g2.enableUTF8Print();
  message("Kino Muenchen", "starte...");
  gData.clear();
  loadCache();                       // show last known data immediately, even offline
  fetchData();                       // then refresh (also sets the clock via NTP)
  updateClock();
  int sc = findCinema(START_CINEMA); gSelC = sc >= 0 ? sc : 0; gSelF = 0;
  rebuildPages(); buildLines();
  gLastActivity = gLastWake = gLastScroll = millis();
}

void loop() {
  uint32_t now = millis();
  Gesture g = pollButton();

  // ---- screen on/off (night) and dimming ----
  bool night = gHaveTime && (nowHour() >= NIGHT_FROM && nowHour() < NIGHT_TO);
  if (g != G_NONE) gLastWake = now;
  bool wantOn = !night || (now - gLastWake < WAKE_MS);
  bool wokeNow = wantOn && !gScreenOn;
  if (wantOn != gScreenOn) { gScreenOn = wantOn; u8g2.setPowerSave(gScreenOn ? 0 : 1); gDirty = true; }
  bool wantDim = (now - gLastActivity > DIM_AFTER_MS);
  if (wantDim != gDimmed) { gDimmed = wantDim; u8g2.setContrast(gDimmed ? 8 : 255); }

  if (!gScreenOn) { delay(50); return; }
  if (wokeNow) g = G_NONE;                       // the press that woke the screen is not a command

  // ---- clock tick + page refresh every 20 s ----
  if (now - gLastView > 20000 || gLastView == 0) {
    gLastView = now;
    if (updateClock()) { rebuildPages(); buildLines(); gDirty = true; }
  }

  // ---- data refresh ----
  bool idle = (now - gLastActivity > 10000);
  uint32_t since = now - gLastFetchTry;
  bool due = (!gHaveData || !gHaveTime) ? since > 30000
           : gFetchFailed ? since > REFRESH_FAIL_MS
           : (now - gLastFetchOk > REFRESH_OK_MS) || (gHaveTime && gView.viewDay < 0 && since > REFRESH_FAIL_MS);
  if (due && idle) { fetchData(); updateClock(); rebuildPages(); buildLines(); gLastView = millis(); }

  // ---- gestures ----
  if (g == G_TAP) go(+1);
  else if (g == G_DOUBLE) go(-1);
  else if (g == G_HOLD) nextMode();

  // ---- auto scroll ----
  {
    if (gLines.n > BODY_LINES && now - gLastScroll > SCROLL_MS && now > gSplashUntil) {
      gLastScroll = now;
      gScroll += BODY_LINES / 2;
      if (gScroll >= gLines.n) gScroll = 0;
      gDirty = true;
    }
  }

  if (gDirty || now < gSplashUntil) { draw(); gDirty = false; }
  if (gSplashUntil && now >= gSplashUntil) { gSplashUntil = 0; gDirty = true; }
  delay(10);
}
