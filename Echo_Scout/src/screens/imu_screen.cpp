#include "screens/imu_screen.h"
#include "tabs/imu_tabs.h"

void drawImuBase() {
    Display::tft.fillScreen(Display::Colors::BG);
    Display::drawHeader("ATTITUDE");

    if (!ImuState::ready) {
        Display::tft.setTextColor(Display::Colors::RED, Display::Colors::BG);
        Display::tft.drawCentreString("BNO085 NOT FOUND", 120, 150, 2);
        Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
        Display::tft.drawCentreString("CHECK WIRING AND RESET", 120, 175, 1);
        return;
    }

    static const TabManager::TabEntry tabs[] = {
        {"CUBE", CubeTab::drawTab,  CubeTab::tick,  nullptr},
        {"PLMB", PlumbTab::drawTab, PlumbTab::tick, nullptr},
        {"SCPE", ScopeTab::drawTab, ScopeTab::tick, nullptr},
    };
    TabManager::init(tabs, 3);
}

void tickIMU() {
    if (!ImuState::ready) return;
    TabManager::tick();
}

void handleImuTouch(int tx, int ty) {
    TabManager::handleTouch(tx, ty);
}
