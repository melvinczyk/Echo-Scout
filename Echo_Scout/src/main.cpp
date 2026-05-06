#include <Arduino.h>
#include <esp_sleep.h>

#include "screen_manager.h"
#include "display.h"
#include "grid.h"
#include "settings.h"
#include "radar.h"
#include "touch.h"
#include "imu.h"


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
