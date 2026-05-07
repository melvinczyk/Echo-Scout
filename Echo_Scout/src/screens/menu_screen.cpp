#include "screens/menu_screen.h"
#include "base/app_state.h"
#include "base/display.h"
#include "devices/device_state.h"
#include "screens/ascii_art.h"

static MenuState menuState = {
    .animPhase = MenuAnimPhase::Corners,
    .cornerStep = 0,
    .launchBlink = false,
    .timer = 0,
    .scoutGlow = 0.0f,
    .scoutGlowUp = true,
    .scoutGlowTimer = 0,
    .bracketSegs    = {
        {6,   6,   18, true,  Display::Colors::GREEN_DIM},
        {6,   6,   18, false, Display::Colors::GREEN_DIM},
        {216, 6,   18, true,  Display::Colors::GREEN_DIM},
        {233, 6,   18, false, Display::Colors::GREEN_DIM},
        {6,   313, 18, true,  Display::Colors::GREEN_DIM},
        {6,   295, 18, false, Display::Colors::GREEN_DIM},
        {216, 313, 18, true,  Display::Colors::GREEN_DIM},
        {233, 295, 18, false, Display::Colors::GREEN_DIM},
    }
};


static Display::AsciiArt echo = {AsciiArtInstance::ECHO, std::size(AsciiArtInstance::ECHO), 5, 9, 42, 16};
static Display::AsciiArt scout = {AsciiArtInstance::SCOUT, std::size(AsciiArtInstance::SCOUT), 5, 9, 30, 80};
    
static const Display::Button toolButtons[] = {
    { MenuScreen::SCANNER_X,  MenuScreen::SCANNER_Y,  MenuScreen::SCANNER_W,  MenuScreen::SCANNER_H,  1, "SCANNER",
      Display::Colors::GREEN_DIM, Display::Colors::GREEN_FAINT, Display::Colors::GREEN_DIM },
    { MenuScreen::ATTITUDE_X, MenuScreen::ATTITUDE_Y, MenuScreen::ATTITUDE_W, MenuScreen::ATTITUDE_H, 1, "ATTITUDE",
      Display::Colors::GREEN_DIM, Display::Colors::GREEN_FAINT, Display::Colors::GREEN_DIM },
};

static const Display::Button utilButtons[] = {
    { MenuScreen::SETTINGS_X, MenuScreen::SETTINGS_Y, MenuScreen::SETTINGS_W, MenuScreen::SETTINGS_BH, 1, "SETTINGS",
      Display::Colors::GREEN_DIM, Display::Colors::GREEN_FAINT, Display::Colors::GREEN_DIM },
    { MenuScreen::BATTERY_X,  MenuScreen::BATTERY_Y,  MenuScreen::BATTERY_W,  MenuScreen::BATTERY_H,   1, "BATTERY",
      Display::Colors::GREEN_DIM, Display::Colors::GREEN_FAINT, Display::Colors::GREEN_DIM },
};

static void drawTitleFrame() {
    Display::tft.drawRect(8,  8,  224, 138, Display::Colors::GREEN_DIM);
    Display::tft.drawRect(12, 12, 216, 130, Display::Colors::GREEN);
    Display::tft.drawFastHLine(16,  16,  16, Display::Colors::GREEN);
    Display::tft.drawFastVLine(16,  16,  16, Display::Colors::GREEN);
    Display::tft.drawFastHLine(207, 16,  16, Display::Colors::GREEN);
    Display::tft.drawFastVLine(223, 16,  16, Display::Colors::GREEN);
    Display::tft.drawFastHLine(16,  138, 16, Display::Colors::GREEN);
    Display::tft.drawFastVLine(16,  123, 16, Display::Colors::GREEN);
    Display::tft.drawFastHLine(207, 138, 16, Display::Colors::GREEN);
    Display::tft.drawFastVLine(223, 123, 16, Display::Colors::GREEN);
    Display::tft.fillRect(70, 138, 100, 6, Display::Colors::BG);
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("VERSION: 1.0", 120, 137, 1);
}

