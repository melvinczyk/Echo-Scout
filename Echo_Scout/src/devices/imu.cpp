#include <math.h>
#include "imu.h"
#include "config.h"
#include "device_state.h"

static Adafruit_BNO08x bno;
static sh2_SensorValue_t sensorVal;


bool imuInit() {
    if (!bno.begin_I2C(Config::IMU_ADDRESS, &Wire)) {
        ImuState::ready = false;
        Serial.println("BNO085 not found");
        return false;
    }
    bno.enableReport(SH2_GAME_ROTATION_VECTOR, 20000);
    ImuState::ready = true;
    return true;
}

static void frameCorrectValues(float ri, float rj, float rk, float rr,
                                float& oi, float& oj, float& ok, float& or_) {
        or_ = rr;
        oi = rj;
        oj = rk;
        ok = ri;
    }


bool imuUpdate() {
    if (!ImuState::ready) return false;
    if (!bno.getSensorEvent(&sensorVal)) return false;
    if (sensorVal.sensorId != SH2_GAME_ROTATION_VECTOR) return false;

    float rawI = sensorVal.un.gameRotationVector.i;
    float rawJ = sensorVal.un.gameRotationVector.j;
    float rawK = sensorVal.un.gameRotationVector.k;
    float rawR = sensorVal.un.gameRotationVector.real;

    float ci, cj, ck, cr;
    frameCorrectValues(rawI, rawJ, rawK, rawR, ci, cj, ck, cr);

    if (fabsf(ci - ImuState::qI) < 0.005f && fabsf(cj - ImuState::qJ) < 0.005f &&
        fabsf(ck - ImuState::qK) < 0.005f && fabsf(cr - ImuState::qR) < 0.005f)
        return false;

    ImuState::qI = ci;
    ImuState::qJ = cj;
    ImuState::qK = ck;
    ImuState::qR = cr;
    return true;
}