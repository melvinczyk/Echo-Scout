#ifndef MENU_H
#define MENU_H

#include "config.h"
#include "settings.h"
#include "display.h"
#include <Arduino.h>
#include <TFT_eSPI.h>

#define MENU_ANIM_CORNERS 0
#define MENU_ANIM_HOLD 1
#define NUM_BRACKET_SEGS 8

struct BracketSeg {
  int x, y, len, isH;
  uint16_t col;
};

extern int menuAnimPhase;
extern int cornerStep;
extern bool launchBlink;
extern unsigned long menuTimer;
extern float scoutGlow;
extern bool scoutGlowUp;
extern unsigned long scoutGlowTimer;
extern BracketSeg bracketSegs[NUM_BRACKET_SEGS];

void drawLaunchButton(bool highlight);
void drawSettingsMenuButton();
void drawAsciiArt(const char **lines, int numLines, int charW, int lineH,
                  int startX, int startY, uint16_t col);
void drawScoutArt(uint16_t col);
void startMenu();
void tickMenu();
void drawImuButton();

#endif