#include "screens/power_screen.h"
#include "base/display.h"
#include "base/settings.h"
#include <esp_sleep.h>
#include <esp_system.h>


void drawPowerConfirm() {
    Display::tft.fillScreen(Display::Colors::BG);
    Display::tft.drawRect(2, 2, 236, 316, Display::Colors::RED);
    Display::tft.drawRect(4, 4, 232, 312, Display::Colors::RED);

    Display::tft.setTextColor(Display::Colors::RED, Display::Colors::BG);
    Display::tft.drawCentreString("POWER", 120, 55, 4);
    Display::tft.drawCentreString("OPTIONS", 120, 90, 4);

    Display::tft.drawRect(20, 148, 200, 44, Display::Colors::AMBER);
    Display::tft.drawRect(22, 150, 196, 40, Display::Colors::AMBER);
    Display::tft.setTextColor(Display::Colors::AMBER, Display::Colors::BG);
    Display::tft.drawCentreString("[ RESTART ]", 120, 162, 2);
    Display::tft.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
    Display::tft.drawCentreString("rescans all devices", 120, 185, 1);

    Display::tft.drawRect(20, 204, 200, 44, Display::Colors::RED);
    Display::tft.drawRect(22, 206, 196, 40, Display::Colors::RED);
    Display::tft.setTextColor(Display::Colors::RED, Display::Colors::BG);
    Display::tft.drawCentreString("[ POWER OFF ]", 120, 218, 2);
    Display::tft.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
    Display::tft.drawCentreString("tap screen to wake", 120, 241, 1);

    Display::tft.drawRect(20, 260, 200, 44, Display::Colors::GREEN_DIM);
    Display::tft.drawRect(22, 262, 196, 40, Display::Colors::GREEN_DIM);
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("[ CANCEL ]", 120, 274, 2);
}


void executeRestart() {
    int startBright = blVals[cfg.brightnessIdx];
    for (int v = startBright; v >= 0; v -= 8) {
        ledcWrite(Config::BL_PWM_CH, max(v, 0));
        delay(8);
    }
    ledcWrite(Config::BL_PWM_CH, 0);
    delay(200);
    esp_restart();
}


void executePowerOff() {
    int startBright = blVals[cfg.brightnessIdx];
    for (int v = startBright; v >= 0; v -= 6) {
        ledcWrite(Config::BL_PWM_CH, max(v, 0));
        delay(12);
    }
    ledcWrite(Config::BL_PWM_CH, 0);

    ledcWrite(Config::BL_PWM_CH, 8);
    Display::tft.fillScreen(Display::Colors::BG);
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("POWERING OFF", 120, 130, 2);
    Display::tft.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
    Display::tft.drawCentreString("Tap screen to wake", 120, 165, 1);
    delay(1800);

    ledcWrite(Config::BL_PWM_CH, 0);
    esp_deep_sleep_start();
}
