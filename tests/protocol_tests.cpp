#include "TestSupport.h"
#include "Protocol.h"

#include <cstring>

int main() {
  Suite suite;
  suite.testCase("door state names use lowercase English values", [] {
    CHECK(std::strcmp(Protocol::name(Protocol::DoorState::OPEN), "open") == 0);
    CHECK(std::strcmp(Protocol::name(Protocol::DoorState::CLOSED), "closed") == 0);
    CHECK(std::strcmp(Protocol::name(Protocol::DoorState::UNKNOWN), "unknown") == 0);
    CHECK(std::strcmp(Protocol::name(static_cast<Protocol::DoorState>(255)), "unknown") == 0);
  });
  suite.testCase("message type names use lowercase English values", [] {
    CHECK(std::strcmp(Protocol::name(Protocol::Type::REED_STATE), "reed_state") == 0);
    CHECK(std::strcmp(Protocol::name(Protocol::Type::RFID_PRESENTED), "rfid_presented") == 0);
    CHECK(std::strcmp(Protocol::name(Protocol::Type::HEARTBEAT), "heartbeat") == 0);
    CHECK(std::strcmp(Protocol::name(static_cast<Protocol::Type>(255)), "unknown") == 0);
  });
  suite.testCase("UID formatting produces uppercase hexadecimal text", [] {
    const uint8_t uid[] = {0x00, 0x39, 0xA5, 0xFF};
    char output[9];
    CHECK(Protocol::uidToHex(uid, sizeof(uid), output, sizeof(output)));
    CHECK(std::strcmp(output, "0039A5FF") == 0);
  });
  suite.testCase("UID formatting rejects invalid pointers, lengths, and capacity", [] {
    const uint8_t uid[11]{};
    char output[23];
    CHECK(!Protocol::uidToHex(nullptr, 4, output, sizeof(output)));
    CHECK(!Protocol::uidToHex(uid, 4, nullptr, sizeof(output)));
    CHECK(!Protocol::uidToHex(uid, 11, output, sizeof(output)));
    CHECK(!Protocol::uidToHex(uid, 4, output, 8));
  });
  suite.testCase("UID parsing accepts mixed case and whitespace", [] {
    uint8_t uid[10]{};
    uint8_t length = 0;
    CHECK(Protocol::hexToUid(" 39 61 d7 06 ", uid, sizeof(uid), length));
    CHECK(length == 4);
    CHECK(uid[0] == 0x39 && uid[1] == 0x61 && uid[2] == 0xD7 && uid[3] == 0x06);
  });
  suite.testCase("UID parsing rejects malformed and oversized text", [] {
    uint8_t uid[10]{};
    uint8_t length = 99;
    CHECK(!Protocol::hexToUid(nullptr, uid, sizeof(uid), length) && length == 0);
    CHECK(!Protocol::hexToUid("00", nullptr, sizeof(uid), length) && length == 0);
    CHECK(!Protocol::hexToUid("", uid, sizeof(uid), length) && length == 0);
    CHECK(!Protocol::hexToUid("123", uid, sizeof(uid), length) && length == 0);
    CHECK(!Protocol::hexToUid("GG", uid, sizeof(uid), length) && length == 0);
    CHECK(!Protocol::hexToUid("0011", uid, 1, length) && length == 0);
    CHECK(!Protocol::hexToUid("00112233445566778899AA", uid, sizeof(uid), length) && length == 0);
  });
  suite.testCase("protocol message remains compact and versioned", [] {
    Protocol::Message message{};
    message.version = Protocol::VERSION;
    message.type = Protocol::Type::HEARTBEAT;
    message.sequence = 42;
    message.doorState = Protocol::DoorState::CLOSED;
    message.uptimeMs = 1234;
    CHECK(Protocol::VERSION == 1);
    CHECK(sizeof(Protocol::Message) == 22);
    CHECK(message.sequence == 42 && message.doorState == Protocol::DoorState::CLOSED);
  });
  return suite.result();
}
