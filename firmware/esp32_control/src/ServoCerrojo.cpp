#include "ServoCerrojo.h"

#include "config.h"
#include <Arduino.h>
#include <driver/gpio.h>

ServoCerrojo::ServoCerrojo(uint8_t pin, uint32_t tiempoDetachMs)
  : pin_(pin), tiempoDetachMs_(tiempoDetachMs), inicializado_(false),
    conectado_(false), ultimoMovimiento_(0) {}

bool ServoCerrojo::begin() {
  inicializado_ = GPIO_IS_VALID_OUTPUT_GPIO(pin_);
  if (inicializado_) servo_.setPeriodHertz(50);
  return inicializado_;
}

bool ServoCerrojo::moverA(int angulo) {
  if (!inicializado_ ||
      (angulo != Config::ANGULO_BLOQUEO && angulo != Config::ANGULO_DESBLOQUEO)) return false;
  if (!servo_.attached()) {
    const int resultado = servo_.attach(pin_);
    // En ESP32Servo 0.13.0, attach devuelve 1 al aceptar; comprobar tambien attached.
    if (resultado <= 0 || !servo_.attached()) {
      servo_.detach();
      conectado_ = false;
      return false;
    }
  }
  servo_.write(angulo); // La biblioteca no devuelve confirmacion fisica del movimiento.
  conectado_ = servo_.attached();
  if (!conectado_) return false;
  ultimoMovimiento_ = millis();
  return true;
}

bool ServoCerrojo::bloquear() { return moverA(Config::ANGULO_BLOQUEO); }
bool ServoCerrojo::desbloquear() { return moverA(Config::ANGULO_DESBLOQUEO); }

void ServoCerrojo::actualizar() {
  if (conectado_ && static_cast<uint32_t>(millis() - ultimoMovimiento_) >= tiempoDetachMs_) {
    servo_.detach();
    conectado_ = false;
  }
}
