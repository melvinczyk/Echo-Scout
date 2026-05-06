#include <math.h>
#include "imu.h"
#include "config.h"
#include "device_state.h"

static Adafruit_BNO08x bno;
static sh2_SensorValue_t sensorVal;


bool imuInit() {
    if (!bno.begin_I2C(Config::IMU_ADDRESS, &Wire)) {
        ImuState::found = false;
        ImuState::ready = false;
        Serial.println("BNO085 not found");
        return false;
    }
    ImuState::found = true;
    if (!bno.enableReport(SH2_ROTATION_VECTOR, 20000)) {
        ImuState::ready = false;
        Serial.println("BNO085 report enable failed");
        return false;
    }
    ImuState::ready = true;
    return true;
}

// IMU mounting (back of board, looking at display):
//   IMU X → LEFT on display,  IMU Y → DOWN,  IMU Z → INTO screen (away from viewer)
//
// We need output K = world UP = -IMU Y, so the yaw formula extracts
// tilt-compensated heading. A -90° rotation around IMU X maps Y→-Z, Z→Y:
//   new I = IMU X  →  oi = rawI
//   new J = IMU Z  →  oj = rawK
//   new K = -IMU Y →  ok = -rawJ   ← vertical axis, tilt-compensated yaw
static void frameCorrectValues(float ri, float rj, float rk, float rr,
                                float& oi, float& oj, float& ok, float& or_) {
    or_ =  rr;
    oi  =  ri;
    oj  =  rk;
    ok  = -rj;
}


bool imuUpdate() {
    if (!ImuState::ready) return false;
    if (!bno.getSensorEvent(&sensorVal)) return false;
    if (sensorVal.sensorId != SH2_ROTATION_VECTOR) return false;

    float rawI = sensorVal.un.rotationVector.i;
    float rawJ = sensorVal.un.rotationVector.j;
    float rawK = sensorVal.un.rotationVector.k;
    float rawR = sensorVal.un.rotationVector.real;

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