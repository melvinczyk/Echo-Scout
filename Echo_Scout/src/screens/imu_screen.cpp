#include <math.h>
#include "imu_screen.h"
#include "imu.h"
#include "device_state.h"


// ═══════════════════════════════════════════════════════════
//  CUBE GEOMETRY
// ═══════════════════════════════════════════════════════════
#define CUBE_SIZE 55
#define CUBE_CX 120
#define CUBE_CY 148
#define CUBE_YMAX 235
#define PROJ_DIST 280.0f

static const float cubeVerts[8][3] = {
    {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, // back  0-3
    {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1},  // front 4-7
};

static const uint8_t cubeEdges[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0}, // back face
    {4, 5}, {5, 6}, {6, 7}, {7, 4}, // front face
    {0, 4}, {1, 5}, {2, 6}, {3, 7}, // connecting edges
};

// ═══════════════════════════════════════════════════════════
//  ROTATION + PROJECTION
//  Sensor: X=right, Y=down, Z=out back of display
//  Screen: X=right, Y=up(inverted), depth=Z
// ═══════════════════════════════════════════════════════════
static void rotatePoint(float x, float y, float z, float& ox, float& oy,
                        float& oz) {
  float r = ImuState::qR, i = ImuState::qI, j = ImuState::qJ, k = ImuState::qK;
  float tx = 2.0f * (j * z - k * y);
  float ty = 2.0f * (k * x - i * z);
  float tz = 2.0f * (i * y - j * x);
  ox = x + r * tx + (j * tz - k * ty);
  oy = y + r * ty + (k * tx - i * tz);
  oz = z + r * tz + (i * ty - j * tx);
}

static void project(float x, float y, float z, int& sx, int& sy) {
  float scale = PROJ_DIST / (PROJ_DIST + z * CUBE_SIZE);
  sx = CUBE_CX + (int)(x * CUBE_SIZE * scale);
  sy = CUBE_CY + (int)(-y * CUBE_SIZE * scale); // invert Y so up is up
  sx = constrain(sx, 2, Config::SCREEN_W - 2);
  sy = constrain(sy, Config::HEADER_H + 2, CUBE_YMAX);
}

// ═══════════════════════════════════════════════════════════
//  DRAW / ERASE CUBE
// ═══════════════════════════════════════════════════════════
static int prevSx[8], prevSy[8];
static bool cubeDrawn = false;

static void drawCubeEdges(uint16_t col) {
  int sx[8], sy[8];
  for (int v = 0; v < 8; v++) {
    float rx, ry, rz;
    rotatePoint(cubeVerts[v][0], cubeVerts[v][1], cubeVerts[v][2], rx, ry, rz);
    project(rx, ry, rz, sx[v], sy[v]);
    prevSx[v] = sx[v];
    prevSy[v] = sy[v];
  }
  for (int e = 0; e < 12; e++) {
    uint8_t a = cubeEdges[e][0], b = cubeEdges[e][1];
    tft.drawLine(sx[a], sy[a], sx[b], sy[b], col);
  }
}

static void eraseCube() {
  if (!cubeDrawn)
    return;
  tft.fillRect(2, Config::HEADER_H + 2, Config::SCREEN_W - 4,
               CUBE_YMAX - Config::HEADER_H - 2, Config::C_BG);
}

// ═══════════════════════════════════════════════════════════
//  EULER READOUT
// ═══════════════════════════════════════════════════════════
static float prevRoll = -9999, prevPitch = -9999, prevYaw = -9999;

