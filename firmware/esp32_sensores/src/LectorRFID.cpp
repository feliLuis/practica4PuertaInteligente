#include "LectorRFID.h"

#include "config.h"
#include <Arduino.h>

LectorRFID::LectorRFID(uint8_t ss, uint8_t rst)
  : ss_(ss), rst_(rst), disponible_(false),
    ultimaPresentacion_(0), uidAnteriorLen_(0),
    lectorMfrc_(ss, rst) {
  for (uint8_t i = 0; i < 10; ++i) uidAnterior_[i] = 0;
}

bool LectorRFID::begin() {
  SPI.begin(Config::RC522_SCK, Config::RC522_MISO, Config::RC522_MOSI, ss_);
  lectorMfrc_.PCD_Init();
  const byte version = lectorMfrc_.PCD_ReadRegister(lectorMfrc_.VersionReg);
  disponible_ = !(version == 0x00 || version == 0xFF);
  if (!disponible_) {
    Serial.println("[DIAG] RC522 no responde: VersionReg=0x00 o 0xFF. Revise 3V3, GND y cables SPI.");
  }
  return disponible_;
}

bool LectorRFID::disponible() const {
  return disponible_;
}

bool LectorRFID::leer(uint8_t* uid, uint8_t& longitud) {
  if (!disponible_ || uid == nullptr || !lectorMfrc_.PICC_IsNewCardPresent()) return false;
  if (!lectorMfrc_.PICC_ReadCardSerial()) return false;

  longitud = lectorMfrc_.uid.size;
  if (longitud > 10) longitud = 10;
  for (uint8_t i = 0; i < longitud; ++i) uid[i] = lectorMfrc_.uid.uidByte[i];

  const uint32_t ahora = millis();
  bool igualAnterior = longitud == uidAnteriorLen_;
  if (igualAnterior) {
    for (uint8_t i = 0; i < longitud && igualAnterior; ++i) {
      igualAnterior = uid[i] == uidAnterior_[i];
    }
  }

  lectorMfrc_.PICC_HaltA();
  lectorMfrc_.PCD_StopCrypto1();

  if (igualAnterior && static_cast<uint32_t>(ahora - ultimaPresentacion_) < Config::ANTIRREPETICION_RFID_MS) {
    return false;
  }

  ultimaPresentacion_ = ahora;
  uidAnteriorLen_ = longitud;
  for (uint8_t i = 0; i < longitud; ++i) uidAnterior_[i] = uid[i];
  return true;
}
