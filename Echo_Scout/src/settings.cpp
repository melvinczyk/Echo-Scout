#include "settings.h"
#include "grid.h"

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

const Settings DEFAULT_SETTINGS = {2, 1, 3, 5, 3, 0, 0, true};
Settings cfg = DEFAULT_SETTINGS;

void buildGridTable();

void applySettings() { buildGridTable(); }