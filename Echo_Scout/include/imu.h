#ifndef IMU_H
#define IMU_H

#include <Adafruit_BNO08x.h>

bool imuInit();
void imuUpdate();

#endif