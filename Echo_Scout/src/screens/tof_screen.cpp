#include <math.h>
#include "tof_screen.h"
#include "tof.h"
#include "imu.h"
#include "display.h"
#include "touch.h"
#include "device_state.h"
#include "device_icons.h"

// ── Layout ────────────────────────────────────────────────────────────────────
static constexpr int TAB_Y     = 280;
static constexpr int TAB_H     = Display::SCREEN_H - TAB_Y;
static constexpr int CONTENT_Y = Display::HEADER_H;            // 28
static constexpr int TAB_COUNT = 3;
static constexpr int TAB_W     = Display::SCREEN_W / TAB_COUNT; // 80
enum TofTab { TAB_GRID = 0, TAB_ANGLE = 1, TAB_DIST = 2 };
static TofTab activeTab = TAB_GRID;
static const char* tabLabels[TAB_COUNT] = { "GRID", "ANGL", "DIST" };

// ── Tab bar ───────────────────────────────────────────────────────────────────
static void drawTabBar() {
    Display::tft.fillRect(0, TAB_Y, Display::SCREEN_W, TAB_H, Display::Colors::BG);
    Display::tft.drawFastHLine(0, TAB_Y, Display::SCREEN_W, Display::Colors::SEP);
    for (int t = 0; t < TAB_COUNT; t++) {
        int x = t * TAB_W;
        bool active = (t == (int)activeTab);
        if (active) {
            Display::tft.fillRect(x, TAB_Y+1, TAB_W-1, TAB_H-1, Display::Colors::GREEN_FAINT);
            Display::tft.drawFastHLine(x, TAB_Y, TAB_W-1, Display::Colors::GREEN);
            Display::tft.setTextColor(Display::Colors::GREEN, (uint16_t)Display::Colors::GREEN_FAINT);
        } else {
            Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
        }
        Display::tft.drawCentreString(tabLabels[t], x + TAB_W/2, TAB_Y + 14, 1);
    }
}

// ── Distance heat colour (red=close → green=far) ──────────────────────────────
static uint16_t distToColor(float d) {
    if (d <= 0.0f)   return 0x2104;
    if (d < 300.0f)  return 0xF800;
    if (d < 600.0f)  { float t=(d-300.0f)/300.0f; return (uint16_t)((31u<<11)|((uint8_t)(t*20.0f)<<5)); }
    if (d < 1200.0f) { float t=(d-600.0f)/600.0f; return (uint16_t)((31u<<11)|((uint8_t)(20.0f+t*43.0f)<<5)); }
    if (d < 2500.0f) { float t=(d-1200.0f)/1300.0f; return (uint16_t)(((uint8_t)(31.0f*(1.0f-t))<<11)|(63u<<5)); }
    return Display::Colors::GREEN_DIM;
}

// ── GRID TAB ─────────────────────────────────────────────────────────────────
static constexpr int CELL  = 30;
static constexpr int GRID_Y = CONTENT_Y;
static constexpr int GRID_H = CELL * 8;
static constexpr int AVG_Y  = GRID_Y + GRID_H;

static float prevDistances[64];
static float prevAvg = -1.0f;

