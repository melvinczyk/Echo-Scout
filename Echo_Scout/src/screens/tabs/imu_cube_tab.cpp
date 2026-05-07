#include "tabs/imu_tabs.h"
#include "screens/imu_screen.h"
using namespace ImuTabs;

static const float cubeVerts[8][3] = {
    {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
    {-1,-1,1}, {1,-1,1}, {1,1,1}, {-1,1,1},
};
static const uint8_t cubeEdges[12][2] = {
    {0,1},{1,2},{2,3},{3,0},
    {4,5},{5,6},{6,7},{7,4},
    {0,4},{1,5},{2,6},{3,7},
};

static bool cubeDrawn = false;
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

namespace CubeTab {

void drawTab() {
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
void tick() {
    if (!imuUpdate()) return;
    eraseCube();
    drawCubeEdges(Display::Colors::GREEN);
    cubeDrawn = true;
    float roll, pitch, yaw; toEuler(roll, pitch, yaw);
    updateEulerRow(roll, pitch, yaw);
}

} // namespace CubeTab
