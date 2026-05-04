#include "display.h"
#include <math.h>


TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);


Blip blips[3] = {{0, 0, false}, {0, 0, false}, {0, 0, false}};
bool farZoneDrawn[4] = {false, false, false, false};

static float prevDashDist = -9999;
static float prevDashAngle = -9999;
static float prevDashSpeed = -9999;
static bool prevDashPresent = false;
static bool dashNeedsFullRedraw = true;

void drawHeader() {
  tft.fillRect(0, 0, Config::SCREEN_W, Config::HEADER_H, Config::C_BG);
  tft.drawFastHLine(0, Config::HEADER_H - 1, Config::SCREEN_W, Config::C_SEP);
  tft.drawRoundRect(3, 3, 64, Config::HEADER_H - 6, 3, Config::C_GREEN_DIM);
  tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
  tft.drawCentreString("< MENU", 35, 7, 2);
  tft.setTextColor(Config::C_GREEN, Config::C_BG);
  tft.drawCentreString("ECHO SCOUT", 120, 9, 2);
}

void drawDashboardFrame() {
  tft.fillRect(0, Config::DASH_Y, Config::SCREEN_W, Config::DASH_H,
               Config::C_BG);
  tft.drawFastHLine(0, Config::DASH_Y + Config::DASH_H, Config::SCREEN_W,
                    Config::C_SEP);
  int cardW = 76, pad = 4;
  for (int i = 0; i < 3; i++) {
    int cx = pad + i * (cardW + pad);
    tft.drawRect(cx, Config::DASH_Y + 2, cardW, Config::DASH_H - 4,
                 Config::C_GREEN_DIM);
  }
  tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
  tft.drawCentreString("DIST", pad + cardW / 2, Config::DASH_Y + 5, 1);
  tft.drawCentreString("ANGLE", pad + (cardW + pad) + cardW / 2,
                       Config::DASH_Y + 5, 1);
  tft.drawCentreString("SPEED", pad + 2 * (cardW + pad) + cardW / 2,
                       Config::DASH_Y + 5, 1);
}

void resetDashboard() {
  prevDashPresent = false;
  prevDashDist = -9999;
  prevDashAngle = -9999;
  prevDashSpeed = -9999;
  dashNeedsFullRedraw = true;
}

void updateDashboard(float distMM, float angleDeg, float speedCms,
                     bool present) {
  bool changed = dashNeedsFullRedraw || (present != prevDashPresent) ||
                 (present && (fabsf(distMM - prevDashDist) >= 50.0f ||
                              fabsf(angleDeg - prevDashAngle) >= 1.0f ||
                              fabsf(speedCms - prevDashSpeed) >= 5.0f));
  if (!changed)
    return;

  dashNeedsFullRedraw = false;
  prevDashPresent = present;
  prevDashDist = distMM;
  prevDashAngle = angleDeg;
  prevDashSpeed = speedCms;

  int cardW = 76, pad = 4, valY = Config::DASH_Y + 20;
  tft.fillRect(pad + 1, valY - 1, 3 * cardW + 2 * pad - 2, 30, Config::C_BG);

  // DIST
  if (present && distMM > 0) {
    char dbuf[10];
    dtostrf(distMM / 1000.0f, 4, 2, dbuf);
    strcat(dbuf, "m");
    tft.setTextColor(Config::C_GREEN, Config::C_BG);
    tft.drawCentreString(dbuf, pad + cardW / 2, valY, 4);
  } else {
    tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
    tft.drawCentreString("--", pad + cardW / 2, valY, 2);
  }
  // ANGLE
  if (present) {
    char abuf[10];
    sprintf(abuf, "%+.0f\xB0", angleDeg);
    tft.setTextColor(Config::C_AMBER, Config::C_BG);
    tft.drawCentreString(abuf, pad + (cardW + pad) + cardW / 2, valY, 4);
  } else {
    tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
    tft.drawCentreString("--", pad + (cardW + pad) + cardW / 2, valY, 2);
  }
  // SPEED
  if (present) {
    char sbuf[12];
    sprintf(sbuf, "%dcm/s", abs((int)speedCms));
    tft.setTextColor(Config::C_RED, Config::C_BG);
    tft.drawCentreString(sbuf, pad + 2 * (cardW + pad) + cardW / 2, valY, 4);
  } else {
    tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
    tft.drawCentreString("--", pad + 2 * (cardW + pad) + cardW / 2, valY, 2);
  }

  // Redraw borders and labels
  for (int i = 0; i < 3; i++) {
    int cx = pad + i * (cardW + pad);
    tft.drawRect(cx, Config::DASH_Y + 2, cardW, Config::DASH_H - 4,
                 Config::C_GREEN_DIM);
  }
  tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
  tft.drawCentreString("DIST", pad + cardW / 2, Config::DASH_Y + 5, 1);
  tft.drawCentreString("ANGLE", pad + (cardW + pad) + cardW / 2,
                       Config::DASH_Y + 5, 1);
  tft.drawCentreString("SPEED", pad + 2 * (cardW + pad) + cardW / 2,
                       Config::DASH_Y + 5, 1);
}


