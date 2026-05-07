#include <math.h>
#include "imu_screen.h"
#include "imu.h"
#include "display.h"
#include "touch.h"
#include "device_state.h"

// ---- Layout ----
static constexpr int CONTENT_Y = Display::HEADER_H;            // 28
static constexpr int TAB_Y     = 280;
static constexpr int TAB_H     = Display::SCREEN_H - TAB_Y;    // 40
static constexpr int CONTENT_H = TAB_Y - CONTENT_Y;            // 252
static constexpr int TAB_COUNT = 3;
static constexpr int TAB_W     = Display::SCREEN_W / TAB_COUNT; // 80

enum ImuTab { TAB_CUBE=0, TAB_PLUMB=1, TAB_SCOPE=2 };
static ImuTab activeTab = TAB_CUBE;
static const char* tabLabels[TAB_COUNT] = {"CUBE","PLMB","SCPE"};

// ---- Shared Euler helper ----
static void toEuler(float& roll, float& pitch, float& yaw) {
    float qi=ImuState::qI, qj=ImuState::qJ, qk=ImuState::qK, qr=ImuState::qR;
    roll  = atan2f(2.0f*(qr*qi+qj*qk), 1.0f-2.0f*(qi*qi+qj*qj)) * 180.0f/PI;
    float sp = 2.0f*(qr*qj-qk*qi);
    pitch = (fabsf(sp)>=1.0f) ? copysignf(90.0f,sp) : asinf(sp)*180.0f/PI;
    yaw   = atan2f(2.0f*(qr*qk+qi*qj), 1.0f-2.0f*(qj*qj+qk*qk)) * 180.0f/PI;
}

// ==================== TAB BAR ====================
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

