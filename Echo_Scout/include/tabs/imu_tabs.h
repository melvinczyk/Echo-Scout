#ifndef IMU_TABS_H
#define IMU_TABS_H

#include <math.h>
#include "base/display.h"
#include "devices/device_state.h"
#include "devices/imu.h"
#include "base/tab_manager.h"

inline constexpr int CONTENT_Y = Display::HEADER_H;
inline constexpr int CONTENT_H = TabManager::TAB_Y - CONTENT_Y;

inline void toEuler(float& roll, float& pitch, float& yaw) {
    float qi=ImuState::qI, qj=ImuState::qJ, qk=ImuState::qK, qr=ImuState::qR;
    roll = atan2f(2.0f*(qr*qi+qj*qk), 1.0f-2.0f*(qi*qi+qj*qj)) * 180.0f/PI;
    float sp = 2.0f*(qr*qj-qk*qi);
    pitch = (fabsf(sp)>=1.0f) ? copysignf(90.0f,sp) : asinf(sp)*180.0f/PI;
    yaw = atan2f(2.0f*(qr*qk+qi*qj), 1.0f-2.0f*(qj*qj+qk*qk)) * 180.0f/PI;
}

// ---- Tab functions ----
void drawCubeTab();
void tickCubeTab();

void drawPlumbTab();
void tickPlumbTab();

void drawScopeTab();
void tickScopeTab();

#endif
