#include <Arduino.h>
#include <Wire.h>
#include <esp_sleep.h>

#include "screen_manager.h"
#include "display.h"
#include "grid.h"
#include "settings.h"
#include "radar.h"
#include "touch.h"
#include "imu.h"
#include "tof.h"

// ── Loading screen ────────────────────────────────────────────────────────────
static constexpr int  LD_BAR_X = 20;
static constexpr int  LD_BAR_Y = 185;
static constexpr int  LD_BAR_W = 200;
static constexpr int  LD_BAR_H = 14;
static uint8_t        ld_prevPct = 0xFF;

namespace AsciiArt {
  const char *scoutLines[] = {
      "  _________                    __  ",
      " /   _____/ ____  ____  __ ___/  |_",
      " \\_____  \\_/ ___\\/  _ \\|  |  \\   __\\",
      " /        \\  \\__(  <_> )  |  /|  | ",
      "/_______  /\\___  >____/|____/ |__| ",
      "        \\/     \\/                  ",
  };
  const Display::AsciiArt scout = {scoutLines, 6, 4, 8, 64, 100};

  const char *echoLines[] = {
      "___________      .__          ",     "\\_   _____/ ____ |  |__   ____",
      " |    __)__/ ___\\|  |  \\ /  _ \\", " |        \\  \\___|   Y  (  <_> )",
      "/_______  /\\___  >___|  /\\____/",  "        \\/     \\/     \\/       ",
  };
const Display::AsciiArt echo = {echoLines, 6, 4, 8, 34, 50};
}

static void drawLoadingScreen() {
    Display::tft.fillScreen(Display::Colors::BG);

    Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
    Display::drawAsciiArt(AsciiArt::scout, Display::Colors::GREEN);

    Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
    Display::drawAsciiArt(AsciiArt::echo, Display::Colors::GREEN);

    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("BOOTING SENSOR FIRMWARE", 120, 162, 1);

    Display::tft.drawRect(LD_BAR_X - 1, LD_BAR_Y - 1,
                          LD_BAR_W + 2, LD_BAR_H + 2, Display::Colors::GREEN_DIM);
}

static void updateLoadingBar(uint8_t pct) {
    if (pct == ld_prevPct) return;
    ld_prevPct = pct;

    int filled = (int)(LD_BAR_W * pct / 100);
    // Filled portion
    Display::tft.fillRect(LD_BAR_X, LD_BAR_Y, filled, LD_BAR_H, Display::Colors::GREEN);
    // Empty portion
    if (filled < LD_BAR_W)
        Display::tft.fillRect(LD_BAR_X + filled, LD_BAR_Y,
                              LD_BAR_W - filled, LD_BAR_H, Display::Colors::BG);
    // Percentage text
    char buf[8]; sprintf(buf, "%d%%", pct);
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString(buf, 120, LD_BAR_Y + LD_BAR_H + 6, 1);
}
// ─────────────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  pinMode(42, OUTPUT);
  digitalWrite(42, LOW);
  touchInit();
  Display::initDisplay();
  loadSettings();
  buildGridTable();
  initBacklight();
  esp_sleep_enable_ext0_wakeup((gpio_num_t)Config::WAKE_PIN, 0);
  imuInit();

  drawLoadingScreen();
  updateLoadingBar(0);
  tofInit(updateLoadingBar);

  delay(250);
  Serial1.begin(Config::RADAR_BAUD, SERIAL_8N1, Config::RADAR_RX, Config::RADAR_TX);
  delay(500);
  radarInit();
  Serial.println("=== ECHO SCOUT ===");
  ScreenManager::switchScreen(Display::Screen::MENU);
}


void loop() {
  ScreenManager::tick();
  ScreenManager::handleTouch();
}
