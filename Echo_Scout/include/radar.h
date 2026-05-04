#ifndef RADAR_H
#define RADAR_H
#include "config.h"
#include "settings.h"
#include <Arduino.h>

struct RawTarget {
  int16_t x, y, speed;
  uint16_t pixelDist;
  bool present;
};


extern float smoothedX[3];
extern float smoothedY[3];
extern float lastBlipX[3];
extern float lastBlipY[3];
extern uint8_t hitCount[3];
extern uint8_t dropCount[3];

RawTarget parseTarget(const uint8_t *b);
void applyPersistence(RawTarget targets[3]);

void radarResetState();

#endif