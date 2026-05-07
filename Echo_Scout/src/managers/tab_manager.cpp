#include "base/tab_manager.h"
#include "base/display.h"
#include "base/app_state.h"

namespace {
    static const TabManager::TabEntry* _tabs = nullptr;
    static uint8_t _count = 0;
    static uint8_t _active = 0;
    static int _tabW = 0;
}

void TabManager::drawTabBar() {
    Display::tft.fillRect(0, TAB_Y, Display::SCREEN_W, TAB_H, Display::Colors::BG);
    Display::tft.drawFastHLine(0, TAB_Y, Display::SCREEN_W, Display::Colors::SEP);
    for (int t = 0; t < _count; t++) {
        int x = t * _tabW;
        bool active = (t == _active);
        if (active) {
            Display::tft.fillRect(x, TAB_Y+1, _tabW-1, TAB_H-1, Display::Colors::GREEN_FAINT);
            Display::tft.drawFastHLine(x, TAB_Y, _tabW-1, Display::Colors::GREEN);
            Display::tft.setTextColor(Display::Colors::GREEN, (uint16_t)Display::Colors::GREEN_FAINT);
        } else {
            Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
        }
        Display::tft.drawCentreString(_tabs[t].label, x + _tabW/2, TAB_Y + 14, 1);
    }
}

void TabManager::init(const TabEntry* tabs, uint8_t count, uint8_t defaultTab) {
    _tabs = tabs;
    _count = count;
    _active = defaultTab;
    _tabW = Display::SCREEN_W / count;
    AppState::currentTab = defaultTab;
    drawTabBar();
    if (_tabs[_active].draw) _tabs[_active].draw();
}

void TabManager::switchTab(uint8_t idx) {
    if (idx >= _count || idx == _active) return;
    _active = idx;
    AppState::currentTab = idx;
    drawTabBar();
    if (_tabs[_active].draw) _tabs[_active].draw();
}

void TabManager::tick() {
    if (!_tabs) return;
    if (_tabs[_active].tick) _tabs[_active].tick();
}

bool TabManager::handleTouch(int tx, int ty) {
    if (!_tabs) return false;
    if (ty >= TAB_Y) {
        int t = constrain(tx / _tabW, 0, _count - 1);
        switchTab((uint8_t)t);
        return true;
    }
    if (_tabs[_active].onTouch) _tabs[_active].onTouch(tx, ty);
    return false;
}

uint8_t TabManager::activeIndex() {
    return _active;
}
