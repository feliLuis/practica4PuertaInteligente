#include "TestSupport.h"
#include "DoorSensor.h"
#include "RfidReader.h"
#include "SerialLink.h"
#include "config.h"

#include <cstring>

using Door = Protocol::DoorState;

void presentCard(byte length = 4) {
  Fake::cardPresent = true;
  Fake::rfidReadOk = true;
  Fake::uidLen = length;
  for (byte i = 0; i < 10; ++i) Fake::uid[i] = i + 1;
}

int main() {
  Suite suite;
  suite.testCase("Reed starts unknown until the input is stable for 100 ms", [] {
    Fake::level[Config::PIN_REED] = LOW;
    DoorSensor sensor;
    sensor.begin();
    CHECK(Fake::mode[Config::PIN_REED] == INPUT_PULLUP);
    CHECK(sensor.state() == Door::UNKNOWN);
    Fake::now = 99;
    CHECK(!sensor.update());
    Fake::now = 100;
    CHECK(sensor.update() && sensor.state() == Door::CLOSED);
    CHECK(!sensor.update());
  });
  suite.testCase("initial Reed bounce restarts stabilization", [] {
    Fake::level[Config::PIN_REED] = LOW;
    DoorSensor sensor;
    sensor.begin();
    Fake::now = 50;
    Fake::level[Config::PIN_REED] = HIGH;
    CHECK(!sensor.update());
    Fake::now = 149;
    CHECK(!sensor.update());
    Fake::now = 150;
    CHECK(sensor.update() && sensor.state() == Door::OPEN);
  });
  suite.testCase("stable Reed changes emit no redundant transitions", [] {
    DoorSensor sensor;
    sensor.begin();
    Fake::now = 100;
    CHECK(sensor.update() && sensor.state() == Door::OPEN);
    Fake::level[Config::PIN_REED] = LOW;
    Fake::now = 110;
    CHECK(!sensor.update());
    Fake::level[Config::PIN_REED] = HIGH;
    Fake::now = 140;
    CHECK(!sensor.update());
    Fake::level[Config::PIN_REED] = LOW;
    Fake::now = 160;
    CHECK(!sensor.update());
    Fake::now = 259;
    CHECK(!sensor.update());
    ++Fake::now;
    CHECK(sensor.update() && sensor.state() == Door::CLOSED);
    CHECK(!sensor.update());
  });
  suite.testCase("Reed debounce handles millis wraparound", [] {
    Fake::now = UINT32_MAX - 50;
    DoorSensor sensor;
    sensor.begin();
    Fake::now += 99;
    CHECK(!sensor.update());
    ++Fake::now;
    CHECK(sensor.update());
  });
  suite.testCase("RFID initializes SPI with the fixed pins", [] {
    RfidReader reader(Config::RC522_SS, Config::RC522_RST);
    CHECK(reader.begin());
    CHECK(Fake::spi[0] == Config::RC522_SCK);
    CHECK(Fake::spi[1] == Config::RC522_MISO);
    CHECK(Fake::spi[2] == Config::RC522_MOSI);
    CHECK(Fake::spi[3] == Config::RC522_SS);
  });
  suite.testCase("RFID retries an initial failure every five seconds", [] {
    Fake::rfidVersion = 0;
    RfidReader reader(Config::RC522_SS, Config::RC522_RST);
    CHECK(!reader.begin());
    Fake::rfidVersion = 0x92;
    Fake::now = Config::RFID_RETRY_MS - 1;
    reader.update();
    CHECK(!reader.available() && Fake::rfidInitializations == 1);
    ++Fake::now;
    reader.update();
    CHECK(reader.available() && Fake::rfidInitializations == 2);
  });
  suite.testCase("RFID loss and recovery remain rate limited", [] {
    RfidReader reader(Config::RC522_SS, Config::RC522_RST);
    CHECK(reader.begin());
    Fake::rfidVersion = 0xFF;
    Fake::now = Config::RFID_CHECK_MS - 1;
    reader.update();
    CHECK(reader.available());
    ++Fake::now;
    reader.update();
    CHECK(!reader.available());
    Fake::rfidVersion = 0x92;
    Fake::now += Config::RFID_RETRY_MS - 1;
    reader.update();
    CHECK(Fake::rfidInitializations == 1);
    ++Fake::now;
    reader.update();
    CHECK(reader.available() && Fake::rfidInitializations == 2);
  });
  suite.testCase("RFID validates UID length, buffer capacity, and read failure", [] {
    RfidReader reader(Config::RC522_SS, Config::RC522_RST);
    CHECK(reader.begin());
    uint8_t uid[10];
    uint8_t length = 99;
    Fake::now = Config::RFID_POLL_MS;
    presentCard(3);
    CHECK(!reader.read(uid, sizeof(uid), length) && length == 0);
    Fake::now += Config::RFID_POLL_MS;
    presentCard(11);
    CHECK(!reader.read(uid, sizeof(uid), length));
    Fake::now += Config::RFID_POLL_MS;
    presentCard(7);
    CHECK(!reader.read(uid, 4, length));
    Fake::now += Config::RFID_POLL_MS;
    presentCard(7);
    CHECK(reader.read(uid, sizeof(uid), length) && length == 7);
    Fake::now += Config::RFID_POLL_MS;
    presentCard(10);
    CHECK(reader.read(uid, sizeof(uid), length) && length == 10);
    Fake::now += Config::RFID_POLL_MS;
    presentCard(4);
    Fake::rfidReadOk = false;
    CHECK(!reader.read(uid, sizeof(uid), length) && length == 0);
    Fake::now += Config::RFID_POLL_MS;
    presentCard(4);
    CHECK(reader.read(uid, sizeof(uid), length) && length == 4);
  });
  suite.testCase("RFID suppresses repeated presentations for 1500 ms", [] {
    RfidReader reader(Config::RC522_SS, Config::RC522_RST);
    CHECK(reader.begin());
    uint8_t uid[10];
    uint8_t length = 0;
    Fake::now = Config::RFID_POLL_MS;
    presentCard();
    CHECK(reader.read(uid, sizeof(uid), length));
    Fake::now += Config::RFID_POLL_MS;
    presentCard();
    CHECK(!reader.read(uid, sizeof(uid), length));
    Fake::now = Config::RFID_REPEAT_SUPPRESSION_MS;
    presentCard();
    CHECK(!reader.read(uid, sizeof(uid), length));
    Fake::now += Config::RFID_POLL_MS;
    presentCard();
    CHECK(reader.read(uid, sizeof(uid), length));
    Fake::now += Config::RFID_POLL_MS;
    presentCard();
    Fake::uid[0] = 99;
    CHECK(reader.read(uid, sizeof(uid), length));
  });
  suite.testCase("RFID repeat suppression handles millis wraparound", [] {
    RfidReader reader(Config::RC522_SS, Config::RC522_RST);
    CHECK(reader.begin());
    uint8_t uid[10];
    uint8_t length = 0;
    Fake::now = UINT32_MAX - 1000;
    presentCard();
    CHECK(reader.read(uid, sizeof(uid), length));
    Fake::now += Config::RFID_REPEAT_SUPPRESSION_MS - 1;
    presentCard();
    CHECK(!reader.read(uid, sizeof(uid), length));
    Fake::now += Config::RFID_POLL_MS + 1;
    presentCard();
    CHECK(reader.read(uid, sizeof(uid), length));
  });
  suite.testCase("serial link preserves privacy and reports English protocol values", [] {
    SerialLink link;
    CHECK(link.begin());
    Protocol::Message message{};
    message.version = Protocol::VERSION;
    message.type = Protocol::Type::RFID_PRESENTED;
    message.sequence = 7;
    message.uidLen = 4;
    message.uid[0] = 0xAA;
    message.uid[1] = 0xBB;
    message.uid[2] = 0xCC;
    message.uid[3] = 0xDD;
    message.doorState = Door::CLOSED;
    CHECK(link.send(message));
    CHECK(Serial.output.find("NO LINK") != std::string::npos);
    CHECK(Serial.output.find("sequence=7") != std::string::npos);
    CHECK(Serial.output.find("type=rfid_presented") != std::string::npos);
    CHECK(Serial.output.find("doorState=closed") != std::string::npos);
    CHECK((Serial.output.find("AABBCCDD") != std::string::npos) == Config::SHOW_FULL_UID);
    CHECK((Serial.output.find("uid=hidden") != std::string::npos) == !Config::SHOW_FULL_UID);
    message.uidLen = 11;
    CHECK(!link.send(message));
  });
  return suite.result();
}
