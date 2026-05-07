#ifndef MAP3D_SCREEN_H
#define MAP3D_SCREEN_H

#include "base/display.h"

namespace Map3dScreen {
    constexpr int   VIEW_Y     = Display::HEADER_H;
    constexpr int   BTN_BAR    = 48;
    constexpr int   VIEW_H     = Display::SCREEN_H - VIEW_Y - BTN_BAR;
    constexpr int   VIEW_CX    = Display::SCREEN_W / 2;
    constexpr int   VIEW_CY    = VIEW_Y + VIEW_H / 2;
    constexpr float FOCAL      = 200.0f;
    constexpr float NEAR       = 80.0f;

    constexpr int   MAX_PTS    = 65536;
    constexpr int   RENDER_MAX = 8192;

    constexpr int   BTN_Y      = Display::SCREEN_H - BTN_BAR + 7;
    constexpr int   BTN_BH     = 32;
    constexpr int   CLR_X      = 8,   CLR_W = 106;
    constexpr int   PAL_X      = 126, PAL_W = 106;

    constexpr float ZONE_STEP  = 5.625f * 3.14159265f / 180.0f;
}

void drawMap3dBase();
void tickMap3d();
void handleMap3dTouch(int tx, int ty);

#endif
