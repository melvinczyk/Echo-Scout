#include "screens/settings_screen.h"
#include "base/screen_manager.h"
#include "base/display.h"
#include "devices/touch.h"


static const char *htLabel(uint8_t i) { static char b[4]; sprintf(b, "%d", hitsVals[i]); return b; }
static const char *drLabel(uint8_t i) { static char b[4]; sprintf(b, "%d", dropsVals[i]); return b; }
static const char *mrLabel(uint8_t i) { return minRangeLabels[i]; }
static const char *maLabel(uint8_t i) { return maxAngleLabels[i]; }
static const char *arLabel(uint8_t i) { return accRangeLabels[i]; }
static const char *sgLabel(uint8_t i) { return speedGateLabels[i]; }
static const char *mvLabel(uint8_t i) { return moveThreshLabels[i]; }
static const char *smLabel(uint8_t i) { return i ? "ON" : "OFF"; }
static const char *secLabel(uint8_t)  { return ""; }
static const char *blLabel(uint8_t i) { return blLabels[i]; }
static const char *slLabel(uint8_t i) { return sleepTimeoutLabels[i]; }
static const char *duLabel(uint8_t i) { return distUnitLabels[i]; }
static const char *suLabel(uint8_t i) { return speedUnitLabels[i]; }


SRow settingRows[SettingsScreen::NUM_SETTING_ROWS] = {
    {"DETECTION", "", nullptr, 0, secLabel, true},
    {"MIN FRAMES", "hits to confirm", &cfg.hitsIdx, NUM_HITS, htLabel, false},
    {"DROP FRAMES", "misses to drop", &cfg.dropsIdx, NUM_DROPS, drLabel, false},
    {"SPEED GATE", "max speed cm/s", &cfg.speedGateIdx, NUM_SG, sgLabel, false},
    {"GEOMETRY", "", nullptr, 0, secLabel, true},
    {"MIN RANGE", "ignore closer", &cfg.minRangeIdx, NUM_MR, mrLabel, false},
    {"MAX ANGLE", "cone half-width", &cfg.maxAngleIdx, NUM_MA, maLabel, false},
    {"ACC RANGE", "precise track zone", &cfg.accRangeIdx, NUM_AR, arLabel, false},
    {"MOVEMENT", "", nullptr, 0, secLabel, true},
    {"MOVE THRESH", "min move to redraw", &cfg.moveThreshIdx, NUM_MVTHR, mvLabel, false},
    {"SMOOTHING", "position blending", (uint8_t*)&cfg.smoothingOn, 2, smLabel, false},
    {"DISPLAY", "", nullptr, 0, secLabel, true},
    {"BACKLIGHT", "brightness", &cfg.brightnessIdx, NUM_BL, blLabel, false},
    {"UNITS", "distance units", &cfg.distUnitIdx, NUM_DIST_UNIT, duLabel, false},
    {"SPEED", "speed units", &cfg.speedUnitIdx, NUM_SPEED_UNIT, suLabel, false},
    {"POWER", "", nullptr, 0, secLabel, true},
    {"SLEEP", "idle sleep timeout", &cfg.sleepTimeoutIdx, NUM_SLEEP, slLabel, false},
};


int settingsTotalH() {
    int h = 0;
    for (int i = 0; i < SettingsScreen::NUM_SETTING_ROWS; i++)
        h += settingRows[i].isSectionHeader ? SettingsScreen::SET_HEADER_H : SettingsScreen::SET_ROW_H;
    return h;
}

int settingRowY(int row) {
    int y = Display::HEADER_H + SettingsScreen::SET_START_Y - SettingsScreen::settingsScrollY;
    for (int i = 0; i < row; i++)
        y += settingRows[i].isSectionHeader ? SettingsScreen::SET_HEADER_H : SettingsScreen::SET_ROW_H;
    return y;
}

