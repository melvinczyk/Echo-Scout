#ifndef SETTINGS_SCREEN_H
#define SETTINGS_SCREEN_H
#include "config.h"
#include "display.h"
#include "settings.h"
#include "touch.h"
#include <Arduino.h>


constexpr int SET_ROW_H = 46;
constexpr int SET_HEADER_H = 22;
constexpr int SET_START_Y = 4;
constexpr int SET_RESET_H = 44;

constexpr int NUM_SETTING_ROWS = 11;

struct SRow {
  const char *label;
  const char *desc;
  uint8_t *idx;
  uint8_t count;
  const char *(*getLabel)(uint8_t i);
  bool isSectionHeader;
};

extern int settingsScrollY;
extern int touchStartY;
extern int touchStartScroll;
extern bool touchMoved;

extern SRow settingRows[NUM_SETTING_ROWS];

int settingsTotalH();
int settingRowY(int row);
void drawSettingRowAt(int row, int y);
void drawSettingsScreen();
void handleSettingsTouch(int tx, int ty);

#endif