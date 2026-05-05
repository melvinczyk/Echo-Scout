#include "power_screen.h"
#include "display.h"
#include "settings.h"
#include <esp_sleep.h>


void drawPowerConfirm() {
    Display::tft.fillScreen(Display::Colors::BG);
    Display::tft.drawRect(2, 2, 236, 316, Display::Colors::RED);
    Display::tft.drawRect(4, 4, 232, 312, Display::Colors::RED);

    Display::tft.setTextColor(Display::Colors::RED, Display::Colors::BG);
    Display::tft.drawCentreString("POWER OFF?", 120, 70, 4);

    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("Press BOOT button to", 120, 130, 1);
    Display::tft.drawCentreString("wake the device again.", 120, 145, 1);

    // CONFIRM button
    Display::tft.drawRect(20, 180, 200, 44, Display::Colors::RED);
    Display::tft.drawRect(22, 182, 196, 40, Display::Colors::RED);
    Display::tft.setTextColor(Display::Colors::RED, Display::Colors::BG);
    Display::tft.drawCentreString("[ CONFIRM OFF ]", 120, 194, 2);

    // CANCEL button
    Display::tft.drawRect(20, 236, 200, 44, Display::Colors::GREEN_DIM);
    Display::tft.drawRect(22, 238, 196, 40, Display::Colors::GREEN_DIM);
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("[ CANCEL ]", 120, 250, 2);
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
    Display::tft.drawCentreString("Hold BOOT button", 120, 165, 1);
    Display::tft.drawCentreString("to wake device", 120, 180, 1);
    delay(1800);

    ledcWrite(Config::BL_PWM_CH, 0);
    esp_deep_sleep_start();
}
