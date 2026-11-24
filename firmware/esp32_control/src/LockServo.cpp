#include "LockServo.h"

#include "config.h"
#include <Arduino.h>
#include <driver/gpio.h>

LockServo::LockServo(uint8_t pin, uint32_t detachDelayMs)
  : pin_(pin), detachDelayMs_(detachDelayMs), initialized_(false),
    attached_(false), lastMovement_(0) {}

bool LockServo::begin() {
  initialized_ = GPIO_IS_VALID_OUTPUT_GPIO(pin_);
  if (initialized_) servo_.setPeriodHertz(50);
  return initialized_;
}

bool LockServo::moveTo(int angle) {
  if (!initialized_ ||
      (angle != Config::LOCK_ANGLE && angle != Config::UNLOCK_ANGLE)) return false;
  if (!servo_.attached()) {
    const int result = servo_.attach(pin_);
    // ESP32Servo 0.13.0 returns 1 when it accepts attach. Also check attached().
    if (result <= 0 || !servo_.attached()) {
      servo_.detach();
      attached_ = false;
      return false;
    }
  }
  servo_.write(angle); // The library cannot confirm physical movement.
  attached_ = servo_.attached();
  if (!attached_) return false;
  lastMovement_ = millis();
  return true;
}

bool LockServo::lock() { return moveTo(Config::LOCK_ANGLE); }
bool LockServo::unlock() { return moveTo(Config::UNLOCK_ANGLE); }

void LockServo::update() {
  if (attached_ && static_cast<uint32_t>(millis() - lastMovement_) >= detachDelayMs_) {
    servo_.detach();
    attached_ = false;
  }
}
