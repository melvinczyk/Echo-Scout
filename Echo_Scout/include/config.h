#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>

// Screen IDs
#define SCREEN_MENU     0
#define SCREEN_RADAR    1
#define SCREEN_SETTINGS 2
#define SCREEN_IMU      3
#define SCREEN_BATTERY  4
#define SCREEN_POWER_CONFIRM 5

namespace Config {
    // Serial
    constexpr uint8_t RADAR_RX = 44;
    constexpr uint8_t RADAR_TX = 43;
    constexpr uint32_t RADAR_BAUD = 256000;

    // Touch
    constexpr uint8_t TOUCH_SDA = 16;
    constexpr uint8_t TOUCH_SCL = 15;
    constexpr uint8_t TOUCH_RST = 18;
    constexpr uint8_t TOUCH_INT = 17;
    constexpr uint8_t TOUCH_ADDR = 0x38;

    // Screen dimensions
    constexpr int SCREEN_H = 320;
    constexpr int SCREEN_W = 240;
    constexpr int HEADER_H = 28;
    constexpr int DASH_Y = 30;
    constexpr int DASH_H = 54;
    constexpr int APEX_X = 120;
    constexpr int APEX_Y = 316;

    // Colors
    constexpr uint16_t C_BG = 0x0000;
    constexpr uint16_t C_GREEN = 0x07E0;
    constexpr uint16_t C_GREEN_DIM = 0x0300;
    constexpr uint16_t C_GREEN_FAINT = 0x0180;
    constexpr uint16_t C_AMBER = 0xFAE0;
    constexpr uint16_t C_WHITE = 0xFFFF;
    constexpr uint16_t C_SEP = 0x02C0;
    constexpr uint16_t C_RED = 0xF800;

    // Blip
    constexpr int BLIP_RADIUS = 5;
    constexpr uint16_t BLIP_COLOR = 0xF800;

    // Radar
    constexpr float MAX_RANGE_MM = 5000.0f;

    // Sensors
    constexpr int     IMU_ADDRESS  = 0x4A;
    constexpr uint8_t TFT_BL_PIN   = 45;
    constexpr uint8_t BL_PWM_CH    =  0;
    constexpr uint32_t BL_PWM_FREQ = 20000;
    constexpr uint8_t BL_PWM_RES   =  8;
    constexpr uint8_t WAKE_PIN      =  0;
}


#define CONE_TOP (Config::DASH_Y + Config::DASH_H + 1)
#define CONE_LEN (Config::APEX_Y - CONE_TOP - 4)

inline bool inRect(int tx, int ty, int rx, int ry, int rw, int rh) {
    return tx >= rx && tx <= rx + rw && ty >= ry && ty <= ry + rh;
}

#endif