#pragma once

#include <stdint.h>
#include <ESP32Servo.h>

class LockServo {
public:
  LockServo(uint8_t pin, uint32_t detachDelayMs);
  bool begin(); // Configure the servo without attaching PWM or moving.
  bool lock();
  bool unlock();
  bool moveTo(int angle); // Accept only configured positions. Return true once the command is issued.
  void update();

private:
  Servo servo_;
  uint8_t pin_;
  uint32_t detachDelayMs_;
  bool initialized_;
  bool attached_;
  uint32_t lastMovement_;
};
