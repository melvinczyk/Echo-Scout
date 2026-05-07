#ifndef IMU_SCREEN_H
#define IMU_SCREEN_H

namespace ImuScreen {
    constexpr int CUBE_SIZE = 55;
    constexpr int CUBE_CX = 120;
    constexpr int CUBE_CY = 145;
    constexpr int CUBE_YMAX = 244;
    constexpr float PROJ_DIST = 280.0f;
}

void drawImuBase();
void tickIMU();
void handleImuTouch(int tx, int ty);

#endif
