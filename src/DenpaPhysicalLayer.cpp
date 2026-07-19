#include "DenpaPhysicalLayer.h"

DenpaPhysicalLayer::DenpaPhysicalLayer(Module* module)
    : PhysicalLayer(),
      module_(module) {
    freqStep = 1.0f;
}

Module* DenpaPhysicalLayer::getMod() {
    return module_;
}

void DenpaPhysicalLayer::setHexDumpEnabled(bool enabled) {
    hexDumpEnabled_ = enabled;
    resetHexDump();
}

void DenpaPhysicalLayer::setDacEnabled(bool enabled) {
    dacEnabled_ = enabled;

    if (dacEnabled_) {
        dacWrite(dacPin_, dacIdleLevel());
    }
}

void DenpaPhysicalLayer::setDacLevels(uint8_t markLevel, uint8_t spaceLevel) {
    markLevel_ = markLevel;
    spaceLevel_ = spaceLevel;
}

void DenpaPhysicalLayer::setDacPin(uint8_t pin) {
    dacPin_ = pin;
}

int16_t DenpaPhysicalLayer::setEncoding(uint8_t encoding) {
    Serial.print("[Denpa] Encoding: 0x");
    Serial.println(encoding, HEX);
    return RADIOLIB_ERR_NONE;
}

int16_t DenpaPhysicalLayer::setDataShaping(uint8_t shaping) {
    Serial.print("[Denpa] Shaping: 0x");
    Serial.println(shaping, HEX);
    return RADIOLIB_ERR_NONE;
}

int16_t DenpaPhysicalLayer::setFrequencyDeviation(float deviation) {
    Serial.print("[Denpa] Frequency deviation: ");
    Serial.println(deviation, 3);
    return RADIOLIB_ERR_NONE;
}

int16_t DenpaPhysicalLayer::transmitDirect(uint32_t frf) {
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

bool DenpaPhysicalLayer::decodeBit(uint32_t frf) const {
    return frf < VirtualCenterFrequencyHz;
}

uint8_t DenpaPhysicalLayer::dacIdleLevel() const {
    return (static_cast<uint16_t>(markLevel_) + static_cast<uint16_t>(spaceLevel_)) / 2;
}

void DenpaPhysicalLayer::writeDacBit(bool bit) {
    dacWrite(dacPin_, bit ? markLevel_ : spaceLevel_);
}

void DenpaPhysicalLayer::appendBit(bool bit) {
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

void DenpaPhysicalLayer::printByte(uint8_t value) {
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

int16_t DenpaPhysicalLayer::standby() {
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
        Serial.print("[Denpa] Total bits: ");
        Serial.println(totalBits_);

        Serial.print("[Denpa] Total bytes: ");
        Serial.println((totalBits_ + 7) / 8);
    } else {
        Serial.print("[Denpa] TX bits: ");
        Serial.println(totalBits_);
    }

    resetHexDump();

    return RADIOLIB_ERR_NONE;
}

void DenpaPhysicalLayer::resetHexDump() {
    currentByte_ = 0;
    bitsInByte_ = 0;
    totalBits_ = 0;
    bytesOnLine_ = 0;
}
