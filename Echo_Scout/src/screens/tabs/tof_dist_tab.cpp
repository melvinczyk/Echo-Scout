#include "tabs/tof_tabs.h"
#include "devices/touch.h"
#include "base/measurements.h"
using namespace TofTabs;

static constexpr int DIST_BTN_X = 20;
static constexpr int DIST_BTN_Y = TAB_Y - 44;
static constexpr int DIST_BTN_W = 200;
static constexpr int DIST_BTN_H = 44;
static constexpr int DIST_NUM_H = 36;
static constexpr int DIST_VIZ_Y = CONTENT_Y + DIST_NUM_H;
static constexpr int DIST_VIZ_BOT = DIST_BTN_Y - 4;
static constexpr int DIST_VIZ_H = DIST_VIZ_BOT - DIST_VIZ_Y;
static constexpr int DIST_VIZ_CX = 120;
static constexpr float DIST_VIZ_MAX = 4000.0f;

enum DistState { DIST_LIVE, DIST_LOCKED };
static DistState distState = DIST_LIVE;
static float distLocked = 0.0f;
static float distPrevLive = -9999.0f;

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
        Measurements::fmtDist(d, buf);
        Display::spr.setTextColor(col, Display::Colors::BG);
        Display::spr.drawCentreString(buf, 120, 6, 4);
    }
    Display::spr.pushSprite(0, CONTENT_Y);
    Display::spr.deleteSprite();
}

static void drawDistViz(float d) {
    Display::tft.fillRect(0, DIST_VIZ_Y, Display::SCREEN_W, DIST_VIZ_H, Display::Colors::BG);

    int devY = DIST_VIZ_BOT;
    int topY = DIST_VIZ_Y;
    int spread = 52;

    Display::tft.drawLine(DIST_VIZ_CX, devY, DIST_VIZ_CX - spread, topY, Display::Colors::GREEN_FAINT);
    Display::tft.drawLine(DIST_VIZ_CX, devY, DIST_VIZ_CX + spread, topY, Display::Colors::GREEN_FAINT);
    Display::tft.drawFastVLine(DIST_VIZ_CX, topY, DIST_VIZ_H, Display::Colors::GREEN_FAINT);

    for (float r = 500.0f; r <= DIST_VIZ_MAX + 1.0f; r += 500.0f) {
        float t = r / DIST_VIZ_MAX;
        int sy = devY - (int)(t * DIST_VIZ_H);
        int sw = (int)(spread * t);
        Display::tft.drawFastHLine(DIST_VIZ_CX - sw, sy, sw * 2 + 1, Display::Colors::GREEN_FAINT);
        char lbuf[8];
        Measurements::fmtDistShort(r, lbuf);
        Display::tft.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
        Display::tft.drawString(lbuf, DIST_VIZ_CX + sw + 3, sy - 4, 1);
    }

    Display::tft.fillRect(DIST_VIZ_CX - 7, devY - 5, 14, 6, Display::Colors::GREEN_DIM);

    if (d > 10.0f) {
        float dc = (d > DIST_VIZ_MAX) ? DIST_VIZ_MAX : d;
        float t = dc / DIST_VIZ_MAX;
        int ty = devY - (int)(t * DIST_VIZ_H);
        int tw = (int)(spread * t);
        Display::tft.fillRect(DIST_VIZ_CX - tw, ty - 1, tw * 2 + 1, 3,
                               d > DIST_VIZ_MAX ? Display::Colors::RED : Display::Colors::GREEN);
        Display::tft.fillCircle(DIST_VIZ_CX, ty, 5, Display::Colors::GREEN);
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

namespace DistTab {

void drawTab() {
    Display::tft.fillRect(0, CONTENT_Y, Display::SCREEN_W, TAB_Y - CONTENT_Y, Display::Colors::BG);
    distState = DIST_LIVE;
    distPrevLive = -9999.0f;
    tofUpdate();
    float d = centerDist();
    drawDistValue(d, false);
    drawDistViz(d);
    drawDistBtn(false);
}

void tick() {
    if (distState == DIST_LOCKED) return;
    if (!tofUpdate()) return;
    float d = centerDist();
    if (fabsf(d - distPrevLive) < 5.0f) return;
    distPrevLive = d;
    drawDistValue(d, false);
    drawDistViz(d);
}

void onTouch(int tx, int ty) {
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

} // namespace DistTab
