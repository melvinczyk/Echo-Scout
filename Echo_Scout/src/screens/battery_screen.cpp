#include "battery_screen.h"
#include "radar_screen.h"
#include "config.h"

#define BAT_ADC_PIN       9
#define BAT_MV_FULL    4200
#define BAT_MV_EMPTY   3400
#define BAT_CAPACITY_MAH 1000

#define USB_MV_THRESHOLD  4050
#define USB_CONFIRM_COUNT    3

static uint8_t usbHighCount = 0;
static uint8_t usbLowCount  = 0;
static bool    _usbState    = false;

static void updateUsbState(float rawMv) {
    if (rawMv >= USB_MV_THRESHOLD) {
        usbLowCount = 0;
        if (usbHighCount < USB_CONFIRM_COUNT) usbHighCount++;
        if (usbHighCount >= USB_CONFIRM_COUNT) _usbState = true;
    } else {
        usbHighCount = 0;
        if (usbLowCount < USB_CONFIRM_COUNT) usbLowCount++;
        if (usbLowCount >= USB_CONFIRM_COUNT) _usbState = false;
    }
}
static bool usbConnected() { return _usbState; }

#define STATUS_Y      (Config::HEADER_H + 4)
#define STATUS_H      16

#define LABEL_CHG_Y   58
#define BAR_X         10
#define BAR_Y         72
#define BAR_W        196
#define BAR_H         46
#define BAR_CORNER     5
#define TIP_W          8
#define TIP_H         18
#define TIP_X         (BAR_X + BAR_W)
#define TIP_Y         (BAR_Y + (BAR_H - TIP_H) / 2)

#define LABEL_VOLT_Y  128
#define VOLT_Y        146

#define LABEL_MAH_Y   180
#define MAH_VAL_Y     198

#define SEP_Y         220

#define LABEL_PCT_Y   228
#define PCT_Y         246

static float         prevVoltage  = -1.0f;
static int           prevPct      = -1;
static bool          prevUsbState = false;
static float         lastBatV     = 0.0f;
static bool          haveBatV     = false;
static unsigned long lastSampleMs = 0;

#define SMOOTH_N 8
static float   voltBuf[SMOOTH_N] = {0};
static uint8_t voltIdx   = 0;
static bool    voltFilled = false;

static float sampleVoltage() {
    float rawMv = analogReadMilliVolts(BAT_ADC_PIN) * 2.0f;
    updateUsbState(rawMv);

    voltBuf[voltIdx] = rawMv;
    voltIdx = (voltIdx + 1) % SMOOTH_N;
    if (voltIdx == 0) voltFilled = true;
    int n = voltFilled ? SMOOTH_N : (voltIdx ? voltIdx : 1);
    float sum = 0;
    for (int i = 0; i < n; i++) sum += voltBuf[i];
    float v = sum / n / 1000.0f;

    if (!usbConnected()) { lastBatV = v; haveBatV = true; }
    return v;
}

static int voltsToPercent(float v) {
    float mv = v * 1000.0f;
    if (mv >= BAT_MV_FULL)  return 100;
    if (mv <= BAT_MV_EMPTY) return 0;
    return (int)((mv - BAT_MV_EMPTY) * 100.0f / (BAT_MV_FULL - BAT_MV_EMPTY));
}

static uint16_t pctColor(int pct) {
    if (pct > 40) return Config::C_GREEN;
    if (pct > 15) return Config::C_AMBER;
    return Config::C_RED;
}


static void drawUsbBanner(bool usb) {
    tft.fillRect(0, STATUS_Y, Config::SCREEN_W, STATUS_H, Config::C_BG);
    if (usb) {
        tft.drawRect(4, STATUS_Y, Config::SCREEN_W - 8, STATUS_H - 2, Config::C_AMBER);
        tft.setTextColor(Config::C_AMBER, Config::C_BG);
        tft.drawCentreString("USB - LAST KNOWN VALUE", 120, STATUS_Y + 3, 1);
    } else {
        tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
        tft.drawCentreString("BATTERY ONLY", 120, STATUS_Y + 3, 1);
    }
}


static void drawBatteryShell() {
    tft.drawRoundRect(BAR_X, BAR_Y, BAR_W, BAR_H, BAR_CORNER, Config::C_GREEN_DIM);
    tft.fillRoundRect(TIP_X, TIP_Y, TIP_W + BAR_CORNER, TIP_H, BAR_CORNER, Config::C_GREEN_DIM);
    tft.fillRect(TIP_X, TIP_Y + 2, BAR_CORNER + 1, TIP_H - 4, Config::C_BG);
}

