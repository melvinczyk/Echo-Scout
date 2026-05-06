#include <math.h>

#include "app_state.h"
#include "device_state.h"
#include "radar.h"
#include "menu_screen.h"
#include "settings_screen.h"
#include "imu_screen.h"
#include "horizon_screen.h"
#include "spirit_screen.h"
#include "touch.h"
#include "imu.h"
#include "battery_screen.h"
#include "display.h"
#include "power_screen.h"
#include <esp_sleep.h>


static bool wasTouched = false;


void setup() {
  Serial.begin(115200);
  pinMode(42, OUTPUT);
  digitalWrite(42, LOW);
  touchInit();
  Display::initDisplay();
  buildGridTable();
  initBacklight();
  esp_sleep_enable_ext0_wakeup((gpio_num_t)Config::WAKE_PIN, 0);
  imuInit();
  Serial1.begin(Config::RADAR_BAUD, SERIAL_8N1, Config::RADAR_RX, Config::RADAR_TX);
  delay(500);
  radarInit();
  Serial.println("=== ECHO SCOUT ===");
  startMenu();
}


static void tickScreens() {
  if (AppState::currentScreen == Display::Screen::MENU)
    tickMenu();
  if (AppState::currentScreen == Display::Screen::IMU)
    tickIMU();
  if (AppState::currentScreen == Display::Screen::BATTERY)
    tickBattery();
  if (AppState::currentScreen == Display::Screen::HORIZON)
    tickHorizon();
  if (AppState::currentScreen == Display::Screen::SPIRIT)
    tickSpirit();
}

static void handleTouch() {
  int tx, ty;
  bool touched = touchRead(tx, ty);

  if (AppState::currentScreen == Display::Screen::SETTINGS) {
    if (touched && !wasTouched) {
      touchStartY      = ty;
      touchStartScroll = settingsScrollY;
      touchMoved       = false;
    } else if (touched && wasTouched) {
      int drag = touchStartY - ty;
      if (abs(drag) > 8) {
        touchMoved = true;
        int maxScroll = max(0, settingsTotalH() -
                               (Display::SCREEN_H - Display::HEADER_H - SET_RESET_H));
        settingsScrollY = constrain(touchStartScroll + drag, 0, maxScroll);
        int visTop    = Display::HEADER_H;
        int visBottom = Display::SCREEN_H - SET_RESET_H;
        Display::tft.fillRect(0, visTop, Display::SCREEN_W, visBottom - visTop,
                              Display::Colors::BG);
        for (int i = 0; i < NUM_SETTING_ROWS; i++)
          drawSettingRowAt(i, settingRowY(i));
      }
    } else if (!touched && wasTouched) {
      if (!touchMoved)
        handleSettingsTouch(tx, ty);
    }
  } else {
    if (touched && !wasTouched) {
      if (AppState::currentScreen == Display::Screen::POWER_CONFIRM) {
        if (inRect(tx, ty, 20, 180, 200, 44))
          executePowerOff();
        if (inRect(tx, ty, 20, 236, 200, 44)) {
          AppState::currentScreen = Display::Screen::MENU;
          startMenu();
        }
      } else if (AppState::currentScreen == Display::Screen::MENU) {
        if (inRect(tx, ty, MenuScreen::LAUNCH_X,   MenuScreen::LAUNCH_Y,
                            MenuScreen::LAUNCH_W,   MenuScreen::LAUNCH_H)) {
          AppState::currentScreen = Display::Screen::RADAR;
          radarResetState();
          drawRadarBase();
        } else if (inRect(tx, ty, MenuScreen::SETTINGS_X, MenuScreen::SETTINGS_Y,
                                   MenuScreen::SETTINGS_W, MenuScreen::SETTINGS_BH)) {
          AppState::currentScreen = Display::Screen::SETTINGS;
          settingsScrollY = 0;
          drawSettingsScreen();
        } else if (inRect(tx, ty, MenuScreen::IMU_X, MenuScreen::IMU_Y,
                                   MenuScreen::IMU_W, MenuScreen::IMU_BH)) {
          AppState::currentScreen = Display::Screen::IMU;
          drawImuBase();
        } else if (inRect(tx, ty, MenuScreen::BATTERY_X, MenuScreen::BATTERY_Y,
                                   MenuScreen::BATTERY_W, MenuScreen::BATTERY_H)) {
          AppState::currentScreen = Display::Screen::BATTERY;
          drawBatteryBase();
        } else if (inRect(tx, ty, MenuScreen::HORIZON_X, MenuScreen::HORIZON_Y,
                                   MenuScreen::HORIZON_W, MenuScreen::HORIZON_H)) {
          AppState::currentScreen = Display::Screen::HORIZON;
          drawHorizonBase();
        } else if (inRect(tx, ty, MenuScreen::SPIRIT_X, MenuScreen::SPIRIT_Y,
                                   MenuScreen::SPIRIT_W, MenuScreen::SPIRIT_H)) {
          AppState::currentScreen = Display::Screen::SPIRIT;
          drawSpiritBase();
        } else if (inRect(tx, ty, MenuScreen::POWER_X, MenuScreen::POWER_Y,
                                   MenuScreen::POWER_W, MenuScreen::POWER_H)) {
          AppState::currentScreen = Display::Screen::POWER_CONFIRM;
          drawPowerConfirm();
        }
      } else if (AppState::currentScreen == Display::Screen::SPIRIT) {
        handleSpiritTouch(tx, ty);
        if (inRect(tx, ty, 3, 3, 64, Display::HEADER_H - 6)) {
          AppState::currentScreen = Display::Screen::MENU;
          startMenu();
        }
      } else {
        if (inRect(tx, ty, 3, 3, 64, Display::HEADER_H - 6)) {
          AppState::currentScreen = Display::Screen::MENU;
          startMenu();
        }
      }
    }
  }
  wasTouched = touched;
}


void loop() {
  radarProcessSerial();
  tickScreens();
  handleTouch();
}
