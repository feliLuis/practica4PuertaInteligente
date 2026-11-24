#pragma once

#include <stdint.h>
#include <ESP32Servo.h>

class ServoCerrojo {
public:
  ServoCerrojo(uint8_t pin, uint32_t tiempoDetachMs);
  bool begin(); // Configura sin conectar PWM ni mover.
  bool bloquear();
  bool desbloquear();
  bool moverA(int angulo); // Solo posiciones acordadas; true = orden emitida.
  void actualizar();

private:
  Servo servo_;
  uint8_t pin_;
  uint32_t tiempoDetachMs_;
  bool inicializado_;
  bool conectado_;
  uint32_t ultimoMovimiento_;
};
