#pragma once

#include "hardware/BoardProfileTypes.h"

namespace transitink::hardware {

// AZSMZ EPaper 2.9"（ESP8266 / ESP32 两用主板）
// 接线约定（按 AZSMZ-EPAPER 文档）：
//   BUSY=4, RST=2, DC=5, CS=15, CLK=14, DIN=13
// Mini 版本：RST=-1（接 ESP32 BOOT），USR_BTN=5；标准版 USR_BTN=12。
inline constexpr BoardProfile kBoardProfileAzsmzEpaper29{
    "azsmz_epaper_29",
    "AZSMZ EPaper 2.9",
    {
        DisplayDriverKind::Uc8151,
        296,      // width
        128,      // height
        5,        // DC
        15,       // chip select
        14,       // clock
        13,       // MOSI
        2,        // reset
        4,        // busy
        -1,       // panel power (unconfigured)
        1,        // panel power active HIGH
        0,        // busy active LOW
        2000000,  // SPI frequency
    },
    {
        false,    // battery available
        -1,       // ADC
        1,        // voltage multiplier
        AdcAttenuation::Db0,
        -1,       // sense power
        0,        // sense power active HIGH
        -1,       // charging detect
        0,        // charging active LOW
        -1,       // charge-full detect
        0,        // charge-full active HIGH
        -1,       // charge-indicator
        0,        // indicator active LOW
        1,        // ADC samples
    },
    {
        12,  // home / user button (standard), Mini: 5
        12,  // volume up
        12,  // volume down
        12,  // config
        12,  // factory-reset up
        12,  // factory-reset down
        PinBias::PullUp,
        0,
        true,
        true,
        5000,
        30,
        1200,
    },
};

}  // namespace transitink::hardware
