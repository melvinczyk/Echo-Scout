#include "menu_screen.h"
#include "app_state.h"

MenuState menuState = {
    MenuAnimPhase::Corners,
    0,
    0,
    false,
    0UL,
    0.0f,
    true,
    0UL,
    {
        {6,   6,   18, true,  Config::C_GREEN_DIM},
        {6,   6,   18, false, Config::C_GREEN_DIM},
        {216, 6,   18, true,  Config::C_GREEN_DIM},
        {233, 6,   18, false, Config::C_GREEN_DIM},
        {6,   313, 18, true,  Config::C_GREEN_DIM},
        {6,   295, 18, false, Config::C_GREEN_DIM},
        {216, 313, 18, true,  Config::C_GREEN_DIM},
        {233, 295, 18, false, Config::C_GREEN_DIM},
    }
};

void drawAsciiArt(const char** lines, int numLines, int charW, int lineH,
                  int startX, int startY, uint16_t col) {
  tft.setTextColor(col, Config::C_BG);
  for (int row = 0; row < numLines; row++) {
    const char *line = lines[row];
    int len = strlen(line);
    for (int c = 0; c < len; c++) {
      if (line[c] == ' ') continue;
      char buf[2] = {line[c], 0};
      tft.drawString(buf, startX + c * charW, startY + row * lineH, 1);
    }
  }
}

void drawScoutArt(uint16_t col) {
  const char *scoutArt[] = {
      "  _________                    __  ",
      " /   _____/ ____  ____  __ ___/  |_",
      " \\_____  \\_/ ___\\/  _ \\|  |  \\   __\\",
      " /        \\  \\__(  <_> )  |  /|  | ",
      "/_______  /\\___  >____/|____/ |__| ",
      "        \\/     \\/                  ",
  };
  tft.setTextColor(col, Config::C_BG);
  drawAsciiArt(scoutArt, 6, 5, 9, 30, 80, col);
}

static void drawEchoArt() {
  const char *echoArt[] = {
      "___________      .__          ",
      "\\_   _____/ ____ |  |__   ____",
      " |    __)__/ ___\\|  |  \\ /  _ \\",
      " |        \\  \\___|   Y  (  <_> )",
      "/_______  /\\___  >___|  /\\____/",
      "        \\/     \\/     \\/       ",
  };
  tft.setTextColor(Config::C_GREEN, Config::C_BG);
  drawAsciiArt(echoArt, 6, 5, 9, 42, 16, Config::C_GREEN);
}

static void drawTitleFrame() {
  tft.drawRect(8,  8,  224, 138, Config::C_GREEN_DIM);
  tft.drawRect(12, 12, 216, 130, Config::C_GREEN);
  tft.drawFastHLine(16,  16,  16, Config::C_GREEN);
  tft.drawFastVLine(16, 16, 16, Config::C_GREEN);
  
  tft.drawFastHLine(207, 16,  16, Config::C_GREEN);
  tft.drawFastVLine(223, 16,  16, Config::C_GREEN);

  tft.drawFastHLine(16,  138, 16, Config::C_GREEN);
  tft.drawFastVLine(16,  123, 16, Config::C_GREEN);

  tft.drawFastHLine(207, 138, 16, Config::C_GREEN);
  tft.drawFastVLine(223, 123, 16, Config::C_GREEN);
  tft.fillRect(70, 138, 100, 6, Config::C_BG);
  tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
  tft.drawCentreString("VERSION: 1.0", 120, 137, 1);
}

static void drawStatusUnit(int centreX, int y, int h,
                           bool ok, const char* okLabel, const char* errLabel) {
  uint16_t col  = ok ? Config::C_GREEN : Config::C_RED;
  const char* label = ok ? okLabel : errLabel;

  int textW = strlen(label) * 6;
  int unitW = 7 + 3 + textW;
  int startX = centreX - unitW / 2;

  int dotCX = startX + 3;        
  int textX = startX + 7 + 3;       
  int dotCY = y + h / 2;

  tft.fillCircle(dotCX, dotCY, 3, col);
  tft.setTextColor(col, Config::C_BG);
  tft.drawString(label, textX, dotCY - 4, 1);
}

static void drawStatusBar() {
  const int sy = 147, sh = 13;
  tft.fillRect(0, sy, Config::SCREEN_W, sh, Config::C_BG);

  tft.drawFastHLine(8, sy,      224, Config::C_SEP);
  tft.drawFastHLine(8, sy+sh-1, 224, Config::C_SEP);

  tft.drawFastVLine(120, sy+2, sh-4, Config::C_SEP);

  drawStatusUnit(63,  sy, sh, radarFound, "RADAR", "NO RADAR");
  drawStatusUnit(176, sy, sh, imuFound,   "IMU",   "NO IMU");
}


