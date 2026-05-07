#include "screens/tof_screen.h"
#include "tabs/tof_tabs.h"

void drawTofBase() {
    Display::tft.fillScreen(Display::Colors::BG);
    Display::drawHeader("TOF IMAGER");

    if (!TofState::ready) {
        Display::tft.setTextColor(Display::Colors::RED, Display::Colors::BG);
        Display::tft.drawCentreString("VL53L5CX NOT FOUND", 120, 150, 2);
        return;
    }

    static const TabManager::TabEntry tabs[] = {
        {"GRID", drawGridTab, tickGridTab, nullptr},
        {"ANGL", drawAngleTab, tickAngleTab, handleAngleTouch},
        {"DIST", drawDistTab, tickDistTab, handleDistTouch},
    };
    TabManager::init(tabs, 3);
}

void tickTof() {
    if (!TofState::ready) return;
    TabManager::tick();
}

void handleTofTouch(int tx, int ty) {
    TabManager::handleTouch(tx, ty);
}