// ==================== CUBE TAB ====================
static const float cubeVerts[8][3] = {
    {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
    {-1,-1,1}, {1,-1,1}, {1,1,1}, {-1,1,1},
};
static const uint8_t cubeEdges[12][2] = {
    {0,1},{1,2},{2,3},{3,0},
    {4,5},{5,6},{6,7},{7,4},
    {0,4},{1,5},{2,6},{3,7},
};

static bool  cubeDrawn = false;
static float prevRoll=-9999, prevPitch=-9999, prevYaw=-9999;

static void rotatePoint(float x,float y,float z,float& ox,float& oy,float& oz) {
    float r=ImuState::qR, i=ImuState::qI, j=ImuState::qJ, k=ImuState::qK;
    float tx=2.0f*(j*z-k*y), ty=2.0f*(k*x-i*z), tz=2.0f*(i*y-j*x);
    ox = x+r*tx+(j*tz-k*ty);
    oy = y+r*ty+(k*tx-i*tz);
    oz = z+r*tz+(i*ty-j*tx);
}
static void project(float x,float y,float z,int& sx,int& sy) {
    float s = ImuScreen::PROJ_DIST/(ImuScreen::PROJ_DIST+z*ImuScreen::CUBE_SIZE);
    sx = ImuScreen::CUBE_CX+(int)(x*ImuScreen::CUBE_SIZE*s);
    sy = ImuScreen::CUBE_CY+(int)(-y*ImuScreen::CUBE_SIZE*s);
    sx = constrain(sx, 2, Display::SCREEN_W-2);
    sy = constrain(sy, CONTENT_Y+2, ImuScreen::CUBE_YMAX);
}
static void drawThickLine(int x0,int y0,int x1,int y1,uint16_t col) {
    Display::tft.drawLine(x0,y0,x1,y1,col);
    int dx=x1-x0, dy=y1-y0;
    if (abs(dx)>=abs(dy)) {
        Display::tft.drawLine(x0,y0-1,x1,y1-1,col);
        Display::tft.drawLine(x0,y0+1,x1,y1+1,col);
    } else {
        Display::tft.drawLine(x0-1,y0,x1-1,y1,col);
        Display::tft.drawLine(x0+1,y0,x1+1,y1,col);
    }
}
static void drawCubeEdges(uint16_t col) {
    int sx[8], sy[8]; float rz[8];
    for (int v=0;v<8;v++) {
        float rx,ry;
        rotatePoint(cubeVerts[v][0],cubeVerts[v][1],cubeVerts[v][2],rx,ry,rz[v]);
        project(rx,ry,rz[v],sx[v],sy[v]);
    }
    for (int e=0;e<12;e++) {
        Display::tft.drawLine(sx[cubeEdges[e][0]],sy[cubeEdges[e][0]],
                              sx[cubeEdges[e][1]],sy[cubeEdges[e][1]], col);
    }
    float botZ=(rz[0]+rz[1]+rz[5]+rz[4])*0.25f;
    float topZ=(rz[3]+rz[2]+rz[6]+rz[7])*0.25f;
    uint16_t xCol=(botZ<=topZ)?0xA000:Display::Colors::RED;
    drawThickLine(sx[0],sy[0],sx[5],sy[5],xCol);
    drawThickLine(sx[1],sy[1],sx[4],sy[4],xCol);
}
static void eraseCube() {
    if (!cubeDrawn) return;
    Display::tft.fillRect(2, CONTENT_Y+2, Display::SCREEN_W-4,
                          ImuScreen::CUBE_YMAX-CONTENT_Y-2, Display::Colors::BG);
}
static void updateEulerRow(float roll, float pitch, float yaw) {
    bool changed = fabsf(roll-prevRoll)>0.5f || fabsf(pitch-prevPitch)>0.5f || fabsf(yaw-prevYaw)>0.5f;
    if (!changed) return;
    prevRoll=roll; prevPitch=pitch; prevYaw=yaw;
    Display::tft.fillRect(0, 260, Display::SCREEN_W, 18, Display::Colors::BG);
    char buf[12];
    sprintf(buf, "R%+.0f", roll);
    Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
    Display::tft.drawCentreString(buf, 40, 261, 1);
    sprintf(buf, "P%+.0f", pitch);
    Display::tft.setTextColor(Display::Colors::AMBER, Display::Colors::BG);
    Display::tft.drawCentreString(buf, 120, 261, 1);
    sprintf(buf, "Y%+.0f", yaw);
    Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
    Display::tft.drawCentreString(buf, 200, 261, 1);
}
static void drawCubeTab() {
    Display::tft.fillRect(0, CONTENT_Y, Display::SCREEN_W, CONTENT_H, Display::Colors::BG);
    cubeDrawn = false; prevRoll=prevPitch=prevYaw=-9999;
    Display::tft.drawFastHLine(0, 250, Display::SCREEN_W, Display::Colors::GREEN_FAINT);
    Display::tft.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
    Display::tft.drawCentreString("ROLL", 40, 252, 1);
    Display::tft.drawCentreString("PITCH", 120, 252, 1);
    Display::tft.drawCentreString("YAW", 200, 252, 1);
    drawCubeEdges(Display::Colors::GREEN);
    cubeDrawn = true;
}
static void tickCubeTab() {
    if (!imuUpdate()) return;
    eraseCube();
    drawCubeEdges(Display::Colors::GREEN);
    cubeDrawn = true;
    float roll, pitch, yaw; toEuler(roll, pitch, yaw);
    updateEulerRow(roll, pitch, yaw);
}

// ==================== PLUMB TAB ====================
// Pivot at top-centre; bob hangs down showing left/right tilt.
// Visual indicator (top-right): side-view of device showing pivot at top edge.
static constexpr int PLUMB_AX  = 120, PLUMB_AY = 60, PLUMB_LEN = 150;
static float prevPbOx = -9999;

// Bounding box for erase
static void plumbBobBBox(float ox, int& x0, int& y0, int& w, int& h) {
    float chord = sqrtf((float)PLUMB_LEN*PLUMB_LEN - ox*ox);
    int bx = constrain(PLUMB_AX+(int)ox, 10, Display::SCREEN_W-10);
    int by = constrain(PLUMB_AY+(int)chord, PLUMB_AY+4, PLUMB_AY+PLUMB_LEN);
    int r = 10;
    // Box encompassing line from anchor to bob
    x0 = min(PLUMB_AX, bx) - r;
    y0 = min(PLUMB_AY, by) - r;
    int x1 = max(PLUMB_AX, bx) + r;
    int y1 = max(PLUMB_AY, by) + r;
    w = x1 - x0;
    h = y1 - y0;
}

static void drawPlumbBob(float ox) {
    float chord = sqrtf((float)PLUMB_LEN*PLUMB_LEN - ox*ox);
    int bx = constrain(PLUMB_AX+(int)ox, 10, Display::SCREEN_W-10);
    int by = constrain(PLUMB_AY+(int)chord, PLUMB_AY+4, PLUMB_AY+PLUMB_LEN);
    Display::tft.drawLine(PLUMB_AX, PLUMB_AY, bx, by, Display::Colors::GREEN);
    Display::tft.fillCircle(bx, by, 9, Display::Colors::GREEN);
    Display::tft.drawCircle(bx, by, 9, Display::Colors::GREEN_DIM);
}

// Orientation indicator: portrait device outline with an up-arrow showing
// which edge to hold up. Redrawn every tick so it survives the erase pass.
static void drawPlumbIndicator() {
    // Portrait device outline — right side of content, above crosshair
    const int IX = 188, IY = CONTENT_Y + 8;
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

static void drawPlumbTab() {
    Display::tft.fillRect(0, CONTENT_Y, Display::SCREEN_W, CONTENT_H, Display::Colors::BG);
    prevPbOx = -9999;

    // Crosshair at bob rest position
    int gx = PLUMB_AX, gy = PLUMB_AY + PLUMB_LEN;
    Display::tft.drawFastHLine(gx-18, gy, 36, Display::Colors::GREEN_FAINT);
    Display::tft.drawFastVLine(gx, gy-18, 36, Display::Colors::GREEN_FAINT);

    // Anchor pivot dot
    Display::tft.fillCircle(PLUMB_AX, PLUMB_AY, 4, Display::Colors::GREEN_DIM);

    // Axis indicator
    drawPlumbIndicator();

    // Bottom label
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("DEVIATION", 120, 262, 1);
}

static void tickPlumbTab() {
    if (!imuUpdate()) return;
    float roll, pitch, yaw; toEuler(roll, pitch, yaw);
    float ox = sinf(roll * PI/180.0f) * PLUMB_LEN;
    if (ox*ox > (float)(PLUMB_LEN*PLUMB_LEN)) ox = copysignf(PLUMB_LEN-1, ox);
    if (fabsf(ox - prevPbOx) < 1.0f) return;

    // Erase previous: fillRect over bounding box of old line+bob
    if (prevPbOx > -9998) {
        int ex, ey, ew, eh;
        plumbBobBBox(prevPbOx, ex, ey, ew, eh);
        Display::tft.fillRect(ex, ey, ew, eh, Display::Colors::BG);
        // Restore crosshair segments that may have been in the bbox
        int gx = PLUMB_AX, gy = PLUMB_AY + PLUMB_LEN;
        if (ey <= gy && gy <= ey+eh)
            Display::tft.drawFastHLine(max(gx-18, ex), gy,
                min(gx+18, ex+ew) - max(gx-18, ex), Display::Colors::GREEN_FAINT);
        if (ex <= gx && gx <= ex+ew)
            Display::tft.drawFastVLine(gx, max(gy-18, ey),
                min(gy+18, ey+eh) - max(gy-18, ey), Display::Colors::GREEN_FAINT);
    }

    prevPbOx = ox;
    drawPlumbBob(ox);
    drawPlumbIndicator();   // redraw after erase pass

    // Restore pivot dot
    Display::tft.fillCircle(PLUMB_AX, PLUMB_AY, 4, Display::Colors::GREEN_DIM);

    // Deviation readout
    float dev = fabsf(atan2f(ox, sqrtf((float)(PLUMB_LEN*PLUMB_LEN)-ox*ox))) * 180.0f/PI;
    char buf[12]; sprintf(buf, "%.1f deg", dev);
    Display::tft.fillRect(40, 268, 160, 14, Display::Colors::BG);
    Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
    Display::tft.drawCentreString(buf, 120, 268, 2);
}

// ==================== OSCILLOSCOPE TAB ====================
static constexpr int SCOPE_SAMPLES = 240;
static constexpr int SCOPE_LANE_H  = 78;
static constexpr int SCOPE_Y0      = CONTENT_Y + 18;   // leave room for legend
static constexpr uint32_t SCOPE_MS = 50;

static float    scopeBuf[3][SCOPE_SAMPLES] = {};
static int      scopeHead = 0;
static bool     scopeFull = false;
static uint32_t scopeLastMs = 0;
// Yaw unwrap state — prevents ±180° wrap from spiking the K lane
static float    scopeYawPrev   = 0.0f;
static float    scopeYawAccum  = 0.0f;

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
static void drawScopeTab() {
    Display::tft.fillRect(0, CONTENT_Y, Display::SCREEN_W, CONTENT_H, Display::Colors::BG);
    // Reset buffers and unwrap state
    memset(scopeBuf, 0, sizeof(scopeBuf));
    scopeHead = 0; scopeFull = false;
    float r, p, y; toEuler(r, p, y);
    scopeYawPrev  = y;
    scopeYawAccum = 0.0f;
    drawScopeIndicator();
    drawScopeGridLines();
}
static void tickScopeTab() {
    imuUpdate();
    if (millis() - scopeLastMs < SCOPE_MS) return;
    scopeLastMs = millis();
    float roll, pitch, yaw; toEuler(roll, pitch, yaw);

    // Unwrap yaw: detect ±360 wrap and accumulate continuously
    float dyaw = yaw - scopeYawPrev;
    if (dyaw >  180.0f) dyaw -= 360.0f;
    if (dyaw < -180.0f) dyaw += 360.0f;
    scopeYawAccum += dyaw;
    scopeYawPrev   = yaw;

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

// ==================== DISPATCH ====================
static void enterTab() {
    switch(activeTab) {
        case TAB_CUBE:  drawCubeTab();  break;
        case TAB_PLUMB: drawPlumbTab(); break;
        case TAB_SCOPE: drawScopeTab(); break;
    }
}

// ==================== PUBLIC ====================
void drawImuBase() {
    Display::tft.fillScreen(Display::Colors::BG);
    Display::tft.drawFastHLine(0, Display::HEADER_H-1, Display::SCREEN_W, Display::Colors::SEP);
    Display::tft.drawRoundRect(3, 3, 64, Display::HEADER_H-6, 3, Display::Colors::GREEN_DIM);
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("< MENU", 35, 7, 2);
    Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
    Display::tft.drawCentreString("IMU", 120, 9, 2);

    if (!ImuState::ready) {
        Display::tft.setTextColor(Display::Colors::RED, Display::Colors::BG);
        Display::tft.drawCentreString("BNO085 NOT FOUND", 120, 150, 2);
        Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
        Display::tft.drawCentreString("CHECK WIRING AND RESET", 120, 175, 1);
        return;
    }

    activeTab = TAB_CUBE;
    drawTabBar();
    enterTab();
}

void tickIMU() {
    if (!ImuState::ready) return;
    switch(activeTab) {
        case TAB_CUBE:  tickCubeTab();  break;
        case TAB_PLUMB: tickPlumbTab(); break;
        case TAB_SCOPE: tickScopeTab(); break;
    }
}

void handleImuTouch(int tx, int ty) {
    if (ty >= TAB_Y) {
        int t = constrain(tx / TAB_W, 0, TAB_COUNT-1);
        if (t != (int)activeTab) {
            activeTab = (ImuTab)t;
            drawTabBar();
            enterTab();
        }
        return;
    }
    // No touch handlers needed for remaining tabs
}
