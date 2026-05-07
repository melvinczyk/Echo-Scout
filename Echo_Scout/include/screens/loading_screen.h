#ifndef LOADING_SCREEN_H
#define LOADING_SCREEN_H

#include <cstdint>

namespace LoadingScreen {
    constexpr uint8_t LD_BAR_X = 20;
    constexpr uint8_t LD_BAR_Y = 185;
    constexpr uint8_t LD_BAR_W = 200;
    constexpr uint8_t LD_BAR_H = 14;
    inline uint8_t ld_prevPct = 0xFF;
};

void drawLoadingScreen();
void updateLoadingBar(uint8_t pct);

#endif
