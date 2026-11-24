#include "Arduino.h"
#include "MFRC522.h"
#include <algorithm>
#include <cstring>

FakeSerial Serial;
FakeSpi SPI;
namespace Fake {
uint32_t now = 0;
int level[40]{};
int mode[40]{};
bool attachFailure = false;
int writes = 0;
int lastAngle = -1;
byte rfidVersion = 0x92;
bool cardPresent = false;
bool rfidReadOk = true;
byte uid[10]{};
byte uidLen = 4;
int rfidInitializations = 0;
int spi[4]{};
void reset() {
  now = 0;
  std::fill(level, level + 40, HIGH);
  std::fill(mode, mode + 40, 0);
  attachFailure = false;
  writes = 0;
  lastAngle = -1;
  rfidVersion = 0x92;
  cardPresent = false;
  rfidReadOk = true;
  std::memset(uid, 0, sizeof(uid));
  uidLen = 4;
  rfidInitializations = 0;
  Serial.output.clear();
  Serial.input.clear();
}
}
