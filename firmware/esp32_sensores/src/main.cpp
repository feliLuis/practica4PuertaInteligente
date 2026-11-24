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

void enviar(Protocolo::Tipo tipo, const uint8_t* uid = nullptr, uint8_t uidLen = 0) {
  if ((uidLen != 0 && uidLen != 4 && uidLen != 7 && uidLen != 10) ||
      (uidLen > 0 && uid == nullptr)) return;
  Protocolo::Mensaje mensaje{};
  mensaje.version = Protocolo::VERSION;
  mensaje.tipo = tipo;
  mensaje.seq = ++secuencia;
  mensaje.puerta = sensorPuerta.estado();
  mensaje.uidLen = uidLen;
  for (uint8_t i = 0; i < uidLen; ++i) mensaje.uid[i] = uid[i];
  mensaje.uptimeMs = millis();
  if (!enlace.enviar(mensaje)) Serial.println("[SIM-TX] Mensaje rechazado por el simulador.");
}

void setup() {
  Serial.begin(Config::VELOCIDAD_SERIE);
  enlace.begin();
  lectorRfid.begin();
  // La estabilizacion del Reed comienza despues de la inicializacion bloqueante del RC522.
  sensorPuerta.begin();
  ultimoLatido = millis();
  Serial.println("[BOOT] Reed desconocido hasta completar 100 ms estables.");
  Serial.println("[BOOT] Solo salida serie simulada: sin comunicacion con ESP32 #1.");
  Serial.println(Config::MOSTRAR_UID_COMPLETO ? "[DIAG] UID completo habilitado solo en USB."
                                            : "[NORMAL] UID oculto en USB.");
}

void loop() {
  // Leer antes de posibles esperas internas del RC522.
  if (sensorPuerta.actualizar()) enviar(Protocolo::Tipo::REED_ESTADO);
  lectorRfid.actualizar();
  uint8_t uid[10];
  uint8_t longitud = 0;
  if (lectorRfid.leer(uid, sizeof(uid), longitud))
    enviar(Protocolo::Tipo::RFID_PRESENTADA, uid, longitud);

  const uint32_t ahora = millis();
  if (static_cast<uint32_t>(ahora - ultimoLatido) >= Config::LATIDO_MS) {
    ultimoLatido = ahora;
    enviar(Protocolo::Tipo::LATIDO);
  }
}
