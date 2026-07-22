#pragma once

#include <Arduino.h>

// TODO: Make it configurable by serial/web and saved in NVS

namespace AppConfig {

inline constexpr char WIFI_SSID[] = "CHANGE_ME";
inline constexpr char WIFI_PASSWORD[] = "CHANGE_ME";

inline constexpr char DAPNET_HOST[] =
    "dapnet.afu.rwth-aachen.de";

inline constexpr uint16_t DAPNET_PORT = 43434;

inline constexpr char CALLSIGN[] = "CHANGE_ME";
inline constexpr char AUTH_KEY[] = "CHANGE_ME";

}
