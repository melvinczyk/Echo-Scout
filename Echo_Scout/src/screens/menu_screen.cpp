#include "menu_screen.h"

MenuState menuState = {
    MenuAnimPhase::Corners,
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

// ═══════════════════════════════════════════════════════════
//  BUTTONS
// ═══════════════════════════════════════════════════════════
void drawLaunchButton(bool highlight) {
  int bx = 24, by = 220, bw = 192, bh = 50;
  uint16_t col = highlight ? Config::C_GREEN : Config::C_GREEN_DIM;
  tft.fillRect(bx, by, bw, bh, Config::C_BG);
  tft.drawRect(bx, by, bw, bh, Config::C_GREEN_FAINT);
  tft.drawRect(bx + 2, by + 2, bw - 4, bh - 4, col);
  tft.drawFastHLine(bx + 6, by + 6, 12, col);
  tft.drawFastVLine(bx + 6, by + 6, 12, col);
  tft.drawFastHLine(bx + bw - 18, by + 6, 12, col);
  tft.drawFastVLine(bx + bw - 7, by + 6, 12, col);
  tft.drawFastHLine(bx + 6, by + bh - 7, 12, col);
  tft.drawFastVLine(bx + 6, by + bh - 18, 12, col);
  tft.drawFastHLine(bx + bw - 18, by + bh - 7, 12, col);
  tft.drawFastVLine(bx + bw - 7, by + bh - 18, 12, col);
  tft.setTextColor(col, Config::C_BG);
  tft.drawCentreString("[ ACTIVATE ]", 120, by + 16, 4);
}

void drawSettingsMenuButton() {
  int bx = 60, by = 178, bw = 120, sh = 28;
  tft.fillRect(bx, by, bw, sh, Config::C_BG);
  tft.drawRect(bx, by, bw, sh, Config::C_AMBER);
  tft.drawRect(bx + 2, by + 2, bw - 4, sh - 4, Config::C_AMBER);
  tft.setTextColor(Config::C_AMBER, Config::C_BG);
  tft.drawCentreString("[ SETTINGS ]", 120, by + 8, 2);
}

void drawImuMenuButton() {
  int bx = 72, by = 280, bw = 96, sh = 22;
  tft.fillRect(bx, by, bw, sh, Config::C_BG);
  tft.drawRect(bx, by, bw, sh, Config::C_GREEN_FAINT);
  tft.setTextColor(Config::C_GREEN_FAINT, Config::C_BG);
  tft.drawCentreString("[ IMU ]", 120, by + 6, 1);
}

// ═══════════════════════════════════════════════════════════
//  ASCII ART
// ═══════════════════════════════════════════════════════════
void drawAsciiArt(const char** lines, int numLines, int charW, int lineH,
                  int startX, int startY, uint16_t col) {
  for (int row = 0; row < numLines; row++) {
    const char *line = lines[row];
    int len = strlen(line);
    for (int c = 0; c < len; c++) {
      char buf[2] = {line[c], 0};
      if (line[c] != ' ')
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
  drawAsciiArt(scoutArt, 6, 5, 9, 30, 78, col);
}

// ═══════════════════════════════════════════════════════════
//  START MENU
// ═══════════════════════════════════════════════════════════
void startMenu() {
  menuState.launchBlink   = false;
  menuState.scoutGlow     = 0.0f;
  menuState.scoutGlowUp   = true;
  menuState.animPhase     = MenuAnimPhase::Corners;
  menuState.cornerStep    = 0;
  menuState.timer         = millis();
  menuState.scoutGlowTimer = millis();

  tft.fillScreen(Config::C_BG);

  // Outer frame
  tft.drawRect(2, 2, 236, 316, Config::C_GREEN_FAINT);

  // Title box
  tft.drawRect(8, 8, 224, 138, Config::C_GREEN_DIM);
  tft.drawRect(12, 12, 216, 130, Config::C_GREEN);

  // Corner ticks
  tft.drawFastHLine(16, 16, 16, Config::C_GREEN);
  tft.drawFastVLine(16, 16, 16, Config::C_GREEN);
  tft.drawFastHLine(212, 16, 16, Config::C_GREEN);
  tft.drawFastVLine(227, 16, 16, Config::C_GREEN);
  tft.drawFastHLine(16, 138, 16, Config::C_GREEN);
  tft.drawFastVLine(16, 123, 16, Config::C_GREEN);
  tft.drawFastHLine(212, 138, 16, Config::C_GREEN);
  tft.drawFastVLine(227, 123, 16, Config::C_GREEN);

  // VERSION tag on bottom border
  tft.fillRect(70, 138, 100, 6, Config::C_BG);
  tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
  tft.drawCentreString("VERSION: 1.0", 120, 137, 1);

  // ECHO art
  const char *echoArt[] = {
      "___________      .__          ",
      "\\_   _____/ ____ |  |__   ____",
      " |    __)__/ ___\\|  |  \\ /  _ \\",
      " |        \\  \\___|   Y  (  <_> )",
      "/_______  /\\___  >___|  /\\____/",
      "        \\/     \\/     \\/       ",
  };
  tft.setTextColor(Config::C_GREEN, Config::C_BG);
  drawAsciiArt(echoArt, 6, 5, 9, 42, 18, Config::C_GREEN);
  drawScoutArt(Config::C_GREEN_FAINT);

  tft.drawFastHLine(16, 150, 208, Config::C_SEP);

  drawSettingsMenuButton();
  tft.drawFastHLine(16, 212, 208, Config::C_SEP);
  drawLaunchButton(true);

  drawImuMenuButton();

  tft.setTextColor(Config::C_GREEN_FAINT, Config::C_BG);
}

// ═══════════════════════════════════════════════════════════
//  TICK MENU
// ═══════════════════════════════════════════════════════════
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
