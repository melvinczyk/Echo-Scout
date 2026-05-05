#include "grid.h"
#include "radar_screen.h"

GridPx gridPx[MAX_RING_PX];
int gridPxCount = 0;

float scaleDist(float distMM) {
  float ratio = constrain(distMM / cfgAccRange(), 0.0f, 1.0f);
  float power = (cfgAccRange() <= 2000)   ? 0.62f
                : (cfgAccRange() <= 4000) ? 0.75f
                                          : 0.85f;
  return powf(ratio, power) * 0.88f;
}

void radarToScreen(float xMM, float yMM, int& sx, int& sy) {
  float dist = sqrtf(xMM * xMM + yMM * yMM);
  if (dist < 1.0f) {
    sx = Config::APEX_X;
    sy = Config::APEX_Y;
    return;
  }
  float norm = scaleDist(dist);
  float px = norm * CONE_LEN;
  sx = Config::APEX_X + (int)(px * (xMM / dist));
  sy = Config::APEX_Y - (int)(px * (yMM / dist));
}

void radarToScreenAngle(float distMM, float azDeg, int& sx, int& sy) {
  float norm = scaleDist(distMM);
  float px = norm * CONE_LEN;
  float rad = azDeg * PI / 180.0f;
  sx = Config::APEX_X + (int)(px * sinf(rad));
  sy = Config::APEX_Y - (int)(px * cosf(rad));
}

void buildGridTable() {
  gridPxCount = 0;
  float accRange = cfgAccRange();
  float halfDeg = cfgMaxAngle();
  float spokes[] = {-halfDeg, -halfDeg / 2, 0, halfDeg / 2, halfDeg};

  for (float rMM = 1000.0f; rMM < accRange; rMM += 1000.0f) {
    if (gridPxCount >= MAX_RING_PX - 120)
      break;
    float px = scaleDist(rMM) * CONE_LEN;
    for (float a = -halfDeg; a <= halfDeg; a += 1.5f) {
      if (gridPxCount >= MAX_RING_PX)
        break;
      float rad = a * PI / 180.0f;
      int gx = Config::APEX_X + (int)(px * sinf(rad));
      int gy = Config::APEX_Y - (int)(px * cosf(rad));
      if (gy < CONE_TOP || gy >= Config::SCREEN_H)
        continue;
      gridPx[gridPxCount++] = {(int16_t)gx, (int16_t)gy, Config::C_GREEN_FAINT};
    }
  }
  // Boundary ring
  {
    float px = scaleDist(accRange) * CONE_LEN;
    for (float a = -halfDeg; a <= halfDeg; a += 1.5f) {
      if (gridPxCount >= MAX_RING_PX)
        break;
      float rad = a * PI / 180.0f;
      int gx = Config::APEX_X + (int)(px * sinf(rad));
      int gy = Config::APEX_Y - (int)(px * cosf(rad));
      if (gy < CONE_TOP || gy >= Config::SCREEN_H)
        continue;
      gridPx[gridPxCount++] = {(int16_t)gx, (int16_t)gy, Config::C_GREEN_DIM};
    }
  }
  // Far zone arc
  {
    float px = 1.0f * CONE_LEN;
    for (float a = -halfDeg; a <= halfDeg; a += 1.5f) {
      if (gridPxCount >= MAX_RING_PX)
        break;
      float rad = a * PI / 180.0f;
      int gx = Config::APEX_X + (int)(px * sinf(rad));
      int gy = Config::APEX_Y - (int)(px * cosf(rad));
      if (gy < CONE_TOP || gy >= Config::SCREEN_H)
        continue;
      gridPx[gridPxCount++] = {(int16_t)gx, (int16_t)gy, Config::C_GREEN_FAINT};
    }
  }
  // Spokes
  for (int s = 0; s < 5; s++) {
    uint16_t col =
        (s == 0 || s == 4) ? Config::C_GREEN_DIM : Config::C_GREEN_FAINT;
    float rad = spokes[s] * PI / 180.0f;
    for (float px_d = 0; px_d <= 1.0f * CONE_LEN; px_d += 3.0f) {
      if (gridPxCount >= MAX_RING_PX)
        break;
      int gx = Config::APEX_X + (int)(px_d * sinf(rad));
      int gy = Config::APEX_Y - (int)(px_d * cosf(rad));
      if (gy < CONE_TOP || gy >= Config::SCREEN_H)
        continue;
      gridPx[gridPxCount++] = {(int16_t)gx, (int16_t)gy, col};
    }
  }
  // Min range arc
  if (cfgMinRange() > 0) {
    float px = scaleDist(cfgMinRange()) * CONE_LEN;
    for (float a = -halfDeg; a <= halfDeg; a += 1.5f) {
      if (gridPxCount >= MAX_RING_PX)
        break;
      float rad = a * PI / 180.0f;
      int gx = Config::APEX_X + (int)(px * sinf(rad));
      int gy = Config::APEX_Y - (int)(px * cosf(rad));
      if (gy < CONE_TOP || gy >= Config::SCREEN_H)
        continue;
      gridPx[gridPxCount++] = {(int16_t)gx, (int16_t)gy, Config::C_GREEN_FAINT};
    }
  }
}

