#ifndef GRID_H
#define GRID_H

#include "config.h"
#include "settings.h"

struct GridPx {
  int16_t x, y;
  uint16_t col;
};

struct SpokeEndpoint {
  int16_t  ex, ey;
  float    angleDeg;
  uint16_t col;
};

#define MAX_RING_PX 650

extern GridPx gridPx[MAX_RING_PX];
extern int gridPxCount;
extern SpokeEndpoint spokePts[5];
inline bool gridDirty = true;

float scaleDist(float distMM);
void radarToScreen(float xMM, float yMM, int& sx, int& sy);
void radarToScreenAngle(float distMM, float azDeg, int& sx, int& sy);

void buildGridTable();
void repairGrid(int cx, int cy, int radius);
void drawConeGrid();

#endif