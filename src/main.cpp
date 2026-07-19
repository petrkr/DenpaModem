/* Copyright (C) 2026 Petr Kracik - OK1PKR
 *
 * This file is part of DenpaModem
 */

#include <Arduino.h>
#include <RadioLib.h>

#include "DenpaPhysicalLayer.h"

Module module(
    RADIOLIB_NC,
    RADIOLIB_NC,
    RADIOLIB_NC,
    RADIOLIB_NC
);

DenpaPhysicalLayer phy(&module, 100000000UL);
PagerClient pager(&phy);

int16_t state;

void setup() {
    Serial.begin(115200);
    delay(1500);

    Serial.println();
    Serial.println("DenpaModem POCSAG DAC test");

    phy.setDacPin(25);
    phy.setDacLevels(150, 50);

    // Use OutputMode::HexDump to inspect decoded POCSAG bytes on Serial.
    // Use OutputMode::Dac to send the same decoded bit stream to ESP32 DAC.
    // phy.setOutputMode(OutputMode::HexDump);
    phy.setOutputMode(OutputMode::Dac);

    /*
     * The base frequency is virtual. RadioLib calls transmitDirect() with
     * centerFrequency +/- shift, and DenpaPhysicalLayer maps that to bits.
     *
     * speed: 1200 baud
     * invert: false
     * shift: 4500 Hz
     */
     state = pager.begin(
        100.0f,
        1200
    );

    Serial.print("pager.begin(): ");
    Serial.println(state);

    if(state != RADIOLIB_ERR_NONE) {
        return;
    }
}

void loop() {
    state = pager.transmit(
        "ahoj",
        1234,
        RADIOLIB_PAGER_ASCII
    );

    Serial.print("pager.transmit(): ");
    Serial.println(state);
    delay(5000);
}
