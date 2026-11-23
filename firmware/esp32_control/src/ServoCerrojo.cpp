#include "ServoCerrojo.h"

#include "config.h"
#include <Arduino.h>
#include <ESP32Servo.h>

static Servo servoCerrojo;

ServoCerrojo::ServoCerrojo(uint8_t pin, uint32_t tiempoDetachMs)
  : pin_(pin), tiempoDetachMs_(tiempoDetachMs),
    conectado_(false), momentoDetach_(0) {}

void ServoCerrojo::begin() {
  servoCerrojo.setPeriodHertz(50);
}

void ServoCerrojo::moverA(int angulo) {
  if (angulo < 0) angulo = 0;
  if (angulo > 180) angulo = 180;
  if (!conectado_) {
    servoCerrojo.attach(pin_);
    conectado_ = true;
  }
  servoCerrojo.write(angulo);
  momentoDetach_ = millis() + tiempoDetachMs_;
}

void ServoCerrojo::bloquear() {
  moverA(Config::ANGULO_BLOQUEO);
}

void ServoCerrojo::desbloquear() {
  moverA(Config::ANGULO_DESBLOQUEO);
}

void ServoCerrojo::actualizar() {
  if (conectado_ && momentoDetach_ != 0 &&
      static_cast<uint32_t>(millis() - momentoDetach_) < 0x80000000UL) {
    servoCerrojo.detach();
    conectado_ = false;
    momentoDetach_ = 0;
  }
}
