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

DenpaPhysicalLayer phy(&module);
PagerClient pager(&phy);

int16_t state;

void setup() {
    Serial.begin(115200);
    delay(1500);

    Serial.println();
    Serial.println("DenpaModem POCSAG DAC test");

    phy.setDacPin(25);
    phy.setDacLevels(150, 50);
    phy.setDacEnabled(true);

    // Hex dump debug prints decoded POCSAG bytes. It can disturb TX timing.
    phy.setHexDumpEnabled(false);
    // phy.setHexDumpEnabled(true);

    /*
     * The base frequency is virtual. Keep it tied to DenpaPhysicalLayer so
     * PagerClient and the fixed bit decision threshold use the same value.
     *
     * speed: 1200 baud
     * invert: false
     * shift: 4500 Hz
     */
     state = pager.begin(
        DenpaPhysicalLayer::VirtualCenterFrequencyMHz,
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
