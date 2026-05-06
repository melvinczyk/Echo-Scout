#include <math.h>
#include "spirit_screen.h"
#include "imu.h"
#include "display.h"
#include "device_state.h"

// ── 3-D Spirit Level ──────────────────────────────────────────────────────────
// A ball rolls inside a circular bowl.  Its XY displacement is proportional
// to roll (X) and pitch (Y) extracted from the quaternion.  The ball position
// is physics-smoothed with a simple damped integrator so it glides realistically.
// A cross-hair, ring graduations, and a "LEVEL" indicator complete the display.

static const int CX          = SpiritScreen::CX;
static const int CY          = SpiritScreen::CY;
static const int R           = SpiritScreen::RADIUS;
static const int BALL_RADIUS = SpiritScreen::BALL_R;

// Colour palette
static constexpr uint16_t COL_BOWL    = Display::Colors::GREEN_DIM;
static constexpr uint16_t COL_BALL_OK = Display::Colors::GREEN;
static constexpr uint16_t COL_BALL_NG = Display::Colors::RED;
static constexpr uint16_t COL_CROSS   = Display::Colors::GREEN_DIM;
static constexpr uint16_t COL_CENTRE  = Display::Colors::GREEN;

// Centre dead-zone radius (px) — ball is green when inside this
static constexpr int DEAD_ZONE = 12;

// Max ball displacement in pixels (at ±45° tilt)
static constexpr float MAX_DISP = (float)(R - BALL_RADIUS - 4);
static constexpr float TILT_SCALE = MAX_DISP / 45.0f;  // px per degree

// Physics state
static float bx = 0.0f, by = 0.0f;   // current ball position (relative to CX,CY)
static float vx = 0.0f, vy = 0.0f;   // velocity
static int   prevBx = 0, prevBy = 0; // last drawn position (screen coords)
static bool  ballDrawn = false;

static const float DAMPING  = 0.75f;
static const float ACCEL    = 0.35f;   // how strongly tilt pulls the ball

// ── helpers ───────────────────────────────────────────────────────────────────

static void drawStaticElements() {
    // Outer bowl rings
    Display::tft.drawCircle(CX, CY, R,     COL_BOWL);
    Display::tft.drawCircle(CX, CY, R + 1, COL_BOWL);

    // Inner dead-zone ring
    Display::tft.drawCircle(CX, CY, DEAD_ZONE,     0x0200);
    Display::tft.drawCircle(CX, CY, DEAD_ZONE + 1, 0x0200);

    // Cross-hair lines
    Display::tft.drawFastHLine(CX - R + 6, CY, 2*(R - 6) + 1, COL_CROSS);
    Display::tft.drawFastVLine(CX, CY - R + 6, 2*(R - 6) + 1, COL_CROSS);

    // Graduation ticks at ±30° (half-range)
    float ang30 = 30.0f * 3.14159f / 180.0f;
    int tx = (int)(MAX_DISP * 0.667f);   // ~30° displacement
    // Horizontal ticks
    Display::tft.drawFastVLine(CX + tx, CY - 5, 10, COL_CROSS);
    Display::tft.drawFastVLine(CX - tx, CY - 5, 10, COL_CROSS);
    // Vertical ticks
    Display::tft.drawFastHLine(CX - 5, CY + tx, 10, COL_CROSS);
    Display::tft.drawFastHLine(CX - 5, CY - tx, 10, COL_CROSS);
}

static void eraseBall(int bsx, int bsy) {
    if (!ballDrawn) return;
    Display::tft.fillCircle(bsx, bsy, BALL_RADIUS + 1, Display::Colors::BG);
    // Redraw cross-hair segments that may have been erased
    // Horizontal segment in ball area
    int hx0 = max(CX - R + 6, bsx - BALL_RADIUS - 1);
    int hx1 = min(CX + R - 6, bsx + BALL_RADIUS + 1);
    if (hx0 < hx1 && bsy - 1 <= CY && CY <= bsy + 1) {
        Display::tft.drawFastHLine(hx0, CY, hx1 - hx0 + 1, COL_CROSS);
    }
    int vy0 = max(CY - R + 6, bsy - BALL_RADIUS - 1);
    int vy1 = min(CY + R - 6, bsy + BALL_RADIUS + 1);
    if (vy0 < vy1 && bsx - 1 <= CX && CX <= bsx + 1) {
        Display::tft.drawFastVLine(CX, vy0, vy1 - vy0 + 1, COL_CROSS);
    }
}

static void drawBall(int bsx, int bsy, bool isLevel) {
    uint16_t col = isLevel ? COL_BALL_OK : COL_BALL_NG;
    // Filled circle
    Display::tft.fillCircle(bsx, bsy, BALL_RADIUS, col);
    // Highlight spot (top-left quadrant)
    Display::tft.fillCircle(bsx - BALL_RADIUS/3, bsy - BALL_RADIUS/3, BALL_RADIUS/4, Display::Colors::WHITE);
    // Border
    Display::tft.drawCircle(bsx, bsy, BALL_RADIUS, Display::Colors::BG);
}

