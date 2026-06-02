#include <Arduino.h>
// #include <Wire.h>
#include <SPI.h>
#include <Preferences.h>
// #include <esp_sleep.h>
#include <GxEPD2_BW.h>
// #include <gdey/GxEPD2_270_GDEY027T91.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <qrcode.h>

#include "badge_pins.h"

// Networking
#include <WiFi.h>
#include <HTTPClient.h>

// ── I²C / buttons ─────────────────────────────────────────────────────────────
#define TCA9534_ADDR   0x20
#define TCA9534_INPUT  0x00
#define TCA9534_CONFIG 0x03

#define BTN_LEFT   (1 << 0)
#define BTN_DOWN   (1 << 1)
#define BTN_UP     (1 << 2)
#define BTN_RIGHT  (1 << 3)
#define BTN_SELECT (1 << 4)
#define BTN_CANCEL (1 << 5)

// ── Display ───────────────────────────────────────────────────────────────────
GxEPD2_BW<GxEPD2_270_GDEY027T91, GxEPD2_270_GDEY027T91::HEIGHT> display(
    GxEPD2_270_GDEY027T91(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));

static const char* EVENT_TITLE = "Onion DAO";
static const char* EVENT_DETAIL = "June 2026";
static const char* ATTENDEE_NAME = "Nathan Mundo";
static const char* ATTENDEE_ROLE = "";
static const char* BADGE_TAGLINE = "Null City";
static const char* QR_TEXT = "https://linkedin.com/in/nmundo/";

static int32_t qr_origin_x = 0;
static int32_t qr_origin_y = 0;
static int32_t qr_scale = 2;

static void qrcode_display_func(esp_qrcode_handle_t qrcode) {
  int size = esp_qrcode_get_size(qrcode);
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      if (esp_qrcode_get_module(qrcode, x, y)) {
        int px = qr_origin_x + x * qr_scale;
        int py = qr_origin_y + y * qr_scale;
        display.fillRect(px, py, qr_scale, qr_scale, GxEPD_BLACK);
      }
    }
  }
}

static void drawQRCode(const char* text, int originX, int originY, int scale) {
  qr_origin_x = originX;
  qr_origin_y = originY;
  qr_scale = scale;

  esp_qrcode_config_t qrcfg = ESP_QRCODE_CONFIG_DEFAULT();
  qrcfg.display_func = qrcode_display_func;
  qrcfg.max_qrcode_version = 4;
  qrcfg.qrcode_ecc_level = ESP_QRCODE_ECC_MED;

  esp_err_t err = esp_qrcode_generate(&qrcfg, text);
  if (err != ESP_OK) {
    Serial.printf("QR generation failed: %d\n", err);
  }
}

static uint8_t chooseNameTextSize(const char* name) {
  size_t len = strlen(name);
  if (len <= 16) {
    return 2;
  }
  return 1;
}

static int printStackedName(const char* name, int x, int y, uint8_t textSize) {
  display.setTextSize(textSize);
  const char* lastSpace = strrchr(name, ' ');
  int lineHeight = (textSize == 2) ? 24 : 16;
  if (lastSpace && lastSpace != name) {
    size_t firstLen = lastSpace - name;
    char firstLine[32];
    char secondLine[32];
    if (firstLen >= sizeof(firstLine)) {
      firstLen = sizeof(firstLine) - 1;
    }
    memcpy(firstLine, name, firstLen);
    firstLine[firstLen] = '\0';
    strncpy(secondLine, lastSpace + 1, sizeof(secondLine) - 1);
    secondLine[sizeof(secondLine) - 1] = '\0';

    display.setCursor(x, y);
    display.print(firstLine);
    display.setCursor(x, y + lineHeight);
    display.print(secondLine);
    return lineHeight * 2;
  }

  display.setCursor(x, y);
  display.print(name);
  return lineHeight;
}

