#include <math.h>
#include <SPIFFS.h>
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
static constexpr int CELL   = 4;
static constexpr int GRID_W = VIEW_W / CELL;  // 60
static constexpr int GRID_H = VIEW_H / CELL;  // 61
struct Cell { uint8_t r, g, b, n; };
static Cell grid[GRID_W][GRID_H];

// ── Point cloud storage — PSRAM ───────────────────────────────────────────────
// One struct serves two modes:
//   VOXEL mode  (cfg.map3dVoxelIdx > 0): pts[] is a spatial hash map.
//               x/y/z = voxel-snapped centroid. hits = accumulation count.
//               Empty slot = hits==0.
//   RING mode   (cfg.map3dVoxelIdx == 0): pts[] is a plain ring buffer.
//               x/y/z = raw world coords. hits = 1 always.
//
// 8 bytes/pt × 262144 slots = 2 MB PSRAM.
static constexpr uint32_t HASH_SLOTS = 262144;   // 2^18 — must be power-of-2
static constexpr uint32_t HASH_MASK  = HASH_SLOTS - 1;
static constexpr uint32_t HASH_MAX   = HASH_SLOTS * 3 / 4;  // 75% fill limit
static constexpr uint32_t RING_RENDER = 49152;   // max pts rendered per frame in ring mode

struct MapPoint {
    int16_t x, y, z;  // world coords in mm (±32 m range, 1 mm resolution)
    uint8_t hits;      // 0 = empty slot; ≥1 = occupied
    uint8_t dist8;     // dist / 16  →  16 mm steps, covers 0–4080 mm
};  // 8 bytes

static MapPoint* pts   = nullptr;  // PSRAM
static uint32_t  nPts  = 0;        // occupied voxels (voxel) OR stored pts (ring)
static uint32_t  nRing = 0;        // ring-write cursor (ring mode only)
static uint8_t   prevVoxelIdx = 255;  // detect setting changes → clear map

// ── Zone FoV ─────────────────────────────────────────────────────────────────
static constexpr float ZONE_STEP  = 5.625f * 3.14159265f / 180.0f;
static constexpr float DISC_RATIO = 1.4f;

// ── Buttons ───────────────────────────────────────────────────────────────────
static constexpr int BTN_Y  = Display::SCREEN_H - BTN_BAR + 8;
static constexpr int BTN_BH = 30;
static constexpr int SAVE_X = 4,   SAVE_W = 70;
static constexpr int LOAD_X = 82,  LOAD_W = 70;
static constexpr int CLR_X  = 160, CLR_W  = 76;

static constexpr const char* MAP_FILE = "/map3d.bin";

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
    gx = sx / CELL;
    gy = (sy - VIEW_Y) / CELL;
    if (gx >= GRID_W) gx = GRID_W - 1;
    if (gy >= GRID_H) gy = GRID_H - 1;
    return true;
}

