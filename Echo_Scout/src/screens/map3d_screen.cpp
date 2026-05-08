#include <math.h>
#include "screens/map3d_screen.h"
#include "devices/imu.h"
#include "devices/tof.h"
#include "base/display.h"
#include "devices/touch.h"
#include "devices/device_state.h"
#include "base/settings.h"
#include <esp_heap_caps.h>

// ── Layout ───────────────────────────────────────────────────────────────────
static constexpr int VIEW_Y  = Display::HEADER_H;
static constexpr int BTN_BAR = 48;
static constexpr int VIEW_H  = Display::SCREEN_H - VIEW_Y - BTN_BAR;
static constexpr int VIEW_W  = Display::SCREEN_W;
static constexpr int VIEW_CX = VIEW_W / 2;
static constexpr int VIEW_CY = VIEW_Y + VIEW_H / 2;
static constexpr float FOCAL = 200.0f;
static constexpr float NEAR  = 80.0f;

// ── Screen-space accumulation grid ───────────────────────────────────────────
// CELL=2 sizes the array at finest resolution (120×122).
// s_cellSize is set at render time from cfgMap3dCell() and drives actual spacing.
static constexpr int CELL   = 2;
static constexpr int GRID_W = VIEW_W / CELL;   // 120
static constexpr int GRID_H = VIEW_H / CELL;   // 122
// 2-byte cell: bits 0-14 = closest distance (0 = empty), bit 15 = live ToF flag.
// Colour is looked up from distColLUT at draw time — no RGB stored per cell.
// 120×122×2 = 29 KB vs 7-byte Cell × same grid = 102 KB.
struct Cell { uint16_t minDist; };
static Cell grid[GRID_W][GRID_H];

static int s_cellSize = 3;   // runtime dot spacing, updated each renderView

// ── Point cloud — PSRAM spatial hash map ─────────────────────────────────────
// Each voxel (bucket) hashes to a fixed slot.  Writing to the same region of
// space always lands in the same slot and overwrites it, so transient objects
// (hand in front of wall) get replaced when the scanner moves on — no explosion.
static constexpr int MAX_PTS    = 196608;
static constexpr int RENDER_MAX = 49152;

struct MapPoint { int16_t x, y, z; uint16_t dist; };
static MapPoint* pts  = nullptr;
static int       nPts = 0;   // unique filled slots (for display counter)

// Hash voxel coordinates to a slot index (fixed 25 mm buckets).
static inline uint32_t voxelSlot(int wx, int wy, int wz) {
    constexpr int V = 25;
    int32_t vx = wx / V, vy = wy / V, vz = wz / V;
    uint32_t h = (uint32_t)(vx * 2654435761u)
               ^ (uint32_t)(vy * 2246822519u)
               ^ (uint32_t)(vz * 3266489917u);
    return h % (uint32_t)MAX_PTS;
}

// ── Render-skip state ─────────────────────────────────────────────────────────
// Skip renderView() when orientation barely changed AND no new points added.
// Eliminates constant re-renders from hand tremor when the view is static.
static bool  s_newPoints = false;
static float s_lastQR=0, s_lastQI=0, s_lastQJ=0, s_lastQK=0;
static constexpr float RENDER_SKIP_DQ = 0.012f;  // ~0.7° total quaternion delta

// ── Constants ─────────────────────────────────────────────────────────────────
static constexpr float ZONE_STEP = 5.625f * 3.14159265f / 180.0f;
static constexpr int   BTN_Y  = Display::SCREEN_H - BTN_BAR + 7;
static constexpr int   BTN_BH = 32;
static constexpr int   CLR_X  = 8, CLR_W = 224;

// ── Math ──────────────────────────────────────────────────────────────────────
static inline void qRot(float qr, float qi, float qj, float qk,
                         float vx, float vy, float vz,
                         float& rx, float& ry, float& rz) {
    float tx = 2*(qj*vz - qk*vy);
    float ty = 2*(qk*vx - qi*vz);
    float tz = 2*(qi*vy - qj*vx);
    rx = vx + qr*tx + (qj*tz - qk*ty);
    ry = vy + qr*ty + (qk*tx - qi*tz);
    rz = vz + qr*tz + (qi*ty - qj*tx);
}

