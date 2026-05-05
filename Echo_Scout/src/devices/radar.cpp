#include <math.h>
#include "radar.h"
#include "settings.h"
#include "device_state.h"
#include "app_state.h"
#include "radar_screen.h"

static float smoothedX[3]  = {0, 0, 0};
static float smoothedY[3]  = {0, 0, 0};
static float lastBlipX[3]  = {0, 0, 0};
static float lastBlipY[3]  = {0, 0, 0};
static uint8_t hitCount[3] = {0, 0, 0};
static uint8_t dropCount[3]= {0, 0, 0};

void radarInit() {
    unsigned long start = millis();
    while (millis() - start < 500) {
        if (Serial1.available()) {
            RadarState::found = true;
            return;
        }
    }
    RadarState::found = false;
    Serial.println("Radar not found");
}

RawTarget parseTarget(const uint8_t* b) {
  RawTarget t;
  uint16_t rx = b[0] | (b[1] << 8);
  uint16_t ry = b[2] | (b[3] << 8);
  uint16_t rs = b[4] | (b[5] << 8);
  t.pixelDist = b[6] | (b[7] << 8);
  t.present = !(rx == 0 && ry == 0 && rs == 0);
  t.x = (rx & 0x7FFF);
  if (!(rx & 0x8000))
    t.x = -t.x;
  t.y = (ry & 0x7FFF);
  if (!(ry & 0x8000))
    t.y = -t.y;
  t.speed = (rs & 0x7FFF);
  if (!(rs & 0x8000))
    t.speed = -t.speed;
  t.x = -t.x;
  if (t.present && t.y <= 0)
    t.present = false;
  if (t.present && t.y < cfgMinRange())
    t.present = false;
  if (t.present && t.y > 8000.0f)
    t.present = false;
  if (t.present) {
    float a = fabsf(atan2f((float)t.x, (float)t.y) * 180.0f / PI);
    if (a > cfgMaxAngle())
      t.present = false;
  }
  if (t.present && cfgSpeedGate() > 0 && abs(t.speed) > cfgSpeedGate())
    t.present = false;
  return t;
}


void applyPersistence(RawTarget targets[3]) {
  for (int i = 0; i < 3; i++) {
    if (targets[i].present) {
      dropCount[i] = 0;
      if (hitCount[i] < cfgHits())
        hitCount[i]++;
      if (hitCount[i] < cfgHits())
        targets[i].present = false;
    } else {
      hitCount[i] = 0;
      if (dropCount[i] < cfgDrops())
        dropCount[i]++;
      if (dropCount[i] < cfgDrops())
        targets[i].present = true;
    }
  }
}


bool smoothTarget(int slot, bool wasTracked, float rawX, float rawY,
                  float& outX, float& outY) {
  float dx = rawX - lastBlipX[slot];
  float dy = rawY - lastBlipY[slot];
  if (sqrtf(dx * dx + dy * dy) >= 180.0f && wasTracked)
    return false;

  if (cfgSmoothing() && wasTracked) {
    smoothedX[slot] = SMOOTH_ALPHA * rawX + (1.0f - SMOOTH_ALPHA) * smoothedX[slot];
    smoothedY[slot] = SMOOTH_ALPHA * rawY + (1.0f - SMOOTH_ALPHA) * smoothedY[slot];
  } else {
    smoothedX[slot] = rawX;
    smoothedY[slot] = rawY;
  }

  float mdx = smoothedX[slot] - lastBlipX[slot];
  float mdy = smoothedY[slot] - lastBlipY[slot];
  if (sqrtf(mdx * mdx + mdy * mdy) > cfgMoveThresh() || !wasTracked) {
    lastBlipX[slot] = smoothedX[slot];
    lastBlipY[slot] = smoothedY[slot];
    outX = smoothedX[slot];
    outY = smoothedY[slot];
    return true;
  }
  return false;
}


void clearTargetSmoothing(int slot) {
  smoothedX[slot] = 0;
  smoothedY[slot] = 0;
  lastBlipX[slot] = 0;
  lastBlipY[slot] = 0;
}


void radarResetState() {
  for (int i = 0; i < 3; i++) {
    smoothedX[i]  = 0;
    smoothedY[i]  = 0;
    lastBlipX[i]  = 0;
    lastBlipY[i]  = 0;
    hitCount[i]   = 0;
    dropCount[i]  = 0;
  }
}


static uint8_t frameBuf[30];
static size_t  frameIdx   = 0;
static uint8_t frameState = 0;

static float radarDistance = 0, radarAngle = 0, radarSpeed = 0;
static bool  radarPresent = false;

void radarProcessSerial() {
  while (Serial1.available()) {
    uint8_t b = Serial1.read();
    switch (frameState) {
    case 0:
      if (b == 0xAA) frameState = 1;
      break;
    case 1:
      frameState = (b == 0xFF) ? 2 : 0;
      break;
    case 2:
      frameState = (b == 0x03) ? 3 : 0;
      break;
    case 3:
      if (b == 0x00) {
        frameIdx   = 0;
        frameState = 4;
      } else
        frameState = 0;
      break;
    case 4:
      frameBuf[frameIdx++] = b;
      if (frameIdx >= 26) {
        if (frameBuf[24] == 0x55 && frameBuf[25] == 0xCC) {
          if (!RadarState::ready) {
            RadarState::ready = true;
            if (AppState::currentScreen == Display::Screen::RADAR) {
              radarResetState();
              drawRadarBase();
            }
          }
          RawTarget t[3];
          t[0] = parseTarget(frameBuf + 0);
          t[1] = parseTarget(frameBuf + 8);
          t[2] = parseTarget(frameBuf + 16);
          applyPersistence(t);

          if (AppState::currentScreen == Display::Screen::RADAR) {
            radarPresent    = false;
            float bestDist  = cfgAccRange() + 1;
            bool newFarZone[4] = {false, false, false, false};

            for (int i = 0; i < 3; i++) {
              if (t[i].present) {
                float dist  = sqrtf((float)t[i].x * t[i].x +
                                    (float)t[i].y * t[i].y);
                float angle = atan2f((float)t[i].x, (float)t[i].y) *
                              180.0f / PI;

                if (dist <= cfgAccRange()) {
                  float outX, outY;
                  if (smoothTarget(i, isBlipActive(i), t[i].x, t[i].y,
                                   outX, outY)) {
                    int sx, sy;
                    radarToScreen(outX, outY, sx, sy);
                    if (sy >= CONE_TOP && sy < Display::SCREEN_H)
                      updateBlip(i, sx, sy, true);
                  }
                } else {
                  updateBlip(i, 0, 0, false);
                  clearTargetSmoothing(i);
                  newFarZone[getZone(angle)] = true;
                }

                if (dist < bestDist) {
                  bestDist      = dist;
                  radarDistance = dist;
                  radarAngle    = angle;
                  radarSpeed    = t[i].speed;
                  radarPresent  = true;
                }
              } else {
                updateBlip(i, 0, 0, false);
                clearTargetSmoothing(i);
              }
            }

            for (int z = 0; z < 4; z++)
              drawFarZone(z, newFarZone[z]);
            updateDashboard(radarDistance, radarAngle, radarSpeed, radarPresent);
          }
        }
        frameState = 0;
        frameIdx   = 0;
      }
      break;
    }
  }
}