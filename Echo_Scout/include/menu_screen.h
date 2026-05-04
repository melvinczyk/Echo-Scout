#ifndef MENU_H
#define MENU_H

#include "radar_screen.h"

constexpr uint8_t NUM_BRACKET_SEGS = 8;

enum class MenuAnimPhase : uint8_t { Corners, Hold };

struct BracketSeg {
  int x, y, len;
  bool isH;
  uint16_t col;
};

struct MenuState {
  MenuAnimPhase animPhase;
  int           cornerStep;
  bool          launchBlink;
  unsigned long timer;
  float         scoutGlow;
  bool          scoutGlowUp;
  unsigned long scoutGlowTimer;
  BracketSeg    bracketSegs[NUM_BRACKET_SEGS];
};

extern MenuState menuState;

void drawLaunchButton(bool highlight);
void drawSettingsMenuButton();
void drawImuMenuButton();
void drawAsciiArt(const char** lines, int numLines, int charW, int lineH,
                  int startX, int startY, uint16_t col);
void drawScoutArt(uint16_t col);
void startMenu();
void tickMenu();

#endif
