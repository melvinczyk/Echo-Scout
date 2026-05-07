#include "tabs/imu_tabs.h"

// Yaw unwrap state - prevents +/-180 wrap from spiking the K lane
static constexpr int SCOPE_SAMPLES = 240;
static constexpr int SCOPE_LANE_H = 78;
static constexpr int SCOPE_Y0 = CONTENT_Y + 18; // leave room for legend
static constexpr uint32_t SCOPE_MS = 50;

static float scopeBuf[3][SCOPE_SAMPLES] = {};
static int scopeHead = 0;
static bool scopeFull = false;
static uint32_t scopeLastMs = 0;
static float scopeYawPrev = 0.0f;
static float scopeYawAccum = 0.0f;

static int scopeToY(float val, int laneY) {
    return constrain(laneY + SCOPE_LANE_H/2 - (int)(val*(SCOPE_LANE_H/2-4)/180.0f),
                     laneY+1, laneY+SCOPE_LANE_H-2);
}

// Small axis-indicator in top-right: 3 coloured squares + labels
static void drawScopeIndicator() {
    const int IX = 178, IY = CONTENT_Y + 4;
    // I = Roll (GREEN), J = Pitch (AMBER), K = Yaw (GREEN_DIM)
    static const uint16_t cols[3] = {
        Display::Colors::GREEN, Display::Colors::AMBER, Display::Colors::GREEN_DIM
    };
    static const char* lbl[3] = {"I","J","K"};
    for (int n = 0; n < 3; n++) {
        int x = IX + n * 20;
        Display::tft.fillRect(x, IY, 10, 10, cols[n]);
        Display::tft.setTextColor(cols[n], Display::Colors::BG);
        Display::tft.drawString(lbl[n], x + 12, IY + 1, 1);
    }
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
}

static void drawScopeGridLines() {
    Display::tft.drawFastHLine(0, SCOPE_Y0+SCOPE_LANE_H,   Display::SCREEN_W, Display::Colors::GREEN_FAINT);
    Display::tft.drawFastHLine(0, SCOPE_Y0+SCOPE_LANE_H*2, Display::SCREEN_W, Display::Colors::GREEN_FAINT);
    Display::tft.drawFastHLine(0, SCOPE_Y0+SCOPE_LANE_H/2,              Display::SCREEN_W, 0x0100);
    Display::tft.drawFastHLine(0, SCOPE_Y0+SCOPE_LANE_H+SCOPE_LANE_H/2,   Display::SCREEN_W, 0x0100);
    Display::tft.drawFastHLine(0, SCOPE_Y0+SCOPE_LANE_H*2+SCOPE_LANE_H/2, Display::SCREEN_W, 0x0100);
    // Channel labels
    Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
    Display::tft.drawString("R", 2, SCOPE_Y0+2, 1);
    Display::tft.setTextColor(Display::Colors::AMBER, Display::Colors::BG);
    Display::tft.drawString("P", 2, SCOPE_Y0+SCOPE_LANE_H+2, 1);
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawString("Y", 2, SCOPE_Y0+SCOPE_LANE_H*2+2, 1);
}

static void drawScopeWave(int lane, uint16_t col) {
    int laneY = SCOPE_Y0 + lane*SCOPE_LANE_H;
    int count = scopeFull ? SCOPE_SAMPLES : scopeHead;
    if (count < 2) return;
    for (int i=1; i<count; i++) {
        int xi0=(scopeHead-count+i-1+SCOPE_SAMPLES)%SCOPE_SAMPLES;
        int xi1=(scopeHead-count+i  +SCOPE_SAMPLES)%SCOPE_SAMPLES;
        Display::tft.drawLine(i-1, scopeToY(scopeBuf[lane][xi0],laneY),
                              i,   scopeToY(scopeBuf[lane][xi1],laneY), col);
    }
}

void drawScopeTab() {
    Display::tft.fillRect(0, CONTENT_Y, Display::SCREEN_W, CONTENT_H, Display::Colors::BG);
    // Reset buffers and unwrap state
    memset(scopeBuf, 0, sizeof(scopeBuf));
    scopeHead = 0; scopeFull = false;
    float r, p, y; toEuler(r, p, y);
    scopeYawPrev = y;
    scopeYawAccum = 0.0f;
    drawScopeIndicator();
    drawScopeGridLines();
}

void tickScopeTab() {
    imuUpdate();
    if (millis() - scopeLastMs < SCOPE_MS) return;
    scopeLastMs = millis();
    float roll, pitch, yaw; toEuler(roll, pitch, yaw);

    // Unwrap yaw: detect +/-360 wrap and accumulate continuously
    float dyaw = yaw - scopeYawPrev;
    if (dyaw >  180.0f) dyaw -= 360.0f;
    if (dyaw < -180.0f) dyaw += 360.0f;
    scopeYawAccum += dyaw;
    scopeYawPrev = yaw;

    int h = scopeHead;
    scopeBuf[0][h] = roll;
    scopeBuf[1][h] = pitch;
    scopeBuf[2][h] = scopeYawAccum;
    int nextH = (h+1) % SCOPE_SAMPLES;

    static const uint16_t waveCols[3] = {
        Display::Colors::GREEN, Display::Colors::AMBER, Display::Colors::GREEN_DIM
    };
    for (int lane=0; lane<3; lane++) {
        int laneY = SCOPE_Y0 + lane*SCOPE_LANE_H;
        Display::tft.fillRect(nextH, laneY+1, 1, SCOPE_LANE_H-2, Display::Colors::BG);
        Display::tft.drawPixel(nextH, laneY+SCOPE_LANE_H/2, 0x0100);
        int y1 = scopeToY(scopeBuf[lane][h], laneY);
        if (h > 0) {
            int y0 = scopeToY(scopeBuf[lane][(h-1+SCOPE_SAMPLES)%SCOPE_SAMPLES], laneY);
            Display::tft.drawLine(h-1, y0, h, y1, waveCols[lane]);
        } else {
            Display::tft.drawPixel(h, y1, waveCols[lane]);
        }
    }
    scopeHead = nextH;
    if (nextH == 0) scopeFull = true;
}
