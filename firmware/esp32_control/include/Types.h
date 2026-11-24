#pragma once

#include <stdint.h>
#include <Protocol.h>

enum class Source : uint8_t {
  VOICE,
  RFID,
  EDGE,
  CLOUD,
  DIAGNOSTIC
};

enum class LockState : uint8_t {
  UNKNOWN,
  LOCKED,
  UNLOCKED
};

enum class Mode : uint8_t {
  MANUAL,
  AUTOMATIC,
  LOCKED
};

enum class Action : uint8_t {
  LOCK,
  UNLOCK
};

enum class EventType : uint8_t {
  ACCESS,
  UNLOCK,
  LOCK,
  DOOR,
  SENSOR,
  MODE,
  AUTO_LOCK,
  REQUEST // A rejection, failure, or request that requires no movement.
};

enum class EventResult : uint8_t {
  EXECUTED,
  REJECTED
};

struct Result {
  EventResult result;
  const char* reason;
};

struct Event {
  Source source;
  EventType type;
  EventResult result;
  bool executed;
  char reason[48];
  Protocol::DoorState doorState;
  LockState lockState; // Estimated from the PWM command. Physical position is unverified.
  Mode mode;
  char credentialId[24]; // Owned copy that never contains the UID.
  uint32_t seq; // Local to this boot session. It cannot serve as a persistent AWS identifier.
  bool simulated;
};
