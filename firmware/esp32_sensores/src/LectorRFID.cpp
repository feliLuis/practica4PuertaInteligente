#include "LectorRFID.h"

#include "config.h"
#include <Arduino.h>

LectorRFID::LectorRFID(uint8_t ss, uint8_t rst)
  : ss_(ss), disponible_(false), spiIniciado_(false),
    ultimoIntento_(0), ultimaComprobacion_(0), ultimoSondeo_(0),
    ultimaPresentacion_(0), uidAnterior_{}, uidAnteriorLen_(0),
    lectorMfrc_(ss, rst) {}

bool LectorRFID::begin() {
  ultimoIntento_ = millis();
  if (!spiIniciado_) {
    SPI.begin(Config::RC522_SCK, Config::RC522_MISO, Config::RC522_MOSI, ss_);
    spiIniciado_ = true;
  }
  // PCD_Init tiene esperas internas de la biblioteca. Solo arranque/reintento cada 5 s.
  lectorMfrc_.PCD_Init();
  const byte version = lectorMfrc_.PCD_ReadRegister(lectorMfrc_.VersionReg);
  disponible_ = version != 0x00 && version != 0xFF;
  ultimaComprobacion_ = millis();
  Serial.println(disponible_ ? "[DIAG] RC522 responde; lectura fisica pendiente de verificar."
                            : "[DIAG] RC522 no responde. Revise 3V3, GND y SPI.");
  return disponible_;
}

void LectorRFID::actualizar() {
  const uint32_t ahora = millis();
  if (!disponible_) {
    if (static_cast<uint32_t>(ahora - ultimoIntento_) >= Config::REINTENTO_RFID_MS) begin();
    return;
  }
  if (static_cast<uint32_t>(ahora - ultimaComprobacion_) < Config::COMPROBACION_RFID_MS) return;
  ultimaComprobacion_ = ahora;
  const byte version = lectorMfrc_.PCD_ReadRegister(lectorMfrc_.VersionReg);
  if (version == 0x00 || version == 0xFF) {
    disponible_ = false;
    ultimoIntento_ = ahora; // No reinicializar inmediatamente ante el fallo.
    Serial.println("[DIAG] Se perdio el RC522; reintento espaciado.");
  }
}

bool LectorRFID::disponible() const { return disponible_; }

bool LectorRFID::leer(uint8_t* uid, uint8_t capacidad, uint8_t& longitud) {
  longitud = 0;
  if (!disponible_ || uid == nullptr) return false;
  const uint32_t ahora = millis();
  if (static_cast<uint32_t>(ahora - ultimoSondeo_) < Config::SONDEO_RFID_MS) return false;
  ultimoSondeo_ = ahora;
  if (!lectorMfrc_.PICC_IsNewCardPresent() || !lectorMfrc_.PICC_ReadCardSerial()) return false;

  const uint8_t cantidad = lectorMfrc_.uid.size;
  if ((cantidad != 4 && cantidad != 7 && cantidad != 10) || cantidad > capacidad) {
    lectorMfrc_.PICC_HaltA();
    lectorMfrc_.PCD_StopCrypto1();
    return false;
  }

  bool igualAnterior = cantidad == uidAnteriorLen_;
  for (uint8_t i = 0; i < cantidad && igualAnterior; ++i)
    igualAnterior = lectorMfrc_.uid.uidByte[i] == uidAnterior_[i];
  lectorMfrc_.PICC_HaltA();
  lectorMfrc_.PCD_StopCrypto1();
  if (igualAnterior &&
      static_cast<uint32_t>(ahora - ultimaPresentacion_) < Config::ANTIRREPETICION_RFID_MS) return false;

  longitud = cantidad;
  ultimaPresentacion_ = ahora;
  uidAnteriorLen_ = cantidad;
  for (uint8_t i = 0; i < cantidad; ++i)
    uid[i] = uidAnterior_[i] = lectorMfrc_.uid.uidByte[i];
  return true;
}
