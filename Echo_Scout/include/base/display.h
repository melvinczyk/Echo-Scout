#ifndef DISPLAY_H
#define DISPLAY_H

#include <cstdint>
#include <TFT_eSPI.h>
#include "base/config.h"

namespace Display {

    extern TFT_eSPI tft;
    extern TFT_eSprite spr;

    inline constexpr int SCREEN_H = 320;
    inline constexpr int SCREEN_W = 240;
    inline constexpr int HEADER_H = 28;

    inline constexpr int CAL_BTN_X = 173;
    inline constexpr int CAL_BTN_Y = 3;
    inline constexpr int CAL_BTN_W = 64;
    inline constexpr int CAL_BTN_H = HEADER_H - 6;

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
        SPIRIT,
        TOF,
        MAP3D
    };

    struct Button {
        uint16_t x;
        uint16_t y;
        uint16_t w;
        uint16_t h;
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
    void drawLoadingScreen();
    void drawButton(const Button& button);
    void drawHeader(const char* title, bool showCal = true);
    void drawErrorScreen(const char* text);
    void drawAsciiArt(const AsciiArt& art, Colors col);
}

#endif
