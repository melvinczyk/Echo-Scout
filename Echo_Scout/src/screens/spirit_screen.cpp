#include <math.h>
#include "spirit_screen.h"
#include "imu.h"
#include "display.h"
#include "device_state.h"

static const int CX          = SpiritScreen::CX;
static const int CY          = SpiritScreen::CY;
static const int R           = SpiritScreen::RADIUS;
static const int BALL_RADIUS = SpiritScreen::BALL_R;

static constexpr uint16_t COL_BOWL    = Display::Colors::GREEN_DIM;
static constexpr uint16_t COL_BALL_OK = Display::Colors::GREEN;
static constexpr uint16_t COL_BALL_NG = Display::Colors::RED;
static constexpr uint16_t COL_CROSS   = Display::Colors::GREEN_DIM;
static constexpr uint16_t COL_CENTRE  = Display::Colors::GREEN;

static constexpr int DEAD_ZONE = 12;

static constexpr float MAX_DISP = (float)(R - BALL_RADIUS - 4);
static constexpr float TILT_SCALE = MAX_DISP / 45.0f;  // px per degree

static float bx = 0.0f, by = 0.0f;   // current ball position (relative to CX,CY)
static float vx = 0.0f, vy = 0.0f;   // velocity
static int   prevBx = 0, prevBy = 0; // last drawn position (screen coords)
static bool  ballDrawn = false;

static const float DAMPING  = 0.75f;
static const float ACCEL    = 0.35f;   // how strongly tilt pulls the ball


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

// ── calibrate button ──────────────────────────────────────────────────────────
// Shown in the top-right header area. Active (green) only when ball is centred.
// x=162..237, y=4..22

static bool  spiritLevel  = false;  // current level state (used by touch handler)
static bool  calFlash     = false;  // show "CAL OK" briefly after calibration
static unsigned long calFlashMs = 0;

static void drawCalButton(bool isLevel) {
    const int BX = 162, BY = 4, BW = 76, BH = 18;

    // If cal flash is active, show CAL OK for 1.5 s then clear
    if (calFlash) {
        if (millis() - calFlashMs < 1500) {
            Display::tft.fillRect(BX, BY, BW, BH, Display::Colors::BG);
            Display::tft.drawRect(BX, BY, BW, BH, Display::Colors::GREEN);
            Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
            Display::tft.drawCentreString("CAL OK", BX + BW/2, BY + 5, 1);
            return;
        }
        calFlash = false;
    }

    Display::tft.fillRect(BX, BY, BW, BH, Display::Colors::BG);
    if (isLevel) {
        Display::tft.drawRect(BX, BY, BW, BH, Display::Colors::GREEN);
        Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
        Display::tft.drawCentreString("CALIBRATE", BX + BW/2, BY + 5, 1);
    } else {
        Display::tft.drawRect(BX, BY, BW, BH, Display::Colors::GREEN_DIM);
        Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
        Display::tft.drawCentreString("CALIBRATE", BX + BW/2, BY + 5, 1);
    }
}

void handleSpiritTouch(int tx, int ty) {
    // Only calibrate if ball is centred (level) and touch is in button area
    if (!spiritLevel) return;
    if (tx < 162 || tx > 238 || ty < 4 || ty > 22) return;

    // Capture the raw (pre-calibration) quaternion as the new reference
    ImuState::calR = ImuState::rawQR;
    ImuState::calI = ImuState::rawQI;
    ImuState::calJ = ImuState::rawQJ;
    ImuState::calK = ImuState::rawQK;
    ImuState::calibrated = true;

    calFlash   = true;
    calFlashMs = millis();
}
// ─────────────────────────────────────────────────────────────────────────────

// ── readout ───────────────────────────────────────────────────────────────────
static float prevRollS = -9999, prevPitchS = -9999;
static bool  prevLevelS = false;

static void drawReadout(float roll, float pitch, bool isLevel) {
    bool changed = fabsf(roll  - prevRollS)  > 0.3f ||
                   fabsf(pitch - prevPitchS) > 0.3f ||
                   isLevel != prevLevelS || calFlash;
    if (!changed) return;
    prevRollS = pitch; prevPitchS = roll; prevLevelS = isLevel;

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

    drawCalButton(isLevel);
}

