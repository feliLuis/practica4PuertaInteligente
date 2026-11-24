#pragma once
#include "Arduino.h"

class Servo {
public:
  void setPeriodHertz(int) {}
  int attach(int) { conectado_ = !Falso::falloAttach; return conectado_ ? 1 : 0; }
  bool attached() const { return conectado_; }
  void detach() { conectado_ = false; }
  void write(int angulo) {
    if (conectado_) { ++Falso::escrituras; Falso::ultimoAngulo = angulo; }
  }
private:
  bool conectado_ = false;
};
