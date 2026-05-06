#ifndef MENU_H
#define MENU_H

#include "radar_screen.h"

namespace MenuScreen {
    constexpr int LAUNCH_X = 12,  LAUNCH_Y = 160, LAUNCH_W = 216, LAUNCH_H = 48;
    // Row 1: utility buttons
    constexpr int SETTINGS_X =  8,  SETTINGS_Y = 214, SETTINGS_W = 72, SETTINGS_BH = 26;
    constexpr int IMU_X      = 84,  IMU_Y      = 214, IMU_W      = 72, IMU_BH      = 26;
    constexpr int BATTERY_X  = 160, BATTERY_Y  = 214, BATTERY_W  = 72, BATTERY_H   = 26;
    // Row 2: IMU feature buttons
    constexpr int HORIZON_X  =  8,  HORIZON_Y  = 246, HORIZON_W  = 72, HORIZON_H   = 26;
    constexpr int SPIRIT_X   = 84,  SPIRIT_Y   = 246, SPIRIT_W   = 72, SPIRIT_H    = 26;
    constexpr int COMPASS_X  = 160, COMPASS_Y  = 246, COMPASS_W  = 72, COMPASS_H   = 26;
    // Power
    constexpr int POWER_X = 40,  POWER_Y = 295, POWER_W = 160, POWER_H = 18;
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