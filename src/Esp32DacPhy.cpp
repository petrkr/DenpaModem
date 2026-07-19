#include "Esp32DacPhy.h"

Esp32DacPhy::Esp32DacPhy(Module* module)
    : PhysicalLayer(),
      module_(module) {
    freqStep = 1.0f;
}

Module* Esp32DacPhy::getMod() {
    return module_;
}

void Esp32DacPhy::setHexDumpEnabled(bool enabled) {
    hexDumpEnabled_ = enabled;
    resetHexDump();
}

void Esp32DacPhy::setDacEnabled(bool enabled) {
    dacEnabled_ = enabled;

    if (dacEnabled_) {
        dacWrite(dacPin_, dacIdleLevel());
    }
}

void Esp32DacPhy::setDacLevels(uint8_t markLevel, uint8_t spaceLevel) {
    markLevel_ = markLevel;
    spaceLevel_ = spaceLevel;
}

void Esp32DacPhy::setDacPin(uint8_t pin) {
    dacPin_ = pin;
}

int16_t Esp32DacPhy::setEncoding(uint8_t encoding) {
    Serial.print("[Esp32DacPhy] Encoding: 0x");
    Serial.println(encoding, HEX);
    return RADIOLIB_ERR_NONE;
}

int16_t Esp32DacPhy::setDataShaping(uint8_t shaping) {
    Serial.print("[Esp32DacPhy] Shaping: 0x");
    Serial.println(shaping, HEX);
    return RADIOLIB_ERR_NONE;
}

int16_t Esp32DacPhy::setFrequencyDeviation(float deviation) {
    Serial.print("[Esp32DacPhy] Frequency deviation: ");
    Serial.println(deviation, 3);
    return RADIOLIB_ERR_NONE;
}

int16_t Esp32DacPhy::transmitDirect(uint32_t frf) {
    const bool bit = decodeBit(frf);

    if (dacEnabled_) {
        writeDacBit(bit);
    }

    if (hexDumpEnabled_) {
        appendBit(bit);
    } else {
        totalBits_++;
    }

    return RADIOLIB_ERR_NONE;
}

bool Esp32DacPhy::decodeBit(uint32_t frf) const {
    return frf < VirtualCenterFrequencyHz;
}

uint8_t Esp32DacPhy::dacIdleLevel() const {
    return (static_cast<uint16_t>(markLevel_) + static_cast<uint16_t>(spaceLevel_)) / 2;
}

void Esp32DacPhy::writeDacBit(bool bit) {
    dacWrite(dacPin_, bit ? markLevel_ : spaceLevel_);
}

void Esp32DacPhy::appendBit(bool bit) {
    currentByte_ <<= 1;

    if (bit) {
        currentByte_ |= 0x01;
    }

    bitsInByte_++;
    totalBits_++;

    if (bitsInByte_ == 8) {
        printByte(currentByte_);

        currentByte_ = 0;
        bitsInByte_ = 0;
    }
}

void Esp32DacPhy::printByte(uint8_t value) {
    if (value < 0x10) {
        Serial.print('0');
    }

    Serial.print(value, HEX);
    Serial.print(' ');

    bytesOnLine_++;

    if (bytesOnLine_ >= 16) {
        Serial.println();
        bytesOnLine_ = 0;
    }
}

int16_t Esp32DacPhy::standby() {
    if (dacEnabled_) {
        dacWrite(dacPin_, dacIdleLevel());
    }

    if (hexDumpEnabled_) {
        if (bitsInByte_ != 0) {
            currentByte_ <<= (8 - bitsInByte_);
            printByte(currentByte_);

            currentByte_ = 0;
            bitsInByte_ = 0;
        }

        if (bytesOnLine_ != 0) {
            Serial.println();
        }

        Serial.println();
        Serial.print("[Esp32DacPhy] Total bits: ");
        Serial.println(totalBits_);

        Serial.print("[Esp32DacPhy] Total bytes: ");
        Serial.println((totalBits_ + 7) / 8);
    } else {
        Serial.print("[Esp32DacPhy] TX bits: ");
        Serial.println(totalBits_);
    }

    resetHexDump();

    return RADIOLIB_ERR_NONE;
}

void Esp32DacPhy::resetHexDump() {
    currentByte_ = 0;
    bitsInByte_ = 0;
    totalBits_ = 0;
    bytesOnLine_ = 0;
}
