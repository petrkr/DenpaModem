/* Copyright (C) 2026 Petr Kracik - OK1PKR
 *
 * This file is part of DenpaModem
 */

#include <Arduino.h>
#include <RadioLib.h>
#include <stdlib.h>
#include <string.h>

#include "Esp32DacPhy.h"

Module module(
    RADIOLIB_NC,
    RADIOLIB_NC,
    RADIOLIB_NC,
    RADIOLIB_NC
);

Esp32DacPhy phy(&module);
PagerClient pager(&phy);

static constexpr size_t CommandBufferSize = 192;

char commandBuffer[CommandBufferSize];
size_t commandLength = 0;
bool lastCharWasLineEnd = false;
bool serialEchoEnabled = true;

bool pagerReady = false;
uint16_t pocsagSpeed = 1200;
uint8_t dacPin = 25;
uint8_t dacMarkLevel = 235;
uint8_t dacSpaceLevel = 20;
bool dacEnabled = true;
bool hexDumpEnabled = false;

char* skipSpaces(char* text) {
    while (*text == ' ' || *text == '\t') {
        text++;
    }

    return text;
}

char* readToken(char*& text) {
    text = skipSpaces(text);

    if (*text == '\0') {
        return nullptr;
    }

    char* token = text;

    while (*text != '\0' && *text != ' ' && *text != '\t') {
        text++;
    }

    if (*text != '\0') {
        *text = '\0';
        text++;
    }

    return token;
}

