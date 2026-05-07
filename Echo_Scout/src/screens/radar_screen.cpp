#include <math.h>
#include "screens/radar_screen.h"
#include "devices/radar.h"
#include "devices/device_state.h"
#include "base/grid.h"
#include "base/measurements.h"


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
  for (int i = 0; i < 3; i++) {
    int cx = RadarScreen::DASH_PAD + i * (RadarScreen::CARD_W + RadarScreen::DASH_PAD);
    Display::tft.drawRect(cx, RadarScreen::DASH_Y + 2, RadarScreen::CARD_W, RadarScreen::DASH_H - 4,
                 Display::Colors::GREEN_DIM);
  }
  Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
  Display::tft.drawCentreString("DIST",  RadarScreen::DASH_PAD + RadarScreen::CARD_W / 2, RadarScreen::DASH_Y + 5, 1);
  Display::tft.drawCentreString("ANGLE", RadarScreen::DASH_PAD + (RadarScreen::CARD_W + RadarScreen::DASH_PAD) + RadarScreen::CARD_W / 2, RadarScreen::DASH_Y + 5, 1);
  Display::tft.drawCentreString("SPEED", RadarScreen::DASH_PAD + 2 * (RadarScreen::CARD_W + RadarScreen::DASH_PAD) + RadarScreen::CARD_W / 2, RadarScreen::DASH_Y + 5, 1);
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

  constexpr int valY = RadarScreen::DASH_Y + 20;
  Display::tft.fillRect(RadarScreen::DASH_PAD + 1, valY - 1,
                        3 * RadarScreen::CARD_W + 2 * RadarScreen::DASH_PAD - 2, 30, Display::Colors::BG);

  if (present && distMM > 0) {
    char dbuf[16];
    Measurements::fmtDist(distMM, dbuf, 16);
    Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
    Display::tft.drawCentreString(dbuf, RadarScreen::DASH_PAD + RadarScreen::CARD_W / 2, valY, 4);
  } else {
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("--", RadarScreen::DASH_PAD + RadarScreen::CARD_W / 2, valY, 2);
  }
  if (present) {
    char abuf[10];
    sprintf(abuf, "%+.0f\xB0", angleDeg);
    Display::tft.setTextColor(Display::Colors::AMBER, Display::Colors::BG);
    Display::tft.drawCentreString(abuf, RadarScreen::DASH_PAD + (RadarScreen::CARD_W + RadarScreen::DASH_PAD) + RadarScreen::CARD_W / 2, valY, 4);
  } else {
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("--", RadarScreen::DASH_PAD + (RadarScreen::CARD_W + RadarScreen::DASH_PAD) + RadarScreen::CARD_W / 2, valY, 2);
  }
  if (present) {
    char sbuf[16];
    Measurements::fmtSpeed(speedCms, sbuf, 16);
    Display::tft.setTextColor(Display::Colors::RED, Display::Colors::BG);
    Display::tft.drawCentreString(sbuf, RadarScreen::DASH_PAD + 2 * (RadarScreen::CARD_W + RadarScreen::DASH_PAD) + RadarScreen::CARD_W / 2, valY, 4);
  } else {
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("--", RadarScreen::DASH_PAD + 2 * (RadarScreen::CARD_W + RadarScreen::DASH_PAD) + RadarScreen::CARD_W / 2, valY, 2);
  }
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
  for (int i = 0; i < farZonePxCount[zone]; i++)
    Display::tft.drawPixel(farZonePx[zone][i].x, farZonePx[zone][i].y, col);
  if (!on) {
    float halfDeg = cfgMaxAngle();
    float aStart = (zone == 0) ? -halfDeg : (zone == 1) ? -20.0f : (zone == 2) ? 0.0f : 20.0f;
    float aEnd   = (zone == 0) ? -20.0f   : (zone == 1) ?  0.0f  : (zone == 2) ? 20.0f : halfDeg;
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