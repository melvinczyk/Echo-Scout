#include <math.h>
#include "horizon_screen.h"
#include "imu.h"
#include "display.h"
#include "device_state.h"

// ── Artificial Horizon (Attitude Indicator) ──────────────────────────────────
// Draws a clipped horizon band: sky above, ground below, bank angle arc,
// pitch ladder lines, and roll/pitch readout cards at the bottom.
// Uses only the game rotation vector quaternion already in ImuState.

static const int CX     = HorizonScreen::CX;
static const int CY     = HorizonScreen::CY;
static const int RADIUS = HorizonScreen::RADIUS;

// Sky / ground colours (RGB565)
static constexpr uint16_t COL_SKY    = 0x02B5;   // dark teal-blue
static constexpr uint16_t COL_GROUND = 0x3200;   // dark earth-brown
static constexpr uint16_t COL_HORIZ  = 0xFFE0;   // yellow horizon line
static constexpr uint16_t COL_RETICLE= Display::Colors::GREEN;
static constexpr uint16_t COL_DIM    = Display::Colors::GREEN_DIM;

// Clipping circle bounds
static const int CLIP_TOP    = Display::HEADER_H + 4;
static const int CLIP_BOTTOM = HorizonScreen::DASH_Y - 4;

// ── helpers ──────────────────────────────────────────────────────────────────

static inline bool inCircle(int x, int y) {
    int dx = x - CX, dy = y - CY;
    return (dx*dx + dy*dy) <= RADIUS*RADIUS;
}

// Draw a horizontal span inside the clipping circle
static void hSpanClipped(int y, int xL, int xR, uint16_t col) {
    if (y < CLIP_TOP || y > CLIP_BOTTOM) return;
    // clip to circle
    int dy = y - CY;
    int halfW = (int)sqrtf(max(0.0f, (float)(RADIUS*RADIUS - dy*dy)));
    int xa = max(xL, CX - halfW);
    int xb = min(xR, CX + halfW);
    if (xa > xb) return;
    Display::tft.drawFastHLine(xa, y, xb - xa + 1, col);
}

// Fill the circle with sky/ground split by a horizon line at angle rollDeg,
// displaced vertically by pitchPx pixels.
static void drawHorizonFill(float rollRad, int pitchPx) {
    float sinR = sinf(rollRad), cosR = cosf(rollRad);

    for (int y = CLIP_TOP; y <= CLIP_BOTTOM; y++) {
        int dy = y - CY;
        int halfW = (int)sqrtf(max(0.0f, (float)(RADIUS*RADIUS - dy*dy)));
        if (halfW == 0) continue;
        int xLeft  = CX - halfW;
        int xRight = CX + halfW;

        // For each pixel row, find where the horizon line crosses
        // Horizon line passes through (CX, CY - pitchPx) with normal (sinR, cosR)
        // line eq: sinR*(x-CX) + cosR*(y-(CY-pitchPx)) = 0
        // => x = CX - cosR*(y-(CY-pitchPx)) / sinR   (when sinR != 0)
        // Determine which side is sky (above horizon in world = cosR*(dy') + sinR*(dx') < 0)

        // For this scanline: the horizon crossing x
        float dy2 = (float)(y - (CY - pitchPx));
        // sign: point (x,y) is "sky side" if sinR*(x-CX) + cosR*dy2 < 0
        // At xLeft:
        float signLeft = sinR * (float)(xLeft - CX) + cosR * dy2;

        if (fabsf(sinR) < 0.01f) {
            // Nearly level roll: pure horizontal split
            uint16_t col = (dy2 < 0) ? COL_SKY : COL_GROUND;
            Display::tft.drawFastHLine(xLeft, y, xRight - xLeft + 1, col);
            continue;
        }

        // x coordinate where horizon crosses this scanline
        float xHoriz = (float)CX - cosR * dy2 / sinR;
        int xi = (int)xHoriz;

        if (signLeft < 0) {
            // xLeft..xi = sky, xi..xRight = ground
            int split = constrain(xi, xLeft, xRight);
            if (split > xLeft)
                Display::tft.drawFastHLine(xLeft, y, split - xLeft, COL_SKY);
            if (split < xRight)
                Display::tft.drawFastHLine(split, y, xRight - split + 1, COL_GROUND);
        } else {
            // xLeft..xi = ground, xi..xRight = sky
            int split = constrain(xi, xLeft, xRight);
            if (split > xLeft)
                Display::tft.drawFastHLine(xLeft, y, split - xLeft, COL_GROUND);
            if (split < xRight)
                Display::tft.drawFastHLine(split, y, xRight - split + 1, COL_SKY);
        }
    }
}

