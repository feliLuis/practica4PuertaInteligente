#include "DoorSensor.h"

#include <Arduino.h>

DoorSensor::DoorSensor()
  : stable_(Protocol::DoorState::UNKNOWN),
    candidate_(Protocol::DoorState::UNKNOWN),
    stableSince_(0) {}

void DoorSensor::begin() {
  pinMode(Config::PIN_REED, INPUT_PULLUP);
  const bool reedCerrado = (digitalRead(Config::PIN_REED) == LOW);
  stable_ = Protocol::DoorState::UNKNOWN;
  candidate_ = Config::REED_CLOSED_IS_LOW
    ? (reedCerrado ? Protocol::DoorState::CLOSED : Protocol::DoorState::OPEN)
    : (reedCerrado ? Protocol::DoorState::OPEN : Protocol::DoorState::CLOSED);
  stableSince_ = millis();
}

bool DoorSensor::update() {
  const bool reedCerrado = (digitalRead(Config::PIN_REED) == LOW);
  const Protocol::DoorState reading = Config::REED_CLOSED_IS_LOW
    ? (reedCerrado ? Protocol::DoorState::CLOSED : Protocol::DoorState::OPEN)
    : (reedCerrado ? Protocol::DoorState::OPEN : Protocol::DoorState::CLOSED);

  const uint32_t now = millis();
  if (reading != candidate_) {
    candidate_ = reading;
    stableSince_ = now;
  }

  if (candidate_ != stable_ && static_cast<uint32_t>(now - stableSince_) >= Config::DEBOUNCE_MS) {
    stable_ = candidate_;
    return true;
  }
  return false;
}

Protocol::DoorState DoorSensor::state() const {
  return stable_;
}
