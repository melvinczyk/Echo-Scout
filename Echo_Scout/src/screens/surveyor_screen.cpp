#include "screens/surveyor_screen.h"
#include "base/display.h"
#include "base/screen_manager.h"
#include "devices/device_state.h"
#include "devices/tof.h"
#include "devices/touch.h"
#include <math.h>

using namespace SurveyorScreen;

// ─── Quaternion rotate (for ToF surface fit + perp-dist only) ────────────────
static void qRotVec(float qr, float qi, float qj, float qk,
                    float vx, float vy, float vz,
                    float& rx, float& ry, float& rz)
{
    float tx = 2.0f*(qj*vz - qk*vy);
    float ty = 2.0f*(qk*vx - qi*vz);
    float tz = 2.0f*(qi*vy - qj*vx);
    rx = vx + qr*tx + qj*tz - qk*ty;
    ry = vy + qr*ty + qk*tx - qi*tz;
    rz = vz + qr*tz + qi*ty - qj*tx;
}

// ─── Surface fit ──────────────────────────────────────────────────────────────
static constexpr float ZONE_STEP = 5.625f * 3.14159265f / 180.0f;
static float s_pts[8][8][3];
static bool  s_valid[8][8];
static float s_normX, s_normY, s_normZ;
static float s_centerDist;
static bool  s_haveSurface = false;

static bool fitSurface() {
    for (int z = 0; z < 64; z++) {
        int r = z/8, c = z%8;
        float ah = (c-3.5f)*ZONE_STEP, av = (r-3.5f)*ZONE_STEP;
        float d = TofState::distances[z];
        if (d < 50.0f || d > 4500.0f) { s_valid[r][c]=false; continue; }
        s_pts[r][c][0]=sinf(ah)*d; s_pts[r][c][1]=sinf(av)*d;
        s_pts[r][c][2]=cosf(ah)*cosf(av)*d;
        s_valid[r][c]=true;
    }
    int n=0;
    for (int r=0;r<8;r++) for (int c=0;c<8;c++) if (s_valid[r][c]) n++;
    if (n < 4) return false;

    float nnx=0,nny=0,nnz=0; int nc=0;
    for (int r=1;r<7;r++) for (int c=1;c<7;c++) {
        if (!s_valid[r][c]||!s_valid[r][c-1]||!s_valid[r][c+1]||
            !s_valid[r-1][c]||!s_valid[r+1][c]) continue;
        float dux=s_pts[r][c+1][0]-s_pts[r][c-1][0],
              duy=s_pts[r][c+1][1]-s_pts[r][c-1][1],
              duz=s_pts[r][c+1][2]-s_pts[r][c-1][2];
        float dvx=s_pts[r+1][c][0]-s_pts[r-1][c][0],
              dvy=s_pts[r+1][c][1]-s_pts[r-1][c][1],
              dvz=s_pts[r+1][c][2]-s_pts[r-1][c][2];
        float lnx=duy*dvz-duz*dvy, lny=duz*dvx-dux*dvz, lnz=dux*dvy-duy*dvx;
        float l=sqrtf(lnx*lnx+lny*lny+lnz*lnz);
        if (l<1.0f) continue;
        nnx+=lnx/l; nny+=lny/l; nnz+=lnz/l; nc++;
    }
    if (nc==0) return false;
    s_normX=nnx/nc; s_normY=nny/nc; s_normZ=nnz/nc;
    float l=sqrtf(s_normX*s_normX+s_normY*s_normY+s_normZ*s_normZ);
    if (l>1e-6f){s_normX/=l;s_normY/=l;s_normZ/=l;}
    if (s_normZ>0){s_normX=-s_normX;s_normY=-s_normY;s_normZ=-s_normZ;}

    float sum=0; int cnt=0;
    int centers[4]={27,28,35,36};
    for (int i=0;i<4;i++){float d=TofState::distances[centers[i]];if(d>50&&d<4500){sum+=d;cnt++;}}
    s_centerDist = cnt>0 ? sum/cnt : 0.0f;
    return s_centerDist > 50.0f;
}

static float perpDist() {
    float bwx,bwy,bwz;
    qRotVec(ImuState::qR,ImuState::qI,ImuState::qJ,ImuState::qK,0,0,1,bwx,bwy,bwz);
    float wnx,wny,wnz;
    qRotVec(ImuState::qR,ImuState::qI,ImuState::qJ,ImuState::qK,
            s_normX,s_normY,s_normZ,wnx,wny,wnz);
    return s_centerDist * fabsf(bwx*wnx+bwy*wny+bwz*wnz);
}

// ─── Captured surfaces ────────────────────────────────────────────────────────
static float s_surfDist[S_COUNT] = {};
static bool  s_surfValid[S_COUNT] = {};

