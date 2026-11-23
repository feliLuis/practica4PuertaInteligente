#pragma once

#include <stdint.h>

class Buzzer {
public:
  Buzzer(uint8_t pin, bool activo);
  void begin();
  void sonarPatron(uint32_t encendidoMs, uint32_t apagadoMs, uint8_t repeticiones);
  void actualizar();
  void apagar();

private:
  uint8_t pin_;
  bool activo_;
  bool sonando_;
  bool faseEncendida_;
  uint32_t cambio_;
  uint32_t encendidoMs_;
  uint32_t apagadoMs_;
  uint8_t pitidosPendientes_;
};
