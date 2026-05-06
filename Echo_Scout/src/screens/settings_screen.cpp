#include "settings_screen.h"
#include "screen_manager.h"
#include "display.h"
#include "touch.h"


int settingsScrollY = 0;


static const char *htLabel(uint8_t i) {
  static char b[4];
  sprintf(b, "%d", hitsVals[i]);
  return b;
}
static const char *drLabel(uint8_t i) {
  static char b[4];
  sprintf(b, "%d", dropsVals[i]);
  return b;
}
static const char *mrLabel(uint8_t i) { return minRangeLabels[i]; }
static const char *maLabel(uint8_t i) { return maxAngleLabels[i]; }
static const char *arLabel(uint8_t i) { return accRangeLabels[i]; }
static const char *sgLabel(uint8_t i) { return speedGateLabels[i]; }
static const char *mvLabel(uint8_t i) { return moveThreshLabels[i]; }
static const char *smLabel(uint8_t i) { return i ? "ON" : "OFF"; }
static const char *secLabel(uint8_t i) { return ""; }
static const char *blLabel(uint8_t i)  { return blLabels[i]; }
static const char *slLabel(uint8_t i)  { return sleepTimeoutLabels[i]; }


SRow settingRows[NUM_SETTING_ROWS] = {
    {"-- DETECTION --", "", nullptr, 0, secLabel, true},
    {"MIN FRAMES", "frames to confirm target", &cfg.hitsIdx, NUM_HITS, htLabel,
     false},
    {"DROP FRAMES", "frames before target lost", &cfg.dropsIdx, NUM_DROPS,
     drLabel, false},
    {"SPEED GATE", "max speed cm/s, OFF=none", &cfg.speedGateIdx, NUM_SG,
     sgLabel, false},
    {"-- GEOMETRY --", "", nullptr, 0, secLabel, true},
    {"MIN RANGE", "ignore below this dist", &cfg.minRangeIdx, NUM_MR, mrLabel,
     false},
    {"MAX ANGLE", "cone width in degrees", &cfg.maxAngleIdx, NUM_MA, maLabel,
     false},
    {"ACC RANGE", "precise tracking limit", &cfg.accRangeIdx, NUM_AR, arLabel,
     false},
    {"-- MOVEMENT --", "", nullptr, 0, secLabel, true},
    {"MOVE THRESH", "min move to redraw blip", &cfg.moveThreshIdx, NUM_MVTHR,
     mvLabel, false},
    {"SMOOTHING", "position blend at 0.4", (uint8_t *)&cfg.smoothingOn, 2,
     smLabel, false},
    {"-- DISPLAY --", "", nullptr, 0, secLabel, true},
    {"BACKLIGHT", "screen brightness", &cfg.brightnessIdx, NUM_BL,
     blLabel, false},
    {"-- POWER --", "", nullptr, 0, secLabel, true},
    {"SLEEP", "auto sleep when idle", &cfg.sleepTimeoutIdx, NUM_SLEEP,
     slLabel, false},
};


int settingsTotalH() {
  int h = 0;
  for (int i = 0; i < NUM_SETTING_ROWS; i++)
    h += settingRows[i].isSectionHeader ? SET_HEADER_H : SET_ROW_H;
  return h;
}

int settingRowY(int row) {
  int y = Display::HEADER_H + SET_START_Y - settingsScrollY;
  for (int i = 0; i < row; i++)
    y += settingRows[i].isSectionHeader ? SET_HEADER_H : SET_ROW_H;
  return y;
}


