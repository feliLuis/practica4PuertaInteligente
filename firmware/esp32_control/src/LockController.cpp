#include "LockController.h"

#include "config.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

LockController::LockController(bool simulated)
  : lockState_(LockState::UNKNOWN), mode_(Mode::AUTOMATIC),
    doorState_(Protocol::DoorState::UNKNOWN), lastSensorSignal_(0),
    autoLockSeconds_(Config::DEFAULT_AUTO_LOCK_SECONDS), autoLockStartedAt_(0),
    eventSequence_(0), sensorHasData_(false), autoLockActive_(false),
    simulated_(simulated) {
  credentials_.add(Config::CARD_UID, Config::CARD_UID_LENGTH,
                       Action::UNLOCK, Config::CARD_ID);
  credentials_.add(Config::KEYFOB_UID, Config::KEYFOB_UID_LENGTH,
                       Action::UNLOCK, Config::KEYFOB_ID);
}

void LockController::onEvent(std::function<void(const Event&)> callback) {
  eventCallback_ = callback;
}

void LockController::onMovement(std::function<bool(Action)> callback) {
  movementCallback_ = callback;
}

void LockController::emit(Source source, EventType type, bool executed,
                                const char* reason, const char* credentialId) {
  Event event{};
  event.seq = ++eventSequence_;
  event.source = source;
  event.type = type;
  event.result = executed ? EventResult::EXECUTED : EventResult::REJECTED;
  snprintf(event.reason, sizeof(event.reason), "%s", reason);
  snprintf(event.credentialId, sizeof(event.credentialId), "%s",
           credentialId == nullptr ? "" : credentialId);
  event.doorState = doorState();
  event.lockState = lockState_;
  event.mode = mode_;
  event.simulated = simulated_;
  if (eventCallback_) eventCallback_(event);
}

void LockController::cancelAutoLock() { autoLockActive_ = false; }

bool LockController::hasRecentSensorData() const {
  return sensorHasData_ &&
    static_cast<uint32_t>(millis() - lastSensorSignal_) < Config::SENSOR_TIMEOUT_MS;
}

void LockController::checkSensorFreshness() {
  if (!sensorHasData_ || hasRecentSensorData()) return;
  sensorHasData_ = false;
  doorState_ = Protocol::DoorState::UNKNOWN;
  cancelAutoLock();
  emit(Source::EDGE, EventType::SENSOR, false, "sensor_offline");
}

void LockController::evaluateAutoLock(bool reset) {
  if (mode_ != Mode::AUTOMATIC || lockState_ != LockState::UNLOCKED ||
      !hasRecentSensorData() || doorState_ != Protocol::DoorState::CLOSED) {
    cancelAutoLock();
    return;
  }
  if (reset || !autoLockActive_) {
    autoLockStartedAt_ = millis();
    autoLockActive_ = true;
    emit(Source::EDGE, EventType::AUTO_LOCK, true, "temporizador_iniciado");
  }
}

bool LockController::updateDoor(Protocol::DoorState newDoorState, Source source) {
  checkSensorFreshness();
  if (newDoorState != Protocol::DoorState::OPEN &&
      newDoorState != Protocol::DoorState::CLOSED) return false;

  const bool recovered = !sensorHasData_;
  const bool cambio = doorState_ != newDoorState;
  lastSensorSignal_ = millis();
  sensorHasData_ = true;
  doorState_ = newDoorState;
  if (recovered) emit(source, EventType::SENSOR, true, "sensor_online");
  if (cambio) emit(source, EventType::DOOR, true,
                    newDoorState == Protocol::DoorState::OPEN ? "puerta_abierta" : "puerta_cerrada");
  evaluateAutoLock(); // Refreshing the same reading does not restart the timer.
  return true;
}

bool LockController::receiveHeartbeat(Protocol::DoorState reading, Source source) {
  return updateDoor(reading, source);
}

