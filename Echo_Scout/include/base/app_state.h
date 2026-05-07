#ifndef APP_STATE_H
#define APP_STATE_H

#include "base/display.h"

namespace AppState {
    inline Display::Screen currentScreen = Display::Screen::MENU;
    inline uint8_t currentTab = 0;
}
#endif
