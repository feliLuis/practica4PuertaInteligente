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
  if (mensaje.uidLen > sizeof(mensaje.uid)) return false;
  if (mensaje.uidLen > 0) {
    if (Config::MOSTRAR_UID_COMPLETO) {
      if (!Protocolo::uidAHex(mensaje.uid, mensaje.uidLen, uid, sizeof(uid))) return false;
    } else {
      snprintf(uid, sizeof(uid), "oculto");
    }
  }
  Serial.printf("[SIM-TX SIN ENLACE] seq=%lu tipo=%s puerta=%s uid=%s\n",
    static_cast<unsigned long>(mensaje.seq),
    Protocolo::nombre(mensaje.tipo),
    Protocolo::nombre(mensaje.puerta),
    uid);
  return true;
}
