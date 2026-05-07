#ifndef TOUCH_H
#define TOUCH_H

#include "base/config.h"

void touchInit();
bool touchRead(int &tx, int &ty);

inline bool inRect(int tx, int ty, int rx, int ry, int rw, int rh) {
    return tx >= rx && tx <= rx + rw && ty >= ry && ty <= ry + rh;
}

#endif
