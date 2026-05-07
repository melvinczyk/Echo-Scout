#ifndef TAB_MANAGER_H
#define TAB_MANAGER_H

#include <cstdint>

namespace TabManager {

    constexpr int TAB_Y = 280;
    constexpr int TAB_H = 40;

    struct TabEntry {
        const char* label;
        void (*enter)();
        void (*tick)();
        void (*onTouch)(int tx, int ty);
    };

    void init(const TabEntry* tabs, uint8_t count, uint8_t defaultTab = 0);
    void switchTab(uint8_t idx);
    void tick();
    bool handleTouch(int tx, int ty);
    void drawTabBar();
    uint8_t activeIndex();

}

#endif
