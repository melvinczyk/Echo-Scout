#include <math.h>
#include "screens/map3d_screen.h"
#include "base/color_scheme.h"
#include "base/settings.h"
#include "devices/imu.h"
#include "devices/tof.h"
#include "base/display.h"
#include "devices/touch.h"
#include "devices/device_state.h"
#include <esp_heap_caps.h>

using namespace Map3dScreen;

struct MapPoint { float x, y, z; uint16_t col; };

static MapPoint* pts = nullptr;
static int nPts = 0;

static inline void qRot(float qr, float qi, float qj, float qk,
                         float vx, float vy, float vz,
                         float& rx, float& ry, float& rz) {
    float tx = 2*(qj*vz - qk*vy);
    float ty = 2*(qk*vx - qi*vz);
    float tz = 2*(qi*vy - qj*vx);
    rx = vx + qr*tx + (qj*tz - qk*ty);
    ry = vy + qr*ty + (qk*tx - qi*tz);
    rz = vz + qr*tz + (qi*ty - qj*tx);
}

static bool project(float wx, float wy, float wz, int& sx, int& sy) {
    float vx, vy, vz;
    qRot(ImuState::qR, -ImuState::qI, -ImuState::qJ, -ImuState::qK,
         wx, wy, wz, vx, vy, vz);
    float depth = -vy;
    if (depth < NEAR) return false;
    sx = VIEW_CX + (int)(FOCAL * vx / depth);
    sy = VIEW_CY + (int)(FOCAL * vz / depth);
    return sx >= 2 && sx < Display::SCREEN_W - 2 &&
           sy >= VIEW_Y + 2 && sy < VIEW_Y + VIEW_H - 2;
}


static bool zoneWorld(int zone, float dist,
                      float qr, float qi, float qj, float qk,
                      float& wx, float& wy, float& wz) {
    if (dist < 50.0f || dist > 4000.0f) return false;
    int row = zone / 8, col = zone % 8;
    float ah =  (col - 3.5f) * ZONE_STEP;
    float av = -(row - 3.5f) * ZONE_STEP;
    float lx =  sinf(ah);
    float ly = -cosf(ah) * cosf(av);
    float lz =  sinf(av);
    qRot(qr, qi, qj, qk, lx, ly, lz, wx, wy, wz);
    wx *= dist; wy *= dist; wz *= dist;
    return true;
}

static void renderView() {
    Display::spr.deleteSprite();
    Display::spr.createSprite(Display::SCREEN_W, VIEW_H);
    Display::spr.fillSprite(Display::Colors::BG);

    int count = min(nPts, MAX_PTS);
    int step = max(1, count / RENDER_MAX);
    for (int i = 0; i < count; i += step) {
        int sx, sy;
        if (project(pts[i].x, pts[i].y, pts[i].z, sx, sy))
            Display::spr.drawPixel(sx, sy - VIEW_Y, pts[i].col);
    }

    if (TofState::ready) {
        float qr = ImuState::qR, qi = ImuState::qI,
              qj = ImuState::qJ, qk = ImuState::qK;
        for (int z = 0; z < 64; z++) {
            float wx, wy, wz;
            if (!zoneWorld(z, TofState::distances[z], qr, qi, qj, qk, wx, wy, wz)) continue;
            int sx, sy;
            if (project(wx, wy, wz, sx, sy)) {
                int lsy = sy - VIEW_Y;
                uint16_t c = ColorScheme::fromDist(cfg.mapPaletteIdx, TofState::distances[z]);
                Display::spr.fillRect(sx - 1, lsy - 1, 3, 3, c);
                Display::spr.drawPixel(sx, lsy, Display::Colors::WHITE);
            }
        }
    }

    int cx = VIEW_CX, cy = VIEW_CY - VIEW_Y;
    Display::spr.drawFastHLine(cx - 12, cy,      9,  Display::Colors::GREEN_DIM);
    Display::spr.drawFastHLine(cx + 4,  cy,      9,  Display::Colors::GREEN_DIM);
    Display::spr.drawFastVLine(cx,      cy - 12, 9,  Display::Colors::GREEN_DIM);
    Display::spr.drawFastVLine(cx,      cy + 4,  9,  Display::Colors::GREEN_DIM);

    char buf[24];
    if (nPts >= MAX_PTS)
        sprintf(buf, "%dk pts (ring)", nPts/1000);
    else
        sprintf(buf, "%d pts", nPts);
    Display::spr.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
    Display::spr.drawRightString(buf, Display::SCREEN_W - 4, 2, 1);

    Display::spr.pushSprite(0, VIEW_Y);
    Display::spr.deleteSprite();
}

static void drawButtons() {
    int by = Display::SCREEN_H - BTN_BAR;
    Display::tft.fillRect(0, by, Display::SCREEN_W, BTN_BAR, Display::Colors::BG);
    Display::tft.drawFastHLine(0, by, Display::SCREEN_W, Display::Colors::SEP);
    Display::tft.drawRect(CLR_X, BTN_Y, CLR_W, BTN_BH, Display::Colors::RED);
    Display::tft.setTextColor(Display::Colors::RED, Display::Colors::BG);
    Display::tft.drawCentreString("CLEAR", CLR_X + CLR_W/2, BTN_Y + 9, 2);
}

static void captureScan() {
    if (!TofState::ready || !ImuState::ready || !pts) return;
    float qr = ImuState::qR, qi = ImuState::qI,
          qj = ImuState::qJ, qk = ImuState::qK;
    for (int z = 0; z < 64; z++) {
        float wx, wy, wz;
        if (!zoneWorld(z, TofState::distances[z], qr, qi, qj, qk, wx, wy, wz)) continue;
        pts[nPts % MAX_PTS] = { wx, wy, wz, ColorScheme::fromDist(cfg.mapPaletteIdx, TofState::distances[z]) };
        nPts++;
    }
}

void drawMap3dBase() {
    if (!pts)
        pts = (MapPoint*)heap_caps_malloc(MAX_PTS * sizeof(MapPoint), MALLOC_CAP_SPIRAM);
    nPts = 0;
    Display::tft.fillScreen(Display::Colors::BG);
    Display::drawHeader("3D MAP", false);
    drawButtons();
    renderView();
}

void tickMap3d() {
    if (!imuUpdate()) return;
    tofUpdate();
    captureScan();
    renderView();
}

void handleMap3dTouch(int tx, int ty) {
    if (inRect(tx, ty, CLR_X, BTN_Y, CLR_W, BTN_BH)) {
        nPts = 0;
        renderView();
    }
}
