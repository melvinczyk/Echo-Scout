#ifndef DEVICE_ICONS_H
#define DEVICE_ICONS_H

#include "display.h"

// Simple orientation hint icons. Each fits in ~36×36 centred on (cx, cy).

// Portrait rectangle + upward arrow — display faces sky (tilted 90° back)
inline void drawIconFrontHoriz(int cx, int cy) {
    constexpr int W = 14, H = 26;
    int dx = cx - W/2, dy = cy - H/2;
    Display::tft.fillRect(dx - 10, dy - 10, W + 20, H + 8, Display::Colors::BG);
    Display::tft.drawRect(dx, dy, W, H, Display::Colors::GREEN_DIM);
    // upward arrow above the rectangle
    int ax = cx, ay = dy - 4;
    Display::tft.drawFastVLine(ax, ay - 7, 7, Display::Colors::GREEN_DIM);
    Display::tft.drawLine(ax, ay - 7, ax - 3, ay - 4, Display::Colors::GREEN_DIM);
    Display::tft.drawLine(ax, ay - 7, ax + 3, ay - 4, Display::Colors::GREEN_DIM);
}

// Portrait rectangle + forward arrow (→) — display faces horizon (upright)
inline void drawIconBackVert(int cx, int cy) {
    constexpr int W = 14, H = 26;
    int dx = cx - W/2, dy = cy - H/2;
    Display::tft.fillRect(dx - 4, dy - 2, W + 14, H + 4, Display::Colors::BG);
    Display::tft.drawRect(dx, dy, W, H, Display::Colors::GREEN_DIM);
    // rightward arrow beside the rectangle
    int ax = dx + W + 3, ay = cy;
    Display::tft.drawFastHLine(ax, ay, 7, Display::Colors::GREEN_DIM);
    Display::tft.drawLine(ax + 7, ay, ax + 4, ay - 3, Display::Colors::GREEN_DIM);
    Display::tft.drawLine(ax + 7, ay, ax + 4, ay + 3, Display::Colors::GREEN_DIM);
}

#endif
