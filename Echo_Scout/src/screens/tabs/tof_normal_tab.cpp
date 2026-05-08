#include "tabs/tof_tabs.h"
#include <math.h>

static constexpr float ZONE_STEP = 5.625f * 3.14159265f / 180.0f;

// Coordinate system matches 3D map: X=right, Y=down, Z=forward (boresight).
// Tilt is measured from boresight (+Z). Flat wall straight ahead = 0°.
static float zoneDir[64][3];
static float pts[8][8][3];
static bool  valid[8][8];
static float centX, centY, centZ;
static float normX, normY, normZ;
static bool  haveSurface = false;

static void vcross(float ax, float ay, float az, float bx, float by, float bz,
                   float& rx, float& ry, float& rz) {
    rx = ay*bz - az*by; ry = az*bx - ax*bz; rz = ax*by - ay*bx;
}

// Oblique projection: X=right, Y=down, Z=forward (shifts up-right on screen).
static void proj(float x, float y, float z, int& sx, int& sy) {
    float s = NormalTab::SCALE / 500.0f;
    sx = NormalTab::VIZ_CX + (int)((x + z * 0.35f) * s);
    sy = NormalTab::VIZ_CY + (int)((y - z * 0.18f) * s);
}

static bool fitSurface() {
    float sx = 0, sy = 0, sz = 0; int n = 0;
    for (int r = 0; r < 8; r++) for (int c = 0; c < 8; c++) {
        if (!valid[r][c]) continue;
        sx += pts[r][c][0]; sy += pts[r][c][1]; sz += pts[r][c][2]; n++;
    }
    if (n < 4) return false;
    centX = sx/n; centY = sy/n; centZ = sz/n;

    float nnx = 0, nny = 0, nnz = 0; int nc = 0;
    for (int r = 1; r < 7; r++) for (int c = 1; c < 7; c++) {
        if (!valid[r][c] || !valid[r][c-1] || !valid[r][c+1] ||
            !valid[r-1][c] || !valid[r+1][c]) continue;
        // du = rightward, dv = downward (row increases downward)
        float dux=pts[r][c+1][0]-pts[r][c-1][0], duy=pts[r][c+1][1]-pts[r][c-1][1], duz=pts[r][c+1][2]-pts[r][c-1][2];
        float dvx=pts[r+1][c][0]-pts[r-1][c][0], dvy=pts[r+1][c][1]-pts[r-1][c][1], dvz=pts[r+1][c][2]-pts[r-1][c][2];
        float lnx, lny, lnz;
        vcross(dux, duy, duz, dvx, dvy, dvz, lnx, lny, lnz);
        float l = sqrtf(lnx*lnx + lny*lny + lnz*lnz);
        if (l < 1.0f) continue;
        nnx += lnx/l; nny += lny/l; nnz += lnz/l; nc++;
    }
    if (nc == 0) return false;
    normX = nnx/nc; normY = nny/nc; normZ = nnz/nc;
    float l = sqrtf(normX*normX + normY*normY + normZ*normZ);
    if (l > 1e-6f) { normX /= l; normY /= l; normZ /= l; }
    // Flip so normal points toward sensor (normZ < 0, since boresight is +Z).
    if (normZ > 0) { normX = -normX; normY = -normY; normZ = -normZ; }
    return true;
}

static void drawAxes() {
    int ox, oy, ex, ey;
    proj(0, 0, 0, ox, oy);
    Display::tft.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
    proj(300,0,0,ex,ey); Display::tft.drawLine(ox,oy,ex,ey,Display::Colors::GREEN_FAINT); Display::tft.drawString("X",ex+2,ey-4,1);
    proj(0,300,0,ex,ey); Display::tft.drawLine(ox,oy,ex,ey,Display::Colors::GREEN_FAINT); Display::tft.drawString("Y",ex+2,ey-4,1);
    proj(0,0,300,ex,ey); Display::tft.drawLine(ox,oy,ex,ey,Display::Colors::GREEN_FAINT); Display::tft.drawString("Z",ex+2,ey-4,1);
}

