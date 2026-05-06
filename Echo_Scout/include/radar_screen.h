#ifndef RADAR_SCREEN_H
#define RADAR_SCREEN_H

#include "config.h"
#include "grid.h"
#include "settings.h"
#include "display.h"

namespace RadarScreen {
    constexpr int      BLIP_RADIUS = 5;
    constexpr uint16_t BLIP_COLOR  = 0xF800;
    constexpr int DASH_Y = 30;
    constexpr int DASH_H = 54;
    constexpr int APEX_X = 120;
    constexpr int APEX_Y = 316;
}

#define CONE_TOP (RadarScreen::DASH_Y + RadarScreen::DASH_H + 1)
#define CONE_LEN (RadarScreen::APEX_Y - CONE_TOP - 4)

struct Blip {
  int sx, sy;
  bool active;
};
void drawDashboardFrame();
void updateDashboard(float distMM, float angleDeg, float speedCms, bool present);
void resetDashboard();

void updateBlip(int slot, int sx, int sy, bool present);

void drawFarZone(int zone, bool on);
void clearAllFarZones();

void drawRadarBase();
void tickRadar();

#endif