#ifndef RADAR_H
#define RADAR_H

#include "config.h"

struct RawTarget {
  int16_t x, y, speed;
  uint16_t pixelDist;
  bool present;
};

RawTarget parseTarget(const uint8_t *b);
void applyPersistence(RawTarget targets[3]);
bool smoothTarget(int slot, bool wasTracked, float rawX, float rawY,
                  float& outX, float& outY);
void clearTargetSmoothing(int slot);

void radarResetState();

#endif
