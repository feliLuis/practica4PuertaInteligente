#include "SerialLink.h"

#include "config.h"
#include <Arduino.h>
#include <stdio.h>

bool SerialLink::begin() {
  return true;
}

void SerialLink::onReceive(std::function<void(const Protocol::Message&)> callback) {
  callback_ = callback;
}

bool SerialLink::send(const Protocol::Message& message) {
  char uid[21] = "-";
  if (message.uidLen > sizeof(message.uid)) return false;
  if (message.uidLen > 0) {
    if (Config::SHOW_FULL_UID) {
      if (!Protocol::uidToHex(message.uid, message.uidLen, uid, sizeof(uid))) return false;
    } else {
      snprintf(uid, sizeof(uid), "oculto");
    }
  }
  Serial.printf("[SIM-TX SIN ENLACE] seq=%lu tipo=%s puerta=%s uid=%s\n",
    static_cast<unsigned long>(message.seq),
    Protocol::name(message.type),
    Protocol::name(message.doorState),
    uid);
  return true;
}
