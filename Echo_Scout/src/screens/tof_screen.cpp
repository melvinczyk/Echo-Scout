#include <math.h>
#include "tof_screen.h"
#include "tof.h"
#include "imu.h"
#include "display.h"
#include "touch.h"
#include "device_state.h"

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

// ── ANGLE TAB ─────────────────────────────────────────────────────────────────
enum AngleState { ANGLE_IDLE, ANGLE_A_LOCKED, ANGLE_DONE };
static AngleState angleState = ANGLE_IDLE;
static float vecAx, vecAy, vecAz, vecBx, vecBy, vecBz;
static float lockedAngle  = 0.0f;
static float prevLiveAngle = -9999.0f;

// All angle-tab drawing goes through a fixed sprite region to avoid ghost pixels.
static constexpr int ANGL_CONTENT_H = 190;   // area above the button
static constexpr int ANGL_BTN_Y     = CONTENT_Y + ANGL_CONTENT_H + 4;
static constexpr int ANGL_BTN_H     = 44;

static void getBoresightWorld(float& wx, float& wy, float& wz) {
    float r=ImuState::qR, i=ImuState::qI, j=ImuState::qJ, k=ImuState::qK;
    wx = 2.0f*r*j + 2.0f*i*k;
    wy = 2.0f*j*k - 2.0f*r*i;
    wz = 1.0f - 2.0f*i*i - 2.0f*j*j;
    float mag=sqrtf(wx*wx+wy*wy+wz*wz);
    if (mag>0.001f) { wx/=mag; wy/=mag; wz/=mag; }
}

