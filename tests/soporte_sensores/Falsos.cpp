#include "Soporte.h"
#include <cstdarg>
#include <cstdio>

namespace Falso {
uint32_t ahora;
byte nivel[40];
byte modo[40];
byte spi[4];
byte versionRfid;
unsigned inicializacionesRfid;
bool tarjetaPresente;
byte uidLen;
byte uid[10];
}

SerieFalsa Serial;
SpiFalso SPI;

void reiniciarFalsos() {
  Falso::ahora = 0;
  std::fill_n(Falso::nivel, sizeof(Falso::nivel), HIGH);
  std::fill_n(Falso::modo, sizeof(Falso::modo), 0);
  std::fill_n(Falso::spi, sizeof(Falso::spi), 0);
  Falso::versionRfid = 0x92;
  Falso::inicializacionesRfid = 0;
  Falso::tarjetaPresente = false;
  Falso::uidLen = 0;
  std::fill_n(Falso::uid, sizeof(Falso::uid), 0);
  Serial.salida.clear();
}

void SerieFalsa::println(const char* mensaje) {
  salida += mensaje;
  salida += '\n';
}

void SerieFalsa::printf(const char* formato, ...) {
  char mensaje[256];
  va_list argumentos;
  va_start(argumentos, formato);
  const int escritos = std::vsnprintf(mensaje, sizeof(mensaje), formato, argumentos);
  va_end(argumentos);
  if (escritos < 0 || static_cast<size_t>(escritos) >= sizeof(mensaje)) {
    throw std::runtime_error("Salida serie fuera de rango");
  }
  salida.append(mensaje, static_cast<size_t>(escritos));
}
