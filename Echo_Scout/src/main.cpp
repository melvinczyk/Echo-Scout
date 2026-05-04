#include <math.h>

#include "app_state.h"
#include "radar.h"
#include "menu_screen.h"
#include "settings_screen.h"
#include "imu_screen.h"
#include "touch.h"
#include "imu.h"

// Definitions for symbols declared in app_state.h
bool radarFound   = true;
bool imuFound     = false;
int  currentScreen = SCREEN_MENU;

static uint8_t frameBuf[30];
static size_t  frameIdx   = 0;
static uint8_t frameState = 0;

static float radarDistance = 0, radarAngle = 0, radarSpeed = 0;
static float radarX = 0, radarY = 0;
static bool  radarPresent = false;
static bool  wasTouched   = false;


void setup() {
  Serial.begin(115200);
  pinMode(42, OUTPUT);
  digitalWrite(42, LOW);
  touchInit();
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(Config::C_BG);
  buildGridTable();
  imuFound = imuInit();
  Serial1.begin(Config::RADAR_BAUD, SERIAL_8N1, Config::RADAR_RX,
                Config::RADAR_TX);
  delay(500);
  Serial.println("=== ECHO SCOUT ===");
  startMenu();
}


static void processSerial() {
  while (Serial1.available()) {
    uint8_t b = Serial1.read();
    switch (frameState) {
    case 0:
      if (b == 0xAA)
        frameState = 1;
      break;
    case 1:
      frameState = (b == 0xFF) ? 2 : 0;
      break;
    case 2:
      frameState = (b == 0x03) ? 3 : 0;
      break;
    case 3:
      if (b == 0x00) {
        frameIdx  = 0;
        frameState = 4;
      } else
        frameState = 0;
      break;
    case 4:
      frameBuf[frameIdx++] = b;
      if (frameIdx >= 26) {
        if (frameBuf[24] == 0x55 && frameBuf[25] == 0xCC) {
          RawTarget t[3];
          t[0] = parseTarget(frameBuf + 0);
          t[1] = parseTarget(frameBuf + 8);
          t[2] = parseTarget(frameBuf + 16);
          applyPersistence(t);

          if (currentScreen == SCREEN_RADAR) {
            radarPresent = false;
            float bestDist = cfgAccRange() + 1;
            bool newFarZone[4] = {false, false, false, false};

            for (int i = 0; i < 3; i++) {
              if (t[i].present) {
                float dist  = sqrtf((float)t[i].x * t[i].x +
                                    (float)t[i].y * t[i].y);
                float angle = atan2f((float)t[i].x, (float)t[i].y) *
                              180.0f / PI;

                if (dist <= cfgAccRange()) {
                  float outX, outY;
                  if (smoothTarget(i, blips[i].active, t[i].x, t[i].y,
                                   outX, outY)) {
                    int sx, sy;
                    radarToScreen(outX, outY, sx, sy);
                    if (sy >= CONE_TOP && sy < Config::SCREEN_H)
                      updateBlip(i, sx, sy, true);
                  }
                } else {
                  updateBlip(i, 0, 0, false);
                  clearTargetSmoothing(i);
                  newFarZone[getZone(angle)] = true;
                }

                if (dist < bestDist) {
                  bestDist      = dist;
                  radarDistance = dist;
                  radarX        = t[i].x;
                  radarY        = t[i].y;
                  radarAngle    = angle;
                  radarSpeed    = t[i].speed;
                  radarPresent  = true;
                }
              } else {
                updateBlip(i, 0, 0, false);
                clearTargetSmoothing(i);
              }
            }

            for (int z = 0; z < 4; z++) {
              if (newFarZone[z] != farZoneDrawn[z]) {
                drawFarZone(z, newFarZone[z]);
                farZoneDrawn[z] = newFarZone[z];
              }
            }
            updateDashboard(radarDistance, radarAngle, radarSpeed,
                            radarPresent);
          }
        }
        frameState = 0;
        frameIdx   = 0;
      }
      break;
    }
  }
}

static void tickScreens() {
  if (currentScreen == SCREEN_MENU)
    tickMenu();
  if (currentScreen == SCREEN_IMU)
    imuUpdate();
}

static void handleTouch() {
  int tx, ty;
  bool touched = touchRead(tx, ty);

  if (currentScreen == SCREEN_SETTINGS) {
    if (touched && !wasTouched) {
      touchStartY     = ty;
      touchStartScroll = settingsScrollY;
      touchMoved      = false;
    } else if (touched && wasTouched) {
      int drag = touchStartY - ty;
      if (abs(drag) > 8) {
        touchMoved = true;
        int maxScroll = max(0, settingsTotalH() -
                               (Config::SCREEN_H - Config::HEADER_H - SET_RESET_H));
        settingsScrollY = constrain(touchStartScroll + drag, 0, maxScroll);
        int visTop    = Config::HEADER_H;
        int visBottom = Config::SCREEN_H - SET_RESET_H;
        tft.fillRect(0, visTop, Config::SCREEN_W, visBottom - visTop,
                     Config::C_BG);
        for (int i = 0; i < NUM_SETTING_ROWS; i++)
          drawSettingRowAt(i, settingRowY(i));
      }
    } else if (!touched && wasTouched) {
      if (!touchMoved)
        handleSettingsTouch(tx, ty);
    }
  } else {
    if (touched && !wasTouched) {
      if (currentScreen == SCREEN_MENU) {
        if (inRect(tx, ty, 24, 220, 192, 50)) {
          currentScreen = SCREEN_RADAR;
          radarResetState();
          drawRadarBase();
        } else if (inRect(tx, ty, 60, 178, 120, 28)) {
          currentScreen = SCREEN_SETTINGS;
          settingsScrollY = 0;
          drawSettingsScreen();
        } else if (inRect(tx, ty, 72, 280, 96, 22)) {
          currentScreen = SCREEN_IMU;
          drawImuBase();
        }
      } else {
        if (inRect(tx, ty, 3, 3, 64, Config::HEADER_H - 6)) {
          currentScreen = SCREEN_MENU;
          startMenu();
        }
      }
    }
  }
  wasTouched = touched;
}

void loop() {
  processSerial();
  tickScreens();
  handleTouch();
}
