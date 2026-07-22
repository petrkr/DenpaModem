#include "DapnetClient.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

DapnetClient::DapnetClient(Client& client) : client_(client) {}

void DapnetClient::configure(
    const char* host,
    uint16_t port,
    const char* callsign,
    const char* authKey
) {
    host_ = host;
    port_ = port;
    callsign_ = callsign;
    authKey_ = authKey;
}

void DapnetClient::begin() {
    state_ = ConnectionState::Disconnected;
    nextReconnectAt_ = 0;
    reconnectIntervalMs_ = RECONNECT_INITIAL_MS;
    slotMask_ = 0;
    resetRx();
}

void DapnetClient::loop() {
    const uint32_t now = millis();

    if (state_ != ConnectionState::Disconnected && !client_.connected()) {
        handleDisconnect(now);
    }

    if (state_ == ConnectionState::Disconnected) {
        tryConnect(now);
        return;
    }

    readIncoming();
}

bool DapnetClient::connected() const {
    return state_ != ConnectionState::Disconnected && client_.connected();
}

bool DapnetClient::loggedIn() const {
    return state_ == ConnectionState::LoggedIn && client_.connected();
}

ConnectionState DapnetClient::state() const {
    return state_;
}

bool DapnetClient::available() const {
    return queueCount_ > 0;
}

bool DapnetClient::read(DapnetMessage& message) {
    if (queueCount_ == 0) {
        return false;
    }

    message = queue_[queueTail_];
    queueTail_ = (queueTail_ + 1) % MESSAGE_QUEUE_SIZE;
    --queueCount_;
    return true;
}

uint16_t DapnetClient::slotMask() const {
    return slotMask_;
}

void DapnetClient::disconnect() {
    client_.stop();
    state_ = ConnectionState::Disconnected;
    resetRx();
    scheduleReconnect(millis());
}

void DapnetClient::tryConnect(uint32_t now) {
    if (host_ == nullptr || callsign_ == nullptr || authKey_ == nullptr || port_ == 0) {
        return;
    }

    if (static_cast<int32_t>(now - nextReconnectAt_) < 0) {
        return;
    }

    Serial.print("[DAPNET] connecting to ");
    Serial.print(host_);
    Serial.print(':');
    Serial.println(port_);

    state_ = ConnectionState::Connecting;
    if (client_.connect(host_, port_)) {
        reconnectIntervalMs_ = RECONNECT_INITIAL_MS;
        sendLogin();
        state_ = ConnectionState::LoginSent;
        return;
    }

    Serial.println("[DAPNET ERROR] TCP connect failed");
    client_.stop();
    state_ = ConnectionState::Disconnected;
    scheduleReconnect(now);
}

void DapnetClient::sendLogin() {
    char callsignLower[32] = {};
    size_t i = 0;
    while (callsign_[i] != '\0' && i < sizeof(callsignLower) - 1) {
        callsignLower[i] = static_cast<char>(tolower(static_cast<unsigned char>(callsign_[i])));
        ++i;
    }

    client_.print("[DAPNETGateway vESP32-0.1 ");
    client_.print(callsignLower);
    client_.print(' ');
    client_.print(authKey_);
    client_.print("]\r\n");
    Serial.println("[DAPNET TX] login sent");
}

void DapnetClient::handleDisconnect(uint32_t now) {
    Serial.println("[DAPNET] disconnected");
    client_.stop();
    state_ = ConnectionState::Disconnected;
    resetRx();
    scheduleReconnect(now);
}

void DapnetClient::scheduleReconnect(uint32_t now) {
    nextReconnectAt_ = now + reconnectIntervalMs_;
    if (reconnectIntervalMs_ < RECONNECT_MAX_MS) {
        reconnectIntervalMs_ *= 2;
        if (reconnectIntervalMs_ > RECONNECT_MAX_MS) {
            reconnectIntervalMs_ = RECONNECT_MAX_MS;
        }
    }
}

void DapnetClient::readIncoming() {
    while (client_.available() > 0) {
        const int value = client_.read();
        if (value < 0) {
            return;
        }

        const char c = static_cast<char>(value);
        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            if (discardingLine_) {
                Serial.println("[DAPNET ERROR] dropped overlong line");
                resetRx();
                continue;
            }

            rxLine_[rxLineLength_] = '\0';
            char* line = trimLine(rxLine_);
            if (line[0] != '\0') {
                Serial.print("[DAPNET RX] ");
                Serial.println(line);
                handleLine(line);
            }
            resetRx();
            continue;
        }

        if (discardingLine_) {
            continue;
        }

        if (rxLineLength_ >= RX_LINE_SIZE - 1) {
            discardingLine_ = true;
            continue;
        }

        rxLine_[rxLineLength_++] = c;
    }
}