void drawLaunchButton(bool highlight) {
  const int bx = 12, by = 165, bw = 216, bh = 48;
  uint16_t col  = highlight ? Config::C_GREEN     : Config::C_GREEN_DIM;
  uint16_t col2 = highlight ? Config::C_GREEN_DIM : Config::C_GREEN_FAINT;

  tft.fillRect(bx, by, bw, bh, Config::C_BG);
  tft.drawRect(bx,   by,   bw,   bh,   col2);
  tft.drawRect(bx+2, by+2, bw-4, bh-4, col);

  const int arm = 12;
  tft.drawFastHLine(bx+6,        by+6,        arm, col);
  tft.drawFastVLine(bx+6,        by+6,        arm, col);
  tft.drawFastHLine(bx+bw-arm-6, by+6,        arm, col);
  tft.drawFastVLine(bx+bw-7,     by+6,        arm, col);
  tft.drawFastHLine(bx+6,        by+bh-7,     arm, col);
  tft.drawFastVLine(bx+6,        by+bh-arm-6, arm, col);
  tft.drawFastHLine(bx+bw-arm-6, by+bh-7,     arm, col);
  tft.drawFastVLine(bx+bw-7,     by+bh-arm-6, arm, col);

  int ix = bx + 22, iy = by + bh/2;
  tft.drawFastVLine(ix, iy-8, 16, col);            
  tft.drawLine(ix, iy, ix-6, iy-7, col);            
  tft.drawLine(ix, iy, ix+6, iy-7, col);          
  tft.drawFastHLine(ix-7, iy, 15, col);              
  // Two arcs approximated with dots
  for (int a = -50; a <= 50; a += 10) {
    float r = a * 3.14159f / 180.0f;
    int ax = ix + (int)(9 * sinf(r));
    int ay = iy - (int)(9 * cosf(r));
    tft.drawPixel(ax, ay, col);
  }
  for (int a = -50; a <= 50; a += 10) {
    float r = a * 3.14159f / 180.0f;
    int ax = ix + (int)(13 * sinf(r));
    int ay = iy - (int)(13 * cosf(r));
    tft.drawPixel(ax, ay, col);
  }

  tft.setTextColor(col, Config::C_BG);
  tft.drawCentreString("[ RADAR ]", 128, by+14, 4);
}


void drawSettingsMenuButton() {
  const int bx = 8, by = 219, bw = 72, bh = 28;
  tft.fillRect(bx, by, bw, bh, Config::C_BG);
  tft.drawRect(bx,   by,   bw,   bh,   Config::C_GREEN_DIM);
  tft.drawRect(bx+2, by+2, bw-4, bh-4, Config::C_GREEN_FAINT);
  tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
  tft.drawCentreString("SETTINGS", bx+bw/2, by+9, 1);
}

void drawImuMenuButton() {
  const int bx = 84, by = 219, bw = 72, bh = 28;
  tft.fillRect(bx, by, bw, bh, Config::C_BG);
  tft.drawRect(bx,   by,   bw,   bh,   Config::C_GREEN_DIM);
  tft.drawRect(bx+2, by+2, bw-4, bh-4, Config::C_GREEN_FAINT);
  tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
  tft.drawCentreString("IMU", bx+bw/2, by+9, 1);
}

void drawBatteryMenuButton() {
  const int bx = 160, by = 219, bw = 72, bh = 28;
  tft.fillRect(bx, by, bw, bh, Config::C_BG);
  tft.drawRect(bx,   by,   bw,   bh,   Config::C_GREEN_DIM);
  tft.drawRect(bx+2, by+2, bw-4, bh-4, Config::C_GREEN_FAINT);
  tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
  tft.drawCentreString("BATTERY", bx+bw/2, by+9, 1);
}


void drawPowerButton() {
  const int bx = 40, by = 295, bw = 160, bh = 18;
  tft.fillRect(0, 249, Config::SCREEN_W, 67, Config::C_BG);
  tft.drawRect(bx, by, bw, bh, Config::C_RED);
  tft.setTextColor(Config::C_RED, Config::C_BG);
  tft.drawCentreString("[ POWER OFF ]", 120, by+4, 1);
}


void startMenu() {
  menuState.animPhase    = MenuAnimPhase::Corners;
  menuState.cornerStep   = 0;
  menuState.launchBlink  = false;
  menuState.timer        = millis();
  menuState.scoutGlow    = 0.0f;
  menuState.scoutGlowUp  = true;
  menuState.scoutGlowTimer = millis();

  tft.fillScreen(Config::C_BG);

  tft.drawRect(2, 2, 236, 316, Config::C_GREEN_FAINT);

  drawTitleFrame();
  drawEchoArt();
  drawScoutArt(Config::C_GREEN_FAINT);

  drawStatusBar();

  tft.drawFastHLine(8, 161, 224, Config::C_SEP);
  tft.drawFastHLine(8, 215, 224, Config::C_SEP);
  tft.drawFastHLine(8, 249, 224, Config::C_SEP);
  tft.drawFastHLine(8, 291, 224, Config::C_SEP);

  drawLaunchButton(true);
  drawSettingsMenuButton();
  drawImuMenuButton();
  drawBatteryMenuButton();
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
          tft.drawFastHLine(seg.x, seg.y, seg.len, seg.col);
        else
          tft.drawFastVLine(seg.x, seg.y, seg.len, seg.col);
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
    drawScoutArt((uint16_t)(g << 5));
  }
}
