#include "menu_screen.h"
#include "app_state.h"
#include "display.h"
#include "device_state.h"


// Static globals
static MenuState menuState = {
    .animPhase      = MenuAnimPhase::Corners,
    .cornerStep     = 0,
    .launchBlink    = false,
    .timer          = 0,
    .scoutGlow      = 0.0f,
    .scoutGlowUp    = true,
    .scoutGlowTimer = 0,
    .bracketSegs    = {
        // x,   y,   len, isH,   col
        {6,   6,   18, true,  Display::Colors::GREEN_DIM},  // top-left H
        {6,   6,   18, false, Display::Colors::GREEN_DIM},  // top-left V
        {216, 6,   18, true,  Display::Colors::GREEN_DIM},  // top-right H
        {233, 6,   18, false, Display::Colors::GREEN_DIM},  // top-right V
        {6,   313, 18, true,  Display::Colors::GREEN_DIM},  // bot-left H
        {6,   295, 18, false, Display::Colors::GREEN_DIM},  // bot-left V
        {216, 313, 18, true,  Display::Colors::GREEN_DIM},  // bot-right H
        {233, 295, 18, false, Display::Colors::GREEN_DIM},  // bot-right V
    }
};

static const Display::Button menuButtons[] = {
    {
        MenuScreen::SETTINGS_X, MenuScreen::SETTINGS_Y,
        MenuScreen::SETTINGS_W, MenuScreen::SETTINGS_BH, 1, "SETTINGS",
        Display::Colors::GREEN_DIM, Display::Colors::GREEN_FAINT, Display::Colors::GREEN_DIM
    },
    {
        MenuScreen::IMU_X, MenuScreen::IMU_Y,
        MenuScreen::IMU_W, MenuScreen::IMU_BH, 1, "IMU",
        Display::Colors::GREEN_DIM, Display::Colors::GREEN_FAINT, Display::Colors::GREEN_DIM
    },
    {
        MenuScreen::BATTERY_X, MenuScreen::BATTERY_Y,
        MenuScreen::BATTERY_W, MenuScreen::BATTERY_H, 1, "BATTERY",
        Display::Colors::GREEN_DIM, Display::Colors::GREEN_FAINT, Display::Colors::GREEN_DIM
    },
    // Row 2 — IMU feature screens
    {
        MenuScreen::SPIRIT_X, MenuScreen::SPIRIT_Y,
        MenuScreen::SPIRIT_W, MenuScreen::SPIRIT_H, 1, "SPIRIT",
        Display::Colors::GREEN_DIM, Display::Colors::GREEN_FAINT, Display::Colors::GREEN_DIM
    },
};

static const char* scoutLines[] = {
    "  _________                    __  ",
    " /   _____/ ____  ____  __ ___/  |_",
    " \\_____  \\_/ ___\\/  _ \\|  |  \\   __\\",
    " /        \\  \\__(  <_> )  |  /|  | ",
    "/_______  /\\___  >____/|____/ |__| ",
    "        \\/     \\/                  ",
};

static const Display::AsciiArt scout = {
    scoutLines,
    6,
    5,
    9,
    30,
    80,
};

static const char* echoLines[] = {
    "___________      .__          ",
    "\\_   _____/ ____ |  |__   ____",
    " |    __)__/ ___\\|  |  \\ /  _ \\",
    " |        \\  \\___|   Y  (  <_> )",
    "/_______  /\\___  >___|  /\\____/",
    "        \\/     \\/     \\/       ",
};

static const Display::AsciiArt echo = {
    echoLines,
    6,
    5,
    9,
    42,
    16,
};


// Specific drawing functions
static void drawTitleFrame() {
  Display::tft.drawRect(8,  8,  224, 138, Display::Colors::GREEN_DIM);
  Display::tft.drawRect(12, 12, 216, 130, Display::Colors::GREEN);
  Display::tft.drawFastHLine(16,  16,  16, Display::Colors::GREEN);
  Display::tft.drawFastVLine(16, 16, 16, Display::Colors::GREEN);
  
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
  uint16_t col  = ok ? Display::Colors::GREEN : Display::Colors::RED;
  const char* label = ok ? okLabel : errLabel;

  int textW = strlen(label) * 6;
  int unitW = 7 + 3 + textW;
  int startX = centreX - unitW / 2;

  int dotCX = startX + 3;        
  int textX = startX + 7 + 3;       
  int dotCY = y + h / 2;

  Display::tft.fillCircle(dotCX, dotCY, 3, col);
  Display::tft.setTextColor(col, Display::Colors::BG);
  Display::tft.drawString(label, textX, dotCY - 4, 1);
}

static void drawStatusBar() {
  const int sy = 147, sh = 13;
  Display::tft.fillRect(0, sy, Display::SCREEN_W, sh, Display::Colors::BG);

  Display::tft.drawFastHLine(8, sy, 224, Display::Colors::SEP);
  Display::tft.drawFastHLine(8, sy+sh-1, 224, Display::Colors::SEP);

  Display::tft.drawFastVLine(120, sy+2, sh-4, Display::Colors::SEP);

  drawStatusUnit(63, sy, sh, RadarState::found, "RADAR", "NO RADAR");
  drawStatusUnit(176, sy, sh, ImuState::found, "IMU", "NO IMU");
}

