#include "ListaCredenciales.h"

#include <string.h>

ListaCredenciales::ListaCredenciales() : cantidad_(0) {
  for (uint8_t i = 0; i < MAX_CREDENCIALES; ++i) {
    credenciales_[i].longitud = 0;
  }
}

bool ListaCredenciales::coinciden(const Credencial& credencial, const uint8_t* uid, uint8_t longitud) {
  if (credencial.longitud != longitud) return false;
  return memcmp(credencial.uid, uid, longitud) == 0;
}

void ListaCredenciales::agregar(const uint8_t* uid, uint8_t longitud, Accion accion) {
  if (uid == nullptr || longitud == 0 || longitud > 10 || cantidad_ >= MAX_CREDENCIALES) return;
  for (uint8_t i = 0; i < cantidad_; ++i) {
    if (coinciden(credenciales_[i], uid, longitud)) {
      credenciales_[i].accion = accion;
      return;
    }
  }
  Credencial& credencial = credenciales_[cantidad_++];
  credencial.longitud = longitud;
  credencial.accion = accion;
  memcpy(credencial.uid, uid, longitud);
}

bool ListaCredenciales::buscar(const uint8_t* uid, uint8_t longitud, Accion& accion) const {
  if (uid == nullptr) return false;
  for (uint8_t i = 0; i < cantidad_; ++i) {
    if (coinciden(credenciales_[i], uid, longitud)) {
      accion = credenciales_[i].accion;
      return true;
    }
  }
  return false;
}