static void draw() {
    Display::spr.deleteSprite();
    Display::spr.createSprite(Display::SCREEN_W, NormalTab::NUM_H);
    Display::spr.fillSprite(Display::Colors::BG);
    if (!haveSurface) {
        Display::spr.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
        Display::spr.drawCentreString("NO SURFACE", 120, 18, 2);
    } else {
        // tilt = angle from boresight (+Z). Flat wall ahead = 0°.
        float tilt = acosf(constrain(-normZ, -1.0f, 1.0f)) * 180.0f / 3.14159265f;
        // az = horizontal direction the surface normal tilts toward (0° = straight, ±90° = left/right).
        float az   = atan2f(normX, -normZ) * 180.0f / 3.14159265f;
        char buf[32];
        sprintf(buf, "TILT  %.1f deg", tilt);
        Display::spr.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
        Display::spr.drawCentreString(buf, 120, 6, 2);
        sprintf(buf, "AZ  %.1f deg", az);
        Display::spr.setTextColor(Display::Colors::AMBER, Display::Colors::BG);
        Display::spr.drawCentreString(buf, 120, 28, 2);
    }
    Display::spr.pushSprite(0, TofTabs::CONTENT_Y);
    Display::spr.deleteSprite();

    Display::tft.fillRect(0, NormalTab::VIZ_Y, Display::SCREEN_W, NormalTab::VIZ_H, Display::Colors::BG);
    drawAxes();
    if (!haveSurface) return;

    for (int r = 0; r < 8; r++) for (int c = 0; c < 8; c++) {
        if (!valid[r][c]) continue;
        int sx, sy;
        proj(pts[r][c][0]-centX, pts[r][c][1]-centY, pts[r][c][2]-centZ, sx, sy);
        if (sy >= NormalTab::VIZ_Y && sy < NormalTab::VIZ_Y + NormalTab::VIZ_H)
            Display::tft.fillRect(sx-1, sy-1, 3, 3, Display::Colors::GREEN_DIM);
    }

    float ux, uy, uz, vx, vy, vz;
    vcross(normX, normY, normZ, 1, 0, 0, ux, uy, uz);
    if (sqrtf(ux*ux+uy*uy+uz*uz) < 0.01f) vcross(normX, normY, normZ, 0, 1, 0, ux, uy, uz);
    float ul = sqrtf(ux*ux+uy*uy+uz*uz); ux/=ul; uy/=ul; uz/=ul;
    vcross(normX, normY, normZ, ux, uy, uz, vx, vy, vz);
    float vl = sqrtf(vx*vx+vy*vy+vz*vz); vx/=vl; vy/=vl; vz/=vl;

    float R = NormalTab::PLANE_R;
    float corners[4][3] = {
        { R*(ux+vx), R*(uy+vy), R*(uz+vz) }, { R*(-ux+vx), R*(-uy+vy), R*(-uz+vz) },
        { R*(-ux-vx), R*(-uy-vy), R*(-uz-vz) }, { R*(ux-vx), R*(uy-vy), R*(uz-vz) },
    };
    int px[4], py[4];
    for (int i = 0; i < 4; i++) proj(corners[i][0], corners[i][1], corners[i][2], px[i], py[i]);
    for (int i = 0; i < 4; i++)
        Display::tft.drawLine(px[i], py[i], px[(i+1)%4], py[(i+1)%4], Display::Colors::AMBER);

    int ox, oy, ex, ey;
    proj(0, 0, 0, ox, oy);
    proj(normX*NormalTab::ARROW_L, normY*NormalTab::ARROW_L, normZ*NormalTab::ARROW_L, ex, ey);
    Display::tft.drawLine(ox, oy, ex, ey, Display::Colors::GREEN);
    Display::tft.fillCircle(ex, ey, 4, Display::Colors::GREEN);
}

namespace NormalTab {

void drawTab() {
    Display::tft.fillRect(0, TofTabs::CONTENT_Y, Display::SCREEN_W,
                          TofTabs::TAB_Y - TofTabs::CONTENT_Y, Display::Colors::BG);
    // Zone directions matching 3D map: X=right, Y=down, Z=forward.
    for (int z = 0; z < 64; z++) {
        int r = z/8, c = z%8;
        float ah = (c - 3.5f) * ZONE_STEP;
        float av = (r - 3.5f) * ZONE_STEP;
        zoneDir[z][0] = sinf(ah);
        zoneDir[z][1] = sinf(av);
        zoneDir[z][2] = cosf(ah) * cosf(av);
    }
    haveSurface = false;
    draw();
}

void tick() {
    if (!tofUpdate()) return;
    for (int z = 0; z < 64; z++) {
        int r = z/8, c = z%8;
        float d = TofState::distances[z];
        if (d < 50.0f || d > 4000.0f) { valid[r][c] = false; continue; }
        pts[r][c][0] = zoneDir[z][0] * d;
        pts[r][c][1] = zoneDir[z][1] * d;
        pts[r][c][2] = zoneDir[z][2] * d;
        valid[r][c]  = true;
    }
    haveSurface = fitSurface();
    draw();
}

}