// Draw the yellow horizon line
static void drawHorizonLine(float rollRad, int pitchPx) {
    float sinR = sinf(rollRad), cosR = cosf(rollRad);
    // Draw the line: parametric along the horizon direction (-cosR, sinR)
    for (int t = -RADIUS; t <= RADIUS; t++) {
        int x = CX + (int)(-cosR * t);
        int y = (CY - pitchPx) + (int)(sinR * t);
        if (!inCircle(x, y)) continue;
        if (y < CLIP_TOP || y > CLIP_BOTTOM) continue;
        Display::tft.drawPixel(x, y, COL_HORIZ);
        Display::tft.drawPixel(x, y+1, COL_HORIZ);  // 2px thick
    }
}

// Draw pitch ladder marks (±10°, ±20°) as short lines perpendicular to roll
static void drawPitchLadder(float rollRad, float pitchDeg) {
    float sinR = sinf(rollRad), cosR = cosf(rollRad);
    // 1 degree ≈ 3px at this scale
    constexpr float PITCH_SCALE = 3.0f;
    const int marks[] = {-20, -10, 10, 20};
    for (int m : marks) {
        float dPitch = (float)m - pitchDeg;
        int py = (CY) - (int)(dPitch * PITCH_SCALE);
        // Centre of this ladder mark in screen space (accounting for roll)
        int mcx = CX + (int)(sinR * dPitch * PITCH_SCALE);
        int mcy = py  - (int)(0);  // adjusted below
        // Actually: ladder marks are drawn perpendicular to roll direction
        // centre point displaced pitchPx from horizon centre
        float disp = -dPitch * PITCH_SCALE;
        int lx = CX + (int)(sinR * disp);
        int ly = (CY) + (int)(-cosR * disp);

        // Half-length of mark
        int halfLen = (abs(m) == 10) ? 14 : 22;
        // Draw line segment perpendicular to roll at this point
        for (int s = -halfLen; s <= halfLen; s++) {
            int px = lx + (int)(-cosR * s);
            int py2 = ly + (int)(-sinR * s);
            if (!inCircle(px, py2)) continue;
            if (py2 < CLIP_TOP || py2 > CLIP_BOTTOM) continue;
            Display::tft.drawPixel(px, py2, COL_DIM);
        }
    }
}

// Draw bank angle arc at the top of the circle with a tick at current roll
static void drawBankArc(float rollDeg) {
    const int arcR = RADIUS - 6;
    // Tick marks at 0, ±10, ±20, ±30, ±45, ±60 degrees
    const int ticks[] = {-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60};
    for (int t : ticks) {
        float rad = ((float)t - 90.0f) * 3.14159f / 180.0f;  // 0° at top
        int len = (t == 0 || abs(t) == 30 || abs(t) == 60) ? 8 : 5;
        int x1 = CX + (int)(arcR * cosf(rad));
        int y1 = CY + (int)(arcR * sinf(rad));
        int x2 = CX + (int)((arcR - len) * cosf(rad));
        int y2 = CY + (int)((arcR - len) * sinf(rad));
        if (y1 < CLIP_TOP || y2 < CLIP_TOP) continue;
        Display::tft.drawLine(x1, y1, x2, y2, COL_DIM);
    }
    // Moving pointer at current bank angle
    float pRad = (-rollDeg - 90.0f) * 3.14159f / 180.0f;
    int px = CX + (int)((arcR - 2) * cosf(pRad));
    int py = CY + (int)((arcR - 2) * sinf(pRad));
    if (py >= CLIP_TOP && py <= CLIP_BOTTOM)
        Display::tft.fillTriangle(
            px, py,
            px + (int)(cosf(pRad + 1.5f) * 5), py + (int)(sinf(pRad + 1.5f) * 5),
            px + (int)(cosf(pRad - 1.5f) * 5), py + (int)(sinf(pRad - 1.5f) * 5),
            Display::Colors::GREEN
        );
}

