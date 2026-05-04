#ifndef GRID_H
#define GRID_H
#include "config.h"
#include "settings.h"
#include <Arduino.h>
#include <TFT_eSPI.h>

struct GridPx {
  int16_t x, y;
  uint16_t col;
};

#define MAX_RING_PX 650

extern GridPx gridPx[MAX_RING_PX];
extern int gridPxCount;

#define CONE_TOP (Config::DASH_Y + Config::DASH_H + 1)
#define CONE_LEN (Config::APEX_Y - CONE_TOP - 4)

float scaleDist(float distMM);
void radarToScreen(float xMM, float yMM, int &sx, int &sy);
void radarToScreenAngle(float distMM, float azDeg, int &sx, int &sy);

void buildGridTable();
void repairGrid(int cx, int cy, int radius);
void drawConeGrid();

#endif