// Iron colormap  far=black → purple → red → orange → yellow → close=white
static uint16_t distCol(uint32_t dmm) {
    float t = 1.0f - (dmm * (1.0f / 4000.0f));
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
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
    static constexpr int N = 7;
    int i = 0;
    while (i < N - 2 && t > stops[i + 1].t) i++;
    float f = (t - stops[i].t) / (stops[i + 1].t - stops[i].t);
    uint8_t r = (uint8_t)(stops[i].r + f * (int)(stops[i+1].r - stops[i].r));
    uint8_t g = (uint8_t)(stops[i].g + f * (int)(stops[i+1].g - stops[i].g));
    uint8_t b = (uint8_t)(stops[i].b + f * (int)(stops[i+1].b - stops[i].b));
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static bool zoneWorld(int zone, float dist,
                      float qr, float qi, float qj, float qk,
                      float& wx, float& wy, float& wz) {
    uint16_t maxR = cfgMap3dMaxRange();
    if (dist < 50.0f || dist > (float)maxR) return false;
    int row = zone / 8, col = zone % 8;
    float ah = (col - 3.5f) * ZONE_STEP;
    float av = (row - 3.5f) * ZONE_STEP;
    float lx = sinf(ah), ly = sinf(av), lz = cosf(ah) * cosf(av);
    qRot(qr, qi, qj, qk, lx, ly, lz, wx, wy, wz);
    wx *= dist; wy *= dist; wz *= dist;
    return true;
}

static inline void gridAccum(int gx, int gy, uint16_t col565) {
    Cell& c = grid[gx][gy];
    uint8_t cr = (uint8_t)((col565 >> 11) << 3);
    uint8_t cg = (uint8_t)(((col565 >> 5) & 0x3F) << 2);
    uint8_t cb = (uint8_t)((col565 & 0x1F) << 3);
    if (c.n == 0) { c.r = cr; c.g = cg; c.b = cb; c.n = 1; }
    else {
        c.r = (uint8_t)((c.r + cr) >> 1);
        c.g = (uint8_t)((c.g + cg) >> 1);
        c.b = (uint8_t)((c.b + cb) >> 1);
        if (c.n < 255) c.n++;
    }
}

// ── Voxel hash map ────────────────────────────────────────────────────────────
static inline uint32_t voxelHash(int16_t vx, int16_t vy, int16_t vz) {
    uint32_t h = (uint32_t)(uint16_t)vx * 2654435761u
               ^ (uint32_t)(uint16_t)vy * 2246822519u
               ^ (uint32_t)(uint16_t)vz * 3266489917u;
    return h & HASH_MASK;
}

static void voxelInsert(int16_t vx, int16_t vy, int16_t vz, uint8_t d8) {
    if (!pts || nPts >= HASH_MAX) return;
    uint32_t h = voxelHash(vx, vy, vz);
    for (uint32_t k = 0; k < 128; k++) {
        MapPoint& p = pts[(h + k) & HASH_MASK];
        if (p.hits == 0) {
            p = { vx, vy, vz, 1, d8 };
            nPts++;
            return;
        }
        if (p.x == vx && p.y == vy && p.z == vz) {
            if (p.hits < 255) p.hits++;
            // update distance to latest reading (keeps color fresh)
            p.dist8 = d8;
            return;
        }
    }
    // probe limit hit — table very dense, drop silently
}

static void ringInsert(int16_t x, int16_t y, int16_t z, uint8_t d8) {
    if (!pts) return;
    uint32_t slot = nRing & HASH_MASK;
    pts[slot] = { x, y, z, 1, d8 };
    nRing++;
    nPts = (nRing < HASH_SLOTS) ? nRing : HASH_SLOTS;
}

static inline bool isVoxelMode() { return cfg.map3dVoxelIdx > 0; }

static void clearMap() {
    if (pts) memset(pts, 0, HASH_SLOTS * sizeof(MapPoint));
    nPts = 0; nRing = 0;
    prevVoxelIdx = cfg.map3dVoxelIdx;
}

// ── Flash save / load ─────────────────────────────────────────────────────────
// Format: [uint32_t magic][uint32_t count][MapPoint × count]
static constexpr uint32_t MAGIC = 0x4D503344;  // "MP3D"

static void saveMapToFlash() {
    if (!pts || nPts == 0) return;
    if (!SPIFFS.begin(true)) return;
    File f = SPIFFS.open(MAP_FILE, "w");
    if (!f) { SPIFFS.end(); return; }
    f.write((uint8_t*)&MAGIC, 4);
    f.write((uint8_t*)&nPts,  4);
    if (isVoxelMode()) {
        // write only occupied slots
        for (uint32_t i = 0; i < HASH_SLOTS; i++) {
            if (pts[i].hits == 0) continue;
            f.write((uint8_t*)&pts[i], sizeof(MapPoint));
        }
    } else {
        f.write((uint8_t*)pts, nPts * sizeof(MapPoint));
    }
    f.close();
    SPIFFS.end();
}

static void loadMapFromFlash() {
    if (!pts) return;
    if (!SPIFFS.begin(true)) return;
    if (!SPIFFS.exists(MAP_FILE)) { SPIFFS.end(); return; }
    File f = SPIFFS.open(MAP_FILE, "r");
    if (!f) { SPIFFS.end(); return; }
    uint32_t magic = 0, cnt = 0;
    f.read((uint8_t*)&magic, 4);
    f.read((uint8_t*)&cnt,   4);
    if (magic != MAGIC || cnt == 0) { f.close(); SPIFFS.end(); return; }
    clearMap();
    if (isVoxelMode()) {
        MapPoint p;
        for (uint32_t i = 0; i < cnt && nPts < HASH_MAX; i++) {
            if (f.read((uint8_t*)&p, sizeof(MapPoint)) != sizeof(MapPoint)) break;
            voxelInsert(p.x, p.y, p.z, p.dist8);
        }
    } else {
        cnt = min(cnt, HASH_SLOTS);
        f.read((uint8_t*)pts, cnt * sizeof(MapPoint));
        nRing = cnt; nPts = cnt;
    }
    f.close();
    SPIFFS.end();
}

// ── Capture ───────────────────────────────────────────────────────────────────
static void captureScan() {
    if (!TofState::ready || !ImuState::ready || !pts) return;

    // Clear map when voxel mode changes
    if (cfg.map3dVoxelIdx != prevVoxelIdx) clearMap();

    float qr = ImuState::qR, qi = ImuState::qI,
          qj = ImuState::qJ, qk = ImuState::qK;

    float d[8][8];
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            d[r][c] = TofState::distances[r * 8 + c];

    int    UP       = cfgMap3dCaptureUP();
    float  STEP     = 1.0f / UP;
    float  maxR     = (float)cfgMap3dMaxRange();
    uint16_t vsize  = cfgMap3dVoxelMm();

    for (float fr = 0.0f; fr < 7.9999f; fr += STEP) {
        int r0 = (int)fr, r1 = min(r0 + 1, 7);
        float tr = fr - r0;
        for (float fc = 0.0f; fc < 7.9999f; fc += STEP) {
            int c0 = (int)fc, c1 = min(c0 + 1, 7);
            float tc = fc - c0;
            float d00=d[r0][c0], d01=d[r0][c1],
                  d10=d[r1][c0], d11=d[r1][c1];
            float dmin=d00, dmax=d00;
            if (d01<dmin) dmin=d01; if (d01>dmax) dmax=d01;
            if (d10<dmin) dmin=d10; if (d10>dmax) dmax=d10;
            if (d11<dmin) dmin=d11; if (d11>dmax) dmax=d11;
            if (dmin > 10.0f && dmax / dmin > DISC_RATIO) continue;

            float dist = d00*(1-tr)*(1-tc) + d01*(1-tr)*tc
                       + d10*tr*(1-tc)     + d11*tr*tc;
            if (dist < 50.0f || dist > maxR) continue;

            float ah = (fc - 3.5f) * ZONE_STEP;
            float av = (fr - 3.5f) * ZONE_STEP;
            float lx = sinf(ah), ly = sinf(av), lz = cosf(ah) * cosf(av);
            float wx, wy, wz;
            qRot(qr, qi, qj, qk, lx, ly, lz, wx, wy, wz);
            wx *= dist; wy *= dist; wz *= dist;

            uint8_t d8 = (uint8_t)constrain((int)(dist / 16.0f), 1, 255);

            if (isVoxelMode()) {
                float vs = (float)vsize;
                voxelInsert(
                    (int16_t)(roundf(wx / vs) * vs),
                    (int16_t)(roundf(wy / vs) * vs),
                    (int16_t)(roundf(wz / vs) * vs),
                    d8);
            } else {
                ringInsert(
                    (int16_t)constrain((int)wx, -32767, 32767),
                    (int16_t)constrain((int)wy, -32767, 32767),
                    (int16_t)constrain((int)wz, -32767, 32767),
                    d8);
            }
        }
    }
}

// ── Rendering ────────────────────────────────────────────────────────────────
static void renderView(const char* statusMsg = nullptr) {
    memset(grid, 0, sizeof(grid));

    if (pts) {
        if (isVoxelMode()) {
            // Iterate all hash slots — sequential access is cache-friendly.
            // Each voxel contributes exactly ONE accumulation regardless of
            // hit count — hit count only shifts the color toward white so
            // well-scanned surfaces look brighter, but square size is purely
            // determined by how many distinct voxels project to the same cell.
            for (uint32_t i = 0; i < HASH_SLOTS; i++) {
                if (pts[i].hits == 0) continue;
                int gx, gy;
                if (projectFP(pts[i].x, pts[i].y, pts[i].z, gx, gy)) {
                    // Blend distance color toward white as hits accumulate
                    uint16_t c = distCol((uint32_t)pts[i].dist8 * 16u);
                    if (pts[i].hits >= 8) {
                        // Brighten: lerp each channel toward 255 by ~25%
                        uint8_t r5 = (c >> 11) & 0x1F;
                        uint8_t g6 = (c >>  5) & 0x3F;
                        uint8_t b5 = (c      ) & 0x1F;
                        r5 = (uint8_t)min(31, r5 + 6);
                        g6 = (uint8_t)min(63, g6 + 12);
                        b5 = (uint8_t)min(31, b5 + 6);
                        c = (uint16_t)((r5 << 11) | (g6 << 5) | b5);
                    }
                    gridAccum(gx, gy, c);
                }
            }
        } else {
            // Ring buffer — sample up to RING_RENDER points with uniform step
            uint32_t count = nPts;
            uint32_t step  = max(1u, count / RING_RENDER);
            for (uint32_t i = 0; i < count; i += step) {
                int gx, gy;
                if (projectFP(pts[i].x, pts[i].y, pts[i].z, gx, gy))
                    gridAccum(gx, gy, distCol((uint32_t)pts[i].dist8 * 16u));
            }
        }
    }

    // Live ToF overlay (always shown, double-weighted so it dominates)
    if (TofState::ready) {
        float qr = ImuState::qR, qi = ImuState::qI,
              qj = ImuState::qJ, qk = ImuState::qK;
        for (int z = 0; z < 64; z++) {
            float wx, wy, wz;
            if (!zoneWorld(z, TofState::distances[z], qr, qi, qj, qk, wx, wy, wz)) continue;
            int gx, gy;
            if (!projectFP(wx, wy, wz, gx, gy)) continue;
            uint16_t c = distCol((uint32_t)TofState::distances[z]);
            gridAccum(gx, gy, c);
            gridAccum(gx, gy, c);
        }
    }

    // Draw grid into sprite
    Display::spr.deleteSprite();
    Display::spr.createSprite(VIEW_W, VIEW_H);
    Display::spr.fillSprite(Display::Colors::BG);

    for (int gx = 0; gx < GRID_W; gx++) {
        for (int gy = 0; gy < GRID_H; gy++) {
            Cell& c = grid[gx][gy];
            if (c.n == 0) continue;
            uint16_t col = (uint16_t)(((c.r >> 3) << 11) | ((c.g >> 2) << 5) | (c.b >> 3));
            int sz = (c.n >= 8) ? 3 :
                     (c.n >= 3) ? 2 : 1;
            int px = gx * CELL + (CELL - sz) / 2;
            int py = gy * CELL + (CELL - sz) / 2;
            if (sz == 1) Display::spr.drawPixel(px, py, col);
            else         Display::spr.fillRect(px, py, sz, sz, col);
        }
    }

    // Crosshair
    int cx = VIEW_CX, cy = VIEW_CY - VIEW_Y;
    Display::spr.drawFastHLine(cx - 12, cy, 9, Display::Colors::GREEN_DIM);
    Display::spr.drawFastHLine(cx + 4,  cy, 9, Display::Colors::GREEN_DIM);
    Display::spr.drawFastVLine(cx, cy - 12, 9, Display::Colors::GREEN_DIM);
    Display::spr.drawFastVLine(cx, cy + 4,  9, Display::Colors::GREEN_DIM);

    // Status / point count (top-right)
    char buf[32];
    if (statusMsg) {
        strncpy(buf, statusMsg, sizeof(buf) - 1);
    } else if (isVoxelMode()) {
        uint16_t vs = cfgMap3dVoxelMm();
        if (nPts >= HASH_MAX)
            sprintf(buf, "%uk vxl FULL", nPts / 1000);
        else
            sprintf(buf, "%u vxl %umm", nPts, vs);
    } else {
        if (nPts >= HASH_SLOTS)
            sprintf(buf, "%uk/262k pts", nPts / 1000);
        else if (nPts >= 1000)
            sprintf(buf, "%uk pts", nPts / 1000);
        else
            sprintf(buf, "%u pts", nPts);
    }
    Display::spr.setTextColor(Display::Colors::GREEN_FAINT, Display::Colors::BG);
    Display::spr.drawRightString(buf, VIEW_W - 4, 2, 1);

    Display::spr.pushSprite(0, VIEW_Y);
    Display::spr.deleteSprite();
}

static void drawButtons() {
    int by = Display::SCREEN_H - BTN_BAR;
    Display::tft.fillRect(0, by, VIEW_W, BTN_BAR, Display::Colors::BG);
    Display::tft.drawFastHLine(0, by, VIEW_W, Display::Colors::SEP);

    // SAVE
    Display::tft.drawRect(SAVE_X, BTN_Y, SAVE_W, BTN_BH, Display::Colors::GREEN_DIM);
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("SAVE", SAVE_X + SAVE_W/2, BTN_Y + 9, 2);

    // LOAD
    Display::tft.drawRect(LOAD_X, BTN_Y, LOAD_W, BTN_BH, Display::Colors::GREEN_DIM);
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("LOAD", LOAD_X + LOAD_W/2, BTN_Y + 9, 2);

    // CLEAR
    Display::tft.drawRect(CLR_X, BTN_Y, CLR_W, BTN_BH, Display::Colors::RED);
    Display::tft.setTextColor(Display::Colors::RED, Display::Colors::BG);
    Display::tft.drawCentreString("CLEAR", CLR_X + CLR_W/2, BTN_Y + 9, 2);
}

// ── Public API ───────────────────────────────────────────────────────────────
void drawMap3dBase() {
    if (!pts)
        pts = (MapPoint*)heap_caps_malloc(HASH_SLOTS * sizeof(MapPoint), MALLOC_CAP_SPIRAM);
    if (pts && prevVoxelIdx == 255) clearMap();  // first init
    Display::tft.fillScreen(Display::Colors::BG);
    Display::drawHeader("3D MAP", false);
    drawButtons();
    renderView();
}

void tickMap3d() {
    if (!imuUpdate()) return;
    tofUpdate();
    captureScan();
    renderView();
}

void handleMap3dTouch(int tx, int ty) {
    if (inRect(tx, ty, SAVE_X, BTN_Y, SAVE_W, BTN_BH)) {
        renderView("SAVING...");
        saveMapToFlash();
        renderView("SAVED");
        delay(600);
        renderView();
        return;
    }
    if (inRect(tx, ty, LOAD_X, BTN_Y, LOAD_W, BTN_BH)) {
        renderView("LOADING...");
        loadMapFromFlash();
        renderView();
        return;
    }
    if (inRect(tx, ty, CLR_X, BTN_Y, CLR_W, BTN_BH)) {
        clearMap();
        renderView();
    }
}
