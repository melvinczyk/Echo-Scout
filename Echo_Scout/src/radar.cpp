#include "radar.h"
#include "grid.h"
#include <math.h>

float smoothedX[3] = {0, 0, 0};
float smoothedY[3] = {0, 0, 0};
float lastBlipX[3] = {0, 0, 0};
float lastBlipY[3] = {0, 0, 0};
uint8_t hitCount[3] = {0, 0, 0};
uint8_t dropCount[3] = {0, 0, 0};

RawTarget parseTarget(const uint8_t *b) {
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

void radarResetState() {
  for (int i = 0; i < 3; i++) {
    smoothedX[i] = 0;
    smoothedY[i] = 0;
    lastBlipX[i] = 0;
    lastBlipY[i] = 0;
    hitCount[i] = 0;
    dropCount[i] = 0;
  }
}