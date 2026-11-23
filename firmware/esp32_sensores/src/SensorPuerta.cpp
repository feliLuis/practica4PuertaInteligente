#include "SensorPuerta.h"

#include <Arduino.h>

SensorPuerta::SensorPuerta()
  : estable_(Protocolo::EstadoPuerta::DESCONOCIDA),
    candidato_(Protocolo::EstadoPuerta::DESCONOCIDA),
    inicioEstable_(0) {}

void SensorPuerta::begin() {
  pinMode(Config::PIN_REED, INPUT_PULLUP);
  const bool reedCerrado = (digitalRead(Config::PIN_REED) == LOW);
  estable_ = Config::REED_CERRADO_ES_LOW
    ? (reedCerrado ? Protocolo::EstadoPuerta::CERRADA : Protocolo::EstadoPuerta::ABIERTA)
    : (reedCerrado ? Protocolo::EstadoPuerta::ABIERTA : Protocolo::EstadoPuerta::CERRADA);
  candidato_ = estable_;
  inicioEstable_ = millis();
}

bool SensorPuerta::actualizar() {
  const bool reedCerrado = (digitalRead(Config::PIN_REED) == LOW);
  const Protocolo::EstadoPuerta lectura = Config::REED_CERRADO_ES_LOW
    ? (reedCerrado ? Protocolo::EstadoPuerta::CERRADA : Protocolo::EstadoPuerta::ABIERTA)
    : (reedCerrado ? Protocolo::EstadoPuerta::ABIERTA : Protocolo::EstadoPuerta::CERRADA);

  const uint32_t ahora = millis();
  if (lectura != candidato_) {
    candidato_ = lectura;
    inicioEstable_ = ahora;
  }

  if (candidato_ != estable_ && static_cast<uint32_t>(ahora - inicioEstable_) >= Config::DEBOUNCE_MS) {
    estable_ = candidato_;
    return true;
  }
  return false;
}

Protocolo::EstadoPuerta SensorPuerta::estado() const {
  return estable_;
}
