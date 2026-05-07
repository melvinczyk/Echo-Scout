#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

struct Settings {
  uint8_t hitsIdx;
  uint8_t dropsIdx;
  uint8_t minRangeIdx;
  uint8_t maxAngleIdx;
  uint8_t accRangeIdx;
  uint8_t speedGateIdx;
  uint8_t moveThreshIdx;
  bool smoothingOn;
  uint8_t brightnessIdx;
  uint8_t sleepTimeoutIdx;
};

extern const uint8_t hitsVals[];
extern const uint8_t dropsVals[];
extern const uint16_t minRangeVals[];
extern const uint8_t maxAngleVals[];
extern const uint16_t accRangeVals[];
extern const uint16_t speedGateVals[];
extern const uint8_t moveThreshVals[];

extern const char* minRangeLabels[];
extern const char* maxAngleLabels[];
extern const char* accRangeLabels[];
extern const char* speedGateLabels[];
extern const char* moveThreshLabels[];

constexpr uint8_t NUM_HITS = 5;
constexpr uint8_t NUM_DROPS = 5;
constexpr uint8_t NUM_MR = 8;
constexpr uint8_t NUM_MA = 5;
constexpr uint8_t NUM_AR = 8;
constexpr uint8_t NUM_SG = 5;
constexpr uint8_t NUM_MVTHR = 7;

constexpr uint8_t NUM_BL = 5;
constexpr uint8_t NUM_SLEEP = 6;
constexpr float SMOOTH_ALPHA = 0.4f;

extern const uint8_t blVals[];
extern const char* blLabels[];
extern const uint32_t sleepTimeoutVals[];
extern const char* sleepTimeoutLabels[];
extern const Settings DEFAULT_SETTINGS;
extern Settings cfg;

inline uint8_t cfgHits() { return hitsVals[cfg.hitsIdx]; }
inline uint8_t cfgDrops() { return dropsVals[cfg.dropsIdx]; }
inline uint16_t cfgMinRange() { return minRangeVals[cfg.minRangeIdx]; }
inline float cfgMaxAngle() { return (float)maxAngleVals[cfg.maxAngleIdx]; }
inline float cfgAccRange() { return (float)accRangeVals[cfg.accRangeIdx]; }
inline uint16_t cfgSpeedGate() { return speedGateVals[cfg.speedGateIdx]; }
inline float cfgMoveThresh() { return (float)moveThreshVals[cfg.moveThreshIdx]; }
inline bool cfgSmoothing() { return cfg.smoothingOn; }
inline uint8_t cfgBrightness() { return blVals[cfg.brightnessIdx]; }
inline uint32_t cfgSleepTimeoutMs() { return sleepTimeoutVals[cfg.sleepTimeoutIdx]; }

void applySettings();
void initBacklight();
void saveSettings();
void loadSettings();

#endif
