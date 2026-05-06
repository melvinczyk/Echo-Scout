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
    // Frame-corrected quaternion (calibration applied when calibrated=true)
    inline float qI = 0.0f, qJ = 0.0f, qK = 0.0f, qR = 1.0f;
    // Raw frame-corrected quaternion — no calibration applied; used to set calibration reference
    inline float rawQI = 0.0f, rawQJ = 0.0f, rawQK = 0.0f, rawQR = 1.0f;
    // Calibration reference quaternion (captured when device is known-horizontal)
    inline bool  calibrated = false;
    inline float calR = 1.0f, calI = 0.0f, calJ = 0.0f, calK = 0.0f;
}

#endif
