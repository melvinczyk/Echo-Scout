#include <math.h>
#include "imu_screen.h"
#include "imu.h"
#include "device_state.h"



static const float cubeVerts[8][3] = {
    {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
    {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1},
};

static const uint8_t cubeEdges[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    {4, 5}, {5, 6}, {6, 7}, {7, 4},
    {0, 4}, {1, 5}, {2, 6}, {3, 7},
};


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
  float scale = ImuScreen::PROJ_DIST / (ImuScreen::PROJ_DIST + z * ImuScreen::CUBE_SIZE);
  sx = ImuScreen::CUBE_CX + (int)(x * ImuScreen::CUBE_SIZE * scale);
  sy = ImuScreen::CUBE_CY + (int)(-y * ImuScreen::CUBE_SIZE * scale);
  sx = constrain(sx, 2, Display::SCREEN_W - 2);
  sy = constrain(sy, Display::HEADER_H + 2, ImuScreen::CUBE_YMAX);
}


static int prevSx[8], prevSy[8];
static bool cubeDrawn = false;


#define X_COL_FAR   0xA000
#define X_COL_NEAR  Display::Colors::RED

static void drawThickLine(int x0, int y0, int x1, int y1, uint16_t col) {
  Display::tft.drawLine(x0, y0, x1, y1, col);
  int dx = x1 - x0, dy = y1 - y0;
  if (abs(dx) >= abs(dy)) {
    Display::tft.drawLine(x0, y0-1, x1, y1-1, col);
    Display::tft.drawLine(x0, y0+1, x1, y1+1, col);
  } else {
    Display::tft.drawLine(x0-1, y0, x1-1, y1, col);
    Display::tft.drawLine(x0+1, y0, x1+1, y1, col);
  }
}

static void drawCubeEdges(uint16_t col) {
  int sx[8], sy[8];
  float rz[8];

  for (int v = 0; v < 8; v++) {
    float rx, ry;
    rotatePoint(cubeVerts[v][0], cubeVerts[v][1], cubeVerts[v][2], rx, ry, rz[v]);
    project(rx, ry, rz[v], sx[v], sy[v]);
    prevSx[v] = sx[v];
    prevSy[v] = sy[v];
  }

  for (int e = 0; e < 12; e++) {
    uint8_t a = cubeEdges[e][0], b = cubeEdges[e][1];
    Display::tft.drawLine(sx[a], sy[a], sx[b], sy[b], col);
  }

  
  float botZ = (rz[0] + rz[1] + rz[5] + rz[4]) * 0.25f;
  float topZ = (rz[3] + rz[2] + rz[6] + rz[7]) * 0.25f;
  uint16_t xCol = (botZ <= topZ) ? X_COL_FAR : X_COL_NEAR;
  drawThickLine(sx[0], sy[0], sx[5], sy[5], xCol);
  drawThickLine(sx[1], sy[1], sx[4], sy[4], xCol);
}

static void eraseCube() {
  if (!cubeDrawn)
    return;
  Display::tft.fillRect(2, Display::HEADER_H + 2, Display::SCREEN_W - 4,
               ImuScreen::CUBE_YMAX - Display::HEADER_H - 2, Display::Colors::BG);
}


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
  Display::tft.fillRect(pad + 1, valY - 1, 3 * cardW + 2 * pad - 2, 22, Display::Colors::BG);

  char buf[12];
  sprintf(buf, "%+.0f", roll);
  Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
  Display::tft.drawCentreString(buf, pad + cardW / 2, valY, 2);

  sprintf(buf, "%+.0f", pitch);
  Display::tft.setTextColor(Display::Colors::AMBER, Display::Colors::BG);
  Display::tft.drawCentreString(buf, pad + (cardW + pad) + cardW / 2, valY, 2);

  sprintf(buf, "%+.0f", yaw);
  Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
  Display::tft.drawCentreString(buf, pad + 2 * (cardW + pad) + cardW / 2, valY, 2);
}


void drawImuBase() {
  Display::tft.fillScreen(Display::Colors::BG);
  cubeDrawn = false;
  prevRoll = -9999;
  prevPitch = -9999;
  prevYaw = -9999;

  // Header
  Display::tft.fillRect(0, 0, Display::SCREEN_W, Display::HEADER_H, Display::Colors::BG);
  Display::tft.drawFastHLine(0, Display::HEADER_H - 1, Display::SCREEN_W, Display::Colors::SEP);
  Display::tft.drawRoundRect(3, 3, 64, Display::HEADER_H - 6, 3, Display::Colors::GREEN_DIM);
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

  // Readout cards
  Display::tft.drawFastHLine(0, 240, Display::SCREEN_W, Display::Colors::SEP);
  int cardW = 76, pad = 4;
  for (int i = 0; i < 3; i++) {
    int cx = pad + i * (cardW + pad);
    Display::tft.drawRect(cx, 240, cardW, 52, Display::Colors::GREEN_DIM);
  }
  Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
  Display::tft.drawCentreString("ROLL", pad + cardW / 2, 244, 1);
  Display::tft.drawCentreString("PITCH", pad + (cardW + pad) + cardW / 2, 244, 1);
  Display::tft.drawCentreString("YAW", pad + 2 * (cardW + pad) + cardW / 2, 244, 1);

  drawCubeEdges(Display::Colors::GREEN);
  cubeDrawn = true;
}


void tickIMU() {
  if (!imuUpdate()) return;

  eraseCube();
  drawCubeEdges(Display::Colors::GREEN);
  cubeDrawn = true;

  float qI = ImuState::qI, qJ = ImuState::qJ, qK = ImuState::qK, qR = ImuState::qR;

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