#include "AwsTelemetryPayload.h"
#include <inttypes.h>
#include <stdio.h>

namespace {
const char* doorName(Protocol::DoorState state) {
  switch (state) {
    case Protocol::DoorState::OPEN: return "abierta";
    case Protocol::DoorState::CLOSED: return "cerrada";
    default: return "desconocida";
  }
}

const char* lockName(LockState state) {
  switch (state) {
    case LockState::LOCKED: return "bloqueada";
    case LockState::UNLOCKED: return "desbloqueada";
    default: return "desconocida";
  }
}

const char* modeName(Mode value) {
  switch (value) {
    case Mode::MANUAL: return "manual";
    case Mode::AUTOMATIC: return "automatico";
    case Mode::BLOCKED: return "bloqueado";
  }
  return nullptr;
}

const char* eventName(const Event& value) {
  switch (value.type) {
    case EventType::ACCESS: return "acceso";
    case EventType::UNLOCK:
      return value.result == EventResult::EXECUTED && value.lockState == LockState::UNLOCKED ?
        "desbloqueo" : "solicitud";
    case EventType::LOCK:
      return value.result == EventResult::EXECUTED && value.lockState == LockState::LOCKED ?
        "bloqueo" : "solicitud";
    case EventType::DOOR: return "puerta";
    case EventType::SENSOR: return "sensor";
    case EventType::MODE: return "modo";
    case EventType::AUTO_LOCK: return "autocierre";
    case EventType::REQUEST: return "solicitud";
  }
  return nullptr;
}

const char* sourceName(Source value) {
  switch (value) {
    case Source::RFID: return "rfid";
    case Source::VOICE: return "voz";
    case Source::EDGE: return "edge";
    case Source::CLOUD: return "nube";
    default: return nullptr;
  }
}

bool fits(int length, size_t capacity) {
  return length > 0 && static_cast<size_t>(length) < capacity;
}
}

namespace AwsPayload {
bool sameState(const Snapshot& left, const Snapshot& right) {
  return left.door == right.door && left.lock == right.lock && left.mode == right.mode &&
         left.autoLockSeconds == right.autoLockSeconds;
}

bool event(char* output, size_t capacity, const Event& value, uint32_t autoLockSeconds,
           const char* eventId, bool deferred) {
  const char* source = sourceName(value.source);
  const char* type = eventName(value);
  const char* mode = modeName(value.mode);
  if (value.simulated || !source || !type || !mode || autoLockSeconds < 5 || autoLockSeconds > 120)
    return false;
  return fits(snprintf(output, capacity, "{\"eventId\":\"%s\",\"tipo\":\"%s\",\"origen\":\"%s\","
    "\"resultado\":\"%s\",\"puerta\":\"%s\",\"cerradura\":\"%s\",\"modo\":\"%s\","
    "\"autocierre_s\":%" PRIu32 ",\"seq\":%" PRIu32 ",\"diferido\":%s}",
    eventId, type, source, value.result == EventResult::EXECUTED ? "ejecutado" : "rechazado",
    doorName(value.doorState), lockName(value.lockState), mode, autoLockSeconds,
    value.sequence, deferred ? "true" : "false"), capacity);
}

bool reading(char* output, size_t capacity, const Snapshot& snapshot, const char* eventId) {
  return fits(snprintf(output, capacity, "{\"eventId\":\"%s\",\"tipo\":\"lectura\","
    "\"origen\":\"edge\",\"resultado\":\"ejecutado\",\"puerta\":\"%s\"}",
    eventId, doorName(snapshot.door)), capacity);
}

bool reported(char* output, size_t capacity, const Snapshot& snapshot, int64_t timestampMs) {
  const char* mode = modeName(snapshot.mode);
  if (!mode || snapshot.autoLockSeconds < 5 || snapshot.autoLockSeconds > 120) return false;
  return fits(snprintf(output, capacity, "{\"state\":{\"reported\":{\"puerta\":\"%s\","
    "\"cerradura\":\"%s\",\"modo\":\"%s\",\"autocierre_s\":%" PRIu32 ","
    "\"reportado_ms\":%" PRId64 "}}}", doorName(snapshot.door), lockName(snapshot.lock),
    mode, snapshot.autoLockSeconds, timestampMs), capacity);
}
}
