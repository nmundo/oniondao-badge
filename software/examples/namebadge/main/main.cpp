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

static void renderBadge(const char* eventName,
                        const char* eventDetail,
                        const char* name,
                        const char* role,
                        const char* qrText,
                        const char* tagline) {
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
  } while (display.nextPage());
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

  renderBadge(EVENT_TITLE, EVENT_DETAIL, ATTENDEE_NAME, ATTENDEE_ROLE, QR_TEXT, BADGE_TAGLINE);

  // Put panel into low-power mode after drawing
  display.hibernate();
}

void loop() {
  // Static text; nothing else needed
}
