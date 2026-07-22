#pragma once

#include <Arduino.h>

struct DapnetMessage {
    static constexpr size_t MAX_TEXT_LENGTH = 256;

    uint8_t packetId = 0;
    uint8_t type = 0;
    uint8_t subType = 0;
    uint32_t ric = 0;
    uint8_t function = 0;
    char text[MAX_TEXT_LENGTH] = {};
};
