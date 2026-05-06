#ifndef COMPASS_SCREEN_H
#define COMPASS_SCREEN_H

namespace CompassScreen {
    constexpr int CX     = 120;
    constexpr int CY     = 152;
    constexpr int RADIUS = 88;
    constexpr int DASH_Y = 252;
}

void drawCompassBase();
void tickCompass();

#endif
