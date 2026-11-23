#pragma once

#include "Tipos.h"
#include <stdint.h>

class ListaCredenciales {
public:
  ListaCredenciales();
  void agregar(const uint8_t* uid, uint8_t longitud, Accion accion);
  bool buscar(const uint8_t* uid, uint8_t longitud, Accion& accion) const;

private:
  struct Credencial {
    uint8_t uid[10];
    uint8_t longitud;
    Accion accion;
  };
  static constexpr uint8_t MAX_CREDENCIALES = 8;
  Credencial credenciales_[MAX_CREDENCIALES];
  uint8_t cantidad_;
  static bool coinciden(const Credencial& credencial, const uint8_t* uid, uint8_t longitud);
};
