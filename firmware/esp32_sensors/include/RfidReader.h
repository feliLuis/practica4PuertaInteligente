#pragma once

#include <stdint.h>
#include <MFRC522.h>

class RfidReader {
public:
  RfidReader(uint8_t ss, uint8_t rst);
  bool begin();
  void update();
  bool read(uint8_t* uid, uint8_t capacity, uint8_t& length);
  bool available() const;

private:
  uint8_t ss_;
  bool available_;
  bool spiStarted_;
  uint32_t lastAttempt_;
  uint32_t lastCheck_;
  uint32_t lastPoll_;
  uint32_t lastPresentation_;
  uint8_t previousUid_[10];
  uint8_t previousUidLength_;
  MFRC522 mfrcReader_;
};
