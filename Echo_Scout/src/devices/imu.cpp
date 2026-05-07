#include <math.h>
#include <Adafruit_BNO08x.h>
#include "devices/imu.h"
#include "base/config.h"
#include "devices/device_state.h"

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
    ImuState::lastMotionMs = millis();
    if (!bno.enableReport(SH2_GAME_ROTATION_VECTOR, 20000)) {
        ImuState::ready = false;
        Serial.println("BNO085 report enable failed");
        return false;
    }
    bno.enableReport(SH2_STEP_COUNTER, 1000000);
    ImuState::ready = true;
    return true;
}

bool imuUpdate() {
    if (!ImuState::ready) return false;
    if (!bno.getSensorEvent(&sensorVal)) return false;
    if (sensorVal.sensorId != SH2_GAME_ROTATION_VECTOR) return false;

    float ci = sensorVal.un.gameRotationVector.i;
    float cj = sensorVal.un.gameRotationVector.k;
    float ck = -sensorVal.un.gameRotationVector.j;
    float cr = sensorVal.un.gameRotationVector.real;

    ImuState::rawQI = ci; ImuState::rawQJ = cj;
    ImuState::rawQK = ck; ImuState::rawQR = cr;

    if (ImuState::calibrated) {
        float aR =  ImuState::calR, aI = -ImuState::calI;
        float aJ = -ImuState::calJ, aK = -ImuState::calK;
        float bR = cr, bI = ci, bJ = cj, bK = ck;
        cr = aR*bR - aI*bI - aJ*bJ - aK*bK;
        ci = aR*bI + aI*bR + aJ*bK - aK*bJ;
        cj = aR*bJ - aI*bK + aJ*bR + aK*bI;
        ck = aR*bK + aI*bJ - aJ*bI + aK*bR;
    }

    if (fabsf(ci - ImuState::qI) < 0.005f && fabsf(cj - ImuState::qJ) < 0.005f &&
        fabsf(ck - ImuState::qK) < 0.005f && fabsf(cr - ImuState::qR) < 0.005f)
        return false;

    ImuState::qI = ci;
    ImuState::qJ = cj;
    ImuState::qK = ck;
    ImuState::qR = cr;
    ImuState::lastMotionMs = millis();
    return true;
}