#include <math.h>
#include "compass_screen.h"
#include "imu.h"
#include "display.h"
#include "device_state.h"

// ── Compass Rose ─────────────────────────────────────────────────────────────
// A full compass rose that rotates around the fixed north arrow.
// Yaw is derived from the game rotation vector quaternion.
// Cardinal/intercardinal labels, major/minor tick ring, lubber line,
// heading tape at the bottom, and a smoothed bearing readout.

static const int CX = CompassScreen::CX;
static const int CY = CompassScreen::CY;
static const int R  = CompassScreen::RADIUS;

static constexpr uint16_t COL_RING   = Display::Colors::GREEN_DIM;
static constexpr uint16_t COL_NORTH  = Display::Colors::RED;
static constexpr uint16_t COL_CARD   = Display::Colors::GREEN;
static constexpr uint16_t COL_TICK   = Display::Colors::GREEN_DIM;
static constexpr uint16_t COL_NEEDLE = Display::Colors::GREEN;
static constexpr uint16_t COL_LUBBER = Display::Colors::AMBER;

static float prevYawC  = -9999.0f;
static float smoothYaw = 0.0f;

// ── helpers ───────────────────────────────────────────────────────────────────

// Angle wrap to [0, 360)
static inline float wrap360(float a) {
    while (a <   0.0f) a += 360.0f;
    while (a >= 360.0f) a -= 360.0f;
    return a;
}

// Draw a single character centred at (x,y) — used for compass labels
static void drawCharCentred(char c, int x, int y, uint16_t col) {
    char buf[2] = {c, 0};
    Display::tft.setTextColor(col, Display::Colors::BG);
    Display::tft.drawCentreString(buf, x, y - 6, 1);
}

// ── full rose redraw ──────────────────────────────────────────────────────────
// We redraw the entire disk on each tick.  At 240px wide this is fast.

static void drawRoseDisk(float yawDeg) {
    // 1. Fill circle background
    Display::tft.fillCircle(CX, CY, R - 2, Display::Colors::BG);

    float yawRad = yawDeg * 3.14159f / 180.0f;

    // 2. Tick marks & cardinal labels every 10° (major every 30°, cardinals every 90°)
    for (int deg = 0; deg < 360; deg += 5) {
        float a = ((float)deg - yawDeg) * 3.14159f / 180.0f - 3.14159f / 2.0f;
        float ca = cosf(a), sa = sinf(a);

        bool isMajor    = (deg % 30 == 0);
        bool isCardinal = (deg % 90 == 0);
        int  tickLen    = isCardinal ? 14 : (isMajor ? 10 : 5);
        uint16_t col    = isMajor ? COL_RING : 0x0140;

        int x1 = CX + (int)((R - 2)          * ca);
        int y1 = CY + (int)((R - 2)          * sa);
        int x2 = CX + (int)((R - 2 - tickLen)* ca);
        int y2 = CY + (int)((R - 2 - tickLen)* sa);
        Display::tft.drawLine(x1, y1, x2, y2, col);

        // Cardinal and intercardinal labels
        if (deg % 45 == 0) {
            const char* labels[] = {"N","NE","E","SE","S","SW","W","NW"};
            int idx = deg / 45;
            int lx = CX + (int)((R - 20) * ca);
            int ly = CY + (int)((R - 20) * sa);
            uint16_t lcol = (deg == 0) ? COL_NORTH : COL_CARD;
            // Draw first char of label
            char c = labels[idx][0];
            drawCharCentred(c, lx, ly, lcol);
            // For NE/SE/SW/NW draw second char offset
            if (labels[idx][1] != 0) {
                drawCharCentred(labels[idx][1], lx + 5, ly, lcol);
            }
        }
    }

    // 3. Outer ring
    Display::tft.drawCircle(CX, CY, R,     COL_RING);
    Display::tft.drawCircle(CX, CY, R + 1, COL_RING);

    // 4. Inner degree ring
    Display::tft.drawCircle(CX, CY, R - 16, 0x0140);

    // 5. Fixed lubber line (top, points to heading)
    Display::tft.drawFastVLine(CX, CY - R + 2, 18, COL_LUBBER);
    Display::tft.drawFastVLine(CX - 1, CY - R + 2, 18, COL_LUBBER);

    // 6. Fixed centre needle / chevron pointing up
    Display::tft.fillTriangle(
        CX,      CY - 28,
        CX - 8,  CY,
        CX + 8,  CY,
        COL_NEEDLE
    );
    Display::tft.fillTriangle(
        CX,      CY + 28,
        CX - 8,  CY,
        CX + 8,  CY,
        0x0280
    );
    Display::tft.drawCircle(CX, CY, 5, COL_NEEDLE);
    Display::tft.fillCircle(CX, CY, 4, Display::Colors::BG);
    Display::tft.drawCircle(CX, CY, 2, COL_NEEDLE);
}

