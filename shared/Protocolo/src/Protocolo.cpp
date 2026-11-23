#include "Protocolo.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace Protocolo {

const char* nombre(EstadoPuerta estado) {
  switch (estado) {
    case EstadoPuerta::ABIERTA: return "abierta";
    case EstadoPuerta::CERRADA: return "cerrada";
    default: return "desconocida";
  }
}

const char* nombre(Tipo tipo) {
  switch (tipo) {
    case Tipo::REED_ESTADO: return "REED_ESTADO";
    case Tipo::RFID_PRESENTADA: return "RFID_PRESENTADA";
    case Tipo::LATIDO: return "LATIDO";
    default: return "DESCONOCIDO";
  }
}

bool uidAHex(const uint8_t* uid, uint8_t longitud, char* salida, size_t capacidad) {
  if (uid == nullptr || salida == nullptr || longitud > 10) return false;
  if (capacidad < static_cast<size_t>(longitud * 2 + 1)) return false;

  char* cursor = salida;
  for (uint8_t i = 0; i < longitud; ++i) {
    snprintf(cursor, 3, "%02X", uid[i]);
    cursor += 2;
  }
  *cursor = '\0';
  return true;
}

static int valorHex(char caracter) {
  if (caracter >= '0' && caracter <= '9') return caracter - '0';
  caracter = static_cast<char>(toupper(static_cast<unsigned char>(caracter)));
  if (caracter >= 'A' && caracter <= 'F') return caracter - 'A' + 10;
  return -1;
}

bool hexAUid(const char* texto, uint8_t* uid, uint8_t capacidad, uint8_t& longitud) {
  longitud = 0;
  if (texto == nullptr || uid == nullptr) return false;

  char limpio[32];
  size_t n = 0;
  for (const char* p = texto; *p != '\0'; ++p) {
    if (isspace(static_cast<unsigned char>(*p))) continue;
    if (n >= sizeof(limpio) - 1) return false;
    limpio[n++] = *p;
  }
  limpio[n] = '\0';

  if (n == 0 || (n & 1) != 0) return false;
  const uint8_t cantidad = static_cast<uint8_t>(n / 2);
  if (cantidad > capacidad) return false;

  for (uint8_t i = 0; i < cantidad; ++i) {
    int alto = valorHex(limpio[i * 2]);
    int bajo = valorHex(limpio[i * 2 + 1]);
    if (alto < 0 || bajo < 0) return false;
    uid[i] = static_cast<uint8_t>((alto << 4) | bajo);
  }
  longitud = cantidad;
  return true;
}

} // namespace Protocolo