static void drawStatusUnit(int centreX, int y, int h, bool ok, const char* okLabel, const char* errLabel) {
    uint16_t col = ok ? Display::Colors::GREEN : Display::Colors::RED;
    const char* label = ok ? okLabel : errLabel;
    int textW = strlen(label) * 6;
    int unitW = 7 + 3 + textW;
    int startX = centreX - unitW / 2;
    int dotCX = startX + 3, textX = startX + 10, dotCY = y + h / 2;
    Display::tft.fillCircle(dotCX, dotCY, 3, col);
    Display::tft.setTextColor(col, Display::Colors::BG);
    Display::tft.drawString(label, textX, dotCY - 4, 1);
}

static void drawStatusBar() {
    const int sy = 147, sh = 13;
    Display::tft.fillRect(0, sy, Display::SCREEN_W, sh, Display::Colors::BG);
    Display::tft.drawFastHLine(8, sy, 224, Display::Colors::SEP);
    Display::tft.drawFastHLine(8, sy + sh - 1, 224, Display::Colors::SEP);
    Display::tft.drawFastVLine(88,  sy + 2, sh - 4, Display::Colors::SEP);
    Display::tft.drawFastVLine(160, sy + 2, sh - 4, Display::Colors::SEP);
    drawStatusUnit(44,  sy, sh, RadarState::found, "RADAR", "NO RADAR");
    drawStatusUnit(124, sy, sh, ImuState::found,   "IMU",   "NO IMU");
    drawStatusUnit(200, sy, sh, TofState::found,   "TOF",   "NO TOF");
}

void drawLaunchButton(bool highlight) {
    constexpr int bx = MenuScreen::LAUNCH_X, by = MenuScreen::LAUNCH_Y,
                  bw = MenuScreen::LAUNCH_W, bh = MenuScreen::LAUNCH_H;
    uint16_t col = highlight ? Display::Colors::GREEN : Display::Colors::GREEN_DIM;
    uint16_t col2 = highlight ? Display::Colors::GREEN_DIM : Display::Colors::GREEN_FAINT;
    Display::tft.fillRect(bx, by, bw, bh, Display::Colors::BG);
    Display::tft.drawRect(bx,   by,   bw,   bh,   col2);
    Display::tft.drawRect(bx+2, by+2, bw-4, bh-4, col);
    const int arm = 10;
    Display::tft.drawFastHLine(bx+6,        by+6,        arm, col);
    Display::tft.drawFastVLine(bx+6,        by+6,        arm, col);
    Display::tft.drawFastHLine(bx+bw-arm-6, by+6,        arm, col);
    Display::tft.drawFastVLine(bx+bw-7,     by+6,        arm, col);
    Display::tft.drawFastHLine(bx+6,        by+bh-7,     arm, col);
    Display::tft.drawFastVLine(bx+6,        by+bh-arm-6, arm, col);
    Display::tft.drawFastHLine(bx+bw-arm-6, by+bh-7,     arm, col);
    Display::tft.drawFastVLine(bx+bw-7,     by+bh-arm-6, arm, col);
    int ix = bx + 22, iy = by + bh/2;
    Display::tft.drawFastVLine(ix, iy-7, 14, col);
    Display::tft.drawLine(ix, iy, ix-5, iy-6, col);
    Display::tft.drawLine(ix, iy, ix+5, iy-6, col);
    Display::tft.drawFastHLine(ix-6, iy, 13, col);
    for (int a = -50; a <= 50; a += 10) {
        float r = a * 3.14159f / 180.0f;
        Display::tft.drawPixel(ix + (int)(8*sinf(r)), iy - (int)(8*cosf(r)), col);
    }
    for (int a = -50; a <= 50; a += 10) {
        float r = a * 3.14159f / 180.0f;
        Display::tft.drawPixel(ix + (int)(12*sinf(r)), iy - (int)(12*cosf(r)), col);
    }
    Display::tft.setTextColor(col, Display::Colors::BG);
    Display::tft.drawCentreString("[ RADAR ]", 128, by + 13, 4);
}

