#ifndef DEVICE_STATE_H
#define DEVICE_STATE_H

#include <stdint.h>

namespace RadarState {
    inline bool ready = false;
    inline bool found = false;
    inline bool present = false;
    inline int16_t x = 0;
    inline int16_t y = 0;
    inline int16_t speed = 0;
    inline uint16_t pixelDist = 0;
}

namespace ImuState {
    inline bool ready = false;
    inline bool found = false;
    inline float qI = 0.0f;
    inline float qJ = 0.0f;
    inline float qK = 0.0f;
    inline float qR = 1.0f;
}

#endif
