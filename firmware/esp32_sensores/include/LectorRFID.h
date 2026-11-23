#pragma once

#include <stdint.h>
#include <MFRC522.h>

class LectorRFID {
public:
  LectorRFID(uint8_t ss, uint8_t rst);
  bool begin();
  bool leer(uint8_t* uid, uint8_t& longitud);
  bool disponible() const;

private:
  uint8_t ss_;
  uint8_t rst_;
  bool disponible_;
  uint32_t ultimaPresentacion_;
  uint8_t uidAnterior_[10];
  uint8_t uidAnteriorLen_;
  MFRC522 lectorMfrc_;
};
