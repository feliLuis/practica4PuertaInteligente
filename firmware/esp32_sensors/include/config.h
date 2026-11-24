#pragma once

#include <stdint.h>

#ifndef UID_DIAGNOSTICS
#define UID_DIAGNOSTICS 0
#endif

namespace Config {
constexpr uint8_t RC522_SS = 21;
constexpr uint8_t RC522_SCK = 18;
constexpr uint8_t RC522_MOSI = 23;
constexpr uint8_t RC522_MISO = 19;
constexpr uint8_t RC522_RST = 22;

constexpr uint8_t PIN_REED = 27;
constexpr uint32_t DEBOUNCE_MS = 100;
constexpr uint32_t HEARTBEAT_MS = 1000;
constexpr uint32_t RFID_REPEAT_SUPPRESSION_MS = 1500;
constexpr uint32_t RFID_RETRY_MS = 5000;
constexpr uint32_t RFID_CHECK_MS = 1000;
constexpr uint32_t RFID_POLL_MS = 50;
constexpr bool REED_CLOSED_IS_LOW = true;
constexpr bool SHOW_FULL_UID = UID_DIAGNOSTICS != 0;
constexpr uint32_t SERIAL_BAUD = 115200;
}
