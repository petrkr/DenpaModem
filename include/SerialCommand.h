#pragma once

#include <Arduino.h>

#include "Esp32DacModule.h"
#include "PocsagModule.h"

class SerialCommand {
public:
    SerialCommand(Stream& stream, Esp32DacModule& dac, PocsagModule& pocsag);

    void begin();
    void poll();
    void printHelp();

private:
    static constexpr size_t CommandBufferSize = 192;

    Stream& stream_;
    Esp32DacModule& dac_;
    PocsagModule& pocsag_;

    char commandBuffer_[CommandBufferSize] = {};
    size_t commandLength_ = 0;
    bool lastCharWasLineEnd_ = false;
    bool echoEnabled_ = true;

    char* skipSpaces(char* text);
    char* readToken(char*& text);
    bool equalsIgnoreCase(const char* a, const char* b);
    bool parseByte(const char* text, uint8_t& value);
    bool parseUint16(const char* text, uint16_t& value);
    bool parseBool(const char* text, bool& value);
    bool parseAddress(const char* text, uint32_t& address);

    void printStatus();
    void handleCommand(char* line);
    void handleTxCommand(char* args);
    void handleDacCommand(char* args);
    void handlePocsagCommand(char* args);
    void handleDebugCommand(char* args);
    void handleEchoCommand(char* args);
};
