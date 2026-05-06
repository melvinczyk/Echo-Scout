#include <math.h>
#include "radar_screen.h"
#include "radar.h"
#include "device_state.h"
#include "grid.h"


static Blip blips[3] = {{0, 0, false}, {0, 0, false}, {0, 0, false}};
static bool farZoneDrawn[4] = {false, false, false, false};

static float prevDashDist = -9999;
static float prevDashAngle = -9999;
static float prevDashSpeed = -9999;
static bool prevDashPresent = false;
static bool dashNeedsFullRedraw = true;

void drawDashboardFrame() {
  Display::tft.fillRect(0, RadarScreen::DASH_Y, Display::SCREEN_W, RadarScreen::DASH_H,
               Display::Colors::BG);
  Display::tft.drawFastHLine(0, RadarScreen::DASH_Y + RadarScreen::DASH_H, Display::SCREEN_W,
                    Display::Colors::SEP);
  int cardW = 76, pad = 4;
  for (int i = 0; i < 3; i++) {
    int cx = pad + i * (cardW + pad);
    Display::tft.drawRect(cx, RadarScreen::DASH_Y + 2, cardW, RadarScreen::DASH_H - 4,
                 Display::Colors::GREEN_DIM);
  }
  Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
  Display::tft.drawCentreString("DIST", pad + cardW / 2, RadarScreen::DASH_Y + 5, 1);
  Display::tft.drawCentreString("ANGLE", pad + (cardW + pad) + cardW / 2,
                       RadarScreen::DASH_Y + 5, 1);
  Display::tft.drawCentreString("SPEED", pad + 2 * (cardW + pad) + cardW / 2,
                       RadarScreen::DASH_Y + 5, 1);
}

void resetDashboard() {
  prevDashPresent = false;
  prevDashDist = -9999;
  prevDashAngle = -9999;
  prevDashSpeed = -9999;
  dashNeedsFullRedraw = true;
}

void updateDashboard(float distMM, float angleDeg, float speedCms, bool present) {
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

  int cardW = 76, pad = 4, valY = RadarScreen::DASH_Y + 20;
  Display::tft.fillRect(pad + 1, valY - 1, 3 * cardW + 2 * pad - 2, 30, Display::Colors::BG);

  if (present && distMM > 0) {
    char dbuf[10];
    dtostrf(distMM / 1000.0f, 4, 2, dbuf);
    strcat(dbuf, "m");
    Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
    Display::tft.drawCentreString(dbuf, pad + cardW / 2, valY, 4);
  } else {
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("--", pad + cardW / 2, valY, 2);
  }
  if (present) {
    char abuf[10];
    sprintf(abuf, "%+.0f\xB0", angleDeg);
    Display::tft.setTextColor(Display::Colors::AMBER, Display::Colors::BG);
    Display::tft.drawCentreString(abuf, pad + (cardW + pad) + cardW / 2, valY, 4);
  } else {
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("--", pad + (cardW + pad) + cardW / 2, valY, 2);
  }
  if (present) {
    char sbuf[12];
    sprintf(sbuf, "%dcm/s", abs((int)speedCms));
    Display::tft.setTextColor(Display::Colors::RED, Display::Colors::BG);
    Display::tft.drawCentreString(sbuf, pad + 2 * (cardW + pad) + cardW / 2, valY, 4);
  } else {
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("--", pad + 2 * (cardW + pad) + cardW / 2, valY, 2);
  }

  for (int i = 0; i < 3; i++) {
    int cx = pad + i * (cardW + pad);
    Display::tft.drawRect(cx, RadarScreen::DASH_Y + 2, cardW, RadarScreen::DASH_H - 4,
                 Display::Colors::GREEN_DIM);
  }
  Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
  Display::tft.drawCentreString("DIST", pad + cardW / 2, RadarScreen::DASH_Y + 5, 1);
  Display::tft.drawCentreString("ANGLE", pad + (cardW + pad) + cardW / 2,
                       RadarScreen::DASH_Y + 5, 1);
  Display::tft.drawCentreString("SPEED", pad + 2 * (cardW + pad) + cardW / 2,
                       RadarScreen::DASH_Y + 5, 1);
}


