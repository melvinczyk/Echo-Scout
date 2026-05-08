#ifndef SETTINGS_SCREEN_H
#define SETTINGS_SCREEN_H

#include "base/settings.h"
#include "base/display.h"

namespace SettingsScreen {
  constexpr int SET_ROW_H = 46;
  constexpr int SET_HEADER_H = 22;
  constexpr int SET_START_Y = 4;
  constexpr int SET_RESET_H = 44;

  constexpr int NUM_SETTING_ROWS = 22;

  static constexpr int ARROW_W = 52;
  static constexpr int ARROW_CX = ARROW_W / 2;
  static constexpr int ARROW_RX = Display::SCREEN_W - ARROW_W / 2;

  inline int settingsScrollY = 0;
}

struct SRow {
  const char *label;
  const char *desc;
  uint8_t *idx;
  uint8_t count;
  const char *(*getLabel)(uint8_t i);
  bool isSectionHeader;
};

int settingsTotalH();
int settingRowY(int row);
void drawSettingRowAt(int row, int y);
void drawSettingsScreen();
void handleSettingsTouch(int tx, int ty);

#endif
