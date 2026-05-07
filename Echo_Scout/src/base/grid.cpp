#include "base/grid.h"
#include "screens/radar_screen.h"

GridPx gridPx[MAX_RING_PX];
int gridPxCount = 0;
SpokeEndpoint spokePts[5];

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
    sx = RadarScreen::APEX_X;
    sy = RadarScreen::APEX_Y;
    return;
  }
  float norm = scaleDist(dist);
  float px = norm * CONE_LEN;
  sx = RadarScreen::APEX_X + (int)(px * (xMM / dist));
  sy = RadarScreen::APEX_Y - (int)(px * (yMM / dist));
}

void radarToScreenAngle(float distMM, float azDeg, int& sx, int& sy) {
  float norm = scaleDist(distMM);
  float px = norm * CONE_LEN;
  float rad = azDeg * PI / 180.0f;
  sx = RadarScreen::APEX_X + (int)(px * sinf(rad));
  sy = RadarScreen::APEX_Y - (int)(px * cosf(rad));
}

void buildGridTable() {
  if (!gridDirty) return;
  gridDirty = false;

  gridPxCount = 0;
  float accRange = cfgAccRange();
  float halfDeg = cfgMaxAngle();
  float spokes[] = {-halfDeg, -halfDeg / 2, 0, halfDeg / 2, halfDeg};

  for (int s = 0; s < 5; s++) {
    float rad = spokes[s] * PI / 180.0f;
    uint16_t col = (s == 0 || s == 4) ? Display::Colors::GREEN_DIM : Display::Colors::GREEN_FAINT;
    spokePts[s] = {
      (int16_t)(RadarScreen::APEX_X + (int)(CONE_LEN * sinf(rad))),
      (int16_t)(RadarScreen::APEX_Y - (int)(CONE_LEN * cosf(rad))),
      spokes[s],
      col
    };
  }

  for (float rMM = 1000.0f; rMM < accRange; rMM += 1000.0f) {
    if (gridPxCount >= MAX_RING_PX - 120)
      break;
    float px = scaleDist(rMM) * CONE_LEN;
    for (float a = -halfDeg; a <= halfDeg; a += 1.5f) {
      if (gridPxCount >= MAX_RING_PX)
        break;
      float rad = a * PI / 180.0f;
      int gx = RadarScreen::APEX_X + (int)(px * sinf(rad));
      int gy = RadarScreen::APEX_Y - (int)(px * cosf(rad));
      if (gy < CONE_TOP || gy >= Display::SCREEN_H)
        continue;
      gridPx[gridPxCount++] = {(int16_t)gx, (int16_t)gy, Display::Colors::GREEN_FAINT};
    }
  }
  // Boundary ring
  {
    float px = scaleDist(accRange) * CONE_LEN;
    for (float a = -halfDeg; a <= halfDeg; a += 1.5f) {
      if (gridPxCount >= MAX_RING_PX)
        break;
      float rad = a * PI / 180.0f;
      int gx = RadarScreen::APEX_X + (int)(px * sinf(rad));
      int gy = RadarScreen::APEX_Y - (int)(px * cosf(rad));
      if (gy < CONE_TOP || gy >= Display::SCREEN_H)
        continue;
      gridPx[gridPxCount++] = {(int16_t)gx, (int16_t)gy, Display::Colors::GREEN_DIM};
    }
  }
  // Far zone arc
  {
    float px = 1.0f * CONE_LEN;
    for (float a = -halfDeg; a <= halfDeg; a += 1.5f) {
      if (gridPxCount >= MAX_RING_PX)
        break;
      float rad = a * PI / 180.0f;
      int gx = RadarScreen::APEX_X + (int)(px * sinf(rad));
      int gy = RadarScreen::APEX_Y - (int)(px * cosf(rad));
      if (gy < CONE_TOP || gy >= Display::SCREEN_H)
        continue;
      gridPx[gridPxCount++] = {(int16_t)gx, (int16_t)gy, Display::Colors::GREEN_FAINT};
    }
  }
  // Spokes
  for (int s = 0; s < 5; s++) {
    uint16_t col =
        (s == 0 || s == 4) ? Display::Colors::GREEN_DIM : Display::Colors::GREEN_FAINT;
    float rad = spokes[s] * PI / 180.0f;
    for (float px_d = 0; px_d <= 1.0f * CONE_LEN; px_d += 3.0f) {
      if (gridPxCount >= MAX_RING_PX)
        break;
      int gx = RadarScreen::APEX_X + (int)(px_d * sinf(rad));
      int gy = RadarScreen::APEX_Y - (int)(px_d * cosf(rad));
      if (gy < CONE_TOP || gy >= Display::SCREEN_H)
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
      int gx = RadarScreen::APEX_X + (int)(px * sinf(rad));
      int gy = RadarScreen::APEX_Y - (int)(px * cosf(rad));
      if (gy < CONE_TOP || gy >= Display::SCREEN_H)
        continue;
      gridPx[gridPxCount++] = {(int16_t)gx, (int16_t)gy, Display::Colors::GREEN_FAINT};
    }
  }
}

