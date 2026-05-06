#ifndef SPIRIT_SCREEN_H
#define SPIRIT_SCREEN_H

namespace SpiritScreen {
    constexpr int CX     = 120;
    constexpr int CY     = 155;
    constexpr int RADIUS = 88;
    constexpr int DASH_Y = 255;
    constexpr int BALL_R = 10;
}

void drawSpiritBase();
void tickSpirit();

#endif