static bool projectFP(float wx, float wy, float wz, int& gx, int& gy) {
    float vx, vy, vz;
    qRot(ImuState::qR, -ImuState::qI, -ImuState::qJ, -ImuState::qK,
         wx, wy, wz, vx, vy, vz);
    float depth = vz;
    if (depth < NEAR) return false;
    int sx = VIEW_CX + (int)(FOCAL * vx / depth);
    int sy = VIEW_CY + (int)(FOCAL * vy / depth);
    if (sx < 0 || sx >= VIEW_W || sy < VIEW_Y || sy >= VIEW_Y + VIEW_H) return false;
    gx = sx / s_cellSize;
    gy = (sy - VIEW_Y) / s_cellSize;
    if (gx >= GRID_W) gx = GRID_W - 1;
    if (gy >= GRID_H) gy = GRID_H - 1;
    return true;
}

// distColLUT: precomputed iron colormap, indexed by dist_mm>>4 (16 mm steps).
// O(1) table lookup replaces per-point float interpolation every frame.
static uint16_t distColLUT[256];

static void buildDistColLUT() {
    struct Stop { float t; uint8_t r, g, b; };
    static const Stop stops[] = {
        {0.00f,   0,   0,   0},
        {0.25f,  60,   0, 100},
        {0.50f, 180,   0,  40},
        {0.65f, 240,  60,   0},
        {0.80f, 255, 160,   0},
        {0.92f, 255, 240,  60},
        {1.00f, 255, 255, 255},
    };
    for (int idx = 0; idx < 256; idx++) {
        float t = 1.0f - (idx * 16.0f * (1.0f / 4000.0f));
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        int i = 0;
        while (i < 5 && t > stops[i+1].t) i++;
        float f = (t - stops[i].t) / (stops[i+1].t - stops[i].t);
        uint8_t r = (uint8_t)(stops[i].r + f*(int)(stops[i+1].r - stops[i].r));
        uint8_t g = (uint8_t)(stops[i].g + f*(int)(stops[i+1].g - stops[i].g));
        uint8_t b = (uint8_t)(stops[i].b + f*(int)(stops[i+1].b - stops[i].b));
        distColLUT[idx] = (uint16_t)(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
    }
}

static inline uint16_t distCol(float d) {
    uint32_t idx = (uint32_t)d >> 4;
    return distColLUT[idx < 256 ? idx : 255];
}

static bool zoneWorld(int zone, float dist,
                      float qr, float qi, float qj, float qk,
                      float& wx, float& wy, float& wz) {
    if (dist < 50.0f || dist > (float)cfgMap3dMaxRange()) return false;
    int row = zone / 8, col = zone % 8;
    float ah = (col - 3.5f) * ZONE_STEP;
    float av = (row - 3.5f) * ZONE_STEP;
    float lx = sinf(ah), ly = sinf(av), lz = cosf(ah)*cosf(av);
    qRot(qr, qi, qj, qk, lx, ly, lz, wx, wy, wz);
    wx *= dist; wy *= dist; wz *= dist;
    return true;
}

// Closest distance wins (z-buffer). Bit 15 of minDist is the live-ToF flag;
// it is never touched here — callers set it separately after calling gridAccum.
static inline void gridAccum(int gx, int gy, uint16_t dist_mm) {
    Cell& c = grid[gx][gy];
    uint16_t cur = c.minDist & 0x7FFFu;
    if (cur == 0 || dist_mm < cur)
        c.minDist = (c.minDist & 0x8000u) | (dist_mm & 0x7FFFu);
}

// ── Capture ───────────────────────────────────────────────────────────────────
static void storePoint(float wx, float wy, float wz, float dist) {
    int16_t px = (int16_t)constrain((int)wx,-32767,32767);
    int16_t py = (int16_t)constrain((int)wy,-32767,32767);
    int16_t pz = (int16_t)constrain((int)wz,-32767,32767);
    uint32_t slot = voxelSlot(px, py, pz);
    if (pts[slot].dist == 0) nPts++;
    pts[slot] = { px, py, pz, (uint16_t)constrain((int)dist, 0, 65535) };
    s_newPoints = true;
}

static void captureScan() {
    if (!TofState::ready || !ImuState::ready || !pts) return;

    float qr=ImuState::qR, qi=ImuState::qI, qj=ImuState::qJ, qk=ImuState::qK;
    float d[8][8];
    for (int r=0; r<8; r++)
        for (int c=0; c<8; c++)
            d[r][c] = TofState::distances[r*8+c];

    float maxR = (float)cfgMap3dMaxRange();

    if (cfgMap3dBilinear()) {
        // True bilinear interpolation at 8× density.
        // Weights each of the 4 surrounding zone distances by (1-dt_r)*(1-dt_c) etc.
        // so the sampled distance varies smoothly across zone boundaries — no seams.
        constexpr float STEP = 1.0f / 8;
        for (float fr = 0.0f; fr < 8.0f; fr += STEP) {
            int r0 = constrain((int)fr, 0, 7);
            int r1 = min(r0 + 1, 7);
            float wr1 = fr - r0;          // weight toward r1
            float wr0 = 1.0f - wr1;
            for (float fc = 0.0f; fc < 8.0f; fc += STEP) {
                int c0 = constrain((int)fc, 0, 7);
                int c1 = min(c0 + 1, 7);
                float wc1 = fc - c0;      // weight toward c1
                float wc0 = 1.0f - wc1;
                float w00=wr0*wc0, w01=wr0*wc1, w10=wr1*wc0, w11=wr1*wc1;
                float dist = 0, wsum = 0;
                float nb[4] = {d[r0][c0], d[r0][c1], d[r1][c0], d[r1][c1]};
                float wb[4] = {w00, w01, w10, w11};
                for (int i = 0; i < 4; i++)
                    if (nb[i] >= 50.0f && nb[i] <= maxR) { dist += wb[i]*nb[i]; wsum += wb[i]; }
                if (wsum < 0.01f) continue;
                dist /= wsum;
                float ah=(fc-3.5f)*ZONE_STEP, av=(fr-3.5f)*ZONE_STEP;
                float lx=sinf(ah), ly=sinf(av), lz=cosf(ah)*cosf(av);
                float wx,wy,wz;
                qRot(qr,qi,qj,qk,lx,ly,lz,wx,wy,wz);
                storePoint(wx*dist, wy*dist, wz*dist, dist);
            }
        }
    } else {
        // Raw 64 zones — no upsampling.
        for (int z = 0; z < 64; z++) {
            int r = z/8, c = z%8;
            float dist = d[r][c];
            if (dist < 50.0f || dist > maxR) continue;
            float ah=(c-3.5f)*ZONE_STEP, av=(r-3.5f)*ZONE_STEP;
            float lx=sinf(ah), ly=sinf(av), lz=cosf(ah)*cosf(av);
            float wx,wy,wz;
            qRot(qr,qi,qj,qk,lx,ly,lz,wx,wy,wz);
            storePoint(wx*dist, wy*dist, wz*dist, dist);
        }
    }
}

// ── Rendering ────────────────────────────────────────────────────────────────
static void renderView() {
    s_cellSize = (int)cfgMap3dCell();
    memset(grid, 0, sizeof(grid));

    if (pts) {
        // Adaptive step: when few slots are filled show every point (step=1);
        // only sub-sample when the table is dense enough that RENDER_MAX would
        // be exceeded.  Always iterate the full MAX_PTS range so no filled slot
        // is silently skipped regardless of where it landed in the hash table.
        int step = max(1, nPts / RENDER_MAX);
        for (int i = 0; i < MAX_PTS; i += step) {
            if (pts[i].dist == 0) continue;
            int gx, gy;
            if (projectFP(pts[i].x, pts[i].y, pts[i].z, gx, gy))
                gridAccum(gx, gy, pts[i].dist);
        }
    }

    if (TofState::ready) {
        float qr=ImuState::qR, qi=ImuState::qI, qj=ImuState::qJ, qk=ImuState::qK;
        for (int z=0; z<64; z++) {
            float wx,wy,wz;
            if (!zoneWorld(z, TofState::distances[z], qr,qi,qj,qk, wx,wy,wz)) continue;
            int gx,gy;
            if (!projectFP(wx,wy,wz,gx,gy)) continue;
            uint16_t dist = (uint16_t)TofState::distances[z];
            gridAccum(gx, gy, dist);
            grid[gx][gy].minDist |= 0x8000u;  // mark as live ToF hit for larger dot
        }
    }

    Display::spr.deleteSprite();
    Display::spr.createSprite(VIEW_W, VIEW_H);
    Display::spr.fillSprite(Display::Colors::BG);

    for (int gx=0; gx<GRID_W; gx++) {
        for (int gy=0; gy<GRID_H; gy++) {
            Cell& c = grid[gx][gy];
            uint16_t raw = c.minDist & 0x7FFFu;
            if (raw == 0) continue;
            uint16_t col = distCol(raw);
            // Stored cloud = 2px; live ToF (bit 15) = 3×3 to highlight active scan.
            int sz = (c.minDist & 0x8000u) ? 3 : 2;
            int px = gx*s_cellSize + (s_cellSize-sz)/2;
            int py = gy*s_cellSize + (s_cellSize-sz)/2;
            if (sz==1) Display::spr.drawPixel(px,py,col);
            else       Display::spr.fillRect(px,py,sz,sz,col);
        }
    }

    int cx=VIEW_CX, cy=VIEW_CY-VIEW_Y;
    Display::spr.drawFastHLine(cx-12, cy,  9, Display::Colors::GREEN_DIM);
    Display::spr.drawFastHLine(cx+4,  cy,  9, Display::Colors::GREEN_DIM);
    Display::spr.drawFastVLine(cx, cy-12,  9, Display::Colors::GREEN_DIM);
    Display::spr.drawFastVLine(cx, cy+4,   9, Display::Colors::GREEN_DIM);

    char buf[24];
    if (nPts >= MAX_PTS)    sprintf(buf, "192k/192k");
    else if (nPts >= 1000)  sprintf(buf, "%dk pts",  nPts/1000);
    else                    sprintf(buf, "%d pts",   nPts);
    Display::spr.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
    Display::spr.drawRightString(buf, VIEW_W-4, 2, 1);

    Display::spr.pushSprite(0, VIEW_Y);
    Display::spr.deleteSprite();
}

static void drawButtons() {
    int by = Display::SCREEN_H - BTN_BAR;
    Display::tft.fillRect(0, by, VIEW_W, BTN_BAR, Display::Colors::BG);
    Display::tft.drawFastHLine(0, by, VIEW_W, Display::Colors::SEP);
    Display::tft.drawRect(CLR_X, BTN_Y, CLR_W, BTN_BH, Display::Colors::RED);
    Display::tft.setTextColor(Display::Colors::RED, Display::Colors::BG);
    Display::tft.drawCentreString("CLEAR", CLR_X+CLR_W/2, BTN_Y+9, 2);
}

// ── Public API ───────────────────────────────────────────────────────────────
void drawMap3dBase() {
    if (!pts)
        pts = (MapPoint*)heap_caps_malloc(MAX_PTS * sizeof(MapPoint), MALLOC_CAP_SPIRAM);
    if (pts) memset(pts, 0, MAX_PTS * sizeof(MapPoint));
    buildDistColLUT();
    nPts = 0;
    s_newPoints = false;
    s_lastQR = s_lastQI = s_lastQJ = s_lastQK = 0.0f;
    Display::tft.fillScreen(Display::Colors::BG);
    Display::drawHeader("3D MAP", false);
    drawButtons();
    renderView();
}

void tickMap3d() {
    if (!imuUpdate()) return;
    tofUpdate();
    s_newPoints = false;
    captureScan();

    // Skip render if orientation barely changed AND no new points captured.
    // Hand tremor alone won't trigger a full 49K-point re-render.
    float dq = fabsf(ImuState::qR - s_lastQR)
             + fabsf(ImuState::qI - s_lastQI)
             + fabsf(ImuState::qJ - s_lastQJ)
             + fabsf(ImuState::qK - s_lastQK);
    if (!s_newPoints && dq < RENDER_SKIP_DQ) return;

    s_lastQR = ImuState::qR; s_lastQI = ImuState::qI;
    s_lastQJ = ImuState::qJ; s_lastQK = ImuState::qK;
    renderView();
}

void handleMap3dTouch(int tx, int ty) {
    if (inRect(tx, ty, CLR_X, BTN_Y, CLR_W, BTN_BH)) {
        if (pts) memset(pts, 0, MAX_PTS * sizeof(MapPoint));
        nPts = 0;
        renderView();
    }
}