// ── readout ───────────────────────────────────────────────────────────────────

static const char* bearingName(float deg) {
    int d = ((int)(deg + 22.5f)) % 360 / 45;
    const char* names[] = {"N","NE","E","SE","S","SW","W","NW"};
    return names[d];
}

static float prevDispYaw = -9999;

static void drawReadout(float yaw) {
    if (fabsf(yaw - prevDispYaw) < 0.5f) return;
    prevDispYaw = yaw;

    int y = CompassScreen::DASH_Y;
    // Fill interior without touching the 1px frame border on left/right
    Display::tft.fillRect(5, y + 4, Display::SCREEN_W - 10, 58, Display::Colors::BG);
    char buf[8];
    sprintf(buf, "%03.0f", wrap360(yaw));
    Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
    Display::tft.drawCentreString(buf, 80, y + 14, 4);

    // Cardinal direction
    Display::tft.setTextColor(Display::Colors::AMBER, Display::Colors::BG);
    Display::tft.drawString(bearingName(wrap360(yaw)), 168, y + 20, 2);
    Display::tft.drawString("deg", 168, y + 38, 1);
}

// ── public API ────────────────────────────────────────────────────────────────

void drawCompassBase() {
    Display::tft.fillScreen(Display::Colors::BG);
    prevYawC   = -9999.0f;
    prevDispYaw = -9999.0f;
    smoothYaw  = 0.0f;

    Display::drawHeader("COMPASS");

    Display::tft.drawFastHLine(0, CompassScreen::DASH_Y, Display::SCREEN_W, Display::Colors::SEP);
    // Readout area frame
    Display::tft.drawRect(4, CompassScreen::DASH_Y + 2, Display::SCREEN_W - 8, 62,
                          Display::Colors::GREEN_DIM);

    if (!ImuState::ready) {
        Display::tft.setTextColor(Display::Colors::RED, Display::Colors::BG);
        Display::tft.drawCentreString("BNO085 NOT FOUND", 120, 150, 2);
        return;
    }

    drawRoseDisk(0.0f);
    drawReadout(0.0f);
}

void tickCompass() {
    if (!imuUpdate()) return;

    float qI = ImuState::qI, qJ = ImuState::qJ,
          qK = ImuState::qK, qR = ImuState::qR;

    // Yaw (heading)
    float siny = 2.0f*(qR*qK + qI*qJ);
    float cosy = 1.0f - 2.0f*(qJ*qJ + qK*qK);
    float yaw  = atan2f(siny, cosy) * 180.0f / 3.14159f;
    yaw = wrap360(yaw);

    // Smooth yaw — handle 0/360 wraparound
    float delta = yaw - smoothYaw;
    if (delta >  180.0f) delta -= 360.0f;
    if (delta < -180.0f) delta += 360.0f;
    smoothYaw = wrap360(smoothYaw + delta * 0.25f);

    if (fabsf(smoothYaw - prevYawC) < 0.5f) return;
    prevYawC = smoothYaw;

    drawRoseDisk(smoothYaw);
    drawReadout(smoothYaw);
}
