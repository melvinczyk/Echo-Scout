#include <Wire.h>
#include <SparkFun_VL53L5CX_Library.h>
#include "devices/tof.h"
#include "base/config.h"
#include "devices/device_state.h"

static SparkFun_VL53L5CX imager;
static VL53L5CX_ResultsData results;

bool tofInit(void (*progressCb)(uint8_t pct)) {
    Wire.setClock(1000000);   // 1 MHz - speeds up the ~84 KB firmware upload
    if (progressCb) progressCb(5);

    if (!imager.begin(Config::TOF_ADDRESS, Wire)) {
        TofState::found = false;
        TofState::ready = false;
        Wire.setClock(400000);
        Serial.println("VL53L5CX not found");
        if (progressCb) progressCb(100);
        return false;
    }
    if (progressCb) progressCb(80);

    TofState::found = true;
    imager.setResolution(8 * 8);
    if (progressCb) progressCb(90);

    imager.setRangingFrequency(15);
    imager.startRanging();
    Wire.setClock(400000);
    if (progressCb) progressCb(100);

    TofState::ready = true;
    Serial.println("VL53L5CX ready");
    return true;
}

bool tofUpdate() {
    if (!TofState::ready) return false;
    if (!imager.isDataReady()) return false;
    if (!imager.getRangingData(&results)) return false;
    for (int i = 0; i < 64; i++) {
        TofState::distances[i] = (float)results.distance_mm[i];
    }
    return true;
}
