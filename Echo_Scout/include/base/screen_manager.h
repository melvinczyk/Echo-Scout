#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include "base/display.h"

namespace ScreenManager {
    void tick();
    void handleTouch();
    void switchScreen(Display::Screen s);
}

#endif
