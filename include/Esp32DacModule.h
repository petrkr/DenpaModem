#pragma once

#include <Arduino.h>

#include "Esp32DacPhy.h"

class Esp32DacModule {
public:
    explicit Esp32DacModule(Esp32DacPhy& phy);

    void begin();

    void setPin(uint8_t pin);
    void setLevels(uint8_t markLevel, uint8_t spaceLevel);
    void setEnabled(bool enabled);
    void setHexDumpEnabled(bool enabled);

    bool isEnabled() const;
    bool isHexDumpEnabled() const;
    uint8_t getPin() const;
    uint8_t getMarkLevel() const;
    uint8_t getSpaceLevel() const;

private:
    Esp32DacPhy& phy_;

    uint8_t pin_ = 25;
    uint8_t markLevel_ = 235;
    uint8_t spaceLevel_ = 20;
    bool enabled_ = true;
    bool hexDumpEnabled_ = false;
};
