#include "AwsTelemetryPayload.h"
#include "TestSupport.h"
#include <cstring>

int main() {
  Suite suite;
  constexpr char id[] = "1795550000000#0123456789abcdef0123456789abcdef";
  AwsPayload::Snapshot snapshot{Protocol::DoorState::UNKNOWN, LockState::UNKNOWN,
                                Mode::BLOCKED, 21};
  char payload[384];
  suite.testCase("Shadow reports estimated state using the Alexa contract", [&] {
    CHECK(AwsPayload::reported(payload, sizeof(payload), snapshot, 1795550000000LL) &&
              strcmp(payload, "{\"state\":{\"reported\":{\"puerta\":\"desconocida\","
                  "\"cerradura\":\"desconocida\",\"modo\":\"bloqueado\",\"autocierre_s\":21,"
                  "\"reportado_ms\":1795550000000}}}") == 0);
  });

  Event access{};
  access.type = EventType::ACCESS;
  access.source = Source::RFID;
  access.result = EventResult::EXECUTED;
  access.lockState = LockState::UNLOCKED;
  access.doorState = Protocol::DoorState::CLOSED;
  access.mode = Mode::AUTOMATIC;
  access.sequence = 8;
  strcpy(access.credentialId, "local-identifier");
  suite.testCase("RFID events omit local identification and preserve the event ID", [&] {
    CHECK(AwsPayload::event(payload, sizeof(payload), access, 10, id, true) &&
              strstr(payload, "\"tipo\":\"acceso\"") && strstr(payload, "\"origen\":\"rfid\"") &&
              strstr(payload, "\"eventId\":\"1795550000000#0123456789abcdef0123456789abcdef\"") &&
              strstr(payload, "\"diferido\":true") && !strstr(payload, "local-identifier"));
  });

  access.type = EventType::LOCK;
  access.source = Source::EDGE;
  access.lockState = LockState::LOCKED;
  suite.testCase("Successful lock command is mapped to a movement", [&] {
    CHECK(AwsPayload::event(payload, sizeof(payload), access, 10, id, false) &&
          strstr(payload, "\"tipo\":\"bloqueo\"") && strstr(payload, "\"cerradura\":\"bloqueada\""));
  });
  access.result = EventResult::REJECTED;
  suite.testCase("Rejected lock requests are not classified as movement", [&] {
    CHECK(AwsPayload::event(payload, sizeof(payload), access, 10, id, false) &&
          strstr(payload, "\"tipo\":\"solicitud\"") && strstr(payload, "\"resultado\":\"rechazado\""));
  });

  access.simulated = true;
  bool omittedSimulated = !AwsPayload::event(payload, sizeof(payload), access, 10, id, false);
  access.simulated = false;
  access.source = Source::DIAGNOSTIC;
  suite.testCase("Diagnostic and simulated events stay local", [&] {
    CHECK(omittedSimulated && !AwsPayload::event(payload, sizeof(payload), access, 10, id, false));
  });

  suite.testCase("Periodic readings preserve unknown Reed and reject truncated JSON", [&] {
    CHECK(AwsPayload::reading(payload, sizeof(payload), snapshot, id) &&
              strstr(payload, "\"tipo\":\"lectura\"") && strstr(payload, "\"puerta\":\"desconocida\"") &&
              !AwsPayload::reading(payload, 8, snapshot, id));
  });
  return suite.result();
}
