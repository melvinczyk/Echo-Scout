#pragma once
#include "config.h"
#include "display.h"
#include <Adafruit_BNO08x.h>
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════
//  IMU SCREEN
// ═══════════════════════════════════════════════════════════
bool imuInit();
void imuUpdate();
void drawImuScreen();
void drawImuBase();