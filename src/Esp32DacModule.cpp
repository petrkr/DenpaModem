#include "Esp32DacModule.h"

Esp32DacModule::Esp32DacModule(Esp32DacPhy& phy)
    : phy_(phy) {
}

void Esp32DacModule::begin() {
    phy_.setDacPin(pin_);
    phy_.setDacLevels(markLevel_, spaceLevel_);
    phy_.setDacEnabled(enabled_);
    phy_.setHexDumpEnabled(hexDumpEnabled_);
}

void Esp32DacModule::setPin(uint8_t pin) {
    pin_ = pin;
    phy_.setDacPin(pin_);
}

void Esp32DacModule::setLevels(uint8_t markLevel, uint8_t spaceLevel) {
    markLevel_ = markLevel;
    spaceLevel_ = spaceLevel;
    phy_.setDacLevels(markLevel_, spaceLevel_);
}

void Esp32DacModule::setEnabled(bool enabled) {
    enabled_ = enabled;
    phy_.setDacEnabled(enabled_);
}

void Esp32DacModule::setHexDumpEnabled(bool enabled) {
    hexDumpEnabled_ = enabled;
    phy_.setHexDumpEnabled(hexDumpEnabled_);
}

bool Esp32DacModule::isEnabled() const {
    return enabled_;
}

bool Esp32DacModule::isHexDumpEnabled() const {
    return hexDumpEnabled_;
}

uint8_t Esp32DacModule::getPin() const {
    return pin_;
}

uint8_t Esp32DacModule::getMarkLevel() const {
    return markLevel_;
}

uint8_t Esp32DacModule::getSpaceLevel() const {
    return spaceLevel_;
}
