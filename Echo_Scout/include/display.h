#ifndef DISPLAY_H
#define DISPLAY_H

#include <cstdint>
#include <TFT_eSPI.h>
#include "config.h"

namespace Display {

    extern TFT_eSPI tft;
    extern TFT_eSprite spr;

    inline constexpr int SCREEN_H = 320;
    inline constexpr int SCREEN_W = 240;
    inline constexpr int HEADER_H = 28;

    enum Colors : uint16_t {
        BG = 0x0000,
        GREEN = 0x07E0,
        GREEN_DIM = 0x0300,
        GREEN_FAINT = 0x0180,
        AMBER = 0xFAE0,
        WHITE = 0xFFFF,
        SEP = 0x02C0,
        RED = 0xF800
    };

    enum Screen {
        MENU,
        RADAR,
        SETTINGS,
        IMU,
        BATTERY,
        POWER_CONFIRM,
        HORIZON,
        SPIRIT
    };

    struct Button {
        uint8_t x;
        uint8_t y;
        uint8_t w;
        uint8_t h;
        uint8_t font_size;
        const char* text;
        Colors button_color;
        Colors button_secondary_color;
        Colors text_color;
    };

    struct AsciiArt {
        const char** lines;
        uint8_t num_lines;
        int char_w;
        int line_h;
        int start_x;
        int start_y;
    };

    bool initDisplay();
    void drawButton(const Button& button);
    void drawHeader(const char* title);
    void drawErrorScreen(const char* text);
    void drawAsciiArt(const AsciiArt& art, Colors col);
}

inline bool inRect(int tx, int ty, int rx, int ry, int rw, int rh) {
    return tx >= rx && tx <= rx + rw && ty >= ry && ty <= ry + rh;
}

#endif
