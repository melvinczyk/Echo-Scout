#include "tabs/tof_tabs.h"
#include "devices/imu.h"
#include "devices/touch.h"
using namespace TofTabs;
#include <math.h>

enum AngleState { ANGLE_IDLE, ANGLE_A_LOCKED, ANGLE_DONE };
static AngleState angleState = ANGLE_IDLE;
static float vecAx, vecAy, vecAz, vecBx, vecBy, vecBz;
static float lockedAngle = 0.0f;
static float prevLiveAngle = -9999.0f;
static float distA = 0.0f, distB = 0.0f;

static constexpr int ANGL_NUM_H = 76;
static constexpr int ANGL_BTN_H = 44;
static constexpr int ANGL_BTN_Y = TAB_Y - ANGL_BTN_H;
static constexpr int ANGL_VIZ_H = ANGL_BTN_Y - 2 - (CONTENT_Y + ANGL_NUM_H);

static constexpr int VIZ_CX = 120;
static constexpr int VIZ_CY = CONTENT_Y + ANGL_NUM_H + ANGL_VIZ_H / 2 + 20;
static constexpr float VIZ_SCALE = 80.0f;

static void getBoresightWorld(float& wx, float& wy, float& wz) {
    float r=ImuState::qR, i=ImuState::qI, j=ImuState::qJ, k=ImuState::qK;
    wx = 2.0f*r*j + 2.0f*i*k;
    wy = 2.0f*j*k - 2.0f*r*i;
    wz = 1.0f - 2.0f*i*i - 2.0f*j*j;
    float mag=sqrtf(wx*wx+wy*wy+wz*wz);
    if (mag>0.001f) { wx/=mag; wy/=mag; wz/=mag; }
}

static void projectViz(float wx, float wy, float wz, int& sx, int& sy) {
    sx = VIZ_CX + (int)((wx + wy * 0.45f) * VIZ_SCALE);
    sy = VIZ_CY - (int)((wz + wy * 0.25f) * VIZ_SCALE);
}

static void drawVizArrow(float vx, float vy, float vz, uint16_t col) {
    int ox, oy, ex, ey;
    projectViz(0, 0, 0, ox, oy);
    projectViz(vx, vy, vz, ex, ey);
    Display::tft.drawLine(ox, oy, ex, ey, col);
    Display::tft.fillCircle(ex, ey, 3, col);
}

static void drawVizArc(float ax, float ay, float az,
                       float bx, float by, float bz, uint16_t col) {
    float dot = constrain(ax*bx + ay*by + az*bz, -1.0f, 1.0f);
    float omega = acosf(dot);
    if (omega < 0.01f) return;
    float sinOm = sinf(omega);
    const int STEPS = 18;
    const float ARC_R = 0.55f;
    int prevsx = 0, prevsy = 0;
    for (int i = 0; i <= STEPS; i++) {
        float t = (float)i / STEPS;
        float s0 = sinf((1.0f - t) * omega) / sinOm;
        float s1 = sinf(t * omega) / sinOm;
        float nx = (ax * s0 + bx * s1) * ARC_R;
        float ny = (ay * s0 + by * s1) * ARC_R;
        float nz = (az * s0 + bz * s1) * ARC_R;
        int sx, sy; projectViz(nx, ny, nz, sx, sy);
        if (i > 0) Display::tft.drawLine(prevsx, prevsy, sx, sy, col);
        prevsx = sx; prevsy = sy;
    }
}

static void drawAngleViz(bool hasA, bool hasB) {
    Display::tft.fillRect(0, CONTENT_Y + ANGL_NUM_H, Display::SCREEN_W, ANGL_VIZ_H, Display::Colors::BG);
    const float AXIS_LEN = 0.42f;
    int ox, oy;
    projectViz(0, 0, 0, ox, oy);
    int ex, ey;
    projectViz(AXIS_LEN, 0, 0, ex, ey);
    Display::tft.drawLine(ox, oy, ex, ey, Display::Colors::GREEN_FAINT);
    Display::tft.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
    Display::tft.drawString("X", ex + 2, ey - 4, 1);
    projectViz(0, AXIS_LEN, 0, ex, ey);
    Display::tft.drawLine(ox, oy, ex, ey, Display::Colors::GREEN_FAINT);
    Display::tft.drawString("Y", ex + 2, ey - 4, 1);
    projectViz(0, 0, AXIS_LEN, ex, ey);
    Display::tft.drawLine(ox, oy, ex, ey, Display::Colors::GREEN_FAINT);
    Display::tft.drawString("Z", ex + 2, ey - 4, 1);

    float wx, wy, wz;
    getBoresightWorld(wx, wy, wz);

    if (!hasA) {
        drawVizArrow(wx, wy, wz, Display::Colors::GREEN_DIM);
    } else if (!hasB) {
        drawVizArrow(vecAx, vecAy, vecAz, Display::Colors::GREEN);
        drawVizArrow(wx, wy, wz, Display::Colors::AMBER);
        drawVizArc(vecAx, vecAy, vecAz, wx, wy, wz, Display::Colors::GREEN_DIM);
    } else {
        drawVizArrow(vecAx, vecAy, vecAz, Display::Colors::GREEN);
        drawVizArrow(vecBx, vecBy, vecBz, Display::Colors::AMBER);
        drawVizArc(vecAx, vecAy, vecAz, vecBx, vecBy, vecBz, Display::Colors::RED);
    }
}

