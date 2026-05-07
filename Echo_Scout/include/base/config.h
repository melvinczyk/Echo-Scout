#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>

namespace Config {
    constexpr uint8_t RADAR_RX = 44;
    constexpr uint8_t RADAR_TX = 43;
    constexpr uint32_t RADAR_BAUD = 256000;

    constexpr uint8_t TOUCH_SDA = 16;
    constexpr uint8_t TOUCH_SCL = 15;
    constexpr uint8_t TOUCH_RST = 18;
    constexpr uint8_t TOUCH_INT = 17;
    constexpr uint8_t TOUCH_ADDR = 0x38;

    constexpr float MAX_RANGE_MM = 8000.0f;

    constexpr int IMU_ADDRESS = 0x4A;
    constexpr int TOF_ADDRESS = 0x29;
    constexpr uint8_t TFT_BL_PIN = 45;
    constexpr uint8_t BL_PWM_CH = 0;
    constexpr uint32_t BL_PWM_FREQ = 20000;
    constexpr uint8_t BL_PWM_RES = 8;
    constexpr uint8_t WAKE_PIN = TOUCH_INT;

    constexpr uint8_t BAT_ADC_PIN = 9;
    constexpr uint16_t BAT_MV_FULL = 4200;
    constexpr uint16_t BAT_MV_EMPTY = 3400;
    constexpr uint16_t BAT_CAPACITY_MAH = 1000;
}

#endif