// ── public API ────────────────────────────────────────────────────────────────

void drawSpiritBase() {
    Display::tft.fillScreen(Display::Colors::BG);
    bx = 0.0f; by = 0.0f;
    vx = 0.0f; vy = 0.0f;
    ballDrawn = false;
    prevRollS = -9999; prevPitchS = -9999; prevLevelS = false;
    calFlash  = false; spiritLevel = false;

    Display::drawHeader("SPIRIT");
    drawCalButton(false);

    Display::tft.drawFastHLine(0, SpiritScreen::DASH_Y, Display::SCREEN_W, Display::Colors::SEP);
    int cardW = 112, pad = 4, y = SpiritScreen::DASH_Y;
    Display::tft.drawRect(pad,               y + 2, cardW, 52, Display::Colors::GREEN_DIM);
    Display::tft.drawRect(pad + cardW + pad, y + 2, cardW, 52, Display::Colors::GREEN_DIM);
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("X-TILT", pad + cardW/2,               y + 5, 1);
    Display::tft.drawCentreString("Y-TILT", pad + cardW + pad + cardW/2, y + 5, 1);

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

static void drawPlumbIndicator() {
  // Portrait device outline — right side of content, above crosshair
  const int IX = 188, IY = Display::HEADER_H + 8;
  const int IW = 26, IH = 36;
  Display::tft.fillRect(IX - 2, IY - 10, IW + 4, IH + 14, Display::Colors::BG);
  Display::tft.drawRect(IX, IY, IW, IH, Display::Colors::GREEN_DIM);
  // Top edge bright = pivot edge (hold this end UP)
  Display::tft.drawFastHLine(IX + 2, IY, IW - 4, Display::Colors::GREEN);
  // Up-arrow above the top edge (▲)
  int ax = IX + IW / 2, ay = IY - 2;
  Display::tft.drawLine(ax, ay - 5, ax - 4, ay, Display::Colors::GREEN);
  Display::tft.drawLine(ax, ay - 5, ax + 4, ay, Display::Colors::GREEN);
  Display::tft.drawFastVLine(ax, ay - 5, 5, Display::Colors::GREEN);
  // Pivot dot at top-centre of device
  Display::tft.fillCircle(ax, IY, 2, Display::Colors::GREEN);
  // Dashed plumb reference (straight down from pivot)
  for (int d = 4; d < IH - 4; d += 5) {
    Display::tft.drawPixel(ax, IY + d, Display::Colors::GREEN_FAINT);
  }
  // Bob dot at rest (centre-bottom)
  Display::tft.fillCircle(ax, IY + IH - 6, 4, Display::Colors::GREEN_DIM);
}

void tickSpirit() {
    if (!imuUpdate()) return;

    float qI = ImuState::qI, qJ = ImuState::qJ,
          qK = ImuState::qK, qR = ImuState::qR;

    // Extract where "down" (calibration reference direction = body J when horizontal)
    // lands in the current corrected frame.  This avoids Euler singularities and
    // works directly in any orientation.
    // Row J of R(q): world-J column projected into body frame = body tilt components.
    // gI = tilt left/right, gK = tilt forward/back (range ±1).
    float gI =  2.0f*(qI*qJ - qR*qK);   // left/right tilt → ball X
    float gK =  2.0f*(qJ*qK + qR*qI);   // fwd/back tilt   → ball Y

    float tiltX = asinf(constrain(gI, -1.0f, 1.0f)) * 180.0f / 3.14159f;
    float tiltY = asinf(constrain(gK, -1.0f, 1.0f)) * 180.0f / 3.14159f;

    // Target position from tilt
    float tx = constrain(gI * MAX_DISP, -MAX_DISP, MAX_DISP);
    float ty = constrain(gK * MAX_DISP, -MAX_DISP, MAX_DISP);

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
    spiritLevel = isLevel;
    drawBall(newBx, newBy, isLevel);
    ballDrawn = true;

    drawPlumbIndicator();
    drawReadout(tiltX, tiltY, isLevel);
}
