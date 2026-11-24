#pragma once
#include <stddef.h>
#include <stdint.h>
namespace Protocol {
constexpr uint8_t VERSION = 2;
constexpr uint8_t MAX_UID_LENGTH = 10;
constexpr size_t WIRE_SIZE = 34;
enum class Type : uint8_t { REED_STATE=1, RFID_PRESENTED=2, HEARTBEAT=3, ACK=4, STATE_REQUEST=5, STATE_RESPONSE=6 };
enum class DoorState : uint8_t { UNKNOWN=0, OPEN=1, CLOSED=2 };
struct Message {
  uint8_t version; Type type; uint32_t sessionId; uint32_t sequence;
  DoorState doorState; uint8_t uidLen; uint8_t uid[MAX_UID_LENGTH];
  uint32_t correlationSession; uint32_t correlationSequence; uint32_t uptimeMs;
};
enum class ValidationError : uint8_t { NONE, SIZE, VERSION, TYPE, SESSION, SEQUENCE, DOOR_STATE, UID_LENGTH, CONTENT };
const char* name(DoorState state);
const char* name(Type type);
const char* name(ValidationError error);
bool validUidLength(uint8_t length);
bool validUnicastMac(const uint8_t* mac);
bool authorizedSource(const uint8_t* expected, const uint8_t* received);
ValidationError validate(const Message& message);
bool encode(const Message& message, uint8_t* output, size_t capacity);
ValidationError decode(const uint8_t* data, size_t length, Message& message);
bool uidToHex(const uint8_t* uid, uint8_t length, char* output, size_t capacity);
bool hexToUid(const char* text, uint8_t* uid, uint8_t capacity, uint8_t& length);
}