static void drawAngleValue(float deg, bool locked) {
    Display::spr.deleteSprite();
    Display::spr.createSprite(Display::SCREEN_W, ANGL_NUM_H);
    Display::spr.fillSprite(Display::Colors::BG);

    if (angleState == ANGLE_IDLE) {
        Display::spr.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
        Display::spr.drawCentreString("POINT AT TARGET A", 120, 28, 2);
    } else {
        uint16_t col = (angleState == ANGLE_DONE) ? Display::Colors::GREEN : Display::Colors::AMBER;
        char buf[24]; sprintf(buf, "%.1f deg", deg);
        Display::spr.setTextColor(col, Display::Colors::BG);
        Display::spr.drawCentreString(buf, 120, 6, 4);
        if (angleState == ANGLE_DONE && distA > 10.0f && distB > 10.0f) {
            float rad = deg * 3.14159f / 180.0f;
            float d = sqrtf(distA*distA + distB*distB - 2.0f*distA*distB*cosf(rad));
            char dbuf[24];
            if (d < 1000.0f) sprintf(dbuf, "DIST  %.0f mm", d);
            else              sprintf(dbuf, "DIST  %.2f m",  d / 1000.0f);
            Display::spr.setTextColor(Display::Colors::AMBER, Display::Colors::BG);
            Display::spr.drawCentreString(dbuf, 120, 46, 2);
        } else if (angleState == ANGLE_A_LOCKED && distA > 10.0f) {
            char dbuf[24];
            if (distA < 1000.0f) sprintf(dbuf, "A: %.0f mm", distA);
            else                  sprintf(dbuf, "A: %.2f m",  distA / 1000.0f);
            Display::spr.setTextColor(Display::Colors::AMBER, Display::Colors::BG);
            Display::spr.drawCentreString(dbuf, 120, 46, 2);
        }
    }

    Display::spr.pushSprite(0, CONTENT_Y);
    Display::spr.deleteSprite();
}

static void drawAngleBtn() {
    Display::tft.fillRect(20, ANGL_BTN_Y - 2, 200, ANGL_BTN_H + 4, Display::Colors::BG);
    const char* lbl;
    uint16_t col;
    if      (angleState == ANGLE_IDLE)     { lbl = "[ LOCK A ]";  col = Display::Colors::GREEN; }
    else if (angleState == ANGLE_A_LOCKED) { lbl = "[ LOCK B ]";  col = Display::Colors::AMBER; }
    else                                   { lbl = "[ RESET ]";   col = Display::Colors::GREEN_DIM; }
    Display::tft.drawRect(20,     ANGL_BTN_Y,     200,     ANGL_BTN_H,     col);
    Display::tft.drawRect(22,     ANGL_BTN_Y + 2, 196,     ANGL_BTN_H - 4, col);
    Display::tft.setTextColor(col, Display::Colors::BG);
    Display::tft.drawCentreString(lbl, 120, ANGL_BTN_Y + 14, 2);
}

namespace AngleTab {

void drawTab() {
    Display::tft.fillRect(0, CONTENT_Y, Display::SCREEN_W, TAB_Y - CONTENT_Y, Display::Colors::BG);
    angleState = ANGLE_IDLE;
    prevLiveAngle = -9999.0f;
    distA = distB = 0.0f;
    drawAngleViz(false, false);
    drawAngleValue(0.0f, false);
    drawAngleBtn();
}

void tick() {
    if (angleState == ANGLE_DONE) return;
    if (!imuUpdate()) return;

    float wx, wy, wz;
    getBoresightWorld(wx, wy, wz);

    float disp = 0.0f;
    if (angleState == ANGLE_A_LOCKED) {
        float dot = constrain(vecAx*wx + vecAy*wy + vecAz*wz, -1.0f, 1.0f);
        disp = acosf(dot) * 180.0f / 3.14159f;
    }

    if (fabsf(disp - prevLiveAngle) < 0.15f) return;
    prevLiveAngle = disp;
    drawAngleViz(angleState == ANGLE_A_LOCKED, false);
    drawAngleValue(disp, false);
}

void onTouch(int tx, int ty) {
    if (!inRect(tx, ty, 20, ANGL_BTN_Y, 200, ANGL_BTN_H)) return;

    if (angleState == ANGLE_IDLE) {
        getBoresightWorld(vecAx, vecAy, vecAz);
        distA = centerDist();
        distB = 0.0f;
        angleState = ANGLE_A_LOCKED;
        prevLiveAngle = -9999.0f;
        drawAngleViz(true, false);
        drawAngleValue(0.0f, false);
        drawAngleBtn();
    } else if (angleState == ANGLE_A_LOCKED) {
        getBoresightWorld(vecBx, vecBy, vecBz);
        distB = centerDist();
        float dot = constrain(vecAx*vecBx + vecAy*vecBy + vecAz*vecBz, -1.0f, 1.0f);
        lockedAngle = acosf(dot) * 180.0f / 3.14159f;
        angleState = ANGLE_DONE;
        drawAngleViz(true, true);
        drawAngleValue(lockedAngle, true);
        drawAngleBtn();
    } else {
        drawTab();
    }
}

} // namespace AngleTab