void updateBlip(int slot, int sx, int sy, bool present) {
  if (blips[slot].active) {
    Display::tft.fillCircle(blips[slot].sx, blips[slot].sy, RadarScreen::BLIP_RADIUS + 1,
                   Display::Colors::BG);
    repairGrid(blips[slot].sx, blips[slot].sy, RadarScreen::BLIP_RADIUS + 1);
  }
  blips[slot].active = present;
  if (!present)
    return;
  blips[slot].sx = sx;
  blips[slot].sy = sy;
  Display::tft.fillCircle(sx, sy, RadarScreen::BLIP_RADIUS, RadarScreen::BLIP_COLOR);
}


void drawFarZone(int zone, bool on) {
  if (farZoneDrawn[zone] == on) return;
  farZoneDrawn[zone] = on;
  uint16_t col = on ? Display::Colors::RED : Display::Colors::BG;
  float aStart, aEnd;
  if (zone == 0) {
    aStart = -cfgMaxAngle(); aEnd = -20.0f;
  } else if (zone == 1) {
    aStart = -20.0f; aEnd = 0.0f;
  } else if (zone == 2) {
    aStart = 0.0f; aEnd = 20.0f;
  } else {
    aStart = 20.0f; aEnd = cfgMaxAngle();
  }

  for (float frac = 0.88f; frac <= 1.0f; frac += 0.005f) {
    float px = frac * CONE_LEN;
    for (float a = aStart; a <= aEnd; a += 0.5f) {
      float rad = a * PI / 180.0f;
      int rx = RadarScreen::APEX_X + (int)(px * sinf(rad));
      int ry = RadarScreen::APEX_Y - (int)(px * cosf(rad));
      if (ry < CONE_TOP || ry >= Display::SCREEN_H) continue;
      Display::tft.drawPixel(rx, ry, col);
    }
  }
  if (!on) {
    for (int i = 0; i < gridPxCount; i++) {
      float dx = gridPx[i].x - RadarScreen::APEX_X;
      float dy = RadarScreen::APEX_Y - gridPx[i].y;
      float dist = sqrtf(dx * dx + dy * dy);
      if (dist < CONE_LEN * 0.88f || dist > CONE_LEN * 1.01f) continue;
      float angle = atan2f(dx, dy) * 180.0f / PI;
      if (angle >= aStart && angle <= aEnd)
        Display::tft.drawPixel(gridPx[i].x, gridPx[i].y, gridPx[i].col);
    }
  }
}

void clearAllFarZones() {
  for (int z = 0; z < 4; z++) {
    if (farZoneDrawn[z])
      drawFarZone(z, false);
  }
}

void drawRadarBase() {
  Display::tft.fillScreen(Display::Colors::BG);

  if (!RadarState::found) {
    Display::drawHeader("ECHO SCOUT");
    Display::drawErrorScreen("RADAR NOT FOUND");
    return;
  }

  for (int i = 0; i < 3; i++)
    blips[i].active = false;
  clearAllFarZones();
  resetDashboard();
  buildGridTable();

  drawConeGrid();
  Display::drawHeader("ECHO SCOUT");
  drawDashboardFrame();
  updateDashboard(0, 0, 0, false);
}

void tickRadar() {
  radarProcessSerial();

  static bool wasReady = false;
  if (RadarState::ready && !wasReady) {
    wasReady = true;
    drawRadarBase();
  }
  if (!RadarState::newFrame) return;
  RadarState::newFrame = false;

  for (int i = 0; i < 3; i++) {
    if (RadarState::slotActive[i]) {
      int sx, sy;
      radarToScreen(RadarState::slotX[i], RadarState::slotY[i], sx, sy);
      if (sy >= CONE_TOP && sy < Display::SCREEN_H)
        updateBlip(i, sx, sy, true);
      else
        updateBlip(i, 0, 0, false);
    } else {
      updateBlip(i, 0, 0, false);
    }
  }

  for (int z = 0; z < 4; z++)
    drawFarZone(z, RadarState::farZone[z]);

  updateDashboard(RadarState::distance, RadarState::azimuth, RadarState::speed, RadarState::present);
}