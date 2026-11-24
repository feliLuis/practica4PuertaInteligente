#include "RfidReader.h"

#include "config.h"
#include <Arduino.h>

RfidReader::RfidReader(uint8_t ss, uint8_t rst)
  : ss_(ss), available_(false), spiStarted_(false),
    lastAttempt_(0), lastCheck_(0), lastPoll_(0),
    lastPresentation_(0), previousUid_{}, previousUidLength_(0),
    mfrcReader_(ss, rst) {}

bool RfidReader::begin() {
  lastAttempt_ = millis();
  if (!spiStarted_) {
    SPI.begin(Config::RC522_SCK, Config::RC522_MISO, Config::RC522_MOSI, ss_);
    spiStarted_ = true;
  }
  // PCD_Init blocks inside the library. Run it at startup or retry every 5 seconds.
  mfrcReader_.PCD_Init();
  const byte version = mfrcReader_.PCD_ReadRegister(mfrcReader_.VersionReg);
  available_ = version != 0x00 && version != 0xFF;
  lastCheck_ = millis();
  Serial.println(available_ ? "[DIAG] RC522 responds. Physical card reading still needs verification."
                            : "[DIAG] RC522 did not respond. Check 3V3, GND, and SPI.");
  return available_;
}

void RfidReader::update() {
  const uint32_t now = millis();
  if (!available_) {
    if (static_cast<uint32_t>(now - lastAttempt_) >= Config::RFID_RETRY_MS) begin();
    return;
  }
  if (static_cast<uint32_t>(now - lastCheck_) < Config::RFID_CHECK_MS) return;
  lastCheck_ = now;
  const byte version = mfrcReader_.PCD_ReadRegister(mfrcReader_.VersionReg);
  if (version == 0x00 || version == 0xFF) {
    available_ = false;
    lastAttempt_ = now; // Wait before reinitializing after a failure.
    Serial.println("[DIAG] Lost contact with the RC522. Waiting before retrying.");
  }
}

bool RfidReader::available() const { return available_; }

bool RfidReader::read(uint8_t* uid, uint8_t capacity, uint8_t& length) {
  length = 0;
  if (!available_ || uid == nullptr) return false;
  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - lastPoll_) < Config::RFID_POLL_MS) return false;
  lastPoll_ = now;
  if (!mfrcReader_.PICC_IsNewCardPresent() || !mfrcReader_.PICC_ReadCardSerial()) return false;

  const uint8_t count = mfrcReader_.uid.size;
  if ((count != 4 && count != 7 && count != 10) || count > capacity) {
    mfrcReader_.PICC_HaltA();
    mfrcReader_.PCD_StopCrypto1();
    return false;
  }

  bool sameAsPrevious = count == previousUidLength_;
  for (uint8_t i = 0; i < count && sameAsPrevious; ++i)
    sameAsPrevious = mfrcReader_.uid.uidByte[i] == previousUid_[i];
  mfrcReader_.PICC_HaltA();
  mfrcReader_.PCD_StopCrypto1();
  if (sameAsPrevious &&
      static_cast<uint32_t>(now - lastPresentation_) < Config::RFID_REPEAT_SUPPRESSION_MS) return false;

  length = count;
  lastPresentation_ = now;
  previousUidLength_ = count;
  for (uint8_t i = 0; i < count; ++i)
    uid[i] = previousUid_[i] = mfrcReader_.uid.uidByte[i];
  return true;
}
