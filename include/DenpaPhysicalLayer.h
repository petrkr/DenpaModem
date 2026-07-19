#pragma once

#include <Arduino.h>
#include <RadioLib.h>

class DenpaPhysicalLayer : public PhysicalLayer {
public:
    explicit DenpaPhysicalLayer(Module* module);

    int16_t transmitDirect(uint32_t frf = 0) override;
    int16_t standby() override;

    int16_t setEncoding(uint8_t encoding) override;
    int16_t setDataShaping(uint8_t shaping) override;
    int16_t setFrequencyDeviation(float deviation) override;

private:
    Module* getMod() override;

    Module* module_;

    uint32_t centerFrequency_ = 0;
    bool centerKnown_ = false;

    uint8_t currentByte_ = 0;
    uint8_t bitsInByte_ = 0;
    size_t totalBits_ = 0;
    size_t bytesOnLine_ = 0;

    void appendBit(bool bit);
    void printByte(uint8_t value);
};
