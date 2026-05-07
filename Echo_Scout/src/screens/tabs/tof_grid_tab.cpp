#include "tabs/tof_tabs.h"

static constexpr int CELL = 30;
static constexpr int GRID_Y = TofTabs::CONTENT_Y;
static constexpr int GRID_H = CELL * 8;
static constexpr int AVG_Y = GRID_Y + GRID_H;

static float prevDistances[64];
static float prevAvg = -1.0f;

static void drawCell(int zone, float d) {
    int row = zone/8, col = zone%8;
    Display::tft.fillRect(col*CELL+1, GRID_Y+row*CELL+1, CELL-2, CELL-2, TofTabs::distToColor(d));
}
static void drawAvgBar(float avg) {
    Display::tft.fillRect(0, AVG_Y, Display::SCREEN_W, TofTabs::TAB_Y-AVG_Y, Display::Colors::BG);
    char buf[24];
    if (avg < 1000.0f) sprintf(buf, "AVG  %.0f mm", avg);
    else               sprintf(buf, "AVG  %.2f m",  avg/1000.0f);
    Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
    Display::tft.drawCentreString(buf, 120, AVG_Y+2, 1);
}

namespace GridTab {

void drawTab() {
    Display::tft.fillRect(0, TofTabs::CONTENT_Y, Display::SCREEN_W, TofTabs::TAB_Y-TofTabs::CONTENT_Y, Display::Colors::BG);
    for (int i=0; i<64; i++) prevDistances[i]=-1.0f;
    prevAvg=-1.0f;
}
void tick() {
    if (!tofUpdate()) return;
    float sum=0.0f; int valid=0;
    for (int i=0; i<64; i++) {
        float d=TofState::distances[i];
        if (fabsf(d-prevDistances[i])>5.0f) { drawCell(i, d); prevDistances[i]=d; }
        if (d>0.0f) { sum+=d; valid++; }
    }
    float avg = valid>0 ? sum/valid : 0.0f;
    if (fabsf(avg-prevAvg)>5.0f) { drawAvgBar(avg); prevAvg=avg; }
}

} // namespace GridTab
