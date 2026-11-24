#pragma once

#include <stdint.h>

namespace Config {
constexpr uint8_t CARD_UID[] = {0x39, 0x61, 0xD7, 0x06};
constexpr uint8_t CARD_UID_LENGTH = sizeof(CARD_UID);
constexpr uint8_t KEYFOB_UID[] = {0x9C, 0x31, 0x8B, 0x06};
constexpr uint8_t KEYFOB_UID_LENGTH = sizeof(KEYFOB_UID);
constexpr const char* CARD_ID = "card-1";
constexpr const char* KEYFOB_ID = "keyfob-1";

constexpr uint8_t PIN_SERVO = 13;
constexpr int LOCK_ANGLE = 0;
constexpr int UNLOCK_ANGLE = 90;
constexpr uint32_t DEFAULT_AUTO_LOCK_SECONDS = 10;
constexpr uint32_t MIN_AUTO_LOCK_SECONDS = 5;
constexpr uint32_t MAX_AUTO_LOCK_SECONDS = 120;
// Three missed 1 s heartbeats mark the sensor offline in firmware.
#ifndef TEST_SENSOR_TIMEOUT_MS
constexpr uint32_t SENSOR_TIMEOUT_MS = 3000;
#else
constexpr uint32_t SENSOR_TIMEOUT_MS = TEST_SENSOR_TIMEOUT_MS;
#endif
constexpr uint32_t STATE_CONFIRMATION_TIMEOUT_MS = 750;
constexpr uint32_t SERVO_DETACH_MS = 700;

constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint8_t COMMAND_CAPACITY = 80;
}
