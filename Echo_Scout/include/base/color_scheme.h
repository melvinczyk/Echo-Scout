#ifndef COLOR_SCHEME_H
#define COLOR_SCHEME_H

#include <Arduino.h>

namespace ColorScheme {

enum Palette { IRON, VIRIDIS, PLASMA, GRAY, COUNT };

struct PalDef { const char* name; };

inline constexpr PalDef PALETTES[COUNT] = {
    { "IRON"    },
    { "VIRIDIS" },
    { "PLASMA"  },
    { "GRAY"    },
};

inline uint16_t toRGB565(float r, float g, float b) {
    if (r < 0.0f) r = 0.0f; if (r > 1.0f) r = 1.0f;
    if (g < 0.0f) g = 0.0f; if (g > 1.0f) g = 1.0f;
    if (b < 0.0f) b = 0.0f; if (b > 1.0f) b = 1.0f;
    return (uint16_t)((((uint8_t)(r * 255) >> 3) << 11) |
                      (((uint8_t)(g * 255) >> 2) <<  5) |
                       ((uint8_t)(b * 255) >> 3));
}

// Polynomial fits by Inigo Quilez: shadertoy.com/view/WlfXRN

inline uint16_t sampleIron(float t) {
    float r = -0.002136f + t*(0.251661f + t*(8.353717f + t*(-27.668733f + t*(52.176140f + t*(-50.768526f + 18.655705f*t)))));
    float g = -0.000750f + t*(0.677523f + t*(-3.577720f + t*(14.264731f + t*(-27.943606f + t*(29.046583f - 11.489774f*t)))));
    float b = -0.005386f + t*(2.494027f + t*(0.314468f  + t*(-13.649213f + t*(12.944169f + t*(4.234153f  - 5.601962f*t)))));
    return toRGB565(r, g, b);
}

inline uint16_t sampleViridis(float t) {
    float r = 0.277727f + t*(0.105093f + t*(-0.330862f + t*(-4.634230f + t*(6.228270f  + t*(4.776385f  - 5.435456f*t)))));
    float g = 0.005407f + t*(1.404614f + t*(0.214848f  + t*(-5.799101f + t*(14.179933f + t*(-13.745145f + 4.645852f*t)))));
    float b = 0.334100f + t*(1.384590f + t*(0.095095f  + t*(-19.332441f + t*(56.690553f + t*(-65.353033f + 26.312435f*t)))));
    return toRGB565(r, g, b);
}

inline uint16_t samplePlasma(float t) {
    float r = 0.058732f + t*(2.176515f + t*(-2.689460f + t*(6.130349f  + t*(-11.107436f + t*(10.023066f - 3.658714f*t)))));
    float g = 0.023337f + t*(0.238383f + t*(-7.455851f + t*(42.346188f + t*(-82.666311f + t*(71.413618f - 22.931535f*t)))));
    float b = 0.543340f + t*(0.753960f + t*(3.110800f  + t*(-28.518855f + t*(60.139848f + t*(-54.072187f + 18.191908f*t)))));
    return toRGB565(r, g, b);
}

inline uint16_t sampleGray(float t) {
    return toRGB565(t, t, t);
}

using SampleFn = uint16_t(*)(float);
inline constexpr SampleFn SAMPLERS[COUNT] = { sampleIron, sampleViridis, samplePlasma, sampleGray };

inline uint16_t sample(Palette p, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return SAMPLERS[p](t);
}

inline uint16_t fromDist(Palette p,     float distMm, float maxMm = 4000.0f) { return sample(p, 1.0f - (distMm / maxMm)); }
inline uint16_t fromDist(uint8_t idx,   float distMm, float maxMm = 4000.0f) { return fromDist(Palette(idx), distMm, maxMm); }

}

#endif