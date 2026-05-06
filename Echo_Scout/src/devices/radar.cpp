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

int radarGetZone(float angleDeg) {
    if (angleDeg < -20.0f) return 0;
    if (angleDeg <   0.0f) return 1;
    if (angleDeg <  20.0f) return 2;
    return 3;
}

RawTarget parseTarget(const uint8_t* b) {
  RawTarget t;
  uint16_t rx = b[0] | (b[1] << 8);
  uint16_t ry = b[2] | (b[3] << 8);
  uint16_t rs = b[4] | (b[5] << 8);
  t.pixelDist = b[6] | (b[7] << 8);
  t.present = !(rx == 0 && ry == 0 && rs == 0);
  t.x = (rx & 0x7FFF);
  if (!(rx & 0x8000)) t.x = -t.x;
  t.y = (ry & 0x7FFF);
  if (!(ry & 0x8000)) t.y = -t.y;
  t.speed = (rs & 0x7FFF);
  if (!(rs & 0x8000)) t.speed = -t.speed;
  t.x = -t.x;
  if (t.present && t.y <= 0)               t.present = false;
  if (t.present && t.y < cfgMinRange())    t.present = false;
  if (t.present && t.y > 8000.0f)          t.present = false;
  if (t.present) {
    float a = fabsf(atan2f((float)t.x, (float)t.y) * 180.0f / PI);
    if (a > cfgMaxAngle()) t.present = false;
  }
  if (t.present && cfgSpeedGate() > 0 && abs(t.speed) > cfgSpeedGate())
    t.present = false;
  return t;
}

void applyPersistence(RawTarget targets[3]) {
  for (int i = 0; i < 3; i++) {
    if (targets[i].present) {
      dropCount[i] = 0;
      if (hitCount[i] < cfgHits()) hitCount[i]++;
      if (hitCount[i] < cfgHits()) targets[i].present = false;
    } else {
      hitCount[i] = 0;
      if (dropCount[i] < cfgDrops()) dropCount[i]++;
      if (dropCount[i] < cfgDrops()) targets[i].present = true;
    }
  }
}

bool smoothTarget(int slot, bool wasTracked, float rawX, float rawY,
                  float& outX, float& outY) {
  if (cfgSmoothing() && wasTracked) {
    smoothedX[slot] = SMOOTH_ALPHA * rawX + (1.0f - SMOOTH_ALPHA) * smoothedX[slot];
    smoothedY[slot] = SMOOTH_ALPHA * rawY + (1.0f - SMOOTH_ALPHA) * smoothedY[slot];
  } else {
    smoothedX[slot] = rawX;
    smoothedY[slot] = rawY;
  }
  float dx = smoothedX[slot] - lastBlipX[slot];
  float dy = smoothedY[slot] - lastBlipY[slot];
  if (sqrtf(dx * dx + dy * dy) >= 180.0f && wasTracked) return false;
  if (sqrtf(dx * dx + dy * dy) > cfgMoveThresh() || !wasTracked) {
    lastBlipX[slot] = smoothedX[slot];
    lastBlipY[slot] = smoothedY[slot];
    outX = smoothedX[slot];
    outY = smoothedY[slot];
    return true;
  }
  return false;
}

void clearTargetSmoothing(int slot) {
  smoothedX[slot] = smoothedY[slot] = 0;
  lastBlipX[slot] = lastBlipY[slot] = 0;
}

void radarResetState() {
  for (int i = 0; i < 3; i++) {
    smoothedX[i] = smoothedY[i] = 0;
    lastBlipX[i] = lastBlipY[i] = 0;
    hitCount[i]  = dropCount[i] = 0;
    RadarState::slotActive[i] = false;
    RadarState::slotX[i]      = 0;
    RadarState::slotY[i]      = 0;
  }
  for (int i = 0; i < 4; i++) RadarState::farZone[i] = false;
  RadarState::present  = false;
  RadarState::newFrame = false;
}

static uint8_t frameBuf[30];
static size_t  frameIdx   = 0;
static uint8_t frameState = 0;

void radarProcessSerial() {
  while (Serial1.available()) {
    uint8_t b = Serial1.read();
    switch (frameState) {
    case 0: if (b == 0xAA) frameState = 1; break;
    case 1: frameState = (b == 0xFF) ? 2 : 0; break;
    case 2: frameState = (b == 0x03) ? 3 : 0; break;
    case 3:
      if (b == 0x00) { frameIdx = 0; frameState = 4; }
      else             frameState = 0;
      break;
    case 4:
      frameBuf[frameIdx++] = b;
      if (frameIdx >= 26) {
        if (frameBuf[24] == 0x55 && frameBuf[25] == 0xCC) {
          RadarState::ready = true;

          RawTarget t[3];
          t[0] = parseTarget(frameBuf + 0);
          t[1] = parseTarget(frameBuf + 8);
          t[2] = parseTarget(frameBuf + 16);
          applyPersistence(t);

          RadarState::present  = false;
          float bestDist       = cfgAccRange() + 1;
          for (int i = 0; i < 4; i++) RadarState::farZone[i] = false;

          for (int i = 0; i < 3; i++) {
            if (t[i].present) {
              float dist  = sqrtf((float)t[i].x * t[i].x + (float)t[i].y * t[i].y);
              float angle = atan2f((float)t[i].x, (float)t[i].y) * 180.0f / PI;

              if (dist <= cfgAccRange()) {
                float outX, outY;
                if (smoothTarget(i, RadarState::slotActive[i], t[i].x, t[i].y, outX, outY)) {
                  RadarState::slotX[i]      = outX;
                  RadarState::slotY[i]      = outY;
                  RadarState::slotActive[i] = true;
                }
              } else {
                RadarState::slotActive[i] = false;
                clearTargetSmoothing(i);
                RadarState::farZone[radarGetZone(angle)] = true;
              }

              if (dist < bestDist) {
                bestDist             = dist;
                RadarState::distance = dist;
                RadarState::azimuth  = angle;
                RadarState::speed    = t[i].speed;
                RadarState::present  = true;
              }
            } else {
              RadarState::slotActive[i] = false;
              clearTargetSmoothing(i);
            }
          }

          RadarState::newFrame = true;
        }
        frameState = 0;
        frameIdx   = 0;
      }
      break;
    }
  }
}
