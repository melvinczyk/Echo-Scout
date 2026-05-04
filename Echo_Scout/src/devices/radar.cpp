#include <math.h>
#include "radar.h"
#include "settings.h"
#include "device_state.h"

static float smoothedX[3]  = {0, 0, 0};
static float smoothedY[3]  = {0, 0, 0};
static float lastBlipX[3]  = {0, 0, 0};
static float lastBlipY[3]  = {0, 0, 0};
static uint8_t hitCount[3] = {0, 0, 0};
static uint8_t dropCount[3]= {0, 0, 0};


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