static void drawEuler(float roll, float pitch, float yaw) {
  bool changed = fabsf(roll - prevRoll) > 0.5f ||
                 fabsf(pitch - prevPitch) > 0.5f || fabsf(yaw - prevYaw) > 0.5f;
  if (!changed)
    return;
  prevRoll = roll;
  prevPitch = pitch;
  prevYaw = yaw;

  int cardW = 76, pad = 4, valY = 272;
  tft.fillRect(pad + 1, valY - 1, 3 * cardW + 2 * pad - 2, 22, Config::C_BG);

  char buf[12];
  sprintf(buf, "%+.0f", roll);
  tft.setTextColor(Config::C_GREEN, Config::C_BG);
  tft.drawCentreString(buf, pad + cardW / 2, valY, 2);

  sprintf(buf, "%+.0f", pitch);
  tft.setTextColor(Config::C_AMBER, Config::C_BG);
  tft.drawCentreString(buf, pad + (cardW + pad) + cardW / 2, valY, 2);

  sprintf(buf, "%+.0f", yaw);
  tft.setTextColor(Config::C_GREEN, Config::C_BG);
  tft.drawCentreString(buf, pad + 2 * (cardW + pad) + cardW / 2, valY, 2);
}

// ═══════════════════════════════════════════════════════════
//  PUBLIC — BASE SCREEN
// ═══════════════════════════════════════════════════════════
void drawImuBase() {
  tft.fillScreen(Config::C_BG);
  cubeDrawn = false;
  prevRoll = -9999;
  prevPitch = -9999;
  prevYaw = -9999;

  // Header
  tft.fillRect(0, 0, Config::SCREEN_W, Config::HEADER_H, Config::C_BG);
  tft.drawFastHLine(0, Config::HEADER_H - 1, Config::SCREEN_W, Config::C_SEP);
  tft.drawRoundRect(3, 3, 64, Config::HEADER_H - 6, 3, Config::C_GREEN_DIM);
  tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
  tft.drawCentreString("< MENU", 35, 7, 2);
  tft.setTextColor(Config::C_GREEN, Config::C_BG);
  tft.drawCentreString("IMU", 120, 9, 2);

  if (!ImuState::ready) {
    tft.setTextColor(Config::C_RED, Config::C_BG);
    tft.drawCentreString("BNO085 NOT FOUND", 120, 150, 2);
    tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
    tft.drawCentreString("CHECK WIRING AND RESET", 120, 175, 1);
    return;
  }

  // Readout cards
  tft.drawFastHLine(0, 240, Config::SCREEN_W, Config::C_SEP);
  int cardW = 76, pad = 4;
  for (int i = 0; i < 3; i++) {
    int cx = pad + i * (cardW + pad);
    tft.drawRect(cx, 240, cardW, 52, Config::C_GREEN_DIM);
  }
  tft.setTextColor(Config::C_GREEN_DIM, Config::C_BG);
  tft.drawCentreString("ROLL", pad + cardW / 2, 244, 1);
  tft.drawCentreString("PITCH", pad + (cardW + pad) + cardW / 2, 244, 1);
  tft.drawCentreString("YAW", pad + 2 * (cardW + pad) + cardW / 2, 244, 1);

  drawCubeEdges(Config::C_GREEN);
  cubeDrawn = true;
}

// ═══════════════════════════════════════════════════════════
//  PUBLIC — UPDATE
// ═══════════════════════════════════════════════════════════
void tickIMU() {
  imuUpdate();

  eraseCube();
  drawCubeEdges(Config::C_GREEN);
  cubeDrawn = true;

  // Local aliases for Euler math — rotatePoint() reads ImuState:: directly
  float qI = ImuState::qI, qJ = ImuState::qJ, qK = ImuState::qK, qR = ImuState::qR;

  // Euler from corrected quaternion
  float sinr = 2.0f * (qR * qI + qJ * qK);
  float cosr = 1.0f - 2.0f * (qI * qI + qJ * qJ);
  float roll = atan2f(sinr, cosr) * 180.0f / PI;

  float sinp = 2.0f * (qR * qJ - qK * qI);
  float pitch = (fabsf(sinp) >= 1.0f) ? copysignf(90.0f, sinp)
                                      : asinf(sinp) * 180.0f / PI;

  float siny = 2.0f * (qR * qK + qI * qJ);
  float cosy = 1.0f - 2.0f * (qJ * qJ + qK * qK);
  float yaw = atan2f(siny, cosy) * 180.0f / PI;

  drawEuler(roll, pitch, yaw);
}