Result LockController::executeAction(Action action, Source source,
                                             const char* credentialId) {
  checkSensorFreshness();
  if (action != Action::LOCK && action != Action::UNLOCK)
    return {EventResult::REJECTED, "accion_invalida"};
  if (action == Action::UNLOCK && mode_ == Mode::LOCKED)
    return {EventResult::REJECTED, "modo_bloqueado"};
  if (action == Action::LOCK) {
    if (!hasRecentSensorData()) return {EventResult::REJECTED, "sensor_sin_datos"};
    if (doorState_ != Protocol::DoorState::CLOSED)
      return {EventResult::REJECTED, "puerta_no_cerrada"};
  }

  const LockState target = action == Action::LOCK
    ? LockState::LOCKED : LockState::UNLOCKED;
  if (lockState_ == target) return {EventResult::EXECUTED, "sin_cambio"};
  if (!movementCallback_ || !movementCallback_(action))
    return {EventResult::REJECTED, "orden_servo_fallida"};

  // The callback confirms command issuance. There is no latch position sensor.
  lockState_ = target;
  emit(source, action == Action::LOCK ? EventType::LOCK : EventType::UNLOCK,
         true, simulated_ ? "movimiento_simulado" : "orden_pwm_emitida", credentialId);
  evaluateAutoLock();
  return {EventResult::EXECUTED, "ok"};
}

Result LockController::processAction(Action action, Source source) {
  const Result result = executeAction(action, source, nullptr);
  if (strcmp(result.reason, "ok") != 0)
    emit(source, EventType::REQUEST, result.result == EventResult::EXECUTED,
           result.reason);
  return result;
}

Result LockController::processUid(const uint8_t* uid, uint8_t length, Source source) {
  checkSensorFreshness();
  Action action;
  const char* credentialId = nullptr;
  Result result{EventResult::REJECTED, "uid_no_registrado"};
  if (credentials_.buscar(uid, length, action, credentialId))
    result = executeAction(action, source, credentialId);
  // Emit one access event per presentation and a lock or unlock event only for a new command.
  emit(source, EventType::ACCESS, result.result == EventResult::EXECUTED,
         result.reason, credentialId);
  return result;
}

Result LockController::setMode(Mode newMode, Source source) {
  checkSensorFreshness();
  if (newMode != Mode::MANUAL && newMode != Mode::AUTOMATIC && newMode != Mode::LOCKED) {
    emit(source, EventType::MODE, false, "modo_invalido");
    return {EventResult::REJECTED, "modo_invalido"};
  }
  if (mode_ == newMode) return {EventResult::EXECUTED, "sin_cambio"};
  mode_ = newMode;
  emit(source, EventType::MODE, true, "modo_actualizado");
  evaluateAutoLock();
  return {EventResult::EXECUTED, "modo_actualizado"};
}

Result LockController::setAutoLock(uint32_t seconds, Source source) {
  checkSensorFreshness();
  if (seconds < Config::MIN_AUTO_LOCK_SECONDS || seconds > Config::MAX_AUTO_LOCK_SECONDS) {
    emit(source, EventType::AUTO_LOCK, false, "autocierre_fuera_de_rango");
    return {EventResult::REJECTED, "autocierre_fuera_de_rango"};
  }
  if (autoLockSeconds_ == seconds) return {EventResult::EXECUTED, "sin_cambio"};
  autoLockSeconds_ = seconds;
  emit(source, EventType::AUTO_LOCK, true, "autocierre_actualizado");
  evaluateAutoLock(true);
  return {EventResult::EXECUTED, "autocierre_actualizado"};
}

void LockController::update() {
  checkSensorFreshness();
  if (!autoLockActive_) return;
  if (mode_ != Mode::AUTOMATIC || lockState_ != LockState::UNLOCKED ||
      !hasRecentSensorData() || doorState_ != Protocol::DoorState::CLOSED) {
    cancelAutoLock();
    return;
  }
  if (static_cast<uint32_t>(millis() - autoLockStartedAt_) >= autoLockSeconds_ * 1000UL) {
    cancelAutoLock(); // A failure must not trigger a retry on every loop.
    processAction(Action::LOCK, Source::EDGE);
  }
}

LockState LockController::lockState() const { return lockState_; }
Mode LockController::mode() const { return mode_; }
Protocol::DoorState LockController::doorState() const {
  return hasRecentSensorData() ? doorState_ : Protocol::DoorState::UNKNOWN;
}
uint32_t LockController::autoLockSeconds() const { return autoLockSeconds_; }
bool LockController::sensorIsFresh() const { return hasRecentSensorData(); }
bool LockController::autoLockPending() const {
  return autoLockActive_ && hasRecentSensorData();
}
