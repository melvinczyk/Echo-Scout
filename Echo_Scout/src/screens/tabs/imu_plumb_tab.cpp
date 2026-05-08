#include "tabs/imu_tabs.h"
#include "devices/device_icons.h"
#include "base/measurements.h"

static constexpr int PLUMB_AX = 120, PLUMB_AY = 60, PLUMB_LEN = 150;
static float prevPbOx = -9999;

static void plumbBobBBox(float ox, int& x0, int& y0, int& w, int& h) {
    float chord = sqrtf((float)PLUMB_LEN*PLUMB_LEN - ox*ox);
    int bx = constrain(PLUMB_AX+(int)ox, 10, Display::SCREEN_W-10);
    int by = constrain(PLUMB_AY+(int)chord, PLUMB_AY+4, PLUMB_AY+PLUMB_LEN);
    int r = 10;
    x0 = min(PLUMB_AX, bx) - r;
    y0 = min(PLUMB_AY, by) - r;
    int x1 = max(PLUMB_AX, bx) + r;
    int y1 = max(PLUMB_AY, by) + r;
    w = x1 - x0;
    h = y1 - y0;
}

static void drawPlumbBob(float ox) {
    float chord = sqrtf((float)PLUMB_LEN*PLUMB_LEN - ox*ox);
    int bx = constrain(PLUMB_AX+(int)ox, 10, Display::SCREEN_W-10);
    int by = constrain(PLUMB_AY+(int)chord, PLUMB_AY+4, PLUMB_AY+PLUMB_LEN);
    Display::tft.drawLine(PLUMB_AX, PLUMB_AY, bx, by, Display::Colors::GREEN);
    Display::tft.fillCircle(bx, by, 9, Display::Colors::GREEN);
    Display::tft.drawCircle(bx, by, 9, Display::Colors::GREEN_DIM);
}

namespace PlumbTab {

    void drawTab() {
        Display::tft.fillRect(0, ImuTabs::CONTENT_Y, Display::SCREEN_W, ImuTabs::CONTENT_H, Display::Colors::BG);
        prevPbOx = -9999;

        int gx = PLUMB_AX, gy = PLUMB_AY + PLUMB_LEN;
        Display::tft.drawFastHLine(gx-18, gy, 36, Display::Colors::GREEN_FAINT);
        Display::tft.drawFastVLine(gx, gy-18, 36, Display::Colors::GREEN_FAINT);
        Display::tft.fillCircle(PLUMB_AX, PLUMB_AY, 4, Display::Colors::GREEN_DIM);
        drawIconFrontHoriz(200, ImuTabs::CONTENT_Y + 34);
        Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
        Display::tft.drawCentreString("DEVIATION", 120, 262, 1);
    }

    void tick() {
        if (!imuUpdate()) return;
        // Gravity component along the device's horizontal (X) axis.
        // This is the standard roll measurement: how much the device tilts
        // left/right. Bob swings left when device leans left, right when right.
        // Using the direct quaternion formula avoids Euler angle gimbal coupling.
        float sinRoll = 2.0f * (ImuState::qR * ImuState::qI + ImuState::qJ * ImuState::qK);
        sinRoll = constrain(sinRoll, -1.0f, 1.0f);
        float ox = sinRoll * PLUMB_LEN;
        if (fabsf(ox - prevPbOx) < 1.0f) return;

        if (prevPbOx > -9998) {
            int ex, ey, ew, eh;
            plumbBobBBox(prevPbOx, ex, ey, ew, eh);
            Display::tft.fillRect(ex, ey, ew, eh, Display::Colors::BG);
            int gx = PLUMB_AX, gy = PLUMB_AY + PLUMB_LEN;
            if (ey <= gy && gy <= ey+eh)
                Display::tft.drawFastHLine(max(gx-18, ex), gy,
                    min(gx+18, ex+ew) - max(gx-18, ex), Display::Colors::GREEN_FAINT);
            if (ex <= gx && gx <= ex+ew)
                Display::tft.drawFastVLine(gx, max(gy-18, ey),
                    min(gy+18, ey+eh) - max(gy-18, ey), Display::Colors::GREEN_FAINT);
        }

        prevPbOx = ox;
        drawPlumbBob(ox);
        drawIconFrontHoriz(200, ImuTabs::CONTENT_Y + 34);
        Display::tft.fillCircle(PLUMB_AX, PLUMB_AY, 4, Display::Colors::GREEN_DIM);
        float dev = fabsf(atan2f(ox, sqrtf((float)(PLUMB_LEN*PLUMB_LEN)-ox*ox))) * 180.0f/PI;
        char buf[16]; Measurements::fmtAngle(dev, buf, 16);
        Display::tft.fillRect(40, 268, 160, 14, Display::Colors::BG);
        Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
        Display::tft.drawCentreString(buf, 120, 268, 2);
    }

}