void drawLaunchButton(bool highlight) {
  constexpr int bx = MenuScreen::LAUNCH_X,   by = MenuScreen::LAUNCH_Y,
                bw = MenuScreen::LAUNCH_W,   bh = MenuScreen::LAUNCH_H;
  uint16_t col  = highlight ? Display::Colors::GREEN : Display::Colors::GREEN_DIM;
  uint16_t col2 = highlight ? Display::Colors::GREEN_DIM : Display::Colors::GREEN_FAINT;

  Display::tft.fillRect(bx, by, bw, bh, Display::Colors::BG);
  Display::tft.drawRect(bx,   by,   bw,   bh,   col2);
  Display::tft.drawRect(bx+2, by+2, bw-4, bh-4, col);

  const int arm = 12;
  Display::tft.drawFastHLine(bx+6,        by+6,        arm, col);
  Display::tft.drawFastVLine(bx+6,        by+6,        arm, col);
  Display::tft.drawFastHLine(bx+bw-arm-6, by+6,        arm, col);
  Display::tft.drawFastVLine(bx+bw-7,     by+6,        arm, col);
  Display::tft.drawFastHLine(bx+6,        by+bh-7,     arm, col);
  Display::tft.drawFastVLine(bx+6,        by+bh-arm-6, arm, col);
  Display::tft.drawFastHLine(bx+bw-arm-6, by+bh-7,     arm, col);
  Display::tft.drawFastVLine(bx+bw-7,     by+bh-arm-6, arm, col);

  int ix = bx + 22, iy = by + bh/2;
  Display::tft.drawFastVLine(ix, iy-8, 16, col);            
  Display::tft.drawLine(ix, iy, ix-6, iy-7, col);            
  Display::tft.drawLine(ix, iy, ix+6, iy-7, col);          
  Display::tft.drawFastHLine(ix-7, iy, 15, col); 

  // Two arcs with dots
  for (int a = -50; a <= 50; a += 10) {
    float r = a * 3.14159f / 180.0f;
    int ax = ix + (int)(9 * sinf(r));
    int ay = iy - (int)(9 * cosf(r));
    Display::tft.drawPixel(ax, ay, col);
  }
  for (int a = -50; a <= 50; a += 10) {
    float r = a * 3.14159f / 180.0f;
    int ax = ix + (int)(13 * sinf(r));
    int ay = iy - (int)(13 * cosf(r));
    Display::tft.drawPixel(ax, ay, col);
  }

  Display::tft.setTextColor(col, Display::Colors::BG);
  Display::tft.drawCentreString("[ RADAR ]", 128, by+14, 4);
}

void drawPowerButton() {
  constexpr int bx = MenuScreen::POWER_X, by = MenuScreen::POWER_Y,
                bw = MenuScreen::POWER_W, bh = MenuScreen::POWER_H;
  Display::tft.fillRect(0, 280, Display::SCREEN_W, 36, Display::Colors::BG);
  Display::tft.drawRect(bx, by, bw, bh, Display::Colors::RED);
  Display::tft.setTextColor(Display::Colors::RED, Display::Colors::BG);
  Display::tft.drawCentreString("[ POWER OFF ]", 120, by+4, 1);
}



// Start and tick
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

  // Separators between button rows and power
  Display::tft.drawFastHLine(8, 158, 224, Display::Colors::SEP);
  Display::tft.drawFastHLine(8, 212, 224, Display::Colors::SEP);
  Display::tft.drawFastHLine(8, 244, 224, Display::Colors::SEP);
  Display::tft.drawFastHLine(8, 279, 224, Display::Colors::SEP);
  Display::tft.drawFastHLine(8, 291, 224, Display::Colors::SEP);

  // Buttons
  drawLaunchButton(true);
  for (Display::Button button : menuButtons) {
    Display::drawButton(button);
  }
  drawPowerButton();
}


void tickMenu() {
  unsigned long now = millis();

  switch (menuState.animPhase) {

  case MenuAnimPhase::Corners:
    if (now - menuState.timer >= 55) {
      menuState.timer = now;
      if (menuState.cornerStep < NUM_BRACKET_SEGS) {
        BracketSeg &seg = menuState.bracketSegs[menuState.cornerStep];
        if (seg.isH)
          Display::tft.drawFastHLine(seg.x, seg.y, seg.len, seg.col);
        else
          Display::tft.drawFastVLine(seg.x, seg.y, seg.len, seg.col);
        menuState.cornerStep++;
      } else {
        menuState.animPhase = MenuAnimPhase::Hold;
        menuState.timer     = now;
      }
    }
    break;

  case MenuAnimPhase::Hold:
    if (now - menuState.timer >= 550) {
      menuState.timer       = now;
      menuState.launchBlink = !menuState.launchBlink;
      drawLaunchButton(menuState.launchBlink);
    }
    break;
  }

  if (now - menuState.scoutGlowTimer >= 30) {
    menuState.scoutGlowTimer = now;
    if (menuState.scoutGlowUp) {
      menuState.scoutGlow += 0.03f;
      if (menuState.scoutGlow >= 1.0f) {
        menuState.scoutGlow   = 1.0f;
        menuState.scoutGlowUp = false;
      }
    } else {
      menuState.scoutGlow -= 0.03f;
      if (menuState.scoutGlow <= 0.0f) {
        menuState.scoutGlow   = 0.0f;
        menuState.scoutGlowUp = true;
      }
    }
    uint8_t g = (uint8_t)(12 + menuState.scoutGlow * 51.0f);
    Display::drawAsciiArt(scout, (Display::Colors)(g << 5));
  }
}
