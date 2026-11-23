#pragma once

#include <stdint.h>
#include <stddef.h>

namespace Protocolo {

constexpr uint8_t VERSION = 1;

enum class Tipo : uint8_t {
  REED_ESTADO = 1,
  RFID_PRESENTADA = 2,
  LATIDO = 3
};

enum class EstadoPuerta : uint8_t {
  DESCONOCIDA = 0,
  ABIERTA = 1,
  CERRADA = 2
};

struct __attribute__((packed)) Mensaje {
  uint8_t version;
  Tipo tipo;
  uint32_t seq;
  EstadoPuerta puerta;
  uint8_t uidLen;
  uint8_t uid[10];
  uint32_t uptimeMs;
};

const char* nombre(EstadoPuerta estado);
const char* nombre(Tipo tipo);
bool uidAHex(const uint8_t* uid, uint8_t longitud, char* salida, size_t capacidad);
bool hexAUid(const char* texto, uint8_t* uid, uint8_t capacidad, uint8_t& longitud);

} // namespace Protocolo
