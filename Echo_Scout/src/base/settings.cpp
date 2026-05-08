#include "base/settings.h"
#include "base/measurements.h"
#include "base/grid.h"
#include "base/config.h"
#include <Arduino.h>
#include <Preferences.h>

static const char* NVS_NS = "echo";
static const char* NVS_CFG = "cfg";
static const char* NVS_VER = "ver";
static constexpr uint8_t CFG_VERSION = 7;


const uint8_t hitsVals[] = {1, 2, 3, 4, 5};
const uint8_t dropsVals[] = {1, 2, 3, 4, 5};

const uint16_t minRangeVals[] = {0, 100, 300, 500, 600, 800, 1000, 1200};
const char *minRangeLabels[] = {"OFF",   "100mm", "300mm",  "500mm",
                                "600mm", "800mm", "1000mm", "1200mm"};

const uint8_t maxAngleVals[] = {30, 40, 50, 55, 60};
const char *maxAngleLabels[] = {"30deg", "40deg",
                                "50deg", "55deg", "60deg"};

const uint16_t accRangeVals[] = {1000, 2000, 3000, 4000,
                                 5000, 6000, 7000, 8000};
const char *accRangeLabels[] = {"1m", "2m", "3m", "4m", "5m", "6m", "7m", "8m"};

const uint16_t speedGateVals[] = {0, 100, 150, 200, 300};
const char *speedGateLabels[] = {"OFF", "100", "150", "200", "300"};

const uint8_t moveThreshVals[] = {0, 10, 30, 50, 100, 150, 200};
const char *moveThreshLabels[] = {"OFF", "10mm","30mm", "50mm",
                                  "100mm", "150mm", "200mm"};

const uint8_t blVals[] = {25, 51, 102, 178, 255};
const char* blLabels[] = {"10%", "20%", "40%", "70%", "100%"};

const uint32_t sleepTimeoutVals[] = {0, 30000, 60000, 120000, 300000, 600000};
const char* sleepTimeoutLabels[] = {"OFF", "30s", "1min", "2min", "5min", "10min"};

const char* distUnitLabels[]  = {"METRIC", "IMPERIAL"};
const char* speedUnitLabels[] = {"cm/s", "m/s", "km/h", "mph"};

const char* map3dMaxRangeLabels[] = {"1m", "2m", "4m"};
const char* map3dCellLabels[]     = {"2px", "3px", "4px", "6px"};

//                                   hits drops minR maxA accR sgat mvth  smth  bl  slp  du  su  pal  rng   bil   cell
const Settings DEFAULT_SETTINGS = {  2,   1,    3,   3,   3,   0,   0,   true, 4,  2,   0,  0,  0,   2,  true,  1  };
Settings cfg = DEFAULT_SETTINGS;

void initBacklight() {
    ledcSetup(Config::BL_PWM_CH, Config::BL_PWM_FREQ, Config::BL_PWM_RES);
    ledcAttachPin(Config::TFT_BL_PIN, Config::BL_PWM_CH);
    ledcWrite(Config::BL_PWM_CH, blVals[cfg.brightnessIdx]);
}

void saveSettings() {
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    prefs.putUChar(NVS_VER, CFG_VERSION);
    prefs.putBytes(NVS_CFG, &cfg, sizeof(cfg));
    prefs.end();
}

void loadSettings() {
    Preferences prefs;
    prefs.begin(NVS_NS, true);
    uint8_t ver = prefs.getUChar(NVS_VER, 0);
    size_t stored = prefs.getBytesLength(NVS_CFG);
    if (ver == CFG_VERSION && stored == sizeof(cfg))
        prefs.getBytes(NVS_CFG, &cfg, sizeof(cfg));
    prefs.end();
    saveSettings();
}

void applySettings() {
    gridDirty = true;
    buildGridTable();
    ledcWrite(Config::BL_PWM_CH, blVals[cfg.brightnessIdx]);
    Measurements::distUnit  = (Measurements::DistUnit)cfg.distUnitIdx;
    Measurements::speedUnit = (Measurements::SpeedUnit)cfg.speedUnitIdx;
    saveSettings();
}