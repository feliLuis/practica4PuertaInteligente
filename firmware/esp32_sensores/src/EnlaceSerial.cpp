#include "EnlaceSerial.h"

#include "config.h"
#include <Arduino.h>
#include <stdio.h>

bool EnlaceSerial::begin() {
  return true;
}

void EnlaceSerial::alRecibir(std::function<void(const Protocolo::Mensaje&)> callback) {
  callback_ = callback;
}

bool EnlaceSerial::enviar(const Protocolo::Mensaje& mensaje) {
  char uid[21] = "-";
  if (mensaje.uidLen > 0) {
    Protocolo::uidAHex(mensaje.uid, mensaje.uidLen, uid, sizeof(uid));
  }
  Serial.printf("[TX] seq=%lu tipo=%s puerta=%s uid=%s\n",
    static_cast<unsigned long>(mensaje.seq),
    Protocolo::nombre(mensaje.tipo),
    Protocolo::nombre(mensaje.puerta),
    uid);
  return true;
}