void drawSettingRowAt(int row, int y) {
    SRow& r = settingRows[row];
    int visBottom = Display::SCREEN_H - SettingsScreen::SET_RESET_H;
    int rowH = r.isSectionHeader ? SettingsScreen::SET_HEADER_H : SettingsScreen::SET_ROW_H;

    if (y + rowH < Display::HEADER_H || y > visBottom) return;
    int clipTop = max(y, Display::HEADER_H);
    int clipBottom = min(y + rowH, visBottom);
    if (clipTop >= clipBottom) return;

    int sprH = clipBottom - clipTop;
    Display::spr.deleteSprite();
    Display::spr.createSprite(Display::SCREEN_W, sprH);
    Display::spr.fillSprite(Display::Colors::BG);

    int ly = y - clipTop;   // local y within sprite

    if (r.isSectionHeader) {
        Display::spr.drawFastHLine(0, ly + SettingsScreen::SET_HEADER_H - 1, Display::SCREEN_W, Display::Colors::AMBER);
        Display::spr.setTextColor(Display::Colors::AMBER, Display::Colors::BG);
        Display::spr.drawCentreString(r.label, Display::SCREEN_W / 2, ly + 3, 1);
    } else {
        Display::spr.drawFastHLine(0, ly + SettingsScreen::SET_ROW_H - 1, Display::SCREEN_W, Display::Colors::GREEN_FAINT);

        Display::spr.drawFastVLine(SettingsScreen::ARROW_W, ly, SettingsScreen::SET_ROW_H - 1, Display::Colors::GREEN_FAINT);
        Display::spr.drawFastVLine(Display::SCREEN_W - SettingsScreen::ARROW_W, ly, SettingsScreen::SET_ROW_H - 1, Display::Colors::GREEN_FAINT);

        Display::spr.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
        Display::spr.drawCentreString("<", SettingsScreen::ARROW_CX, ly + SettingsScreen::SET_ROW_H/2 - 8, 2);

        Display::spr.drawCentreString(">", SettingsScreen::ARROW_RX, ly + SettingsScreen::SET_ROW_H/2 - 8, 2);

        Display::spr.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
        Display::spr.drawString(r.label, SettingsScreen::ARROW_W + 8, ly + 6, 1);
        if (r.desc[0]) {
            Display::spr.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
            Display::spr.drawString(r.desc, SettingsScreen::ARROW_W + 8, ly + 18, 1);
        }
        Display::spr.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
        Display::spr.drawCentreString(r.getLabel(*r.idx), Display::SCREEN_W / 2,
                                      ly + SettingsScreen::SET_ROW_H/2 + 2, 2);
    }

    Display::spr.pushSprite(0, clipTop);
    Display::spr.deleteSprite();
}


void drawSettingsScreen() {
    Display::tft.fillScreen(Display::Colors::BG);
    Display::drawHeader("SETTINGS", false);

    for (int i = 0; i < SettingsScreen::NUM_SETTING_ROWS; i++)
        drawSettingRowAt(i, settingRowY(i));

    int ry = Display::SCREEN_H - SettingsScreen::SET_RESET_H;
    Display::tft.fillRect(0, ry, Display::SCREEN_W, SettingsScreen::SET_RESET_H, Display::Colors::BG);
    Display::tft.drawFastHLine(0, ry, Display::SCREEN_W, Display::Colors::SEP);
    Display::tft.drawRect(16, ry + 7, Display::SCREEN_W - 32, 26, Display::Colors::RED);
    Display::tft.setTextColor(Display::Colors::RED, Display::Colors::BG);
    Display::tft.drawCentreString("RESET DEFAULTS", 120, ry + 13, 2);
}


void handleSettingsTouch(int tx, int ty) {
    if (inRect(tx, ty, 3, 3, 58, Display::HEADER_H - 6)) {
        applySettings();
        ScreenManager::switchScreen(Display::Screen::MENU);
        return;
    }

    int ry = Display::SCREEN_H - SettingsScreen::SET_RESET_H;
    if (inRect(tx, ty, 16, ry + 7, Display::SCREEN_W - 32, 26)) {
        cfg = DEFAULT_SETTINGS;
        SettingsScreen::settingsScrollY = 0;
        applySettings();
        drawSettingsScreen();
        return;
    }

    for (int i = 0; i < SettingsScreen::NUM_SETTING_ROWS; i++) {
        if (settingRows[i].isSectionHeader) continue;
        int y = settingRowY(i);
        if (ty < y || ty >= y + SettingsScreen::SET_ROW_H) continue;
        if (y + SettingsScreen::SET_ROW_H <= Display::HEADER_H || y >= Display::SCREEN_H - SettingsScreen::SET_RESET_H) continue;

        bool dec = tx < SettingsScreen::ARROW_W;
        bool inc = tx >= Display::SCREEN_W - SettingsScreen::ARROW_W;
        if (!dec && !inc) return;  // tapped in label area - ignore

        if (dec) {
            if (*settingRows[i].idx > 0) (*settingRows[i].idx)--;
            else *settingRows[i].idx = settingRows[i].count - 1;
        } else {
            if (*settingRows[i].idx < settingRows[i].count - 1) (*settingRows[i].idx)++;
            else *settingRows[i].idx = 0;
        }
        drawSettingRowAt(i, y);
        applySettings();
        return;
    }
}
