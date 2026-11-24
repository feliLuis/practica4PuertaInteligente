#pragma once

#include "EnlaceLocal.h"

class EnlaceSerial : public EnlaceLocal {
public:
  bool begin() override;
  bool enviar(const Protocolo::Mensaje& mensaje) override;
  void alRecibir(std::function<void(const Protocolo::Mensaje&)> callback) override;

private:
  std::function<void(const Protocolo::Mensaje&)> callback_;
};
