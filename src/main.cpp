/* Copyright (C) 2026 Petr Kracik - OK1PKR
 *
 * This file is part of DenpaModem
 */

#include <Arduino.h>
#include <RadioLib.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include "AppConfig.h"
#include "DapnetClient.h"
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

WiFiClient dapnetTcpClient;
DapnetClient dapnet(dapnetTcpClient);

enum class WifiDebugState {
    Disconnected,
    Connecting,
    Connected
};

constexpr uint32_t WIFI_RECONNECT_INITIAL_MS = 1000;
constexpr uint32_t WIFI_RECONNECT_MAX_MS = 30000;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;

WifiDebugState wifiState = WifiDebugState::Disconnected;
ConnectionState lastDapnetState = ConnectionState::Disconnected;
uint32_t nextWifiAttemptAt = 0;
uint32_t wifiAttemptStartedAt = 0;
uint32_t wifiReconnectIntervalMs = WIFI_RECONNECT_INITIAL_MS;

bool timeReached(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}

const char* dapnetStateName(ConnectionState state) {
    switch (state) {
    case ConnectionState::Disconnected:
        return "Disconnected";
    case ConnectionState::Connecting:
        return "Connecting";
    case ConnectionState::LoginSent:
        return "LoginSent";
    case ConnectionState::LoggedIn:
        return "LoggedIn";
    }

    return "Unknown";
}

void scheduleWifiReconnect(uint32_t now) {
    nextWifiAttemptAt = now + wifiReconnectIntervalMs;
    if (wifiReconnectIntervalMs < WIFI_RECONNECT_MAX_MS) {
        wifiReconnectIntervalMs *= 2;
        if (wifiReconnectIntervalMs > WIFI_RECONNECT_MAX_MS) {
            wifiReconnectIntervalMs = WIFI_RECONNECT_MAX_MS;
        }
    }
}

void startWifi(uint32_t now) {
    Serial.print("[WIFI] connecting to ");
    Serial.println(AppConfig::WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(AppConfig::WIFI_SSID, AppConfig::WIFI_PASSWORD);

    wifiAttemptStartedAt = now;
    wifiState = WifiDebugState::Connecting;
}

void updateWifi() {
    const uint32_t now = millis();

    if (WiFi.status() == WL_CONNECTED) {
        if (wifiState != WifiDebugState::Connected) {
            wifiState = WifiDebugState::Connected;
            wifiReconnectIntervalMs = WIFI_RECONNECT_INITIAL_MS;
            Serial.print("[WIFI] connected, IP ");
            Serial.println(WiFi.localIP());
        }
        return;
    }

    if (wifiState == WifiDebugState::Connected) {
        Serial.println("[WIFI] disconnected");
        dapnet.disconnect();
        wifiState = WifiDebugState::Disconnected;
        scheduleWifiReconnect(now);
        return;
    }

    if (wifiState == WifiDebugState::Connecting &&
        timeReached(now, wifiAttemptStartedAt + WIFI_CONNECT_TIMEOUT_MS)) {
        Serial.println("[WIFI] connect timeout");
        WiFi.disconnect();
        wifiState = WifiDebugState::Disconnected;
        scheduleWifiReconnect(now);
        return;
    }

    if (wifiState == WifiDebugState::Disconnected && timeReached(now, nextWifiAttemptAt)) {
        startWifi(now);
    }
}

void logDapnetStateChange() {
    const ConnectionState state = dapnet.state();
    if (state == lastDapnetState) {
        return;
    }

    lastDapnetState = state;
    Serial.print("[DAPNET] state ");
    Serial.println(dapnetStateName(state));
}

void drainDapnetDebugMessages() {
    DapnetMessage message;
    while (dapnet.read(message)) {
        Serial.println("DAPNET message:");
        Serial.print("  packetId: ");
        if (message.packetId < 0x10) {
            Serial.print('0');
        }
        Serial.println(message.packetId, HEX);
        Serial.print("  type: ");
        Serial.println(message.type);
        Serial.print("  subtype: ");
        Serial.println(message.subType);
        Serial.print("  RIC: ");
        Serial.println(message.ric);
        Serial.print("  function: ");
        Serial.println(message.function);
        Serial.print("  text: ");
        Serial.println(message.text);
    }
}

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

    dapnet.configure(
        AppConfig::DAPNET_HOST,
        AppConfig::DAPNET_PORT,
        AppConfig::CALLSIGN,
        AppConfig::AUTH_KEY
    );
    dapnet.begin();
    startWifi(millis());
}

void loop() {
    serialCommand.poll();

    updateWifi();
    if (wifiState == WifiDebugState::Connected) {
        dapnet.loop();
    }
    logDapnetStateChange();
    drainDapnetDebugMessages();
}
