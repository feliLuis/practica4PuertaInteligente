#include "ListaCredenciales.h"

#include <string.h>

ListaCredenciales::ListaCredenciales() : credenciales_{}, cantidad_(0) {}

bool ListaCredenciales::coinciden(const Credencial& credencial, const uint8_t* uid, uint8_t longitud) {
  return credencial.longitud == longitud && memcmp(credencial.uid, uid, longitud) == 0;
}

bool ListaCredenciales::agregar(const uint8_t* uid, uint8_t longitud, Accion accion,
                              const char* credentialId) {
  // RFID solo autoriza desbloqueo; nunca registra bloqueos ni alternancias.
  if (uid == nullptr || (longitud != 4 && longitud != 7 && longitud != 10) ||
      credentialId == nullptr || credentialId[0] == '\0' ||
      strlen(credentialId) >= sizeof(credenciales_[0].credentialId) ||
      accion != Accion::DESBLOQUEAR) return false;

  uint8_t indice = 0;
  while (indice < cantidad_ && !coinciden(credenciales_[indice], uid, longitud)) ++indice;
  if (indice == cantidad_) {
    if (cantidad_ == MAX_CREDENCIALES) return false;
    ++cantidad_;
  }
  Credencial& credencial = credenciales_[indice];
  credencial.longitud = longitud;
  credencial.accion = accion;
  memcpy(credencial.uid, uid, longitud);
  strcpy(credencial.credentialId, credentialId);
  return true;
}

bool ListaCredenciales::buscar(const uint8_t* uid, uint8_t longitud, Accion& accion,
                             const char*& credentialId) const {
  credentialId = nullptr;
  if (uid == nullptr || (longitud != 4 && longitud != 7 && longitud != 10)) return false;
  for (uint8_t i = 0; i < cantidad_; ++i) {
    if (coinciden(credenciales_[i], uid, longitud)) {
      accion = credenciales_[i].accion;
      credentialId = credenciales_[i].credentialId;
      return true;
    }
  }
  return false;
}
