#include "tabs/tof_tabs.h"
#include "devices/touch.h"

// Zones 27,28,35,36 are the inner 2x2 of the 8x8 grid (rows 3-4, cols 3-4).
static constexpr int DIST_BTN_X = 20;
static constexpr int DIST_BTN_Y = TAB_Y - 44;
static constexpr int DIST_BTN_W = 200;
static constexpr int DIST_BTN_H = 44;
static constexpr int DIST_NUM_H = 36;                          // number strip height
static constexpr int DIST_VIZ_Y = CONTENT_Y + DIST_NUM_H;     // 64
static constexpr int DIST_VIZ_BOT = DIST_BTN_Y - 4;           // just above button
static constexpr int DIST_VIZ_H = DIST_VIZ_BOT - DIST_VIZ_Y; // ~168px
static constexpr int DIST_VIZ_CX = 120;
static constexpr float DIST_VIZ_MAX = 4000.0f;                // 4 m max display range

enum DistState { DIST_LIVE, DIST_LOCKED };
static DistState distState = DIST_LIVE;
static float distLocked = 0.0f;
static float distPrevLive = -9999.0f;

// Number strip sprite at top
static void drawDistValue(float d, bool locked) {
    uint16_t col = locked ? Display::Colors::AMBER : Display::Colors::GREEN;
    Display::spr.deleteSprite();
    Display::spr.createSprite(Display::SCREEN_W, DIST_NUM_H);
    Display::spr.fillSprite(Display::Colors::BG);
    if (d <= 0.0f) {
        Display::spr.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
        Display::spr.drawCentreString("NO READING", 120, 10, 2);
    } else {
        char buf[24];
        if (d < 1000.0f) sprintf(buf, "%.0f mm", d);
        else              sprintf(buf, "%.3f m",  d / 1000.0f);
        Display::spr.setTextColor(col, Display::Colors::BG);
        Display::spr.drawCentreString(buf, 120, 6, 4);
    }
    Display::spr.pushSprite(0, CONTENT_Y);
    Display::spr.deleteSprite();
}

// 3D perspective rangefinder visualization
static void drawDistViz(float d) {
    Display::tft.fillRect(0, DIST_VIZ_Y, Display::SCREEN_W, DIST_VIZ_H, Display::Colors::BG);

    int devY = DIST_VIZ_BOT; // device at bottom
    int topY = DIST_VIZ_Y;
    int spread = 52;          // half-width of frustum at max range

    // Perspective guide lines
    Display::tft.drawLine(DIST_VIZ_CX, devY, DIST_VIZ_CX - spread, topY, Display::Colors::GREEN_FAINT);
    Display::tft.drawLine(DIST_VIZ_CX, devY, DIST_VIZ_CX + spread, topY, Display::Colors::GREEN_FAINT);
    // Centre beam
    Display::tft.drawFastVLine(DIST_VIZ_CX, topY, DIST_VIZ_H, Display::Colors::GREEN_FAINT);

    // Scale rings every 500 mm
    for (float r = 500.0f; r <= DIST_VIZ_MAX + 1.0f; r += 500.0f) {
        float t = r / DIST_VIZ_MAX;
        int sy = devY - (int)(t * DIST_VIZ_H);
        int sw = (int)(spread * t);
        Display::tft.drawFastHLine(DIST_VIZ_CX - sw, sy, sw * 2 + 1, Display::Colors::GREEN_FAINT);
        char lbuf[8];
        if (r < 1000.0f) sprintf(lbuf, "%.0fmm", r);
        else              sprintf(lbuf, "%.0fm",  r / 1000.0f);
        Display::tft.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
        Display::tft.drawString(lbuf, DIST_VIZ_CX + sw + 3, sy - 4, 1);
    }

    // Device block at origin
    Display::tft.fillRect(DIST_VIZ_CX - 7, devY - 5, 14, 6, Display::Colors::GREEN_DIM);

    // Target marker at measured distance
    if (d > 10.0f) {
        float dc = (d > DIST_VIZ_MAX) ? DIST_VIZ_MAX : d;
        float t = dc / DIST_VIZ_MAX;
        int ty = devY - (int)(t * DIST_VIZ_H);
        int tw = (int)(spread * t);
        // Filled horizontal bar at target distance
        Display::tft.fillRect(DIST_VIZ_CX - tw, ty - 1, tw * 2 + 1, 3,
                               d > DIST_VIZ_MAX ? Display::Colors::RED : Display::Colors::GREEN);
        Display::tft.fillCircle(DIST_VIZ_CX, ty, 5, Display::Colors::GREEN);
        // Fill beam between device and target
        Display::tft.drawLine(DIST_VIZ_CX, devY - 5, DIST_VIZ_CX, ty, Display::Colors::GREEN);
    }
}

static void drawDistBtn(bool locked) {
    Display::tft.fillRect(DIST_BTN_X - 2, DIST_BTN_Y - 2,
                          DIST_BTN_W + 4, DIST_BTN_H + 4, Display::Colors::BG);
    uint16_t c = locked ? Display::Colors::AMBER : Display::Colors::GREEN;
    Display::tft.drawRect(DIST_BTN_X,     DIST_BTN_Y,     DIST_BTN_W,     DIST_BTN_H,     c);
    Display::tft.drawRect(DIST_BTN_X + 2, DIST_BTN_Y + 2, DIST_BTN_W - 4, DIST_BTN_H - 4, c);
    Display::tft.setTextColor(c, Display::Colors::BG);
    Display::tft.drawCentreString(locked ? "[ UNLOCK ]" : "[ MEASURE ]",
                                  120, DIST_BTN_Y + 14, 2);
}

void drawDistTab() {
    Display::tft.fillRect(0, CONTENT_Y, Display::SCREEN_W, TAB_Y - CONTENT_Y, Display::Colors::BG);
    distState = DIST_LIVE;
    distPrevLive = -9999.0f;
    tofUpdate();
    float d = centerDist();
    drawDistValue(d, false);
    drawDistViz(d);
    drawDistBtn(false);
}

void tickDistTab() {
    if (distState == DIST_LOCKED) return;
    if (!tofUpdate()) return;
    float d = centerDist();
    if (fabsf(d - distPrevLive) < 5.0f) return;
    distPrevLive = d;
    drawDistValue(d, false);
    drawDistViz(d);
}

void handleDistTouch(int tx, int ty) {
    if (!inRect(tx, ty, DIST_BTN_X, DIST_BTN_Y, DIST_BTN_W, DIST_BTN_H)) return;
    if (distState == DIST_LIVE) {
        tofUpdate();
        distLocked = centerDist();
        distState = DIST_LOCKED;
        drawDistValue(distLocked, true);
        drawDistViz(distLocked);
        drawDistBtn(true);
    } else {
        distState = DIST_LIVE;
        distPrevLive = -9999.0f;
        drawDistBtn(false);
    }
}
