#include "Buzzer.h"

#include <Arduino.h>

Buzzer::Buzzer(uint8_t pin, bool activo)
  : pin_(pin), activo_(activo), sonando_(false),
    faseEncendida_(false), cambio_(0), encendidoMs_(0),
    apagadoMs_(0), pitidosPendientes_(0) {}

void Buzzer::begin() {
  pinMode(pin_, OUTPUT);
  apagar();
}

void Buzzer::apagar() {
  if (activo_) {
    digitalWrite(pin_, LOW);
  } else {
    noTone(pin_);
  }
  sonando_ = false;
  faseEncendida_ = false;
  pitidosPendientes_ = 0;
}

void Buzzer::sonarPatron(uint32_t encendidoMs, uint32_t apagadoMs, uint8_t repeticiones) {
  encendidoMs_ = encendidoMs;
  apagadoMs_ = apagadoMs;
  pitidosPendientes_ = repeticiones;
  faseEncendida_ = true;
  sonando_ = true;
  cambio_ = millis() + encendidoMs;
  if (activo_) {
    digitalWrite(pin_, HIGH);
  } else {
    tone(pin_, 2000);
  }
}

void Buzzer::actualizar() {
  if (!sonando_ || static_cast<int32_t>(millis() - cambio_) < 0) return;

  if (faseEncendida_) {
    if (activo_) {
      digitalWrite(pin_, LOW);
    } else {
      noTone(pin_);
    }
    faseEncendida_ = false;
    cambio_ = millis() + apagadoMs_;
  } else {
    pitidosPendientes_--;
    if (pitidosPendientes_ == 0) {
      apagar();
      return;
    }
    faseEncendida_ = true;
    cambio_ = millis() + encendidoMs_;
    if (activo_) {
      digitalWrite(pin_, HIGH);
    } else {
      tone(pin_, 2000);
    }
  }
}
