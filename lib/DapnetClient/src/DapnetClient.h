#pragma once

#include <Arduino.h>
#include <Client.h>

#include "DapnetMessage.h"

enum class ConnectionState {
    Disconnected,
    Connecting,
    LoginSent,
    LoggedIn
};

class DapnetClient {
public:
    explicit DapnetClient(Client& client);

    void configure(
        const char* host,
        uint16_t port,
        const char* callsign,
        const char* authKey
    );

    void begin();
    void loop();

    bool connected() const;
    bool loggedIn() const;
    ConnectionState state() const;

    bool available() const;
    bool read(DapnetMessage& message);

    uint16_t slotMask() const;

    void disconnect();

    static bool parsePagingMessage(
        const char* line,
        DapnetMessage& output,
        uint8_t& receivedId
    );

#ifdef DAPNET_RUN_SELF_TEST
    static void runSelfTest();
#endif

private:
    static constexpr size_t RX_LINE_SIZE = 512;
    static constexpr size_t MESSAGE_QUEUE_SIZE = 8;
    static constexpr uint32_t RECONNECT_INITIAL_MS = 1000;
    static constexpr uint32_t RECONNECT_MAX_MS = 30000;

    Client& client_;
    const char* host_ = nullptr;
    uint16_t port_ = 0;
    const char* callsign_ = nullptr;
    const char* authKey_ = nullptr;
    ConnectionState state_ = ConnectionState::Disconnected;
    uint32_t nextReconnectAt_ = 0;
    uint32_t reconnectIntervalMs_ = RECONNECT_INITIAL_MS;
    uint16_t slotMask_ = 0;

    char rxLine_[RX_LINE_SIZE] = {};
    size_t rxLineLength_ = 0;
    bool discardingLine_ = false;

    DapnetMessage queue_[MESSAGE_QUEUE_SIZE] = {};
    size_t queueHead_ = 0;
    size_t queueTail_ = 0;
    size_t queueCount_ = 0;

    void tryConnect(uint32_t now);
    void sendLogin();
    void handleDisconnect(uint32_t now);
    void readIncoming();
    void handleLine(char* line);
    void handleTimeSync(char* line);
    void handleSlotMask(const char* line);
    void handleLoginError(const char* line);
    bool enqueue(const DapnetMessage& message);
    void sendLine(const char* line);
    void sendMessageAck(uint8_t receivedId, bool success);
    void resetRx();
    void scheduleReconnect(uint32_t now);

    static bool parseUnsigned(
        const char* first,
        const char* last,
        uint8_t base,
        unsigned long maxValue,
        unsigned long& value
    );
    static char* trimLine(char* line);
    static bool copyText(const char* source, char* target, size_t targetSize);
};