void repairGrid(int cx, int cy, int radius) {
  float accRange = cfgAccRange();
  float halfDeg = cfgMaxAngle();

  for (int s = 0; s < 5; s++) {
    float dx = spokePts[s].ex - RadarScreen::APEX_X;
    float dy = spokePts[s].ey - RadarScreen::APEX_Y;
    float len = sqrtf(dx * dx + dy * dy);
    float cross =
        fabsf((cx - RadarScreen::APEX_X) * dy - (cy - RadarScreen::APEX_Y) * dx) / len;
    if (cross <= radius + 4) {
      Display::tft.drawLine(RadarScreen::APEX_X, RadarScreen::APEX_Y,
                            spokePts[s].ex, spokePts[s].ey, spokePts[s].col);
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
        sqrtf((float)(cx - RadarScreen::APEX_X) * (cx - RadarScreen::APEX_X) +
              (float)(cy - RadarScreen::APEX_Y) * (cy - RadarScreen::APEX_Y));
    if (fabsf(distFromApex - px) <= radius + 4) {
      uint16_t col = (rings[r] >= accRange - 1) ? Display::Colors::GREEN_DIM
                                                : Display::Colors::GREEN_FAINT;
      for (float a = -halfDeg; a <= halfDeg; a += 0.8f) {
        float rad = a * PI / 180.0f;
        int gx = RadarScreen::APEX_X + (int)(px * sinf(rad));
        int gy = RadarScreen::APEX_Y - (int)(px * cosf(rad));
        if (gy < CONE_TOP || gy >= Display::SCREEN_H)
          continue;
        Display::tft.drawPixel(gx, gy, col);
      }
    }
  }

  float distFromApex =
      sqrtf((float)(cx - RadarScreen::APEX_X) * (cx - RadarScreen::APEX_X) +
            (float)(cy - RadarScreen::APEX_Y) * (cy - RadarScreen::APEX_Y));
  if (fabsf(distFromApex - farArcPx) <= radius + 4) {
    for (float a = -halfDeg; a <= halfDeg; a += 0.8f) {
      float rad = a * PI / 180.0f;
      int gx = RadarScreen::APEX_X + (int)(farArcPx * sinf(rad));
      int gy = RadarScreen::APEX_Y - (int)(farArcPx * cosf(rad));
      if (gy < CONE_TOP || gy >= Display::SCREEN_H)
        continue;
      Display::tft.drawPixel(gx, gy, Display::Colors::GREEN_FAINT);
    }
  }
  Display::tft.fillCircle(RadarScreen::APEX_X, RadarScreen::APEX_Y, 4, Display::Colors::GREEN);
}

void drawConeGrid() {
  float accRange = cfgAccRange();

  for (int i = 0; i < gridPxCount; i++)
    Display::tft.drawPixel(gridPx[i].x, gridPx[i].y, gridPx[i].col);

  for (int s = 0; s < 5; s++) {
    Display::tft.drawLine(RadarScreen::APEX_X, RadarScreen::APEX_Y,
                          spokePts[s].ex, spokePts[s].ey, spokePts[s].col);
  }

  
  if (CONE_TOP > Display::HEADER_H)
    Display::tft.fillRect(0, Display::HEADER_H, Display::SCREEN_W,
                 RadarScreen::DASH_Y - Display::HEADER_H, Display::Colors::BG);
  Display::tft.fillCircle(RadarScreen::APEX_X, RadarScreen::APEX_Y, 4, Display::Colors::GREEN);

  // Inner ring labels
  for (float rMM = 1000.0f; rMM < accRange; rMM += 1000.0f) {
    int lx, ly;
    radarToScreenAngle(rMM, 0.0f, lx, ly);
    ly = constrain(ly, CONE_TOP + 4, RadarScreen::APEX_Y - 6);
    char lbl[8];
    sprintf(lbl, "%dm", (int)(rMM / 1000));
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawString(lbl, lx + 2, ly - 8, 1);
  }
  // Boundary ring label
  {
    int lx, ly;
    radarToScreenAngle(accRange, 0.0f, lx, ly);
    ly = constrain(ly, CONE_TOP + 4, RadarScreen::APEX_Y - 6);
    char lbl[8];
    sprintf(lbl, "%dm", (int)(accRange / 1000));
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawString(lbl, lx + 2, ly - 8, 1);
  }
  // Angle labels
  for (int i = 0; i < 5; i++) {
    int lx = constrain((int)spokePts[i].ex, 4, 230);
    int ly = constrain((int)spokePts[i].ey, CONE_TOP + 2, RadarScreen::APEX_Y - 8);
    char lbl[8];
    sprintf(lbl, "%+.0f", spokePts[i].angleDeg);
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString(lbl, lx, ly, 1);
  }
  // Min range label
  if (cfgMinRange() > 0) {
    int lx, ly;
    radarToScreenAngle(cfgMinRange(), 0.0f, lx, ly);
    ly = constrain(ly, CONE_TOP + 4, RadarScreen::APEX_Y - 6);
    char lbl[10];
    sprintf(lbl, "%dmm", (int)cfgMinRange());
    Display::tft.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
    Display::tft.drawString(lbl, lx + 2, ly - 8, 1);
  }
}