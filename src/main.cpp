/* Copyright (C) 2026 Petr Kracik - OK1PKR
 *
 * This file is part of DenpaModem
 */

#include <Arduino.h>
#include <RadioLib.h>

#include "Esp32DacModule.h"
#include "Esp32DacPhy.h"
#include "PocsagModule.h"
#include "SerialCommand.h"

Module module(
    RADIOLIB_NC,
    RADIOLIB_NC,
    RADIOLIB_NC,
    RADIOLIB_NC
);

Esp32DacPhy phy(&module);
Esp32DacModule dac(phy);
PocsagModule pocsag(phy);
SerialCommand serialCommand(Serial, dac, pocsag);

void setup() {
    Serial.begin(115200);
    delay(1500);

    Serial.println();
    Serial.println("DenpaModem ESP32 DAC test");

    dac.begin();

    if (!pocsag.begin()) {
        return;
    }

    serialCommand.begin();
}

void loop() {
    serialCommand.poll();
}
