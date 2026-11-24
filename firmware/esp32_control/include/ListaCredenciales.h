#pragma once

#include "Tipos.h"
#include <stdint.h>

class ListaCredenciales {
public:
  ListaCredenciales();
  bool agregar(const uint8_t* uid, uint8_t longitud, Accion accion, const char* credentialId);
  bool buscar(const uint8_t* uid, uint8_t longitud, Accion& accion,
              const char*& credentialId) const;

private:
  struct Credencial {
    uint8_t uid[10];
    uint8_t longitud;
    Accion accion;
    char credentialId[24];
  };
  static constexpr uint8_t MAX_CREDENCIALES = 8;
  Credencial credenciales_[MAX_CREDENCIALES];
  uint8_t cantidad_;
  static bool coinciden(const Credencial& credencial, const uint8_t* uid, uint8_t longitud);
};
