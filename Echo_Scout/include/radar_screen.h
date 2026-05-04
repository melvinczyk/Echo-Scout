#ifndef RADAR_SCREEN_H
#define RADAR_SCREEN_H

#include <TFT_eSPI.h>
#include "config.h"
#include "grid.h"
#include "settings.h"

extern TFT_eSPI tft;
extern TFT_eSprite spr;

struct Blip {
  int sx, sy;
  bool active;
};
extern Blip blips[3];
extern bool farZoneDrawn[4];

void drawHeader();

void drawDashboardFrame();
void updateDashboard(float distMM, float angleDeg, float speedCms,
                     bool present);
void resetDashboard();

void updateBlip(int slot, int sx, int sy, bool present);

int getZone(float angleDeg);
void drawFarZone(int zone, bool on);
void clearAllFarZones();

void drawRadarBase();

#endif