static void drawCell(int zone, float d) {
    int row = zone/8, col = zone%8;
    Display::tft.fillRect(col*CELL+1, GRID_Y+row*CELL+1, CELL-2, CELL-2, distToColor(d));
}
static void drawAvgBar(float avg) {
    Display::tft.fillRect(0, AVG_Y, Display::SCREEN_W, TAB_Y-AVG_Y, Display::Colors::BG);
    char buf[24];
    if (avg < 1000.0f) sprintf(buf, "AVG  %.0f mm", avg);
    else               sprintf(buf, "AVG  %.2f m",  avg/1000.0f);
    Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
    Display::tft.drawCentreString(buf, 120, AVG_Y+2, 1);
}
static void drawGridTab() {
    Display::tft.fillRect(0, CONTENT_Y, Display::SCREEN_W, TAB_Y-CONTENT_Y, Display::Colors::BG);
    for (int i=0; i<64; i++) prevDistances[i]=-1.0f;
    prevAvg=-1.0f;
}
static void tickGridTab() {
    if (!tofUpdate()) return;
    float sum=0.0f; int valid=0;
    for (int i=0; i<64; i++) {
        float d=TofState::distances[i];
        if (fabsf(d-prevDistances[i])>5.0f) { drawCell(i,d); prevDistances[i]=d; }
        if (d>0.0f) { sum+=d; valid++; }
    }
    float avg = valid>0 ? sum/valid : 0.0f;
    if (fabsf(avg-prevAvg)>5.0f) { drawAvgBar(avg); prevAvg=avg; }
}

static float centerDist();  // forward declaration — defined in DIST TAB section

// ── ANGLE TAB ─────────────────────────────────────────────────────────────────
enum AngleState { ANGLE_IDLE, ANGLE_A_LOCKED, ANGLE_DONE };
static AngleState angleState = ANGLE_IDLE;
static float vecAx, vecAy, vecAz, vecBx, vecBy, vecBz;
static float lockedAngle  = 0.0f;
static float prevLiveAngle = -9999.0f;
static float distA = 0.0f, distB = 0.0f;  // ToF distances at lock points (mm)

// Layout: 76px number strip at top + viz filling rest down to button
static constexpr int ANGL_NUM_H    = 76;
static constexpr int ANGL_BTN_H    = 44;
static constexpr int ANGL_BTN_Y    = TAB_Y - ANGL_BTN_H;          // 236
static constexpr int ANGL_VIZ_H    = ANGL_BTN_Y - 2 - (CONTENT_Y + ANGL_NUM_H); // 130
static constexpr int ANGL_CONTENT_H = ANGL_NUM_H + ANGL_VIZ_H;

// Viz centre — pushed lower by the taller number strip
static constexpr int   VIZ_CX    = 120;
static constexpr int   VIZ_CY    = CONTENT_Y + ANGL_NUM_H + ANGL_VIZ_H / 2 + 20;  // 169
static constexpr float VIZ_SCALE = 80.0f;

static void getBoresightWorld(float& wx, float& wy, float& wz) {
    float r=ImuState::qR, i=ImuState::qI, j=ImuState::qJ, k=ImuState::qK;
    wx = 2.0f*r*j + 2.0f*i*k;
    wy = 2.0f*j*k - 2.0f*r*i;
    wz = 1.0f - 2.0f*i*i - 2.0f*j*j;
    float mag=sqrtf(wx*wx+wy*wy+wz*wz);
    if (mag>0.001f) { wx/=mag; wy/=mag; wz/=mag; }
}

// Cabinet projection: Y axis goes into the screen (depth cue)
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
        float t  = (float)i / STEPS;
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

    // Faint axis stubs
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
        // IDLE: show live vector faintly
        drawVizArrow(wx, wy, wz, Display::Colors::GREEN_DIM);
    } else if (!hasB) {
        // A locked, B is live
        drawVizArrow(vecAx, vecAy, vecAz, Display::Colors::GREEN);
        drawVizArrow(wx, wy, wz, Display::Colors::AMBER);
        drawVizArc(vecAx, vecAy, vecAz, wx, wy, wz, Display::Colors::GREEN_DIM);
    } else {
        // Both locked
        drawVizArrow(vecAx, vecAy, vecAz, Display::Colors::GREEN);
        drawVizArrow(vecBx, vecBy, vecBz, Display::Colors::AMBER);
        drawVizArc(vecAx, vecAy, vecAz, vecBx, vecBy, vecBz, Display::Colors::RED);
    }
}

