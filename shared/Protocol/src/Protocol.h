#pragma once

#include <stdint.h>
#include <stddef.h>

namespace Protocol {

constexpr uint8_t VERSION = 1;

enum class Type : uint8_t {
  REED_STATE = 1,
  RFID_PRESENTED = 2,
  HEARTBEAT = 3
};

enum class DoorState : uint8_t {
  UNKNOWN = 0,
  OPEN = 1,
  CLOSED = 2
};

struct __attribute__((packed)) Message {
  uint8_t version;
  Type type;
  uint32_t sequence;
  DoorState doorState;
  uint8_t uidLen;
  uint8_t uid[10];
  uint32_t uptimeMs;
};

const char* name(DoorState state);
const char* name(Type type);
bool uidToHex(const uint8_t* uid, uint8_t length, char* output, size_t capacity);
bool hexToUid(const char* text, uint8_t* uid, uint8_t capacity, uint8_t& length);

} // namespace Protocol
