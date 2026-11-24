#pragma once

#include <stdint.h>
#include <MFRC522.h>

class LectorRFID {
public:
  LectorRFID(uint8_t ss, uint8_t rst);
  bool begin();
  void actualizar();
  bool leer(uint8_t* uid, uint8_t capacidad, uint8_t& longitud);
  bool disponible() const;

private:
  uint8_t ss_;
  bool disponible_;
  bool spiIniciado_;
  uint32_t ultimoIntento_;
  uint32_t ultimaComprobacion_;
  uint32_t ultimoSondeo_;
  uint32_t ultimaPresentacion_;
  uint8_t uidAnterior_[10];
  uint8_t uidAnteriorLen_;
  MFRC522 lectorMfrc_;
};
