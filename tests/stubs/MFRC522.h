#pragma once
#include "Arduino.h"
#include <cstring>

struct SpiFalso {
  void begin(int sck, int miso, int mosi, int ss) {
    Falso::spi[0] = sck; Falso::spi[1] = miso; Falso::spi[2] = mosi; Falso::spi[3] = ss;
  }
};
extern SpiFalso SPI;

class MFRC522 {
public:
  enum { VersionReg = 0x37 };
  struct Uid { byte size; byte uidByte[10]; } uid{};
  MFRC522(byte, byte) {}
  void PCD_Init() { ++Falso::inicializacionesRfid; }
  byte PCD_ReadRegister(int) { return Falso::versionRfid; }
  bool PICC_IsNewCardPresent() { return Falso::tarjetaPresente; }
  bool PICC_ReadCardSerial() {
    if (!Falso::lecturaRfidOk) return false;
    uid.size = Falso::uidLen;
    std::memcpy(uid.uidByte, Falso::uid, sizeof(uid.uidByte));
    return true;
  }
  void PICC_HaltA() { Falso::tarjetaPresente = false; }
  void PCD_StopCrypto1() {}
};
