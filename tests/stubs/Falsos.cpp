#include "Arduino.h"
#include "MFRC522.h"
#include <algorithm>
#include <cstring>

SerieFalsa Serial;
SpiFalso SPI;
namespace Falso {
uint32_t ahora = 0;
int nivel[40]{};
int modo[40]{};
bool falloAttach = false;
int escrituras = 0;
int ultimoAngulo = -1;
byte versionRfid = 0x92;
bool tarjetaPresente = false;
bool lecturaRfidOk = true;
byte uid[10]{};
byte uidLen = 4;
int inicializacionesRfid = 0;
int spi[4]{};
void reiniciar() {
  ahora = 0;
  std::fill(nivel, nivel + 40, HIGH);
  std::fill(modo, modo + 40, 0);
  falloAttach = false;
  escrituras = 0;
  ultimoAngulo = -1;
  versionRfid = 0x92;
  tarjetaPresente = false;
  lecturaRfidOk = true;
  std::memset(uid, 0, sizeof(uid));
  uidLen = 4;
  inicializacionesRfid = 0;
  Serial.salida.clear();
  Serial.entrada.clear();
}
}
