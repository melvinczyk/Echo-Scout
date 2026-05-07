#ifndef DEVICE_STATE_H
#define DEVICE_STATE_H

#include <stdint.h>

namespace RadarState {
    inline bool ready = false;
    inline bool found = false;

    inline bool newFrame = false;
    inline bool present = false;
    inline float distance = 0.0f;
    inline float azimuth = 0.0f;
    inline float speed = 0.0f;

    inline bool slotActive[3] = {};
    inline float slotX[3] = {};
    inline float slotY[3] = {};

    inline bool farZone[4] = {};
}

namespace TofState {
    inline bool ready = false;
    inline bool found = false;
    inline float distances[64] = {};
    inline uint8_t zoneCount = 0;
}

namespace ImuState {
    inline bool ready = false;
    inline bool found = false;
    inline float qI = 0.0f, qJ = 0.0f, qK = 0.0f, qR = 1.0f;
    inline float rawQI = 0.0f, rawQJ = 0.0f, rawQK = 0.0f, rawQR = 1.0f;
    inline bool calibrated = false;
    inline float calR = 1.0f, calI = 0.0f, calJ = 0.0f, calK = 0.0f;
    inline uint32_t lastMotionMs = 0;
}

#endif