static float s_liveDist = 0.0f;
static float s_liveTilt = 0.0f;
static bool  s_liveOk   = false;
static int   s_selected = -1;

// ─── View rotation state (finger drag) ───────────────────────────────────────
static float s_viewYaw   =  0.55f;   // radians — nice starting angle
static float s_viewPitch = -0.40f;   // slightly above horizon
static int   s_dragStartX = 0;
static int   s_dragStartY = 0;
static float s_dragBaseYaw   = 0.0f;
static float s_dragBasePitch = 0.0f;
static bool  s_dragInViz = false;    // drag started in viz area

static void applyViewRot(float x, float y, float z,
                         float& rx, float& ry, float& rz)
{
    // Yaw around Y axis
    float cy=cosf(s_viewYaw), sy=sinf(s_viewYaw);
    float x1 = x*cy + z*sy;
    float z1 = -x*sy + z*cy;
    float y1 = y;
    // Pitch around X axis
    float cp=cosf(s_viewPitch), sp=sinf(s_viewPitch);
    rx = x1;
    ry = y1*cp - z1*sp;
    rz = y1*sp + z1*cp;
}

// ─── Projection ───────────────────────────────────────────────────────────────
static constexpr float PROJ_DIST = 280.0f;
static constexpr float PROJ_SCALE = 55.0f;

static void projectRoomPt(float x, float y, float z, float normScale,
                           int& sx, int& sy)
{
    float rx,ry,rz;
    applyViewRot(x*normScale, y*normScale, z*normScale, rx,ry,rz);
    float s = PROJ_DIST / (PROJ_DIST + rz*PROJ_SCALE);
    sx = VIZ_CX + (int)(rx*PROJ_SCALE*s);
    sy = VIZ_CY - (int)(ry*PROJ_SCALE*s);   // Y+ up
    sx = constrain(sx, 2, Display::SCREEN_W-2);
    sy = constrain(sy, VIZ_Y+2, 318);
}

// ─── Button layout ────────────────────────────────────────────────────────────
static const char*    SURF_LABELS[S_COUNT] = {"CEILING","FLOOR","LEFT","RIGHT","FRONT","BACK"};
static const uint16_t SURF_COLS[S_COUNT]   = {
    0x07FF,  // cyan   - ceiling
    0x8410,  // grey   - floor
    0xFD20,  // orange - left
    0x001F,  // blue   - right
    0x07E0,  // green  - front
    0xF800,  // red    - back
};

static void getBtnRect(int s, int& x, int& y, int& w, int& h) {
    w = 119; h = BTN_ROW_H - 2;
    x = (s%2)*121 + 1;
    y = BTN_Y + (s/2)*BTN_ROW_H + 1;
}

