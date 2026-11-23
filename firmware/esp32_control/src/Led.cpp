#include "Led.h"

#include <Arduino.h>

Led::Led(uint8_t pin)
  : pin_(pin), encendido_(false), vence_(0) {}

void Led::begin() {
  pinMode(pin_, OUTPUT);
  apagar();
}

void Led::encender() {
  encendido_ = true;
  vence_ = 0;
  digitalWrite(pin_, HIGH);
}

void Led::apagar() {
  encendido_ = false;
  vence_ = 0;
  digitalWrite(pin_, LOW);
}

void Led::encenderPor(uint32_t milisegundos) {
  encendido_ = true;
  vence_ = millis() + milisegundos;
  digitalWrite(pin_, HIGH);
}

void Led::actualizar() {
  if (encendido_ && vence_ != 0 &&
      static_cast<uint32_t>(millis() - vence_) < 0x80000000UL) {
    apagar();
  }
}
