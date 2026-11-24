#pragma once
#include "Arduino.h"
#include <cstring>

struct FakeSpi {
  void begin(int sck, int miso, int mosi, int ss) {
    Fake::spi[0] = sck; Fake::spi[1] = miso; Fake::spi[2] = mosi; Fake::spi[3] = ss;
  }
};
extern FakeSpi SPI;

class MFRC522 {
public:
  enum { VersionReg = 0x37 };
  struct Uid { byte size; byte uidByte[10]; } uid{};
  MFRC522(byte, byte) {}
  void PCD_Init() { ++Fake::rfidInitializations; }
  byte PCD_ReadRegister(int) { return Fake::rfidVersion; }
  bool PICC_IsNewCardPresent() { return Fake::cardPresent; }
  bool PICC_ReadCardSerial() {
    if (!Fake::rfidReadOk) return false;
    uid.size = Fake::uidLen;
    std::memcpy(uid.uidByte, Fake::uid, sizeof(uid.uidByte));
    return true;
  }
  void PICC_HaltA() { Fake::cardPresent = false; }
  void PCD_StopCrypto1() {}
};
