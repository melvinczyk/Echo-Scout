#ifndef TOF_TABS_H
#define TOF_TABS_H

#include "base/display.h"
#include "devices/device_state.h"
#include "devices/tof.h"
#include "base/tab_manager.h"

namespace TofTabs {
    inline constexpr int CONTENT_Y = Display::HEADER_H;
    inline constexpr int TAB_Y = TabManager::TAB_Y;

    inline uint16_t distToColor(float d) {
        if (d <= 0.0f)   return 0x2104;
        if (d < 300.0f)  return 0xF800;
        if (d < 600.0f)  { float t=(d-300.0f)/300.0f; return (uint16_t)((31u<<11)|((uint8_t)(t*20.0f)<<5)); }
        if (d < 1200.0f) { float t=(d-600.0f)/600.0f; return (uint16_t)((31u<<11)|((uint8_t)(20.0f+t*43.0f)<<5)); }
        if (d < 2500.0f) { float t=(d-1200.0f)/1300.0f; return (uint16_t)(((uint8_t)(31.0f*(1.0f-t))<<11)|(63u<<5)); }
        return Display::Colors::GREEN_DIM;
    }

    inline float centerDist() {
        float d = TofState::distances[27];
        return d > 10.0f ? d : 0.0f;
    }
}

namespace GridTab  { void drawTab(); void tick(); }
namespace AngleTab { void drawTab(); void tick(); void onTouch(int tx, int ty); }
namespace DistTab  { void drawTab(); void tick(); void onTouch(int tx, int ty); }

namespace NormalTab {
    constexpr int   NUM_H   = 52;
    constexpr int   VIZ_Y   = TofTabs::CONTENT_Y + NUM_H;
    constexpr int   VIZ_H   = TofTabs::TAB_Y - VIZ_Y;
    constexpr int   VIZ_CX  = Display::SCREEN_W / 2;
    constexpr int   VIZ_CY  = VIZ_Y + VIZ_H / 2;
    constexpr float SCALE   = 55.0f;
    constexpr float PLANE_R = 400.0f;
    constexpr float ARROW_L = 350.0f;
    void drawTab(); void tick();
}

#endif
