#pragma once

#include "Protocolo.h"

#include <functional>

class EnlaceLocal {
public:
  virtual ~EnlaceLocal() = default;
  virtual bool begin() = 0;
  virtual bool enviar(const Protocolo::Mensaje& mensaje) = 0;
  virtual void alRecibir(std::function<void(const Protocolo::Mensaje&)> callback) = 0;
};