static void drawMapButton() {
    constexpr int bx = MenuScreen::MAP_X, by = MenuScreen::MAP_Y,
                  bw = MenuScreen::MAP_W, bh = MenuScreen::MAP_H;
    Display::tft.fillRect(bx, by, bw, bh, Display::Colors::BG);
    Display::tft.drawRect(bx,   by,   bw,   bh,   Display::Colors::GREEN_FAINT);
    Display::tft.drawRect(bx+2, by+2, bw-4, bh-4, Display::Colors::GREEN_DIM);
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("[ 3D MAP ]", 120, by + bh/2 - 8, 2);
}


void drawPowerButton() {
    constexpr int bx = MenuScreen::POWER_X, by = MenuScreen::POWER_Y,
                  bw = MenuScreen::POWER_W, bh = MenuScreen::POWER_H;
    Display::tft.fillRect(0, by - 4, Display::SCREEN_W, bh + 8, Display::Colors::BG);
    Display::tft.drawRect(bx, by, bw, bh, Display::Colors::RED);
    Display::tft.setTextColor(Display::Colors::RED, Display::Colors::BG);
    Display::tft.drawCentreString("[ POWER OFF ]", 120, by + 3, 1);
}


void startMenu() {
    menuState.animPhase = MenuAnimPhase::Corners;
    menuState.cornerStep = 0;
    menuState.launchBlink = false;
    menuState.timer = millis();
    menuState.scoutGlow = 0.0f;
    menuState.scoutGlowUp = true;
    menuState.scoutGlowTimer = millis();

    Display::tft.fillScreen(Display::Colors::BG);
    Display::tft.drawRect(2, 2, 236, 316, Display::Colors::GREEN_FAINT);

    drawTitleFrame();

    Display::drawAsciiArt(echo, Display::Colors::GREEN);
    Display::drawAsciiArt(scout, Display::Colors::GREEN_FAINT);
    drawStatusBar();

    Display::tft.drawFastHLine(8, 158, 224, Display::Colors::SEP);
    Display::tft.drawFastHLine(8, 204, 224, Display::Colors::SEP);
    Display::tft.drawFastHLine(8, 236, 224, Display::Colors::SEP);
    Display::tft.drawFastHLine(8, 264, 224, Display::Colors::SEP);
    Display::tft.drawFastHLine(8, 290, 224, Display::Colors::SEP);
    Display::tft.drawFastHLine(8, 300, 224, Display::Colors::SEP);

    drawLaunchButton(true);
    drawMapButton();
    for (const Display::Button& b : toolButtons)  Display::drawButton(b);
    for (const Display::Button& b : utilButtons)  Display::drawButton(b);
    drawPowerButton();
}


void tickMenu() {
    unsigned long now = millis();

    switch (menuState.animPhase) {
    case MenuAnimPhase::Corners:
        if (now - menuState.timer >= 55) {
            menuState.timer = now;
            if (menuState.cornerStep < NUM_BRACKET_SEGS) {
                BracketSeg& seg = menuState.bracketSegs[menuState.cornerStep];
                if (seg.isH) Display::tft.drawFastHLine(seg.x, seg.y, seg.len, seg.col);
                else Display::tft.drawFastVLine(seg.x, seg.y, seg.len, seg.col);
                menuState.cornerStep++;
            } else {
                menuState.animPhase = MenuAnimPhase::Hold;
                menuState.timer = now;
            }
        }
        break;
    case MenuAnimPhase::Hold:
        if (now - menuState.timer >= 550) {
            menuState.timer = now;
            menuState.launchBlink = !menuState.launchBlink;
            drawLaunchButton(menuState.launchBlink);
        }
        break;
    }

    if (now - menuState.scoutGlowTimer >= 30) {
        menuState.scoutGlowTimer = now;
        if (menuState.scoutGlowUp) {
            menuState.scoutGlow += 0.03f;
            if (menuState.scoutGlow >= 1.0f) { menuState.scoutGlow = 1.0f; menuState.scoutGlowUp = false; }
        } else {
            menuState.scoutGlow -= 0.03f;
            if (menuState.scoutGlow <= 0.0f) { menuState.scoutGlow = 0.0f; menuState.scoutGlowUp = true; }
        }
        uint8_t g = (uint8_t)(12 + menuState.scoutGlow * 51.0f);
        Display::drawAsciiArt(scout, (Display::Colors)(g << 5));
    }
}
