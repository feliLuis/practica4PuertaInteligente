#pragma once

#include "Led.h"
#include "Buzzer.h"

class IndicadorAcceso {
public:
  IndicadorAcceso(uint8_t pinLedVerde, uint8_t pinLedRojo, uint8_t pinBuzzer, bool buzzerActivo);
  void begin();
  void accesoAceptado();
  void accesoDenegado();
  void testLedVerde();
  void testLedRojo();
  void testBuzzer();
  void actualizar();

private:
  Led ledVerde_;
  Led ledRojo_;
  Buzzer buzzer_;
};