void updateBlip(int slot, int sx, int sy, bool present) {
  if (blips[slot].active) {
    tft.fillCircle(blips[slot].sx, blips[slot].sy, Config::BLIP_RADIUS + 1,
                   Config::C_BG);
    repairGrid(blips[slot].sx, blips[slot].sy, Config::BLIP_RADIUS + 1);
  }
  blips[slot].active = present;
  if (!present)
    return;
  blips[slot].sx = sx;
  blips[slot].sy = sy;
  tft.fillCircle(sx, sy, Config::BLIP_RADIUS, Config::BLIP_COLOR);
}


int getZone(float angleDeg) {
  if (angleDeg < -20.0f)
    return 0;
  if (angleDeg < 0.0f)
    return 1;
  if (angleDeg < 20.0f)
    return 2;
  return 3;
}

void drawFarZone(int zone, bool on) {
  uint16_t col = on ? Config::C_RED : Config::C_BG;
  float aStart, aEnd;
  if (zone == 0) {
    aStart = -cfgMaxAngle();
    aEnd = -20.0f;
  } else if (zone == 1) {
    aStart = -20.0f;
    aEnd = 0.0f;
  } else if (zone == 2) {
    aStart = 0.0f;
    aEnd = 20.0f;
  } else {
    aStart = 20.0f;
    aEnd = cfgMaxAngle();
  }

  for (float frac = 0.88f; frac <= 1.0f; frac += 0.005f) {
    float px = frac * CONE_LEN;
    for (float a = aStart; a <= aEnd; a += 0.5f) {
      float rad = a * PI / 180.0f;
      int rx = Config::APEX_X + (int)(px * sinf(rad));
      int ry = Config::APEX_Y - (int)(px * cosf(rad));
      if (ry < CONE_TOP || ry >= Config::SCREEN_H)
        continue;
      tft.drawPixel(rx, ry, col);
    }
  }
  if (!on) {
    for (int i = 0; i < gridPxCount; i++) {
      float dx = gridPx[i].x - Config::APEX_X;
      float dy = Config::APEX_Y - gridPx[i].y;
      float dist = sqrtf(dx * dx + dy * dy);
      if (dist < CONE_LEN * 0.88f || dist > CONE_LEN * 1.01f)
        continue;
      float angle = atan2f(dx, dy) * 180.0f / PI;
      if (angle >= aStart && angle <= aEnd)
        tft.drawPixel(gridPx[i].x, gridPx[i].y, gridPx[i].col);
    }
  }
}

void clearAllFarZones() {
  for (int z = 0; z < 4; z++) {
    if (farZoneDrawn[z]) {
      drawFarZone(z, false);
      farZoneDrawn[z] = false;
    }
  }
}


void drawRadarBase() {
  tft.fillScreen(Config::C_BG);
  for (int i = 0; i < 3; i++)
    blips[i].active = false;
  clearAllFarZones();
  resetDashboard();
  buildGridTable();
  drawHeader();
  drawDashboardFrame();
  updateDashboard(0, 0, 0, false);
  drawConeGrid();
}