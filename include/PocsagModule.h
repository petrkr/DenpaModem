#pragma once

#include <Arduino.h>
#include <RadioLib.h>

#include "Esp32DacPhy.h"

class PocsagModule {
public:
    explicit PocsagModule(Esp32DacPhy& phy);

    bool begin();
    int16_t send(uint32_t address, const char* message, uint8_t encoding);

    void setSpeed(uint16_t speed);
    uint16_t getSpeed() const;
    bool isReady() const;

private:
    Esp32DacPhy& phy_;
    PagerClient pager_;
    uint16_t speed_ = 1200;
    bool ready_ = false;
};
