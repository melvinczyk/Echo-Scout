#ifndef IMU_SCREEN_H
#define IMU_SCREEN_H

namespace ImuScreen {
    constexpr int CUBE_SIZE = 55;
    constexpr int CUBE_CX = 120;
    constexpr int CUBE_CY = 148;
    constexpr int CUBE_YMAX = 235;
    constexpr float PROJ_DIST = 280.0f;
}

void tickIMU();
void drawImuScreen();
void drawImuBase();

#endif