// Centre reticle (fixed, drawn on top)
static void drawReticle() {
    Display::tft.drawFastHLine(CX - 28, CY, 18, COL_RETICLE);
    Display::tft.drawFastHLine(CX + 10, CY, 18, COL_RETICLE);
    Display::tft.drawFastVLine(CX, CY - 4, 8, COL_RETICLE);
    Display::tft.drawCircle(CX, CY, 3, COL_RETICLE);
}

// Circle border
static void drawCircleBorder() {
    Display::tft.drawCircle(CX, CY, RADIUS,     Display::Colors::GREEN_DIM);
    Display::tft.drawCircle(CX, CY, RADIUS + 1, Display::Colors::GREEN_DIM);
}

// ── readout cards ─────────────────────────────────────────────────────────────
static float prevRoll2 = -9999, prevPitch2 = -9999;

static void drawReadout(float roll, float pitch) {
    bool changed = fabsf(roll  - prevRoll2)  > 0.5f ||
                   fabsf(pitch - prevPitch2) > 0.5f;
    if (!changed) return;
    prevRoll2 = roll; prevPitch2 = pitch;

    int cardW = 112, pad = 4, valY = HorizonScreen::DASH_Y + 18;
    Display::tft.fillRect(pad + 1, valY - 1, 2 * cardW + pad - 2, 22, Display::Colors::BG);

    char buf[12];
    sprintf(buf, "%+.1f", roll);
    Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
    Display::tft.drawCentreString(buf, pad + cardW / 2, valY, 2);

    sprintf(buf, "%+.1f", pitch);
    Display::tft.setTextColor(Display::Colors::AMBER, Display::Colors::BG);
    Display::tft.drawCentreString(buf, pad + cardW + pad + cardW / 2, valY, 2);
}

// ── public API ────────────────────────────────────────────────────────────────

void drawHorizonBase() {
    Display::tft.fillScreen(Display::Colors::BG);
    prevRoll2  = -9999;
    prevPitch2 = -9999;

    Display::drawHeader("HORIZON");

    // Bottom readout frame
    Display::tft.drawFastHLine(0, HorizonScreen::DASH_Y, Display::SCREEN_W, Display::Colors::SEP);
    int cardW = 112, pad = 4;
    Display::tft.drawRect(pad,            HorizonScreen::DASH_Y + 2, cardW,     52, Display::Colors::GREEN_DIM);
    Display::tft.drawRect(pad + cardW + pad, HorizonScreen::DASH_Y + 2, cardW,  52, Display::Colors::GREEN_DIM);
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("ROLL",  pad + cardW / 2,               HorizonScreen::DASH_Y + 5, 1);
    Display::tft.drawCentreString("PITCH", pad + cardW + pad + cardW / 2, HorizonScreen::DASH_Y + 5, 1);

    if (!ImuState::ready) {
        Display::tft.setTextColor(Display::Colors::RED, Display::Colors::BG);
        Display::tft.drawCentreString("BNO085 NOT FOUND", 120, 150, 2);
        return;
    }

    // Initial draw with neutral attitude
    drawHorizonFill(0.0f, 0);
    drawHorizonLine(0.0f, 0);
    drawBankArc(0.0f);
    drawReticle();
    drawCircleBorder();
}

void tickHorizon() {
    if (!imuUpdate()) return;

    float qI = ImuState::qI, qJ = ImuState::qJ,
          qK = ImuState::qK, qR = ImuState::qR;

    // Roll
    float sinr = 2.0f * (qR*qI + qJ*qK);
    float cosr = 1.0f - 2.0f*(qI*qI + qJ*qJ);
    float roll  = atan2f(sinr, cosr) * 180.0f / 3.14159f;

    // Pitch
    float sinp = 2.0f*(qR*qJ - qK*qI);
    float pitch = (fabsf(sinp) >= 1.0f) ? copysignf(90.0f, sinp)
                                         : asinf(sinp) * 180.0f / 3.14159f;

    float rollRad  = -roll  * 3.14159f / 180.0f;
    int   pitchPx  = (int)(pitch * 3.0f);
    pitchPx = constrain(pitchPx, -(RADIUS - 10), RADIUS - 10);

    // Full redraw of the disk each tick (fast on TFT_eSPI)
    drawHorizonFill(rollRad, pitchPx);
    drawPitchLadder(rollRad, pitch);
    drawHorizonLine(rollRad, pitchPx);
    drawBankArc(roll);
    drawReticle();
    drawCircleBorder();
    drawReadout(roll, pitch);
}