void drawSettingRowAt(int row, int y) {
  SRow &r = settingRows[row];
  int visBottom = Display::SCREEN_H - SET_RESET_H;
  int rowH = r.isSectionHeader ? SET_HEADER_H : SET_ROW_H;

  if (y + rowH < Display::HEADER_H || y > visBottom)
    return;
  int clipTop = max(y, Display::HEADER_H);
  int clipBottom = min(y + rowH, visBottom);
  if (clipTop >= clipBottom)
    return;

  int sprH = clipBottom - clipTop;
  Display::spr.deleteSprite();
  Display::spr.createSprite(Display::SCREEN_W, sprH);
  Display::spr.fillSprite(Display::Colors::BG);

  int localY = y - clipTop;

  if (r.isSectionHeader) {
    Display::spr.drawFastHLine(0, localY + SET_HEADER_H - 1, Display::SCREEN_W,
                      Display::Colors::AMBER);
    Display::spr.setTextColor(Display::Colors::AMBER, Display::Colors::BG);
    Display::spr.drawString(r.label, 8, localY + 5, 1);
  } else {
    Display::spr.drawFastHLine(0, sprH - 2, Display::SCREEN_W, Display::Colors::GREEN_FAINT);
    Display::spr.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::spr.drawString(r.label, 6, localY + 6, 1);
    Display::spr.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
    Display::spr.drawString(r.desc, 6, localY + 18, 1);

    int bw = 68, bh = 30, bx = Display::SCREEN_W - bw - 4, by2 = localY + 7;
    Display::spr.drawRoundRect(bx, by2, bw, bh, 3, Display::Colors::GREEN_DIM);
    Display::spr.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::spr.drawString("<", bx + 4, by2 + 10, 1);
    Display::spr.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
    Display::spr.drawCentreString(r.getLabel(*r.idx), bx + bw / 2, by2 + 10, 1);
    Display::spr.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::spr.drawRightString(">", bx + bw - 3, by2 + 10, 1);
  }

  Display::spr.pushSprite(0, clipTop);
  Display::spr.deleteSprite();
}


void drawSettingsScreen() {
  Display::tft.fillScreen(Display::Colors::BG);
  Display::tft.fillRect(0, 0, Display::SCREEN_W, Display::HEADER_H, Display::Colors::BG);
  Display::tft.drawFastHLine(0, Display::HEADER_H - 1, Display::SCREEN_W, Display::Colors::SEP);
  Display::tft.drawRoundRect(3, 3, 58, Display::HEADER_H - 6, 3, Display::Colors::GREEN_DIM);
  Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
  Display::tft.drawCentreString("< BACK", 32, 12, 1);
  Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
  Display::tft.drawCentreString("SETTINGS", 124, 8, 2);

  for (int i = 0; i < NUM_SETTING_ROWS; i++)
    drawSettingRowAt(i, settingRowY(i));

  Display::tft.fillRect(0, Display::SCREEN_H - SET_RESET_H, Display::SCREEN_W, SET_RESET_H,
               Display::Colors::BG);
  Display::tft.drawFastHLine(0, Display::SCREEN_H - SET_RESET_H, Display::SCREEN_W,
                    Display::Colors::SEP);
  int ry = Display::SCREEN_H - SET_RESET_H + 5;
  Display::tft.drawRect(24, ry, 192, 30, Display::Colors::GREEN_DIM);
  Display::tft.drawRect(26, ry + 2, 188, 26, Display::Colors::GREEN_FAINT);
  Display::tft.setTextColor(Display::Colors::RED, Display::Colors::BG);
  Display::tft.drawCentreString("[ RESET DEFAULTS ]", 120, ry + 8, 2);
}


void handleSettingsTouch(int tx, int ty) {
  if (inRect(tx, ty, 3, 3, 58, Display::HEADER_H - 6)) {
    applySettings();
    ScreenManager::switchScreen(Display::Screen::MENU);
    return;
  }
  int ry = Display::SCREEN_H - SET_RESET_H + 5;
  if (inRect(tx, ty, 24, ry, 192, 30)) {
    cfg = DEFAULT_SETTINGS;
    settingsScrollY = 0;
    applySettings();
    drawSettingsScreen();
    return;
  }

  for (int i = 0; i < NUM_SETTING_ROWS; i++) {
    if (settingRows[i].isSectionHeader)
      continue;
    int y = settingRowY(i);
    if (ty < y || ty >= y + SET_ROW_H - 2)
      continue;
    if (y < Display::HEADER_H || y > Display::SCREEN_H - SET_RESET_H)
      continue;

    int bw = 68, bh = 24, bx = Display::SCREEN_W - bw - 4, by2 = y + 6;
    if (inRect(tx, ty, bx, by2, bw / 3, bh)) {
      if (*settingRows[i].idx > 0)
        (*settingRows[i].idx)--;
      else
        *settingRows[i].idx = settingRows[i].count - 1;
      drawSettingRowAt(i, y);
      applySettings();
    } else if (inRect(tx, ty, bx + bw * 2 / 3, by2, bw / 3, bh)) {
      if (*settingRows[i].idx < settingRows[i].count - 1)
        (*settingRows[i].idx)++;
      else
        *settingRows[i].idx = 0;
      drawSettingRowAt(i, y);
      applySettings();
    }
    return;
  }
}
