#pragma once

#include <stdint.h>

class ServoCerrojo {
public:
  ServoCerrojo(uint8_t pin, uint32_t tiempoDetachMs);
  void begin();
  void bloquear();
  void desbloquear();
  void moverA(int angulo);
  void actualizar();

private:
  uint8_t pin_;
  uint32_t tiempoDetachMs_;
  bool conectado_;
  uint32_t momentoDetach_;
};
