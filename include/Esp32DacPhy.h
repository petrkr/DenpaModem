#pragma once

#include <Arduino.h>
#include <RadioLib.h>

class Esp32DacPhy : public PhysicalLayer {
public:
    static constexpr float VirtualCenterFrequencyMHz = 100.0f;

    explicit Esp32DacPhy(Module* module);

    int16_t transmitDirect(uint32_t frf = 0) override;
    int16_t standby() override;

    int16_t setEncoding(uint8_t encoding) override;
    int16_t setDataShaping(uint8_t shaping) override;
    int16_t setFrequencyDeviation(float deviation) override;

    void setHexDumpEnabled(bool enabled);
    void setDacEnabled(bool enabled);
    void setDacLevels(uint8_t markLevel, uint8_t spaceLevel);
    void setDacPin(uint8_t pin);

    bool isHexDumpEnabled() const;
    bool isDacEnabled() const;
    uint8_t getDacPin() const;
    uint8_t getMarkLevel() const;
    uint8_t getSpaceLevel() const;

private:
    Module* getMod() override;

    Module* module_;

    bool hexDumpEnabled_ = false;
    bool dacEnabled_ = true;
    static constexpr uint32_t VirtualCenterFrequencyHz = 100000000UL;

    uint8_t dacPin_ = 25;
    uint8_t markLevel_ = 138;
    uint8_t spaceLevel_ = 118;

    uint8_t currentByte_ = 0;
    uint8_t bitsInByte_ = 0;
    size_t totalBits_ = 0;
    size_t bytesOnLine_ = 0;

    bool decodeBit(uint32_t frf) const;
    uint8_t dacIdleLevel() const;
    void writeDacBit(bool bit);
    void appendBit(bool bit);
    void printByte(uint8_t value);
    void resetHexDump();
};
