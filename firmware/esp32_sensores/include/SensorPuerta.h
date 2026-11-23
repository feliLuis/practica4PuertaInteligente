#pragma once

#include "config.h"
#include <Protocolo.h>
#include <stdint.h>

class SensorPuerta {
public:
  SensorPuerta();
  void begin();
  bool actualizar();
  Protocolo::EstadoPuerta estado() const;

private:
  Protocolo::EstadoPuerta estable_;
  Protocolo::EstadoPuerta candidato_;
  uint32_t inicioEstable_;
};