static void drawFillBar(int pct, uint16_t col) {
    const int PAD = 4;
    int innerX = BAR_X + PAD;
    int innerY = BAR_Y + PAD;
    int innerW = BAR_W - PAD * 2;
    int innerH = BAR_H - PAD * 2;
    int fillW  = (int)(innerW * pct / 100.0f);

    tft.fillRect(innerX, innerY, innerW, innerH, Config::C_BG);
    if (fillW > 0)
        tft.fillRect(innerX, innerY, fillW, innerH, col);

    // Restore border after fill
    tft.drawRoundRect(BAR_X, BAR_Y, BAR_W, BAR_H, BAR_CORNER, Config::C_GREEN_DIM);
}


static void drawVoltage(float v, uint16_t col) {
    tft.fillRect(0, LABEL_VOLT_Y - 2, Config::SCREEN_W,
                 LABEL_MAH_Y - 6 - (LABEL_VOLT_Y - 2), Config::C_BG);

    tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
    tft.drawCentreString("VOLTAGE", 120, LABEL_VOLT_Y, 2);

    char buf[12];
    dtostrf(v, 4, 2, buf);
    strcat(buf, " V");
    tft.setTextColor(col, Config::C_BG);
    tft.drawCentreString(buf, 120, VOLT_Y, 4);
}


static void drawMah(int pct) {
    tft.fillRect(0, LABEL_MAH_Y - 2, Config::SCREEN_W,
                 SEP_Y - (LABEL_MAH_Y - 2), Config::C_BG);

    tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
    tft.drawCentreString("REMAINING", 120, LABEL_MAH_Y, 2);

    int remaining = (int)(BAT_CAPACITY_MAH * pct / 100.0f);
    char buf[24];
    sprintf(buf, "%d / %d mAh", remaining, BAT_CAPACITY_MAH);
    tft.setTextColor(Config::C_GREEN, Config::C_BG);
    tft.drawCentreString(buf, 120, MAH_VAL_Y, 2);
}


static void drawPercent(int pct, uint16_t col) {
    tft.fillRect(0, LABEL_PCT_Y - 2, Config::SCREEN_W,
                 Config::SCREEN_H - (LABEL_PCT_Y - 2), Config::C_BG);

    tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
    tft.drawCentreString("CHARGE", 120, LABEL_PCT_Y, 2);

    char buf[8];
    sprintf(buf, "%d%%", pct);
    tft.setTextColor(col, Config::C_BG);
    tft.drawCentreString(buf, 120, PCT_Y, 6);
}


void drawBatteryBase() {
    tft.fillScreen(Config::C_BG);

    prevVoltage  = -1.0f;
    prevPct      = -1;
    voltFilled   = false;
    voltIdx      = 0;
    usbHighCount = 0;
    usbLowCount  = 0;
    _usbState    = false;
    prevUsbState = true;

    // Header
    tft.drawFastHLine(0, Config::HEADER_H - 1, Config::SCREEN_W, Config::C_SEP);
    tft.drawRoundRect(3, 3, 64, Config::HEADER_H - 6, 3, Config::C_GREEN_DIM);
    tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
    tft.drawCentreString("< MENU", 35, 7, 2);
    tft.setTextColor(Config::C_GREEN, Config::C_BG);
    tft.drawCentreString("BATTERY", 120, 9, 2);

    // Static labels and separators
    tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
    tft.drawCentreString("CHARGE LEVEL", 120, LABEL_CHG_Y, 1);
    drawBatteryShell();
    tft.drawFastHLine(10, SEP_Y, Config::SCREEN_W - 20, Config::C_SEP);

    // Prime ADC buffer with real readings
    pinMode(BAT_ADC_PIN, INPUT);
    for (int i = 0; i < SMOOTH_N; i++) sampleVoltage();

    // Initial draw
    lastSampleMs = millis();
    float    v   = sampleVoltage();
    int      pct = voltsToPercent(v);
    uint16_t col = pctColor(pct);

    prevUsbState = usbConnected();
    drawUsbBanner(prevUsbState);
    drawFillBar(pct, col);
    drawVoltage(v, col);
    drawMah(pct);
    drawPercent(pct, col);

    prevVoltage = v;
    prevPct     = pct;
}


void tickBattery() {
    unsigned long now = millis();
    if (now - lastSampleMs < 500) return;
    lastSampleMs = now;

    bool usb = usbConnected();

    if (usb != prevUsbState) {
        prevUsbState = usb;
        drawUsbBanner(usb);
        prevVoltage = -1.0f;
        prevPct     = -1;
    }

    float v = (usb && haveBatV) ? lastBatV : sampleVoltage();

    int      pct = voltsToPercent(v);
    uint16_t col = pctColor(pct);

    if (pct != prevPct) {
        drawFillBar(pct, col);
        drawPercent(pct, col);
        drawMah(pct);
        prevPct = pct;
    }
    if (fabsf(v - prevVoltage) >= 0.01f) {
        drawVoltage(v, col);
        prevVoltage = v;
    }
}
