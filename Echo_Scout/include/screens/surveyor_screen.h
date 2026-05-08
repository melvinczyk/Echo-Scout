#ifndef SURVEYOR_SCREEN_H
#define SURVEYOR_SCREEN_H

namespace SurveyorScreen {
    // Layout constants
    constexpr int HDR_H   = 28;
    constexpr int INFO_Y  = HDR_H;
    constexpr int INFO_H  = 24;
    constexpr int BTN_Y   = INFO_Y + INFO_H;
    constexpr int BTN_ROWS = 3;
    constexpr int BTN_ROW_H = 26;
    constexpr int BTN_H   = BTN_ROWS * BTN_ROW_H;  // 78px
    constexpr int CLR_Y   = BTN_Y + BTN_H;
    constexpr int CLR_H   = 22;
    constexpr int VIZ_Y   = CLR_Y + CLR_H;
    constexpr int VIZ_H   = 320 - VIZ_Y;           // ~168px
    constexpr int VIZ_CX  = 120;
    constexpr int VIZ_CY  = VIZ_Y + VIZ_H / 2;

    // Named surface slots
    enum Surf { S_CEIL=0, S_FLOOR=1, S_LEFT=2, S_RIGHT=3, S_FRONT=4, S_BACK=5, S_COUNT=6 };
}

void drawSurveyorBase();
void tickSurveyor();
void handleSurveyorTouch(int tx, int ty);
void handleSurveyorDrag(int tx, int ty);
void handleSurveyorDragBegin(int tx, int ty);

namespace SurveyorScreen {
    bool dragWasInViz();
}

#endif