// ─── Draw 3D box ──────────────────────────────────────────────────────────────
static void drawBox3D() {
    Display::tft.fillRect(0, VIZ_Y, Display::SCREEN_W, VIZ_H, Display::Colors::BG);

    bool anyValid = false;
    for (int i=0;i<S_COUNT;i++) if(s_surfValid[i]){anyValid=true;break;}
    if (!anyValid) {
        Display::tft.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
        Display::tft.drawCentreString("SELECT A SURFACE,", VIZ_CX, VIZ_Y+VIZ_H/2-14, 1);
        Display::tft.drawCentreString("AIM AT IT, TAP CAPTURE", VIZ_CX, VIZ_Y+VIZ_H/2, 1);
        return;
    }

    // Room extents centred on user (Y+ = up, gravity = -Y)
    float dL  = s_surfValid[S_LEFT]  ? s_surfDist[S_LEFT]  : 2000.0f;
    float dR  = s_surfValid[S_RIGHT] ? s_surfDist[S_RIGHT] : 2000.0f;
    float dFl = s_surfValid[S_FLOOR] ? s_surfDist[S_FLOOR] : 300.0f;
    float dCe = s_surfValid[S_CEIL]  ? s_surfDist[S_CEIL]  : 2200.0f;
    float dF  = s_surfValid[S_FRONT] ? s_surfDist[S_FRONT] : 2000.0f;
    float dB  = s_surfValid[S_BACK]  ? s_surfDist[S_BACK]  : 2000.0f;

    float maxDim = max(max(dL+dR, dFl+dCe), dF+dB);
    float sc = (maxDim > 0.0f) ? 1.0f/maxDim : 1.0f/4000.0f;

    // 8 corners
    float vx[8]={-dL,-dL,-dL,-dL, dR, dR, dR, dR};
    float vy[8]={-dFl,-dFl,dCe,dCe,-dFl,-dFl,dCe,dCe};
    float vz[8]={-dF,  dB,-dF, dB, -dF,  dB,-dF, dB};

    int sx[8],sy[8];
    for (int i=0;i<8;i++) projectRoomPt(vx[i],vy[i],vz[i],sc,sx[i],sy[i]);

    // Edges
    static const uint8_t edges[12][2]={
        {0,1},{2,3},{4,5},{6,7},
        {0,2},{1,3},{4,6},{5,7},
        {0,4},{1,5},{2,6},{3,7},
    };
    for (auto& e : edges)
        Display::tft.drawLine(sx[e[0]],sy[e[0]],sx[e[1]],sy[e[1]],
                              Display::Colors::GREEN_DIM);

    // Floor face brighter
    int fi[4]={0,1,5,4};
    for(int i=0;i<4;i++)
        Display::tft.drawLine(sx[fi[i]],sy[fi[i]],sx[fi[(i+1)%4]],sy[fi[(i+1)%4]],
                              Display::Colors::GREEN_FAINT);

    // User dot at origin
    int upx,upy;
    projectRoomPt(0,0,0,sc,upx,upy);
    Display::tft.fillCircle(upx,upy,4,Display::Colors::GREEN);
    Display::tft.drawCircle(upx,upy,6,Display::Colors::GREEN_DIM);

    // Dimension labels
    int ly=VIZ_Y+4;
    char buf[20];
    if (s_surfValid[S_LEFT] && s_surfValid[S_RIGHT]) {
        sprintf(buf,"W:%.2fm",(dL+dR)/1000.0f);
        Display::tft.setTextColor(SURF_COLS[S_LEFT],Display::Colors::BG);
        Display::tft.drawString(buf,4,ly,1); ly+=10;
    }
    if (s_surfValid[S_FRONT] && s_surfValid[S_BACK]) {
        sprintf(buf,"D:%.2fm",(dF+dB)/1000.0f);
        Display::tft.setTextColor(SURF_COLS[S_FRONT],Display::Colors::BG);
        Display::tft.drawString(buf,4,ly,1); ly+=10;
    }
    if (s_surfValid[S_CEIL] && s_surfValid[S_FLOOR]) {
        sprintf(buf,"H:%.2fm",(dFl+dCe)/1000.0f);
        Display::tft.setTextColor(SURF_COLS[S_CEIL],Display::Colors::BG);
        Display::tft.drawString(buf,4,ly,1);
    }
}

// ─── UI drawing ───────────────────────────────────────────────────────────────
static void drawInfoBar() {
    Display::tft.fillRect(0, INFO_Y, Display::SCREEN_W, INFO_H, Display::Colors::BG);
    Display::tft.drawFastHLine(0, INFO_Y+INFO_H-1, Display::SCREEN_W, Display::Colors::SEP);
    if (!s_liveOk) {
        Display::tft.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
        Display::tft.drawCentreString("POINT AT SURFACE", 120, INFO_Y+6, 1);
        return;
    }
    char buf[32];
    sprintf(buf,"DIST %.2fm", s_liveDist/1000.0f);
    Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
    Display::tft.drawString(buf, 6, INFO_Y+5, 2);
    sprintf(buf,"TILT %.1fdeg", s_liveTilt);
    Display::tft.setTextColor(Display::Colors::AMBER, Display::Colors::BG);
    Display::tft.drawString(buf, 145, INFO_Y+8, 1);
}

static void drawSurfBtn(int s) {
    int bx,by,bw,bh; getBtnRect(s,bx,by,bw,bh);
    bool cap=s_surfValid[s], sel=(s==s_selected);
    uint16_t frameCol = cap ? SURF_COLS[s] : (uint16_t)Display::Colors::GREEN_FAINT;
    uint16_t fillCol  = sel ? (uint16_t)Display::Colors::GREEN_DIM : (uint16_t)Display::Colors::BG;
    Display::tft.fillRect(bx,by,bw,bh,fillCol);
    Display::tft.drawRect(bx,by,bw,bh,frameCol);
    Display::tft.setTextColor(frameCol,fillCol);
    Display::tft.drawString(SURF_LABELS[s],bx+5,by+4,1);
    if (cap) {
        char buf[12]; sprintf(buf,"%.2fm",s_surfDist[s]/1000.0f);
        Display::tft.setTextColor(Display::Colors::GREEN,fillCol);
        Display::tft.drawString(buf,bx+5,by+13,1);
    } else {
        Display::tft.setTextColor(Display::Colors::GREEN_FAINT,fillCol);
        Display::tft.drawString("--",bx+5,by+13,1);
    }
}

