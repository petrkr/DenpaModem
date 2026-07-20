#include "SerialCommand.h"

#include <stdlib.h>

SerialCommand::SerialCommand(Stream& stream, Esp32DacModule& dac, PocsagModule& pocsag)
    : stream_(stream),
      dac_(dac),
      pocsag_(pocsag) {
}

void SerialCommand::begin() {
    printHelp();
}

char* SerialCommand::skipSpaces(char* text) {
    while (*text == ' ' || *text == '\t') {
        text++;
    }

    return text;
}

char* SerialCommand::readToken(char*& text) {
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

bool SerialCommand::equalsIgnoreCase(const char* a, const char* b) {
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

bool SerialCommand::parseByte(const char* text, uint8_t& value) {
    char* end = nullptr;
    const unsigned long parsed = strtoul(text, &end, 10);

    if (*text == '\0' || *end != '\0' || parsed > 255) {
        return false;
    }

    value = static_cast<uint8_t>(parsed);
    return true;
}

bool SerialCommand::parseUint16(const char* text, uint16_t& value) {
    char* end = nullptr;
    const unsigned long parsed = strtoul(text, &end, 10);

    if (*text == '\0' || *end != '\0' || parsed > 65535) {
        return false;
    }

    value = static_cast<uint16_t>(parsed);
    return true;
}

bool SerialCommand::parseBool(const char* text, bool& value) {
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

bool SerialCommand::parseAddress(const char* text, uint32_t& address) {
    char* end = nullptr;
    const unsigned long parsed = strtoul(text, &end, 10);

    if (*text == '\0' || *end != '\0' || parsed > RADIOLIB_PAGER_ADDRESS_MAX) {
        return false;
    }

    address = static_cast<uint32_t>(parsed);
    return true;
}

void SerialCommand::printHelp() {
    stream_.println("OK commands:");
    stream_.println("  TX POCSAG <addr> <message>");
    stream_.println("  TX POCSAG <addr> ASCII <message>");
    stream_.println("  TX POCSAG <addr> BCD <message>");
    stream_.println("  POCSAG CONFIG <speed>");
    stream_.println("  DAC LEVELS <markHigh> <spaceLow>");
    stream_.println("  DAC PIN <pin>");
    stream_.println("  DAC ENABLE <0|1>");
    stream_.println("  DEBUG HEX <0|1>");
    stream_.println("  ECHO <0|1>");
    stream_.println("  STATUS");
}

void SerialCommand::printStatus() {
    stream_.print("OK pager=");
    stream_.print(pocsag_.isReady() ? 1 : 0);
    stream_.print(" dac=");
    stream_.print(dac_.isEnabled() ? 1 : 0);
    stream_.print(" pin=");
    stream_.print(dac_.getPin());
    stream_.print(" mark=");
    stream_.print(dac_.getMarkLevel());
    stream_.print(" space=");
    stream_.print(dac_.getSpaceLevel());
    stream_.print(" hex=");
    stream_.print(dac_.isHexDumpEnabled() ? 1 : 0);
    stream_.print(" echo=");
    stream_.print(echoEnabled_ ? 1 : 0);
    stream_.print(" pocsag_speed=");
    stream_.println(pocsag_.getSpeed());
}

void SerialCommand::handleTxCommand(char* args) {
    char* protocol = readToken(args);

    if (protocol == nullptr || !equalsIgnoreCase(protocol, "POCSAG")) {
        stream_.println("ERR unsupported-protocol");
        return;
    }

    char* addressText = readToken(args);

    if (addressText == nullptr) {
        stream_.println("ERR missing-address");
        return;
    }

    uint32_t address = 0;

    if (!parseAddress(addressText, address)) {
        stream_.println("ERR invalid-address");
        return;
    }

    args = skipSpaces(args);

    if (*args == '\0') {
        stream_.println("ERR missing-message");
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
        stream_.println("ERR missing-message");
        return;
    }

    if (!pocsag_.isReady()) {
        stream_.println("ERR pager-not-ready");
        return;
    }

    stream_.println("OK tx-start");
    const int16_t state = pocsag_.send(address, message, encoding);

    stream_.print(state == RADIOLIB_ERR_NONE ? "OK tx-done state=" : "ERR tx-failed state=");
    stream_.println(state);
}

void SerialCommand::handleDacCommand(char* args) {
    char* subcommand = readToken(args);

    if (subcommand == nullptr) {
        stream_.println("ERR missing-dac-command");
        return;
    }

    if (equalsIgnoreCase(subcommand, "LEVELS")) {
        char* markText = readToken(args);
        char* spaceText = readToken(args);
        uint8_t mark = 0;
        uint8_t space = 0;

        if (markText == nullptr || spaceText == nullptr || !parseByte(markText, mark) || !parseByte(spaceText, space)) {
            stream_.println("ERR invalid-levels");
            return;
        }

        dac_.setLevels(mark, space);
        stream_.println("OK");
        return;
    }

    if (equalsIgnoreCase(subcommand, "PIN")) {
        char* pinText = readToken(args);
        uint8_t pin = 0;

        if (pinText == nullptr || !parseByte(pinText, pin)) {
            stream_.println("ERR invalid-pin");
            return;
        }

        dac_.setPin(pin);
        stream_.println("OK");
        return;
    }

    if (equalsIgnoreCase(subcommand, "ENABLE")) {
        char* enabledText = readToken(args);
        bool enabled = false;

        if (enabledText == nullptr || !parseBool(enabledText, enabled)) {
            stream_.println("ERR invalid-enable");
            return;
        }

        dac_.setEnabled(enabled);
        stream_.println("OK");
        return;
    }

    stream_.println("ERR unknown-dac-command");
}

void SerialCommand::handlePocsagCommand(char* args) {
    char* subcommand = readToken(args);

    if (subcommand == nullptr || !equalsIgnoreCase(subcommand, "CONFIG")) {
        stream_.println("ERR unknown-pocsag-command");
        return;
    }

    char* speedText = readToken(args);
    uint16_t speed = 0;

    if (speedText == nullptr || !parseUint16(speedText, speed)) {
        stream_.println("ERR invalid-pocsag-config");
        return;
    }

    pocsag_.setSpeed(speed);

    if (pocsag_.begin()) {
        stream_.println("OK");
    } else {
        stream_.println("ERR pager-begin-failed");
    }
}

void SerialCommand::handleDebugCommand(char* args) {
    char* subcommand = readToken(args);

    if (subcommand == nullptr || !equalsIgnoreCase(subcommand, "HEX")) {
        stream_.println("ERR unknown-debug-command");
        return;
    }

    char* enabledText = readToken(args);
    bool enabled = false;

    if (enabledText == nullptr || !parseBool(enabledText, enabled)) {
        stream_.println("ERR invalid-debug-value");
        return;
    }

    dac_.setHexDumpEnabled(enabled);
    stream_.println("OK");
}

void SerialCommand::handleEchoCommand(char* args) {
    char* enabledText = readToken(args);
    bool enabled = false;

    if (enabledText == nullptr || !parseBool(enabledText, enabled)) {
        stream_.println("ERR invalid-echo-value");
        return;
    }

    echoEnabled_ = enabled;
    stream_.println("OK");
}

void SerialCommand::handleCommand(char* line) {
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

    stream_.println("ERR unknown-command");
}

void SerialCommand::poll() {
    while (stream_.available() > 0) {
        const char c = static_cast<char>(stream_.read());

        if (c == '\r' || c == '\n') {
            if (echoEnabled_) {
                stream_.println();
            }

            if (!lastCharWasLineEnd_) {
                commandBuffer_[commandLength_] = '\0';
                handleCommand(commandBuffer_);
            }

            commandLength_ = 0;
            lastCharWasLineEnd_ = true;
            continue;
        }

        lastCharWasLineEnd_ = false;

        if (c == '\b' || c == 0x7F) {
            if (commandLength_ > 0) {
                commandLength_--;

                if (echoEnabled_) {
                    stream_.print("\b \b");
                }
            }

            continue;
        }

        if (commandLength_ >= CommandBufferSize - 1) {
            commandLength_ = 0;
            stream_.println("ERR line-too-long");
            continue;
        }

        commandBuffer_[commandLength_++] = c;

        if (echoEnabled_ && c >= 0x20 && c <= 0x7E) {
            stream_.write(c);
        }
    }
}