// ── readout ───────────────────────────────────────────────────────────────────
static float prevRollS = -9999, prevPitchS = -9999;

static void drawReadout(float roll, float pitch, bool isLevel) {
    bool changed = fabsf(roll  - prevRollS)  > 0.3f ||
                   fabsf(pitch - prevPitchS) > 0.3f;
    if (!changed) return;
    prevRollS = roll; prevPitchS = pitch;

    int y = SpiritScreen::DASH_Y;
    int cardW = 112, pad = 4, valY = y + 18;
    Display::tft.fillRect(pad + 1, valY - 1, 2*cardW + pad - 2, 22, Display::Colors::BG);

    char buf[12];
    sprintf(buf, "%+.1f", roll);
    Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
    Display::tft.drawCentreString(buf, pad + cardW/2, valY, 2);

    sprintf(buf, "%+.1f", pitch);
    Display::tft.setTextColor(Display::Colors::AMBER, Display::Colors::BG);
    Display::tft.drawCentreString(buf, pad + cardW + pad + cardW/2, valY, 2);

    // LEVEL indicator on right side of header area
    Display::tft.fillRect(168, 6, 64, 16, Display::Colors::BG);
    if (isLevel) {
        Display::tft.drawRect(168, 6, 64, 16, Display::Colors::GREEN);
        Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
        Display::tft.drawCentreString("LEVEL", 200, 9, 1);
    }
}

// ── public API ────────────────────────────────────────────────────────────────

void drawSpiritBase() {
    Display::tft.fillScreen(Display::Colors::BG);
    bx = 0.0f; by = 0.0f;
    vx = 0.0f; vy = 0.0f;
    ballDrawn = false;
    prevRollS = -9999; prevPitchS = -9999;

    Display::drawHeader("SPIRIT");

    Display::tft.drawFastHLine(0, SpiritScreen::DASH_Y, Display::SCREEN_W, Display::Colors::SEP);
    int cardW = 112, pad = 4, y = SpiritScreen::DASH_Y;
    Display::tft.drawRect(pad,               y + 2, cardW, 52, Display::Colors::GREEN_DIM);
    Display::tft.drawRect(pad + cardW + pad, y + 2, cardW, 52, Display::Colors::GREEN_DIM);
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("ROLL",  pad + cardW/2,               y + 5, 1);
    Display::tft.drawCentreString("PITCH", pad + cardW + pad + cardW/2, y + 5, 1);

    if (!ImuState::ready) {
        Display::tft.setTextColor(Display::Colors::RED, Display::Colors::BG);
        Display::tft.drawCentreString("BNO085 NOT FOUND", 120, 150, 2);
        return;
    }

    drawStaticElements();
    drawBall(CX, CY, true);
    prevBx = CX; prevBy = CY;
    ballDrawn = true;
}

void tickSpirit() {
    if (!imuUpdate()) return;

    float qI = ImuState::qI, qJ = ImuState::qJ,
          qK = ImuState::qK, qR = ImuState::qR;

    float sinr = 2.0f*(qR*qI + qJ*qK);
    float cosr = 1.0f - 2.0f*(qI*qI + qJ*qJ);
    float roll  = atan2f(sinr, cosr) * 180.0f / 3.14159f;

    float sinp = 2.0f*(qR*qJ - qK*qI);
    float pitch = (fabsf(sinp) >= 1.0f) ? copysignf(90.0f, sinp)
                                         : asinf(sinp) * 180.0f / 3.14159f;

    // Target position from tilt
    float tx = constrain( roll  * TILT_SCALE, -MAX_DISP, MAX_DISP);
    float ty = constrain(-pitch * TILT_SCALE, -MAX_DISP, MAX_DISP);

    // Clamp target inside bowl radius
    float tdist = sqrtf(tx*tx + ty*ty);
    if (tdist > MAX_DISP) { tx *= MAX_DISP/tdist; ty *= MAX_DISP/tdist; }

    // Damped spring physics
    vx = vx * DAMPING + (tx - bx) * ACCEL;
    vy = vy * DAMPING + (ty - by) * ACCEL;
    bx += vx;
    by += vy;

    // Keep ball inside bowl
    float dist = sqrtf(bx*bx + by*by);
    if (dist > MAX_DISP) { bx *= MAX_DISP/dist; by *= MAX_DISP/dist; vx = 0; vy = 0; }

    int newBx = CX + (int)bx;
    int newBy = CY + (int)by;

    // Only redraw if moved more than 1px
    if (abs(newBx - prevBx) < 1 && abs(newBy - prevBy) < 1) return;

    eraseBall(prevBx, prevBy);
    prevBx = newBx; prevBy = newBy;

    bool isLevel = (dist < (float)DEAD_ZONE);
    drawBall(newBx, newBy, isLevel);
    ballDrawn = true;

    drawReadout(roll, pitch, isLevel);
}