void repairGrid(int cx, int cy, int radius) {
  float accRange = cfgAccRange();
  float halfDeg = cfgMaxAngle();
  float spokes[] = {-halfDeg, -halfDeg / 2, 0, halfDeg / 2, halfDeg};

  for (int s = 0; s < 5; s++) {
    float rad = spokes[s] * PI / 180.0f;
    int ex = Config::APEX_X + (int)(1.0f * CONE_LEN * sinf(rad));
    int ey = Config::APEX_Y - (int)(1.0f * CONE_LEN * cosf(rad));
    float dx = ex - Config::APEX_X;
    float dy = ey - Config::APEX_Y;
    float len = sqrtf(dx * dx + dy * dy);
    float cross =
        fabsf((cx - Config::APEX_X) * dy - (cy - Config::APEX_Y) * dx) / len;
    if (cross <= radius + 4) {
      uint16_t col =
          (s == 0 || s == 4) ? Config::C_GREEN_DIM : Config::C_GREEN_FAINT;
      tft.drawLine(Config::APEX_X, Config::APEX_Y, ex, ey, col);
    }
  }

  float rings[20];
  int ringCount = 0;
  for (float rMM = 1000.0f; rMM <= accRange; rMM += 1000.0f)
    rings[ringCount++] = rMM;
  if (cfgMinRange() > 0)
    rings[ringCount++] = cfgMinRange();
  float farArcPx = 1.0f * CONE_LEN;

  for (int r = 0; r < ringCount; r++) {
    float px = scaleDist(rings[r]) * CONE_LEN;
    float distFromApex =
        sqrtf((float)(cx - Config::APEX_X) * (cx - Config::APEX_X) +
              (float)(cy - Config::APEX_Y) * (cy - Config::APEX_Y));
    if (fabsf(distFromApex - px) <= radius + 4) {
      uint16_t col = (rings[r] >= accRange - 1) ? Config::C_GREEN_DIM
                                                : Config::C_GREEN_FAINT;
      for (float a = -halfDeg; a <= halfDeg; a += 0.8f) {
        float rad = a * PI / 180.0f;
        int gx = Config::APEX_X + (int)(px * sinf(rad));
        int gy = Config::APEX_Y - (int)(px * cosf(rad));
        if (gy < CONE_TOP || gy >= Config::SCREEN_H)
          continue;
        tft.drawPixel(gx, gy, col);
      }
    }
  }

  float distFromApex =
      sqrtf((float)(cx - Config::APEX_X) * (cx - Config::APEX_X) +
            (float)(cy - Config::APEX_Y) * (cy - Config::APEX_Y));
  if (fabsf(distFromApex - farArcPx) <= radius + 4) {
    for (float a = -halfDeg; a <= halfDeg; a += 0.8f) {
      float rad = a * PI / 180.0f;
      int gx = Config::APEX_X + (int)(farArcPx * sinf(rad));
      int gy = Config::APEX_Y - (int)(farArcPx * cosf(rad));
      if (gy < CONE_TOP || gy >= Config::SCREEN_H)
        continue;
      tft.drawPixel(gx, gy, Config::C_GREEN_FAINT);
    }
  }
  tft.fillCircle(Config::APEX_X, Config::APEX_Y, 4, Config::C_GREEN);
}