// Number strip at top — angle + distance between points
static void drawAngleValue(float deg, bool locked) {
    Display::spr.deleteSprite();
    Display::spr.createSprite(Display::SCREEN_W, ANGL_NUM_H);
    Display::spr.fillSprite(Display::Colors::BG);

    if (angleState == ANGLE_IDLE) {
        Display::spr.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
        Display::spr.drawCentreString("POINT AT TARGET A", 120, 28, 2);
    } else {
        uint16_t col = (angleState == ANGLE_DONE) ? Display::Colors::GREEN : Display::Colors::AMBER;

        // Angle — centred, font 4
        char buf[24]; sprintf(buf, "%.1f deg", deg);
        Display::spr.setTextColor(col, Display::Colors::BG);
        Display::spr.drawCentreString(buf, 120, 6, 4);

        // Distance — amber, centred below
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

static void drawAngleTab() {
    Display::tft.fillRect(0, CONTENT_Y, Display::SCREEN_W, TAB_Y - CONTENT_Y, Display::Colors::BG);
    angleState    = ANGLE_IDLE;
    prevLiveAngle = -9999.0f;
    distA = distB = 0.0f;
    drawAngleViz(false, false);
    drawAngleValue(0.0f, false);
    drawAngleBtn();
}

static void tickAngleTab() {
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

static void handleAngleTouch(int tx, int ty) {
    if (!inRect(tx, ty, 20, ANGL_BTN_Y, 200, ANGL_BTN_H)) return;

    if (angleState == ANGLE_IDLE) {
        getBoresightWorld(vecAx, vecAy, vecAz);
        distA = centerDist();
        distB = 0.0f;
        angleState    = ANGLE_A_LOCKED;
        prevLiveAngle = -9999.0f;
        drawAngleViz(true, false);
        drawAngleValue(0.0f, false);
        drawAngleBtn();
    } else if (angleState == ANGLE_A_LOCKED) {
        getBoresightWorld(vecBx, vecBy, vecBz);
        distB = centerDist();
        float dot  = constrain(vecAx*vecBx + vecAy*vecBy + vecAz*vecBz, -1.0f, 1.0f);
        lockedAngle = acosf(dot) * 180.0f / 3.14159f;
        angleState  = ANGLE_DONE;
        drawAngleViz(true, true);
        drawAngleValue(lockedAngle, true);
        drawAngleBtn();
    } else {
        drawAngleTab();
    }
}

// ── DIST TAB — center 2×2 distance meter ─────────────────────────────────────
// Zones 27,28,35,36 are the inner 2×2 of the 8×8 grid (rows 3-4, cols 3-4).

static constexpr int DIST_CENTER_ZONES[4] = { 27, 28, 35, 36 };
static constexpr int DIST_BTN_X = 20, DIST_BTN_Y = TAB_Y - 44, DIST_BTN_W = 200, DIST_BTN_H = 44;
static constexpr int DIST_NUM_H = 36;                              // number strip height
static constexpr int DIST_VIZ_Y = CONTENT_Y + DIST_NUM_H;         // 64
static constexpr int DIST_VIZ_BOT = DIST_BTN_Y - 4;               // just above button
static constexpr int DIST_VIZ_H = DIST_VIZ_BOT - DIST_VIZ_Y;     // ~168px
static constexpr int DIST_VIZ_CX = 120;
static constexpr float DIST_VIZ_MAX = 4000.0f;                    // 4 m max display range

enum DistState { DIST_LIVE, DIST_LOCKED };
static DistState distState = DIST_LIVE;
static float     distLocked = 0.0f;
static float     distPrevLive = -9999.0f;

// Average the 4 center zones; returns 0 if all invalid
static float centerDist() {
    // Single cell closest to grid centre — zone 27 = row 3, col 3 (0-indexed)
    float d = TofState::distances[27];
    return d > 10.0f ? d : 0.0f;
}

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

    int devY  = DIST_VIZ_BOT;   // device at bottom
    int topY  = DIST_VIZ_Y;
    int spread = 52;             // half-width of frustum at max range

    // Perspective guide lines
    Display::tft.drawLine(DIST_VIZ_CX, devY, DIST_VIZ_CX - spread, topY, Display::Colors::GREEN_FAINT);
    Display::tft.drawLine(DIST_VIZ_CX, devY, DIST_VIZ_CX + spread, topY, Display::Colors::GREEN_FAINT);
    // Centre beam
    Display::tft.drawFastVLine(DIST_VIZ_CX, topY, DIST_VIZ_H, Display::Colors::GREEN_FAINT);

    // Scale rings every 500 mm
    for (float r = 500.0f; r <= DIST_VIZ_MAX + 1.0f; r += 500.0f) {
        float t = r / DIST_VIZ_MAX;
        int sy  = devY - (int)(t * DIST_VIZ_H);
        int sw  = (int)(spread * t);
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
        float t  = dc / DIST_VIZ_MAX;
        int ty   = devY - (int)(t * DIST_VIZ_H);
        int tw   = (int)(spread * t);
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

static void drawDistTab() {
    Display::tft.fillRect(0, CONTENT_Y, Display::SCREEN_W, TAB_Y - CONTENT_Y, Display::Colors::BG);
    distState    = DIST_LIVE;
    distPrevLive = -9999.0f;
    tofUpdate();
    float d = centerDist();
    drawDistValue(d, false);
    drawDistViz(d);
    drawDistBtn(false);
}

static void tickDistTab() {
    if (distState == DIST_LOCKED) return;
    if (!tofUpdate()) return;
    float d = centerDist();
    if (fabsf(d - distPrevLive) < 5.0f) return;
    distPrevLive = d;
    drawDistValue(d, false);
    drawDistViz(d);
}

static void handleDistTouch(int tx, int ty) {
    if (!inRect(tx, ty, DIST_BTN_X, DIST_BTN_Y, DIST_BTN_W, DIST_BTN_H)) return;
    if (distState == DIST_LIVE) {
        tofUpdate();
        distLocked = centerDist();
        distState  = DIST_LOCKED;
        drawDistValue(distLocked, true);
        drawDistViz(distLocked);
        drawDistBtn(true);
    } else {
        distState    = DIST_LIVE;
        distPrevLive = -9999.0f;
        drawDistBtn(false);
    }
}

// ── Dispatch ──────────────────────────────────────────────────────────────────
static void enterTab() {
    switch(activeTab) {
        case TAB_GRID:  drawGridTab();  break;
        case TAB_ANGLE: drawAngleTab(); break;
        case TAB_DIST:  drawDistTab();  break;
    }
}

// ── Public API ────────────────────────────────────────────────────────────────
void drawTofBase() {
    Display::tft.fillScreen(Display::Colors::BG);
    Display::drawHeader("TOF IMAGER");
    if (!TofState::ready) {
        Display::tft.setTextColor(Display::Colors::RED, Display::Colors::BG);
        Display::tft.drawCentreString("VL53L5CX NOT FOUND", 120, 150, 2);
        return;
    }
    activeTab=TAB_GRID;
    drawTabBar();
    enterTab();
}

void tickTof() {
    if (!TofState::ready) return;
    switch(activeTab) {
        case TAB_GRID:  tickGridTab();  break;
        case TAB_ANGLE: tickAngleTab(); break;
        case TAB_DIST:  tickDistTab();  break;
    }
}

void handleTofTouch(int tx, int ty) {
    if (ty>=TAB_Y) {
        int t=constrain(tx/TAB_W, 0, TAB_COUNT-1);
        if (t!=(int)activeTab) { activeTab=(TofTab)t; drawTabBar(); enterTab(); }
        return;
    }
    switch(activeTab) {
        case TAB_ANGLE: handleAngleTouch(tx,ty); break;
        case TAB_DIST:  handleDistTouch(tx,ty);  break;
        default: break;
    }
}
