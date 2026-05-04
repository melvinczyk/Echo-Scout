#ifndef TOUCH_H
#define TOUCH_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

void touchInit();
bool touchRead(int &tx, int &ty);

#endif