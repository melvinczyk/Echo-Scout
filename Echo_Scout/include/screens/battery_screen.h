#ifndef BATTERY_SCREEN_H
#define BATTERY_SCREEN_H

#include <cstdint>
#include "base/display.h"

namespace BatteryScreen {
    constexpr uint8_t LABEL_CHG_Y = 58;
    constexpr uint8_t BAR_X = 10;
    constexpr uint8_t BAR_Y = 72;
    constexpr uint8_t BAR_W = 196;
    constexpr uint8_t BAR_H = 46;
    constexpr uint8_t BAR_CORNER = 5;
    constexpr uint8_t TIP_W = 8;
    constexpr uint8_t TIP_H = 18;
    constexpr uint8_t TIP_X = BAR_X + BAR_W;
    constexpr int TIP_Y = (BAR_Y + (BAR_H - TIP_H) / 2);
    constexpr uint8_t LABEL_VOLT_Y = 128;
    constexpr uint8_t VOLT_Y = 146;
    constexpr uint8_t LABEL_MAH_Y = 180;
    constexpr uint8_t MAH_VAL_Y = 198;
    constexpr uint8_t SEP_Y = 220;
    constexpr uint8_t LABEL_PCT_Y = 228;
    constexpr uint8_t PCT_Y = 246;
};

void drawBatteryBase();
void tickBattery();

#endif
