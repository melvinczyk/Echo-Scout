#ifndef SPIRIT_SCREEN_H
#define SPIRIT_SCREEN_H

#include "device_state.h"

namespace SpiritScreen {
    constexpr int CX     = 120;
    constexpr int CY     = 155;
    constexpr int RADIUS = 88;
    constexpr int DASH_Y = 255;
    constexpr int BALL_R = 10;
}

void drawSpiritBase();
void tickSpirit();
void handleSpiritTouch(int tx, int ty);

#endif
