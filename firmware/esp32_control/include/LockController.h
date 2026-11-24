#pragma once

#include "Types.h"
#include "CredentialList.h"
#include <functional>

class LockController {
public:
  explicit LockController(bool simulated = false);

  void onEvent(std::function<void(const Event&)> callback);
  // A true result confirms command issuance. Servo position is unverified.
  void onMovement(std::function<bool(Action)> callback);
  void onLockRequest(std::function<void(Source)> callback);
  void update();
  bool updateDoor(Protocol::DoorState doorState, Source source = Source::EDGE);
  // A heartbeat without a valid reading does not refresh the Reed switch.
  bool receiveHeartbeat(Protocol::DoorState doorState = Protocol::DoorState::UNKNOWN,
                        Source source = Source::EDGE);
  void invalidateSensor(Source source = Source::EDGE);
  Result processAction(Action action, Source source);
  Result processUid(const uint8_t* uid, uint8_t length, Source source = Source::RFID);
  Result setMode(Mode mode, Source source);
  Result setAutoLock(uint32_t seconds, Source source);

  LockState lockState() const;
  Mode mode() const;
  Protocol::DoorState doorState() const;
  uint32_t autoLockSeconds() const;
  bool sensorIsFresh() const;
  bool autoLockPending() const;

private:
  Result executeAction(Action action, Source source, const char* credentialId);
  void emit(Source source, EventType type, bool executed, const char* reason,
            const char* credentialId = nullptr);
  void cancelAutoLock();
  void evaluateAutoLock(bool reset = false);
  void checkSensorFreshness();
  bool hasRecentSensorData() const;

  CredentialList credentials_;
  std::function<void(const Event&)> eventCallback_;
  std::function<bool(Action)> movementCallback_;
  std::function<void(Source)> lockRequestCallback_;
  LockState lockState_;
  Mode mode_;
  Protocol::DoorState doorState_;
  uint32_t lastSensorSignal_;
  uint32_t autoLockSeconds_;
  uint32_t autoLockStartedAt_;
  uint32_t eventSequence_;
  bool sensorHasData_;
  bool autoLockActive_;
  bool simulated_;
};
