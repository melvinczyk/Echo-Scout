#ifndef MEASUREMENTS_H
#define MEASUREMENTS_H

#include <math.h>
#include <stdio.h>

namespace Measurements {

    enum DistUnit  { METRIC, IMPERIAL };
    enum SpeedUnit { CMS, MS, KMH, MPH };

    inline DistUnit  distUnit  = METRIC;
    inline SpeedUnit speedUnit = CMS;

    inline void fmtDist(float mm, char* buf, int len = 24) {
        if (distUnit == IMPERIAL) {
            float in = mm / 25.4f;
            if (in < 12.f) snprintf(buf, len, "%.2f in", in);
            else           snprintf(buf, len, "%.2f ft", in / 12.f);
        } else {
            if (mm < 1000.f) snprintf(buf, len, "%.0f mm", mm);
            else             snprintf(buf, len, "%.3f m",  mm / 1000.f);
        }
    }

    inline void fmtDistShort(float mm, char* buf, int len = 8) {
        if (distUnit == IMPERIAL) {
            float in = mm / 25.4f;
            if (in < 12.f) snprintf(buf, len, "%.0fin", in);
            else           snprintf(buf, len, "%.1fft", in / 12.f);
        } else {
            if (mm < 1000.f) snprintf(buf, len, "%.0fmm", mm);
            else             snprintf(buf, len, "%.0fm",  mm / 1000.f);
        }
    }

    inline void fmtAngle(float deg, char* buf, int len = 16) {
        snprintf(buf, len, "%.1f deg", deg);
    }

    inline void fmtSpeed(float cmps, char* buf, int len = 16) {
        float s = fabsf(cmps);
        switch (speedUnit) {
            case CMS: snprintf(buf, len, "%.0f cm/s", s);            break;
            case MS:  snprintf(buf, len, "%.2f m/s",  s / 100.f);    break;
            case KMH: snprintf(buf, len, "%.1f km/h", s * 0.036f);   break;
            case MPH: snprintf(buf, len, "%.1f mph",  s * 0.02237f); break;
        }
    }
}

#endif
