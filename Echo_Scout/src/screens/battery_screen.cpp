#include "battery_screen.h"
#include "display.h"


static uint8_t usbHighCount = 0;
static uint8_t usbLowCount  = 0;
static bool    _usbState    = false;

static void updateUsbState(float rawMv) {
    if (rawMv >= Config::USB_MV_THRESHOLD) {
        usbLowCount = 0;
        if (usbHighCount < Config::USB_CONFIRM_COUNT) usbHighCount++;
        if (usbHighCount >= Config::USB_CONFIRM_COUNT) _usbState = true;
    } else {
        usbHighCount = 0;
        if (usbLowCount < Config::USB_CONFIRM_COUNT) usbLowCount++;
        if (usbLowCount >= Config::USB_CONFIRM_COUNT) _usbState = false;
    }
}
static bool usbConnected() { return _usbState; }

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
    float rawMv = analogReadMilliVolts(Config::BAT_ADC_PIN) * 2.0f;
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
    if (mv >= Config::BAT_MV_FULL)  return 100;
    if (mv <= Config::BAT_MV_EMPTY) return 0;
    return (int)((mv - Config::BAT_MV_EMPTY) * 100.0f / (Config::BAT_MV_FULL - Config::BAT_MV_EMPTY));
}

static uint16_t pctColor(int pct) {
    if (pct > 40) return Display::Colors::GREEN;
    if (pct > 15) return Display::Colors::AMBER;
    return Display::Colors::RED;
}


static void drawUsbBanner(bool usb) {
    Display::tft.fillRect(0, BatteryScreen::STATUS_Y, Display::SCREEN_W, BatteryScreen::STATUS_H, Display::Colors::BG);
    if (usb) {
        Display::tft.drawRect(4, BatteryScreen::STATUS_Y, Display::SCREEN_W - 8, BatteryScreen::STATUS_H - 2, Display::Colors::AMBER);
        Display::tft.setTextColor(Display::Colors::AMBER, Display::Colors::BG);
        Display::tft.drawCentreString("USB - LAST KNOWN VALUE", 120, BatteryScreen::STATUS_Y + 3, 1);
    } else {
        Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
        Display::tft.drawCentreString("BATTERY ONLY", 120, BatteryScreen::STATUS_Y + 3, 1);
    }
}


static void drawBatteryShell() {
    Display::tft.drawRoundRect(BatteryScreen::BAR_X, BatteryScreen::BAR_Y, BatteryScreen::BAR_W, BatteryScreen::BAR_H, BatteryScreen::BAR_CORNER, Display::Colors::GREEN_DIM);
    Display::tft.fillRoundRect(BatteryScreen::TIP_X, BatteryScreen::TIP_Y, BatteryScreen::TIP_W + BatteryScreen::BAR_CORNER, BatteryScreen::TIP_H, BatteryScreen::BAR_CORNER, Display::Colors::GREEN_DIM);
    Display::tft.fillRect(BatteryScreen::TIP_X, BatteryScreen::TIP_Y + 2, BatteryScreen::BAR_CORNER + 1, BatteryScreen::TIP_H - 4, Display::Colors::BG);
}

static void drawFillBar(int pct, uint16_t col) {
    const int PAD = 4;
    int innerX = BatteryScreen::BAR_X + PAD;
    int innerY = BatteryScreen::BAR_Y + PAD;
    int innerW = BatteryScreen::BAR_W - PAD * 2;
    int innerH = BatteryScreen::BAR_H - PAD * 2;
    int fillW  = (int)(innerW * pct / 100.0f);

    Display::tft.fillRect(innerX, innerY, innerW, innerH, Display::Colors::BG);
    if (fillW > 0)
        Display::tft.fillRect(innerX, innerY, fillW, innerH, col);

    // Restore border after fill
    Display::tft.drawRoundRect(BatteryScreen::BAR_X, BatteryScreen::BAR_Y, BatteryScreen::BAR_W, BatteryScreen::BAR_H, BatteryScreen::BAR_CORNER, Display::Colors::GREEN_DIM);
}


static void drawVoltage(float v, uint16_t col) {
    Display::tft.fillRect(0, BatteryScreen::LABEL_VOLT_Y - 2, Display::SCREEN_W,
                 BatteryScreen::LABEL_MAH_Y - 6 - (BatteryScreen::LABEL_VOLT_Y - 2), Display::Colors::BG);

    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("VOLTAGE", 120, BatteryScreen::LABEL_VOLT_Y, 2);

    char buf[12];
    dtostrf(v, 4, 2, buf);
    strcat(buf, " V");
    Display::tft.setTextColor(col, Display::Colors::BG);
    Display::tft.drawCentreString(buf, 120, BatteryScreen::VOLT_Y, 4);
}


static void drawMah(int pct) {
    Display::tft.fillRect(0, BatteryScreen::LABEL_MAH_Y - 2, Display::SCREEN_W,
                 BatteryScreen::SEP_Y - (BatteryScreen::LABEL_MAH_Y - 2), Display::Colors::BG);

    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("REMAINING", 120, BatteryScreen::LABEL_MAH_Y, 2);

    int remaining = static_cast<int>(Config::BAT_CAPACITY_MAH * pct / 100.0f);
    char buf[24];
    sprintf(buf, "%d / %d mAh", remaining, Config::BAT_CAPACITY_MAH);
    Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
    Display::tft.drawCentreString(buf, 120, BatteryScreen::MAH_VAL_Y, 2);
}


static void drawPercent(int pct, uint16_t col) {
    Display::tft.fillRect(0, BatteryScreen::LABEL_PCT_Y - 2, Display::SCREEN_W,
                 Display::SCREEN_H - (BatteryScreen::LABEL_PCT_Y - 2), Display::Colors::BG);

    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("CHARGE", 120, BatteryScreen::LABEL_PCT_Y, 2);

    char buf[8];
    sprintf(buf, "%d%%", pct);
    Display::tft.setTextColor(col, Display::Colors::BG);
    Display::tft.drawCentreString(buf, 120, BatteryScreen::PCT_Y, 6);
}


void drawBatteryBase() {
    Display::tft.fillScreen(Display::Colors::BG);

    prevVoltage  = -1.0f;
    prevPct      = -1;
    voltFilled   = false;
    voltIdx      = 0;
    usbHighCount = 0;
    usbLowCount  = 0;
    _usbState    = false;
    prevUsbState = true;

    Display::drawHeader("BATTERY");

    // Static labels and separators
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("CHARGE LEVEL", 120, BatteryScreen::LABEL_CHG_Y, 1);
    drawBatteryShell();
    Display::tft.drawFastHLine(10, BatteryScreen::SEP_Y, Display::SCREEN_W - 20, Display::Colors::SEP);

    pinMode(Config::BAT_ADC_PIN, INPUT);
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
