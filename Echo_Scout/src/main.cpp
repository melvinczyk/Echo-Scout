#include <HardwareSerial.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <math.h>

#include "config.h"
#include "settings.h"
#include "grid.h"
#include "radar.h"
#include "display.h"
#include "menu.h"
#include "settings_screen.h"
#include "touch.h"


#define SCREEN_MENU 0
#define SCREEN_RADAR 1
#define SCREEN_SETTINGS 2

static uint8_t frameBuf[30];
static size_t frameIdx = 0;
static uint8_t frameState = 0;

int currentScreen = SCREEN_MENU;

float radarDistance = 0, radarAngle = 0, radarSpeed = 0;
float radarX = 0, radarY = 0;
bool radarPresent = false;
bool wasTouched = false;


void setup() {
  Serial.begin(115200);
  while (!Serial)
    delay(10);
  pinMode(42, OUTPUT);
  digitalWrite(42, LOW);
  touchInit();
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(Config::C_BG);
  buildGridTable();
  Serial1.begin(Config::RADAR_BAUD, SERIAL_8N1, Config::RADAR_RX,
                Config::RADAR_TX);
  delay(500);
  Serial.println("=== ECHO SCOUT ===");
  startMenu();
}


void loop() {
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
        frameIdx = 0;
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
                float dist =
                    sqrtf((float)t[i].x * t[i].x + (float)t[i].y * t[i].y);
                float angle =
                    atan2f((float)t[i].x, (float)t[i].y) * 180.0f / PI;

                if (dist <= cfgAccRange()) {
                  float dx = t[i].x - lastBlipX[i];
                  float dy = t[i].y - lastBlipY[i];
                  float frameMovement = sqrtf(dx * dx + dy * dy);
                  bool validMovement =
                      (frameMovement < 180.0f || !blips[i].active);

                  if (validMovement) {
                    if (cfgSmoothing() && blips[i].active) {
                      smoothedX[i] = SMOOTH_ALPHA * t[i].x +
                                     (1.0f - SMOOTH_ALPHA) * smoothedX[i];
                      smoothedY[i] = SMOOTH_ALPHA * t[i].y +
                                     (1.0f - SMOOTH_ALPHA) * smoothedY[i];
                    } else {
                      smoothedX[i] = t[i].x;
                      smoothedY[i] = t[i].y;
                    }

                    float mdx = smoothedX[i] - lastBlipX[i];
                    float mdy = smoothedY[i] - lastBlipY[i];
                    float moved = sqrtf(mdx * mdx + mdy * mdy);

                    if (moved > cfgMoveThresh() || !blips[i].active) {
                      lastBlipX[i] = smoothedX[i];
                      lastBlipY[i] = smoothedY[i];
                      int sx, sy;
                      radarToScreen(smoothedX[i], smoothedY[i], sx, sy);
                      if (sy >= CONE_TOP && sy < Config::SCREEN_H)
                        updateBlip(i, sx, sy, true);
                    }
                  }
                } else {
                  updateBlip(i, 0, 0, false);
                  lastBlipX[i] = 0;
                  lastBlipY[i] = 0;
                  newFarZone[getZone(angle)] = true;
                }

                if (dist < bestDist) {
                  bestDist = dist;
                  radarDistance = dist;
                  radarX = t[i].x;
                  radarY = t[i].y;
                  radarAngle = angle;
                  radarSpeed = t[i].speed;
                  radarPresent = true;
                }
              } else {
                updateBlip(i, 0, 0, false);
                lastBlipX[i] = 0;
                lastBlipY[i] = 0;
                smoothedX[i] = 0;
                smoothedY[i] = 0;
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
        frameIdx = 0;
      }
      break;
    }
  }

  if (currentScreen == SCREEN_MENU)
    tickMenu();

  int tx, ty;
  bool touched = touchRead(tx, ty);

  if (currentScreen == SCREEN_SETTINGS) {
    if (touched && !wasTouched) {
      touchStartY = ty;
      touchStartScroll = settingsScrollY;
      touchMoved = false;
    } else if (touched && wasTouched) {
      int drag = touchStartY - ty;
      if (abs(drag) > 8) {
        touchMoved = true;
        int maxScroll =
            max(0, settingsTotalH() -
                       (Config::SCREEN_H - Config::HEADER_H - SET_RESET_H));
        settingsScrollY = constrain(touchStartScroll + drag, 0, maxScroll);
        int visTop = Config::HEADER_H;
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
        if (inRect(tx, ty, 24, 262, 192, 44)) {
          currentScreen = SCREEN_RADAR;
          radarResetState();
          drawRadarBase();
        } else if (inRect(tx, ty, 36, 210, 168, 36)) {
          currentScreen = SCREEN_SETTINGS;
          settingsScrollY = 0;
          drawSettingsScreen();
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