void DapnetClient::handleLine(char* line) {
    if (strcmp(line, "+") == 0) {
        Serial.println("[DAPNET] positive reply");
        return;
    }

    if (strcmp(line, "-") == 0) {
        Serial.println("[DAPNET ERROR] negative reply");
        return;
    }

    switch (line[0]) {
    case '2':
        handleTimeSync(line);
        break;
    case '4':
        handleSlotMask(line);
        break;
    case '7':
        handleLoginError(line);
        break;
    case '#': {
        DapnetMessage message;
        uint8_t receivedId = 0;
        const bool parsed = parsePagingMessage(line, message, receivedId);
        if (parsed && enqueue(message)) {
            sendMessageAck(receivedId, true);
        } else {
            if (parsed) {
                Serial.println("[DAPNET ERROR] message queue full");
            } else {
                Serial.println("[DAPNET ERROR] paging parser failed");
            }
            sendMessageAck(receivedId, false);
        }
        break;
    }
    default:
        Serial.println("[DAPNET ERROR] unsupported line");
        break;
    }
}

void DapnetClient::handleTimeSync(char* line) {
    if (state_ == ConnectionState::LoginSent) {
        state_ = ConnectionState::LoggedIn;
        Serial.println("[DAPNET] logged in");
    }

    // DAPNETGateway answers type 2 time-sync lines by echoing the frame with
    // its command character adjusted from server request to gateway response.
    if (line[0] == '2') {
        line[0] = '3';
    }
    sendLine(line);
    sendLine("+");
}

void DapnetClient::handleSlotMask(const char* line) {
    const char* slots = line + 1;
    while (*slots == ' ') {
        ++slots;
    }

    if (*slots == '\0') {
        Serial.println("[DAPNET ERROR] empty slot mask");
        sendLine("-");
        return;
    }

    uint16_t mask = 0;
    for (const char* p = slots; *p != '\0'; ++p) {
        const char c = static_cast<char>(toupper(static_cast<unsigned char>(*p)));
        if (c >= '0' && c <= '9') {
            mask |= static_cast<uint16_t>(1U << (c - '0'));
        } else if (c >= 'A' && c <= 'F') {
            mask |= static_cast<uint16_t>(1U << (10 + c - 'A'));
        } else if (c != ' ') {
            Serial.println("[DAPNET ERROR] invalid slot mask");
            sendLine("-");
            return;
        }
    }

    slotMask_ = mask;
    Serial.print("[DAPNET] slot mask 0x");
    Serial.println(slotMask_, HEX);
    sendLine("+");
}

void DapnetClient::handleLoginError(const char* line) {
    Serial.print("[DAPNET ERROR] login rejected: ");
    Serial.println(line);
    client_.stop();
    state_ = ConnectionState::Disconnected;
    resetRx();
    scheduleReconnect(millis());
}

bool DapnetClient::enqueue(const DapnetMessage& message) {
    if (queueCount_ >= MESSAGE_QUEUE_SIZE) {
        return false;
    }

    queue_[queueHead_] = message;
    queueHead_ = (queueHead_ + 1) % MESSAGE_QUEUE_SIZE;
    ++queueCount_;
    return true;
}

void DapnetClient::sendLine(const char* line) {
    if (line == nullptr) {
        return;
    }

    client_.print(line);
    client_.print("\r\n");
    Serial.print("[DAPNET TX] ");
    Serial.println(line);
}

void DapnetClient::sendMessageAck(uint8_t receivedId, bool success) {
    const uint8_t ackId = static_cast<uint8_t>(receivedId + 1U);
    char line[8] = {};
    snprintf(line, sizeof(line), "#%02X %c", ackId, success ? '+' : '-');
    sendLine(line);
}

void DapnetClient::resetRx() {
    rxLineLength_ = 0;
    rxLine_[0] = '\0';
    discardingLine_ = false;
}

