#include <Arduino.h>
#include "config.h"
#include "SensorPuerta.h"
#include "LectorRFID.h"
#include "EnlaceSerial.h"

SensorPuerta sensorPuerta;
LectorRFID lectorRfid(Config::RC522_SS, Config::RC522_RST);
EnlaceSerial enlace;

uint32_t secuencia = 0;
uint32_t ultimoLatido = 0;
uint32_t ultimoIntentoRfid = 0;

void enviar(Protocolo::Tipo tipo, const uint8_t* uid = nullptr, uint8_t uidLen = 0) {
  Protocolo::Mensaje mensaje{};
  mensaje.version = Protocolo::VERSION;
  mensaje.tipo = tipo;
  mensaje.seq = ++secuencia;
  mensaje.puerta = sensorPuerta.estado();
  mensaje.uidLen = uidLen;
  if (uid != nullptr && uidLen > 0) {
    for (uint8_t i = 0; i < uidLen && i < 10; ++i) mensaje.uid[i] = uid[i];
  }
  mensaje.uptimeMs = millis();
  enlace.enviar(mensaje);
}

void setup() {
  Serial.begin(Config::VELOCIDAD_SERIE);
  delay(100);
  sensorPuerta.begin();
  enlace.begin();

  Serial.printf("[DIAG] Reed estable inicial: %s (GPIO %u)\n",
    Protocolo::nombre(sensorPuerta.estado()), Config::PIN_REED);

  if (lectorRfid.begin()) {
    Serial.println("[DIAG] RC522 respondio correctamente.");
  }
  ultimoIntentoRfid = millis();

  ultimoLatido = millis();
  enviar(Protocolo::Tipo::REED_ESTADO);
}

void loop() {
  if (!lectorRfid.disponible()) {
    const uint32_t ahoraReintento = millis();
    if (static_cast<uint32_t>(ahoraReintento - ultimoIntentoRfid) >= Config::REINTENTO_RFID_MS) {
      ultimoIntentoRfid = ahoraReintento;
      lectorRfid.begin();
    }
  }

  if (sensorPuerta.actualizar()) {
    enviar(Protocolo::Tipo::REED_ESTADO);
  }

  uint8_t uid[10];
  uint8_t longitud = 0;
  if (lectorRfid.leer(uid, longitud)) {
    enviar(Protocolo::Tipo::RFID_PRESENTADA, uid, longitud);
  }

  const uint32_t ahora = millis();
  if (static_cast<uint32_t>(ahora - ultimoLatido) >= Config::LATIDO_MS) {
    ultimoLatido = ahora;
    enviar(Protocolo::Tipo::LATIDO);
  }
}
