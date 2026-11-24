#pragma once

#include "config.h"
#include <ctype.h>
#include <stdint.h>
#include <string.h>

// Parser sin String, esperas ni asignaciones dinamicas.
class MonitorSerie {
public:
  enum class Resultado { INCOMPLETA, LISTA, INVALIDA };

  Resultado recibir(char caracter) {
    if (caracter == '\r' || caracter == '\n') {
      if (descartando_) {
        longitud_ = 0;
        descartando_ = false;
        return Resultado::INVALIDA;
      }
      while (longitud_ > 0 && buffer_[longitud_ - 1] == ' ') --longitud_;
      buffer_[longitud_] = '\0';
      longitud_ = 0;
      return buffer_[0] == '\0' ? Resultado::INCOMPLETA : Resultado::LISTA;
    }
    const auto byte = static_cast<unsigned char>(caracter);
    if (descartando_) return Resultado::INCOMPLETA;
    if (byte < 32 || byte > 126 || longitud_ >= sizeof(buffer_) - 1) {
      descartando_ = true; // No ejecutar un sufijo de una linea truncada.
      return Resultado::INCOMPLETA;
    }
    if (longitud_ == 0 && caracter == ' ') return Resultado::INCOMPLETA;
    buffer_[longitud_++] = static_cast<char>(tolower(byte));
    return Resultado::INCOMPLETA;
  }

  const char* linea() const { return buffer_; }

  static bool entero(const char* texto, uint32_t& valor) {
    if (texto == nullptr || *texto == '\0') return false;
    uint32_t numero = 0;
    for (const char* p = texto; *p != '\0'; ++p) {
      if (*p < '0' || *p > '9') return false;
      const uint32_t digito = static_cast<uint32_t>(*p - '0');
      if (numero > (UINT32_MAX - digito) / 10) return false;
      numero = numero * 10 + digito;
    }
    valor = numero;
    return true;
  }

private:
  char buffer_[Config::CAPACIDAD_COMANDO]{};
  uint8_t longitud_ = 0;
  bool descartando_ = false;
};
