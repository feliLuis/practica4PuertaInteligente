#pragma once

#include <stdint.h>

class Led {
public:
  Led(uint8_t pin);
  void begin();
  void encender();
  void apagar();
  void encenderPor(uint32_t milisegundos);
  void actualizar();

private:
  uint8_t pin_;
  bool encendido_;
  uint32_t vence_;
};