// Added footerText parameter to allow updating bottom-right text
static void renderBadge(const char* eventName,
                        const char* eventDetail,
                        const char* name,
                        const char* role,
                        const char* qrText,
                        const char* tagline,
                        const char* footerText) {
  const int leftMargin = 10;
  const int lineRight = display.width() - 10;
  const int qrSizePx = 66;
  const int qrX = display.width() - qrSizePx - 10;
  const int qrY = 50;

  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    display.setFont(&FreeMonoBold9pt7b);
    display.setTextSize(1);
    display.setCursor(leftMargin, 20);
    display.print(eventName);

    display.setFont(&FreeMono9pt7b);
    display.setTextSize(1);
    display.setCursor(display.width() - 110, 20);
    display.print(eventDetail);
    display.drawLine(leftMargin, 30, lineRight, 30, GxEPD_BLACK);

    display.setFont(&FreeMonoBold9pt7b);
    uint8_t nameTextSize = chooseNameTextSize(name);
    int nameHeight = printStackedName(name, leftMargin, 70, nameTextSize);
    display.setTextSize(1);

    display.setFont(&FreeMonoBold9pt7b);
    int roleY = 95 + nameHeight;
    display.setCursor(leftMargin, roleY);
    display.print(role);
    display.drawLine(leftMargin, roleY, lineRight, roleY, GxEPD_BLACK);

    drawQRCode(qrText, qrX, qrY, 2);

    display.setCursor(leftMargin, display.height() - 10);
    display.print(tagline);
    // Draw footer (right aligned)
    if (footerText && footerText[0] != '\0') {
      display.setFont(&FreeMono9pt7b);
      display.setTextSize(1);
      int16_t bx, by;
      uint16_t bw, bh;
      String f = String(footerText);
      display.getTextBounds(f, 0, 0, &bx, &by, &bw, &bh);
      int fx = display.width() - 10 - bw;
      int fy = display.height() - 10;
      if (fx < leftMargin) fx = leftMargin;
      display.setCursor(fx, fy);
      display.print(f);
    }
  } while (display.nextPage());
}

// --- Network / polling helpers ------------------------------------------------
static const char* WIFI_SSID = "CIC Guest"; 
static const char* WIFI_PASSWORD = "1nnovation";
static const char* FOOTER_API_URL = "https://oniondao.dev/portal/__data.json";

static void connectWiFi(unsigned long timeoutMs = 10000) {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.printf("Connecting to WiFi '%s'...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(200);
    Serial.print('.');
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connect timeout");
  }
}

static String fetchFooterText() {
  HTTPClient http;
  String result = "(no data)";
  Serial.printf("Fetching footer from %s\n", FOOTER_API_URL);
  http.begin(FOOTER_API_URL);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    payload.trim();
    if (payload.length() > 0) {
      // truncate to reasonable length
      if (payload.length() > 48) payload = payload.substring(0, 48);
      result = payload;
    }
  } else {
    Serial.printf("HTTP GET failed, code: %d\n", httpCode);
  }
  http.end();
  return result;
}

static void updateBadgeWithFooter(const char* footer) {
  // Wake display, re-render full badge with new footer, then hibernate
  display.init(115200);
  display.setRotation(1);
  display.setTextColor(GxEPD_BLACK);
  renderBadge(EVENT_TITLE, EVENT_DETAIL, ATTENDEE_NAME, ATTENDEE_ROLE, QR_TEXT, BADGE_TAGLINE, footer);
  display.hibernate();
}

void setup() {
  Serial.begin(115200);

  // Enable power to peripherals / display rail
  pinMode(PIN_PWR, OUTPUT);
  digitalWrite(PIN_PWR, HIGH);
  delay(50);

  // Start SPI on the badge's display pins
  SPI.begin(PIN_EPD_SCK, -1, PIN_EPD_MOSI, PIN_EPD_CS);

  // Init e-paper
  display.init(115200);
  display.setRotation(1);
  display.setTextColor(GxEPD_BLACK);

  // Initial render with empty footer
  renderBadge(EVENT_TITLE, EVENT_DETAIL, ATTENDEE_NAME, ATTENDEE_ROLE, QR_TEXT, BADGE_TAGLINE, "");

  // Put panel into low-power mode after drawing
  display.hibernate();
}

void loop() {
  static unsigned long lastPoll = 0;
  const unsigned long POLL_INTERVAL_MS = 3600000UL; // 1 hour

  if (lastPoll == 0 || (millis() - lastPoll) >= POLL_INTERVAL_MS) {
    lastPoll = millis();
    Serial.println("Hourly tick: checking for footer update...");
    connectWiFi(15000);
    if (WiFi.status() == WL_CONNECTED) {
      String footer = fetchFooterText();
      updateBadgeWithFooter(footer.c_str());
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
    } else {
      Serial.println("No WiFi: skipping footer fetch");
    }
  }

  delay(1000);
}