bool equalsIgnoreCase(const char* a, const char* b) {
    while (*a != '\0' && *b != '\0') {
        char ca = *a;
        char cb = *b;

        if (ca >= 'a' && ca <= 'z') {
            ca -= ('a' - 'A');
        }

        if (cb >= 'a' && cb <= 'z') {
            cb -= ('a' - 'A');
        }

        if (ca != cb) {
            return false;
        }

        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

bool parseByte(const char* text, uint8_t& value) {
    char* end = nullptr;
    const unsigned long parsed = strtoul(text, &end, 10);

    if (*text == '\0' || *end != '\0' || parsed > 255) {
        return false;
    }

    value = static_cast<uint8_t>(parsed);
    return true;
}

bool parseUint16(const char* text, uint16_t& value) {
    char* end = nullptr;
    const unsigned long parsed = strtoul(text, &end, 10);

    if (*text == '\0' || *end != '\0' || parsed > 65535) {
        return false;
    }

    value = static_cast<uint16_t>(parsed);
    return true;
}

bool parseBool(const char* text, bool& value) {
    if (equalsIgnoreCase(text, "1") || equalsIgnoreCase(text, "on") || equalsIgnoreCase(text, "true")) {
        value = true;
        return true;
    }

    if (equalsIgnoreCase(text, "0") || equalsIgnoreCase(text, "off") || equalsIgnoreCase(text, "false")) {
        value = false;
        return true;
    }

    return false;
}

bool parseAddress(const char* text, uint32_t& address) {
    char* end = nullptr;
    const unsigned long parsed = strtoul(text, &end, 10);

    if (*text == '\0' || *end != '\0' || parsed > RADIOLIB_PAGER_ADDRESS_MAX) {
        return false;
    }

    address = static_cast<uint32_t>(parsed);
    return true;
}

void printHelp() {
    Serial.println("OK commands:");
    Serial.println("  TX POCSAG <addr> <message>");
    Serial.println("  TX POCSAG <addr> ASCII <message>");
    Serial.println("  TX POCSAG <addr> BCD <message>");
    Serial.println("  POCSAG CONFIG <speed>");
    Serial.println("  DAC LEVELS <markHigh> <spaceLow>");
    Serial.println("  DAC PIN <pin>");
    Serial.println("  DAC ENABLE <0|1>");
    Serial.println("  DEBUG HEX <0|1>");
    Serial.println("  ECHO <0|1>");
    Serial.println("  STATUS");
}

void printStatus() {
    Serial.print("OK pager=");
    Serial.print(pagerReady ? 1 : 0);
    Serial.print(" dac=");
    Serial.print(dacEnabled ? 1 : 0);
    Serial.print(" pin=");
    Serial.print(dacPin);
    Serial.print(" mark=");
    Serial.print(dacMarkLevel);
    Serial.print(" space=");
    Serial.print(dacSpaceLevel);
    Serial.print(" hex=");
    Serial.print(hexDumpEnabled ? 1 : 0);
    Serial.print(" echo=");
    Serial.print(serialEchoEnabled ? 1 : 0);
    Serial.print(" pocsag_speed=");
    Serial.println(pocsagSpeed);
}

bool beginPager() {
    const int16_t state = pager.begin(
        Esp32DacPhy::VirtualCenterFrequencyMHz,
        pocsagSpeed
    );

    Serial.print("pager.begin(): ");
    Serial.println(state);

    pagerReady = state == RADIOLIB_ERR_NONE;
    return pagerReady;
}

void handleTxCommand(char* args) {
    char* protocol = readToken(args);

    if (protocol == nullptr || !equalsIgnoreCase(protocol, "POCSAG")) {
        Serial.println("ERR unsupported-protocol");
        return;
    }

    char* addressText = readToken(args);

    if (addressText == nullptr) {
        Serial.println("ERR missing-address");
        return;
    }

    uint32_t address = 0;

    if (!parseAddress(addressText, address)) {
        Serial.println("ERR invalid-address");
        return;
    }

    args = skipSpaces(args);

    if (*args == '\0') {
        Serial.println("ERR missing-message");
        return;
    }

    uint8_t encoding = RADIOLIB_PAGER_ASCII;
    char* message = args;
    char* encodingEnd = args;

    while (*encodingEnd != '\0' && *encodingEnd != ' ' && *encodingEnd != '\t') {
        encodingEnd++;
    }

    const char saved = *encodingEnd;
    *encodingEnd = '\0';

    if (equalsIgnoreCase(args, "ASCII")) {
        encoding = RADIOLIB_PAGER_ASCII;
        *encodingEnd = saved;
        message = skipSpaces(encodingEnd);
    } else if (equalsIgnoreCase(args, "BCD")) {
        encoding = RADIOLIB_PAGER_BCD;
        *encodingEnd = saved;
        message = skipSpaces(encodingEnd);
    } else {
        *encodingEnd = saved;
    }

    if (*message == '\0') {
        Serial.println("ERR missing-message");
        return;
    }

    if (!pagerReady) {
        Serial.println("ERR pager-not-ready");
        return;
    }

    Serial.println("OK tx-start");
    const int16_t state = pager.transmit(message, address, encoding);

    Serial.print(state == RADIOLIB_ERR_NONE ? "OK tx-done state=" : "ERR tx-failed state=");
    Serial.println(state);
}

void handleDacCommand(char* args) {
    char* subcommand = readToken(args);

    if (subcommand == nullptr) {
        Serial.println("ERR missing-dac-command");
        return;
    }

    if (equalsIgnoreCase(subcommand, "LEVELS")) {
        char* markText = readToken(args);
        char* spaceText = readToken(args);
        uint8_t mark = 0;
        uint8_t space = 0;

        if (markText == nullptr || spaceText == nullptr || !parseByte(markText, mark) || !parseByte(spaceText, space)) {
            Serial.println("ERR invalid-levels");
            return;
        }

        dacMarkLevel = mark;
        dacSpaceLevel = space;
        phy.setDacLevels(dacMarkLevel, dacSpaceLevel);
        Serial.println("OK");
        return;
    }

    if (equalsIgnoreCase(subcommand, "PIN")) {
        char* pinText = readToken(args);
        uint8_t pin = 0;

        if (pinText == nullptr || !parseByte(pinText, pin)) {
            Serial.println("ERR invalid-pin");
            return;
        }

        dacPin = pin;
        phy.setDacPin(dacPin);
        Serial.println("OK");
        return;
    }

    if (equalsIgnoreCase(subcommand, "ENABLE")) {
        char* enabledText = readToken(args);
        bool enabled = false;

        if (enabledText == nullptr || !parseBool(enabledText, enabled)) {
            Serial.println("ERR invalid-enable");
            return;
        }

        dacEnabled = enabled;
        phy.setDacEnabled(dacEnabled);
        Serial.println("OK");
        return;
    }

    Serial.println("ERR unknown-dac-command");
}

void handlePocsagCommand(char* args) {
    char* subcommand = readToken(args);

    if (subcommand == nullptr || !equalsIgnoreCase(subcommand, "CONFIG")) {
        Serial.println("ERR unknown-pocsag-command");
        return;
    }

    char* speedText = readToken(args);
    uint16_t speed = 0;

    if (speedText == nullptr || !parseUint16(speedText, speed)) {
        Serial.println("ERR invalid-pocsag-config");
        return;
    }

    pocsagSpeed = speed;

    if (beginPager()) {
        Serial.println("OK");
    } else {
        Serial.println("ERR pager-begin-failed");
    }
}

void handleDebugCommand(char* args) {
    char* subcommand = readToken(args);

    if (subcommand == nullptr || !equalsIgnoreCase(subcommand, "HEX")) {
        Serial.println("ERR unknown-debug-command");
        return;
    }

    char* enabledText = readToken(args);
    bool enabled = false;

    if (enabledText == nullptr || !parseBool(enabledText, enabled)) {
        Serial.println("ERR invalid-debug-value");
        return;
    }

    hexDumpEnabled = enabled;
    phy.setHexDumpEnabled(hexDumpEnabled);
    Serial.println("OK");
}

void handleEchoCommand(char* args) {
    char* enabledText = readToken(args);
    bool enabled = false;

    if (enabledText == nullptr || !parseBool(enabledText, enabled)) {
        Serial.println("ERR invalid-echo-value");
        return;
    }

    serialEchoEnabled = enabled;
    Serial.println("OK");
}

void handleCommand(char* line) {
    char* cursor = line;
    char* command = readToken(cursor);

    if (command == nullptr) {
        return;
    }

    if (equalsIgnoreCase(command, "HELP")) {
        printHelp();
        return;
    }

    if (equalsIgnoreCase(command, "STATUS")) {
        printStatus();
        return;
    }

    if (equalsIgnoreCase(command, "TX")) {
        handleTxCommand(cursor);
        return;
    }

    if (equalsIgnoreCase(command, "DAC")) {
        handleDacCommand(cursor);
        return;
    }

    if (equalsIgnoreCase(command, "POCSAG")) {
        handlePocsagCommand(cursor);
        return;
    }

    if (equalsIgnoreCase(command, "DEBUG")) {
        handleDebugCommand(cursor);
        return;
    }

    if (equalsIgnoreCase(command, "ECHO")) {
        handleEchoCommand(cursor);
        return;
    }

    Serial.println("ERR unknown-command");
}

void pollSerialCommands() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());

        if (c == '\r' || c == '\n') {
            if (serialEchoEnabled) {
                Serial.println();
            }

            if (!lastCharWasLineEnd) {
                commandBuffer[commandLength] = '\0';
                handleCommand(commandBuffer);
            }

            commandLength = 0;
            lastCharWasLineEnd = true;
            continue;
        }

        lastCharWasLineEnd = false;

        if (c == '\b' || c == 0x7F) {
            if (commandLength > 0) {
                commandLength--;

                if (serialEchoEnabled) {
                    Serial.print("\b \b");
                }
            }

            continue;
        }

        if (commandLength >= CommandBufferSize - 1) {
            commandLength = 0;
            Serial.println("ERR line-too-long");
            continue;
        }

        commandBuffer[commandLength++] = c;

        if (serialEchoEnabled && c >= 0x20 && c <= 0x7E) {
            Serial.write(c);
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1500);

    Serial.println();
    Serial.println("DenpaModem ESP32 DAC test");

    phy.setDacPin(dacPin);
    phy.setDacLevels(dacMarkLevel, dacSpaceLevel);
    phy.setDacEnabled(dacEnabled);

    // Hex dump debug prints decoded POCSAG bytes. It can disturb TX timing.
    phy.setHexDumpEnabled(hexDumpEnabled);
    // phy.setHexDumpEnabled(true);

    if(!beginPager()) {
        return;
    }

    printHelp();
}

void loop() {
    pollSerialCommands();
}
