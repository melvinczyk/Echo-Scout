#ifndef MENU_H
#define MENU_H

#include <stdint.h>

namespace MenuScreen {
    // Main feature buttons (full-width)
    constexpr int LAUNCH_X   = 12,  LAUNCH_Y   = 160, LAUNCH_W   = 216, LAUNCH_H   = 42;
    constexpr int MAP_X      = 12,  MAP_Y      = 204, MAP_W      = 216, MAP_H      = 28;
    // Tools row (2 buttons)
    constexpr int SCANNER_X  = 8,   SCANNER_Y  = 234, SCANNER_W  = 110, SCANNER_H  = 24;
    constexpr int ATTITUDE_X = 122, ATTITUDE_Y = 234, ATTITUDE_W = 110, ATTITUDE_H = 24;
    // Bottom utility row
    constexpr int SETTINGS_X  = 8,   SETTINGS_Y  = 272, SETTINGS_W  = 110, SETTINGS_BH = 20;
    constexpr int BATTERY_X   = 122, BATTERY_Y   = 272, BATTERY_W   = 110, BATTERY_H   = 20;
    // Power
    constexpr int POWER_X = 40, POWER_Y = 302, POWER_W = 160, POWER_H = 14;
    // CALIBRATE header button (top-right corner of menu, same coords as Display::CAL_BTN_*)
    constexpr int CAL_X = 172, CAL_Y = 5, CAL_W = 64, CAL_H = 20;
}

constexpr uint8_t NUM_BRACKET_SEGS = 8;

enum class MenuAnimPhase : uint8_t { Corners, Hold };

struct BracketSeg {
  int x, y, len;
  bool isH;
  uint16_t col;
};

struct MenuState {
  MenuAnimPhase animPhase;
  int cornerStep;
  bool launchBlink;
  unsigned long timer;
  float scoutGlow;
  bool scoutGlowUp;
  unsigned long scoutGlowTimer;
  BracketSeg bracketSegs[NUM_BRACKET_SEGS];
};

void drawLaunchButton(bool highlight);
void drawPowerButton();
void startMenu();
void tickMenu();

#endif
