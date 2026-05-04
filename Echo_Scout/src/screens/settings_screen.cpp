#include "settings_screen.h"
#include "menu_screen.h"
#include "app_state.h"


int settingsScrollY = 0;
int touchStartY = 0;
int touchStartScroll = 0;
bool touchMoved = false;


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
};


int settingsTotalH() {
  int h = 0;
  for (int i = 0; i < NUM_SETTING_ROWS; i++)
    h += settingRows[i].isSectionHeader ? SET_HEADER_H : SET_ROW_H;
  return h;
}

int settingRowY(int row) {
  int y = Config::HEADER_H + SET_START_Y - settingsScrollY;
  for (int i = 0; i < row; i++)
    y += settingRows[i].isSectionHeader ? SET_HEADER_H : SET_ROW_H;
  return y;
}


void drawSettingRowAt(int row, int y) {
  SRow &r = settingRows[row];
  int visBottom = Config::SCREEN_H - SET_RESET_H;
  int rowH = r.isSectionHeader ? SET_HEADER_H : SET_ROW_H;

  if (y + rowH < Config::HEADER_H || y > visBottom)
    return;
  int clipTop = max(y, Config::HEADER_H);
  int clipBottom = min(y + rowH, visBottom);
  if (clipTop >= clipBottom)
    return;

  int sprH = clipBottom - clipTop;
  spr.deleteSprite();
  spr.createSprite(Config::SCREEN_W, sprH);
  spr.fillSprite(Config::C_BG);

  int localY = y - clipTop;

  if (r.isSectionHeader) {
    spr.drawFastHLine(0, localY + SET_HEADER_H - 1, Config::SCREEN_W,
                      Config::C_AMBER);
    spr.setTextColor(Config::C_AMBER, Config::C_BG);
    spr.drawString(r.label, 8, localY + 5, 1);
  } else {
    spr.drawFastHLine(0, sprH - 2, Config::SCREEN_W, Config::C_GREEN_FAINT);
    spr.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
    spr.drawString(r.label, 6, localY + 6, 1);
    spr.setTextColor(Config::C_GREEN_FAINT, Config::C_BG);
    spr.drawString(r.desc, 6, localY + 18, 1);

    int bw = 68, bh = 30, bx = Config::SCREEN_W - bw - 4, by2 = localY + 7;
    spr.drawRoundRect(bx, by2, bw, bh, 3, Config::C_GREEN_DIM);
    spr.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
    spr.drawString("<", bx + 4, by2 + 10, 1);
    spr.setTextColor(Config::C_GREEN, Config::C_BG);
    spr.drawCentreString(r.getLabel(*r.idx), bx + bw / 2, by2 + 10, 1);
    spr.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
    spr.drawRightString(">", bx + bw - 3, by2 + 10, 1);
  }

  spr.pushSprite(0, clipTop);
  spr.deleteSprite();
}


void drawSettingsScreen() {
  tft.fillScreen(Config::C_BG);
  tft.fillRect(0, 0, Config::SCREEN_W, Config::HEADER_H, Config::C_BG);
  tft.drawFastHLine(0, Config::HEADER_H - 1, Config::SCREEN_W, Config::C_SEP);
  tft.drawRoundRect(3, 3, 58, Config::HEADER_H - 6, 3, Config::C_GREEN_DIM);
  tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
  tft.drawCentreString("< BACK", 32, 12, 1);
  tft.setTextColor(Config::C_GREEN, Config::C_BG);
  tft.drawCentreString("SETTINGS", 124, 8, 2);

  for (int i = 0; i < NUM_SETTING_ROWS; i++)
    drawSettingRowAt(i, settingRowY(i));

  tft.fillRect(0, Config::SCREEN_H - SET_RESET_H, Config::SCREEN_W, SET_RESET_H,
               Config::C_BG);
  tft.drawFastHLine(0, Config::SCREEN_H - SET_RESET_H, Config::SCREEN_W,
                    Config::C_SEP);
  int ry = Config::SCREEN_H - SET_RESET_H + 5;
  tft.drawRect(24, ry, 192, 30, Config::C_GREEN_DIM);
  tft.drawRect(26, ry + 2, 188, 26, Config::C_GREEN_FAINT);
  tft.setTextColor(Config::C_RED, Config::C_BG);
  tft.drawCentreString("[ RESET DEFAULTS ]", 120, ry + 8, 2);
}


void handleSettingsTouch(int tx, int ty) {
  if (inRect(tx, ty, 3, 3, 58, Config::HEADER_H - 6)) {
    applySettings();
    currentScreen = SCREEN_MENU;
    startMenu();
    return;
  }
  int ry = Config::SCREEN_H - SET_RESET_H + 5;
  if (inRect(tx, ty, 24, ry, 192, 30)) {
    cfg = DEFAULT_SETTINGS;
    settingsScrollY = 0;
    applySettings();
    drawSettingsScreen();
    return;
  }
  if (touchMoved)
    return;

  for (int i = 0; i < NUM_SETTING_ROWS; i++) {
    if (settingRows[i].isSectionHeader)
      continue;
    int y = settingRowY(i);
    if (ty < y || ty >= y + SET_ROW_H - 2)
      continue;
    if (y < Config::HEADER_H || y > Config::SCREEN_H - SET_RESET_H)
      continue;

    int bw = 68, bh = 24, bx = Config::SCREEN_W - bw - 4, by2 = y + 6;
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