static void drawAllBtns() {
    Display::tft.fillRect(0,BTN_Y,Display::SCREEN_W,BTN_H,Display::Colors::BG);
    Display::tft.drawFastVLine(120,BTN_Y,BTN_H,Display::Colors::SEP);
    for (int r=0;r<=BTN_ROWS;r++)
        Display::tft.drawFastHLine(0,BTN_Y+r*BTN_ROW_H,Display::SCREEN_W,Display::Colors::SEP);
    for (int s=0;s<S_COUNT;s++) drawSurfBtn(s);
}

static void drawClearBar() {
    int y=CLR_Y;
    Display::tft.fillRect(0,y,Display::SCREEN_W,CLR_H,Display::Colors::BG);
    Display::tft.drawFastHLine(0,y+CLR_H-1,Display::SCREEN_W,Display::Colors::SEP);
    uint16_t capCol=(s_liveOk&&s_selected>=0)?Display::Colors::GREEN:Display::Colors::GREEN_FAINT;
    Display::tft.drawRect(4,y+3,140,CLR_H-6,capCol);
    Display::tft.setTextColor(capCol,Display::Colors::BG);
    Display::tft.drawCentreString("CAPTURE",74,y+5,1);
    Display::tft.drawRect(152,y+3,84,CLR_H-6,Display::Colors::RED);
    Display::tft.setTextColor(Display::Colors::RED,Display::Colors::BG);
    Display::tft.drawCentreString("CLEAR ALL",194,y+5,1);
}

// ─── Public API ───────────────────────────────────────────────────────────────
void drawSurveyorBase() {
    Display::tft.fillScreen(Display::Colors::BG);
    Display::drawHeader("ROOM SURVEYOR", false);
    drawInfoBar();
    drawAllBtns();
    drawClearBar();
    drawBox3D();
}

void tickSurveyor() {
    if (!tofUpdate()) return;
    s_haveSurface = fitSurface();
    if (s_haveSurface) {
        float pd = perpDist();
        float bwx,bwy,bwz;
        qRotVec(ImuState::qR,ImuState::qI,ImuState::qJ,ImuState::qK,0,0,1,bwx,bwy,bwz);
        float wnx,wny,wnz;
        qRotVec(ImuState::qR,ImuState::qI,ImuState::qJ,ImuState::qK,
                s_normX,s_normY,s_normZ,wnx,wny,wnz);
        float tilt=acosf(constrain(fabsf(bwx*wnx+bwy*wny+bwz*wnz),0.0f,1.0f))*180.0f/3.14159265f;
        s_liveDist=pd; s_liveTilt=tilt;
        s_liveOk=(pd>50.0f && pd<10000.0f);
    } else {
        s_liveOk=false;
    }
    drawInfoBar();
    drawClearBar();
}

namespace SurveyorScreen {
    bool dragWasInViz() { return s_dragInViz; }
}

// Called once when finger first touches screen
void handleSurveyorDragBegin(int tx, int ty) {
    s_dragStartX   = tx;
    s_dragStartY   = ty;
    s_dragBaseYaw  = s_viewYaw;
    s_dragBasePitch= s_viewPitch;
    s_dragInViz    = (ty >= VIZ_Y);
}

// Called every frame while finger is held
void handleSurveyorDrag(int tx, int ty) {
    if (!s_dragInViz) return;
    constexpr float SENS = 0.007f;   // radians per pixel
    s_viewYaw   = s_dragBaseYaw   + (tx - s_dragStartX) * SENS;
    s_viewPitch = s_dragBasePitch - (ty - s_dragStartY) * SENS;
    s_viewPitch = constrain(s_viewPitch, -1.4f, 1.4f);
    drawBox3D();
}

// Called on tap release (no significant drag) — handles button taps
void handleSurveyorTouch(int tx, int ty) {
    // Surface select buttons
    if (ty >= BTN_Y && ty < BTN_Y+BTN_H) {
        for (int s=0;s<S_COUNT;s++) {
            int bx,by,bw,bh; getBtnRect(s,bx,by,bw,bh);
            if (inRect(tx,ty,bx,by,bw,bh)) {
                s_selected=(s_selected==s)?-1:s;
                drawAllBtns(); drawClearBar(); return;
            }
        }
    }
    // Capture
    if (inRect(tx,ty,4,CLR_Y+3,140,CLR_H-6)) {
        if (!s_liveOk||s_selected<0) return;
        s_surfDist[s_selected]=s_liveDist;
        s_surfValid[s_selected]=true;
        drawSurfBtn(s_selected); drawBox3D(); return;
    }
    // Clear all
    if (inRect(tx,ty,152,CLR_Y+3,84,CLR_H-6)) {
        for (int i=0;i<S_COUNT;i++) s_surfValid[i]=false;
        s_selected=-1;
        drawAllBtns(); drawClearBar(); drawBox3D(); return;
    }
}
