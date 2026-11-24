#pragma once
#include "Arduino.h"

class Servo {
public:
  void setPeriodHertz(int) {}
  int attach(int) { attached_ = !Fake::attachFailure; return attached_ ? 1 : 0; }
  bool attached() const { return attached_; }
  void detach() { attached_ = false; }
  void write(int angle) {
    if (attached_) { ++Fake::writes; Fake::lastAngle = angle; }
  }
private:
  bool attached_ = false;
};
