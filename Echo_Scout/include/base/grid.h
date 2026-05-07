#ifndef GRID_H
#define GRID_H

#include "base/config.h"
#include "base/settings.h"

struct GridPx {
  int16_t x, y;
  uint16_t col;
};

struct SpokeEndpoint {
  int16_t ex, ey;
  float angleDeg;
  uint16_t col;
};

struct FarZonePx {
  int16_t x, y;
};

#define MAX_RING_PX 650
#define MAX_FAR_ZONE_PX 2048

extern GridPx gridPx[MAX_RING_PX];
extern int gridPxCount;
extern SpokeEndpoint spokePts[5];
extern FarZonePx farZonePx[4][MAX_FAR_ZONE_PX];
extern int farZonePxCount[4];
inline bool gridDirty = true;

float scaleDist(float distMM);
void radarToScreen(float xMM, float yMM, int& sx, int& sy);
void radarToScreenAngle(float distMM, float azDeg, int& sx, int& sy);

void buildGridTable();
void repairGrid(int cx, int cy, int radius);
void drawConeGrid();

#endif
