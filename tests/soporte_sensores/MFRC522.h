#pragma once

#include "Arduino.h"
#include <algorithm>

namespace Falso {
extern byte spi[4];
extern byte versionRfid;
extern unsigned inicializacionesRfid;
extern bool tarjetaPresente;
extern byte uidLen;
extern byte uid[10];
}

struct SpiFalso {
  void begin(byte sck, byte miso, byte mosi, byte ss) {
    Falso::spi[0] = sck;
    Falso::spi[1] = miso;
    Falso::spi[2] = mosi;
    Falso::spi[3] = ss;
  }
};
extern SpiFalso SPI;

class MFRC522 {
public:
  enum Registro { VersionReg };
  struct {
    byte size = 0;
    byte uidByte[10] = {};
  } uid;

  MFRC522(byte, byte) {}
  void PCD_Init() { ++Falso::inicializacionesRfid; }
  byte PCD_ReadRegister(Registro) { return Falso::versionRfid; }
  bool PICC_IsNewCardPresent() { return Falso::tarjetaPresente; }
  bool PICC_ReadCardSerial() {
    Falso::tarjetaPresente = false;
    uid.size = Falso::uidLen;
    std::copy_n(Falso::uid, sizeof(uid.uidByte), uid.uidByte);
    return true;
  }
  void PICC_HaltA() {}
  void PCD_StopCrypto1() {}
};