void drawConeGrid() {
  float accRange = cfgAccRange();
  float halfDeg = cfgMaxAngle();
  float spokes[] = {-halfDeg, -halfDeg / 2, 0, halfDeg / 2, halfDeg};

  for (int i = 0; i < gridPxCount; i++)
    tft.drawPixel(gridPx[i].x, gridPx[i].y, gridPx[i].col);

  for (int s = 0; s < 5; s++) {
    float rad = spokes[s] * PI / 180.0f;
    int ex = Config::APEX_X + (int)(1.0f * CONE_LEN * sinf(rad));
    int ey = Config::APEX_Y - (int)(1.0f * CONE_LEN * cosf(rad));
    uint16_t col =
        (s == 0 || s == 4) ? Config::C_GREEN_DIM : Config::C_GREEN_FAINT;
    tft.drawLine(Config::APEX_X, Config::APEX_Y, ex, ey, col);
  }

  
  if (CONE_TOP > Config::HEADER_H)
    tft.fillRect(0, Config::HEADER_H, Config::SCREEN_W,
                 Config::DASH_Y - Config::HEADER_H, Config::C_BG);
  tft.fillCircle(Config::APEX_X, Config::APEX_Y, 4, Config::C_GREEN);

  // Inner ring labels
  for (float rMM = 1000.0f; rMM < accRange; rMM += 1000.0f) {
    int lx, ly;
    radarToScreenAngle(rMM, 0.0f, lx, ly);
    ly = constrain(ly, CONE_TOP + 4, Config::APEX_Y - 6);
    char lbl[8];
    sprintf(lbl, "%dm", (int)(rMM / 1000));
    tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
    tft.drawString(lbl, lx + 2, ly - 8, 1);
  }
  // Boundary ring label
  {
    int lx, ly;
    radarToScreenAngle(accRange, 0.0f, lx, ly);
    ly = constrain(ly, CONE_TOP + 4, Config::APEX_Y - 6);
    char lbl[8];
    sprintf(lbl, "%dm", (int)(accRange / 1000));
    tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
    tft.drawString(lbl, lx + 2, ly - 8, 1);
  }
  // Angle labels
  for (int i = 0; i < 5; i++) {
    float rad = spokes[i] * PI / 180.0f;
    int lx = Config::APEX_X + (int)(1.0f * CONE_LEN * sinf(rad));
    int ly = Config::APEX_Y - (int)(1.0f * CONE_LEN * cosf(rad));
    lx = constrain(lx, 4, 230);
    ly = constrain(ly, CONE_TOP + 2, Config::APEX_Y - 8);
    char lbl[8];
    sprintf(lbl, "%+.0f", spokes[i]);
    tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
    tft.drawCentreString(lbl, lx, ly, 1);
  }
  // Min range label
  if (cfgMinRange() > 0) {
    int lx, ly;
    radarToScreenAngle(cfgMinRange(), 0.0f, lx, ly);
    ly = constrain(ly, CONE_TOP + 4, Config::APEX_Y - 6);
    char lbl[10];
    sprintf(lbl, "%dmm", (int)cfgMinRange());
    tft.setTextColor(Config::C_GREEN_FAINT, Config::C_BG);
    tft.drawString(lbl, lx + 2, ly - 8, 1);
  }
}