#ifndef TOUCH_H
#define TOUCH_H
#pragma once
#include "config.h"
#include <Arduino.h>
#include <Wire.h>

void touchInit();
bool touchRead(int &tx, int &ty);
bool inRect(int tx, int ty, int rx, int ry, int rw, int rh);

#endif