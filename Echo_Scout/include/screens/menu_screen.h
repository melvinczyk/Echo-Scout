#ifndef MENU_H
#define MENU_H

#include <stdint.h>

namespace MenuScreen {
    constexpr int LAUNCH_X = 12,  LAUNCH_Y = 160, LAUNCH_W = 216, LAUNCH_H = 44;
    constexpr int MAP_X = 8,   MAP_Y = 206, MAP_W = 108, MAP_H = 30;
    constexpr int SURV_X = 120, SURV_Y = 206, SURV_W = 108, SURV_H = 30;
    constexpr int SCANNER_X = 8, SCANNER_Y = 238, SCANNER_W = 108, SCANNER_H = 26;
    constexpr int ATTITUDE_X = 120, ATTITUDE_Y = 238, ATTITUDE_W = 108, ATTITUDE_H = 26;
    constexpr int SETTINGS_X = 8, SETTINGS_Y = 266, SETTINGS_W = 108, SETTINGS_BH = 24;
    constexpr int BATTERY_X = 120, BATTERY_Y = 266, BATTERY_W = 108, BATTERY_H = 24;
    constexpr int POWER_X = 40, POWER_Y = 294, POWER_W = 160, POWER_H = 16;
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
