#include "PocsagModule.h"

PocsagModule::PocsagModule(Esp32DacPhy& phy)
    : phy_(phy),
      pager_(&phy_) {
}

bool PocsagModule::begin() {
    const int16_t state = pager_.begin(
        Esp32DacPhy::VirtualCenterFrequencyMHz,
        speed_
    );

    Serial.print("pager.begin(): ");
    Serial.println(state);

    ready_ = state == RADIOLIB_ERR_NONE;
    return ready_;
}

int16_t PocsagModule::send(uint32_t address, const char* message, uint8_t encoding) {
    if (!ready_) {
        return RADIOLIB_ERR_UNKNOWN;
    }

    return pager_.transmit(message, address, encoding);
}

void PocsagModule::setSpeed(uint16_t speed) {
    speed_ = speed;
}

uint16_t PocsagModule::getSpeed() const {
    return speed_;
}

bool PocsagModule::isReady() const {
    return ready_;
}