// Redraws only the value area (sprite → no flicker, no ghost pixels)
static void drawAngleValue(float deg, bool locked) {
    Display::spr.deleteSprite();
    Display::spr.createSprite(Display::SCREEN_W, ANGL_CONTENT_H);
    Display::spr.fillSprite(Display::Colors::BG);

    if (angleState == ANGLE_IDLE) {
        Display::spr.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
        Display::spr.drawCentreString("POINT AT TARGET A", 120, 60, 2);
        Display::spr.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
        Display::spr.drawCentreString("then tap LOCK A", 120, 90, 1);
    } else if (angleState == ANGLE_A_LOCKED) {
        Display::spr.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
        Display::spr.drawCentreString("A LOCKED — POINT AT B", 120, 8, 1);
        char buf[16]; sprintf(buf, "%.1f", deg);
        Display::spr.setTextColor(Display::Colors::AMBER, Display::Colors::BG);
        Display::spr.drawCentreString(buf, 120, 30, 7);
        Display::spr.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
        Display::spr.drawCentreString("deg", 120, 148, 2);
    } else { // DONE
        Display::spr.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
        Display::spr.drawCentreString("ANGLE BETWEEN A & B", 120, 8, 1);
        char buf[16]; sprintf(buf, "%.1f", deg);
        Display::spr.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
        Display::spr.drawCentreString(buf, 120, 30, 7);
        Display::spr.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
        Display::spr.drawCentreString("deg", 120, 148, 2);
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
    drawAngleValue(disp, false);
}

static void handleAngleTouch(int tx, int ty) {
    if (!inRect(tx, ty, 20, ANGL_BTN_Y, 200, ANGL_BTN_H)) return;

    if (angleState == ANGLE_IDLE) {
        getBoresightWorld(vecAx, vecAy, vecAz);
        angleState    = ANGLE_A_LOCKED;
        prevLiveAngle = -9999.0f;
        drawAngleValue(0.0f, false);
        drawAngleBtn();
    } else if (angleState == ANGLE_A_LOCKED) {
        getBoresightWorld(vecBx, vecBy, vecBz);
        float dot  = constrain(vecAx*vecBx + vecAy*vecBy + vecAz*vecBz, -1.0f, 1.0f);
        lockedAngle = acosf(dot) * 180.0f / 3.14159f;
        angleState  = ANGLE_DONE;
        drawAngleValue(lockedAngle, true);
        drawAngleBtn();
    } else {
        drawAngleTab();
    }
}

// ── DIST TAB — center 2×2 distance meter ─────────────────────────────────────
// Zones 27,28,35,36 are the inner 2×2 of the 8×8 grid (rows 3-4, cols 3-4).

static constexpr int DIST_CENTER_ZONES[4] = { 27, 28, 35, 36 };
static constexpr int DIST_BTN_X = 20, DIST_BTN_Y = 220, DIST_BTN_W = 200, DIST_BTN_H = 44;

enum DistState { DIST_LIVE, DIST_LOCKED };
static DistState distState = DIST_LIVE;
static float     distLocked = 0.0f;
static float     distPrevLive = -9999.0f;

// Average the 4 center zones; returns 0 if all invalid
static float centerDist() {
    float sum = 0.0f; int valid = 0;
    for (int z : DIST_CENTER_ZONES) {
        float d = TofState::distances[z];
        if (d > 10.0f) { sum += d; valid++; }
    }
    return valid > 0 ? sum / valid : 0.0f;
}

static void drawDistValue(float d, bool locked) {
    // Clear value area
    Display::tft.fillRect(0, CONTENT_Y + 30, Display::SCREEN_W, 170, Display::Colors::BG);

    uint16_t col = locked ? Display::Colors::AMBER : Display::Colors::GREEN;

    if (d <= 0.0f) {
        Display::tft.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
        Display::tft.drawCentreString("NO READING", 120, CONTENT_Y + 100, 2);
        return;
    }

    char mainBuf[16], unitBuf[8];
    if (d < 1000.0f) {
        sprintf(mainBuf, "%.0f", d);
        strcpy(unitBuf, "mm");
    } else {
        sprintf(mainBuf, "%.2f", d / 1000.0f);
        strcpy(unitBuf, "m");
    }

    Display::tft.setTextColor(col, Display::Colors::BG);
    Display::tft.drawCentreString(mainBuf, 120, CONTENT_Y + 50, 7);
    Display::tft.setTextColor(locked ? Display::Colors::AMBER : Display::Colors::GREEN_DIM,
                               Display::Colors::BG);
    Display::tft.drawCentreString(unitBuf, 120, CONTENT_Y + 150, 4);

    // 4 individual zone readings below the unit
    Display::tft.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
    for (int i = 0; i < 4; i++) {
        float zd = TofState::distances[DIST_CENTER_ZONES[i]];
        char zbuf[12];
        if (zd <= 0.0f) strcpy(zbuf, "---");
        else if (zd < 1000.0f) sprintf(zbuf, "%.0fmm", zd);
        else sprintf(zbuf, "%.2fm", zd / 1000.0f);
        int zx = (i % 2 == 0) ? 60 : 180;
        int zy = CONTENT_Y + 190 + (i / 2) * 16;
        Display::tft.drawCentreString(zbuf, zx, zy, 1);
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

    Display::tft.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
    Display::tft.drawCentreString("CENTER 2x2 DISTANCE", 120, CONTENT_Y + 10, 1);

    drawDistBtn(false);
    tofUpdate();
    drawDistValue(centerDist(), false);
}

static void tickDistTab() {
    if (distState == DIST_LOCKED) return;
    if (!tofUpdate()) return;
    float d = centerDist();
    if (fabsf(d - distPrevLive) < 5.0f) return;
    distPrevLive = d;
    drawDistValue(d, false);
}

static void handleDistTouch(int tx, int ty) {
    if (!inRect(tx, ty, DIST_BTN_X, DIST_BTN_Y, DIST_BTN_W, DIST_BTN_H)) return;
    if (distState == DIST_LIVE) {
        tofUpdate();
        distLocked = centerDist();
        distState  = DIST_LOCKED;
        drawDistValue(distLocked, true);
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