bool DapnetClient::parsePagingMessage(
    const char* line,
    DapnetMessage& output,
    uint8_t& receivedId
) {
    if (line == nullptr || line[0] != '#') {
        return false;
    }

    const char* packetStart = line + 1;
    const char* packetEnd = packetStart;
    while (isxdigit(static_cast<unsigned char>(*packetEnd))) {
        ++packetEnd;
    }

    unsigned long packetValue = 0;
    if (!parseUnsigned(packetStart, packetEnd, 16, UINT8_MAX, packetValue)) {
        return false;
    }

    receivedId = static_cast<uint8_t>(packetValue);
    if (*packetEnd != ' ') {
        return false;
    }

    const char* fieldStart = packetEnd + 1;
    const char* separators[4] = {};
    const char* cursor = fieldStart;
    for (size_t i = 0; i < 4; ++i) {
        separators[i] = strchr(cursor, ':');
        if (separators[i] == nullptr) {
            return false;
        }
        cursor = separators[i] + 1;
    }

    unsigned long typeValue = 0;
    unsigned long subTypeValue = 0;
    unsigned long ricValue = 0;
    unsigned long functionValue = 0;

    if (!parseUnsigned(fieldStart, separators[0], 10, UINT8_MAX, typeValue) ||
        !parseUnsigned(separators[0] + 1, separators[1], 10, UINT8_MAX, subTypeValue) ||
        !parseUnsigned(separators[1] + 1, separators[2], 16, UINT32_MAX, ricValue) ||
        !parseUnsigned(separators[2] + 1, separators[3], 10, UINT8_MAX, functionValue) ||
        !copyText(separators[3] + 1, output.text, sizeof(output.text))) {
        return false;
    }

    output.packetId = static_cast<uint8_t>(packetValue);
    output.type = static_cast<uint8_t>(typeValue);
    output.subType = static_cast<uint8_t>(subTypeValue);
    output.ric = static_cast<uint32_t>(ricValue);
    output.function = static_cast<uint8_t>(functionValue);
    return true;
}

bool DapnetClient::parseUnsigned(
    const char* first,
    const char* last,
    uint8_t base,
    unsigned long maxValue,
    unsigned long& value
) {
    if (first == nullptr || last == nullptr || first >= last) {
        return false;
    }

    char buffer[16] = {};
    const size_t length = static_cast<size_t>(last - first);
    if (length >= sizeof(buffer)) {
        return false;
    }

    memcpy(buffer, first, length);
    buffer[length] = '\0';

    errno = 0;
    char* end = nullptr;
    const unsigned long parsed = strtoul(buffer, &end, base);
    if (errno != 0 || end == buffer || *end != '\0' || parsed > maxValue) {
        return false;
    }

    value = parsed;
    return true;
}

char* DapnetClient::trimLine(char* line) {
    if (line == nullptr) {
        return nullptr;
    }

    while (isspace(static_cast<unsigned char>(*line))) {
        ++line;
    }

    char* end = line + strlen(line);
    while (end > line && isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    *end = '\0';
    return line;
}

bool DapnetClient::copyText(const char* source, char* target, size_t targetSize) {
    if (source == nullptr || target == nullptr || targetSize == 0) {
        return false;
    }

    const size_t length = strlen(source);
    if (length >= targetSize) {
        return false;
    }

    memcpy(target, source, length + 1);
    return true;
}

#ifdef DAPNET_RUN_SELF_TEST
void DapnetClient::runSelfTest() {
    static constexpr const char* VALID[] = {
        "#58 6:1:D0:3:XTIME=2122220726XTIME=2122220726",
        "#59 6:1:E0:3:YYYYMMDDHHMMSS260722212200",
        "#FF 6:1:12345:3:Hello:world",
    };

    static constexpr const char* INVALID[] = {
        "#",
        "#GG 6:1:D0:3:test",
        "#58",
        "#58 6:1",
        "#58 X:1:D0:3:test",
        "#58 6:1:ZZ:3:test",
    };

    DapnetMessage message;
    uint8_t receivedId = 0;

    for (const char* line : VALID) {
        if (!parsePagingMessage(line, message, receivedId)) {
            Serial.print("[DAPNET ERROR] self-test valid input failed: ");
            Serial.println(line);
        }
    }

    for (const char* line : INVALID) {
        if (parsePagingMessage(line, message, receivedId)) {
            Serial.print("[DAPNET ERROR] self-test invalid input accepted: ");
            Serial.println(line);
        }
    }
}
#endif
