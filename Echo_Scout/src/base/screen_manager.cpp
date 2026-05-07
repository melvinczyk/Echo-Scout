#include "screen_manager.h"
#include "app_state.h"
#include "display.h"
#include "touch.h"
#include "radar.h"
#include "settings.h"
#include "device_state.h"
#include "menu_screen.h"
#include "radar_screen.h"
#include "settings_screen.h"
#include "imu_screen.h"
#include "battery_screen.h"
#include "power_screen.h"
#include "spirit_screen.h"
#include "tof_screen.h"
#include <esp_sleep.h>


static void menuOnTouch(int tx, int ty) {
    if (inRect(tx, ty, MenuScreen::LAUNCH_X, MenuScreen::LAUNCH_Y,
                        MenuScreen::LAUNCH_W, MenuScreen::LAUNCH_H)) {
        radarResetState();
        ScreenManager::switchScreen(Display::Screen::RADAR);
    } else if (inRect(tx, ty, MenuScreen::MAP_X, MenuScreen::MAP_Y,
                               MenuScreen::MAP_W, MenuScreen::MAP_H)) {
        // 3D MAP — routes to TOF screen (MAP tab coming soon)
        ScreenManager::switchScreen(Display::Screen::TOF);
    } else if (inRect(tx, ty, MenuScreen::SCANNER_X, MenuScreen::SCANNER_Y,
                               MenuScreen::SCANNER_W, MenuScreen::SCANNER_H)) {
        ScreenManager::switchScreen(Display::Screen::TOF);
    } else if (inRect(tx, ty, MenuScreen::ATTITUDE_X, MenuScreen::ATTITUDE_Y,
                               MenuScreen::ATTITUDE_W, MenuScreen::ATTITUDE_H)) {
        ScreenManager::switchScreen(Display::Screen::IMU);
    } else if (inRect(tx, ty, MenuScreen::BATTERY_X, MenuScreen::BATTERY_Y,
                               MenuScreen::BATTERY_W, MenuScreen::BATTERY_H)) {
        ScreenManager::switchScreen(Display::Screen::BATTERY);
    } else if (inRect(tx, ty, MenuScreen::SETTINGS_X, MenuScreen::SETTINGS_Y,
                               MenuScreen::SETTINGS_W, MenuScreen::SETTINGS_BH)) {
        settingsScrollY = 0;
        ScreenManager::switchScreen(Display::Screen::SETTINGS);
    } else if (inRect(tx, ty, MenuScreen::POWER_X, MenuScreen::POWER_Y,
                               MenuScreen::POWER_W, MenuScreen::POWER_H)) {
        ScreenManager::switchScreen(Display::Screen::POWER_CONFIRM);
    }
}

static void powerOnTouch(int tx, int ty) {
    if (inRect(tx, ty, 20, 180, 200, 44))
        executePowerOff();
    else if (inRect(tx, ty, 20, 236, 200, 44))
        ScreenManager::switchScreen(Display::Screen::MENU);
}

static void spiritOnTouch(int tx, int ty) {
    handleSpiritTouch(tx, ty);
}


namespace {

struct ScreenEntry {
    void (*enter)();
    void (*tick)();
    void (*onTouch)(int tx, int ty);
};

// Init, tick, touch
static const ScreenEntry screenTable[] = {
    { startMenu,          tickMenu,    menuOnTouch  },  // MENU
    { drawRadarBase,      tickRadar,   nullptr      },  // RADAR
    { drawSettingsScreen, nullptr,     nullptr      },  // SETTINGS
    { drawImuBase,        tickIMU,     handleImuTouch},  // IMU
    { drawBatteryBase,    tickBattery, nullptr      },  // BATTERY
    { drawPowerConfirm,   nullptr,     powerOnTouch },  // POWER_CONFIRM
    { drawSpiritBase,     tickSpirit,  spiritOnTouch},  // SPIRIT
    { drawTofBase,        tickTof,     handleTofTouch}, // TOF
};

}

void ScreenManager::switchScreen(Display::Screen s) {
    AppState::currentScreen = s;
    if (screenTable[s].enter)
        screenTable[s].enter();
}

void ScreenManager::tick() {
    uint32_t sleepMs = cfgSleepTimeoutMs();
    if (sleepMs > 0 && ImuState::ready && !RadarState::present &&
        (millis() - ImuState::lastMotionMs) > sleepMs) {
        esp_deep_sleep_start();
    }

    auto fn = screenTable[AppState::currentScreen].tick;
    if (fn) fn();
}

void ScreenManager::handleTouch() {
    static bool wasTouched  = false;
    static int  startY      = 0;
    static int  startScroll = 0;
    static bool dragMoved   = false;

    int tx, ty;
    bool touched  = touchRead(tx, ty);
    if (touched) ImuState::lastMotionMs = millis();
    bool pressed  = touched && !wasTouched;
    bool held     = touched &&  wasTouched;
    bool released = !touched && wasTouched;

    if (AppState::currentScreen == Display::Screen::SETTINGS) {
        if (pressed) {
            startY      = ty;
            startScroll = settingsScrollY;
            dragMoved   = false;
        } else if (held) {
            int drag = startY - ty;
            if (abs(drag) > 8) {
                dragMoved = true;
                int maxScroll = max(0, settingsTotalH() -
                                       (Display::SCREEN_H - Display::HEADER_H - SET_RESET_H));
                settingsScrollY = constrain(startScroll + drag, 0, maxScroll);
                Display::tft.fillRect(0, Display::HEADER_H, Display::SCREEN_W,
                                      Display::SCREEN_H - SET_RESET_H - Display::HEADER_H,
                                      Display::Colors::BG);
                for (int i = 0; i < NUM_SETTING_ROWS; i++)
                    drawSettingRowAt(i, settingRowY(i));
            }
        } else if (released && !dragMoved) {
            handleSettingsTouch(tx, ty);
        }
    } else if (pressed) {
        // Universal back button
        if (AppState::currentScreen != Display::Screen::MENU &&
            AppState::currentScreen != Display::Screen::POWER_CONFIRM &&
            inRect(tx, ty, 3, 3, 64, Display::HEADER_H - 6)) {
            switchScreen(Display::Screen::MENU);
        } else if (AppState::currentScreen != Display::Screen::MENU &&
                   AppState::currentScreen != Display::Screen::POWER_CONFIRM &&
                   AppState::currentScreen != Display::SPIRIT &&
                   inRect(tx, ty, Display::CAL_BTN_X, Display::CAL_BTN_Y, Display::CAL_BTN_W, Display::CAL_BTN_H)){
            switchScreen(Display::Screen::SPIRIT);
        }
        else {
            auto fn = screenTable[AppState::currentScreen].onTouch;
            if (fn) fn(tx, ty);
        }
    }

    wasTouched = touched;
}
