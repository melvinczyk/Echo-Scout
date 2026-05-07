#include <Arduino.h>
#include <Wire.h>
#include <esp_sleep.h>

#include "base/screen_manager.h"
#include "screens/loading_screen.h"
#include "base/display.h"
#include "base/grid.h"
#include "base/settings.h"
#include "devices/radar.h"
#include "devices/touch.h"
#include "devices/imu.h"
#include "devices/tof.h"


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
