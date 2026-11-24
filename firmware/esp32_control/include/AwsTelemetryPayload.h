#pragma once

#include "Types.h"
#include <stddef.h>

namespace AwsPayload {
struct Snapshot {
  Protocol::DoorState door;
  LockState lock;
  Mode mode;
  uint32_t autoLockSeconds;
};

bool sameState(const Snapshot& left, const Snapshot& right);
bool event(char* output, size_t capacity, const Event& value, uint32_t autoLockSeconds,
           const char* eventId, bool deferred);
bool reading(char* output, size_t capacity, const Snapshot& snapshot, const char* eventId);
bool reported(char* output, size_t capacity, const Snapshot& snapshot, int64_t timestampMs);
}
