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
    CHECK(std::strcmp(Protocol::name(Protocol::Type::ACK), "ack") == 0);
    CHECK(std::strcmp(Protocol::name(Protocol::Type::STATE_REQUEST), "state_request") == 0);
    CHECK(std::strcmp(Protocol::name(Protocol::Type::STATE_RESPONSE), "state_response") == 0);
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
  suite.testCase("protocol v2 has a fixed endian-independent 34-byte wire format", [] {
    Protocol::Message message{};
    message.version = Protocol::VERSION;
    message.type = Protocol::Type::HEARTBEAT;
    message.sessionId = 0x11223344;
    message.sequence = 42;
    message.doorState = Protocol::DoorState::CLOSED;
    message.uptimeMs = 1234;
    uint8_t wire[Protocol::WIRE_SIZE]{};
    CHECK(Protocol::VERSION == 2 && Protocol::WIRE_SIZE == 34);
    CHECK(Protocol::encode(message, wire, sizeof(wire)));
    CHECK(wire[2] == 0x44 && wire[3] == 0x33 && wire[4] == 0x22 && wire[5] == 0x11);
    Protocol::Message decoded{};
    CHECK(Protocol::decode(wire, sizeof(wire), decoded) == Protocol::ValidationError::NONE);
    CHECK(decoded.sequence == 42 && decoded.doorState == Protocol::DoorState::CLOSED);
  });
  suite.testCase("decoder rejects wrong sizes versions types sessions and sequences", [] {
    Protocol::Message m{}; uint8_t wire[Protocol::WIRE_SIZE]{};
    CHECK(Protocol::decode(wire, sizeof(wire)-1, m) == Protocol::ValidationError::SIZE);
    m.version=Protocol::VERSION;m.type=Protocol::Type::HEARTBEAT;m.sessionId=1;m.sequence=1;
    CHECK(Protocol::encode(m,wire,sizeof(wire)));wire[0]=99;
    CHECK(Protocol::decode(wire,sizeof(wire),m)==Protocol::ValidationError::VERSION);
    wire[0]=Protocol::VERSION;wire[1]=99;
    CHECK(Protocol::decode(wire,sizeof(wire),m)==Protocol::ValidationError::TYPE);
    wire[1]=uint8_t(Protocol::Type::HEARTBEAT);memset(wire+2,0,4);
    CHECK(Protocol::decode(wire,sizeof(wire),m)==Protocol::ValidationError::SESSION);
    wire[2]=1;memset(wire+6,0,4);
    CHECK(Protocol::decode(wire,sizeof(wire),m)==Protocol::ValidationError::SEQUENCE);
  });
  suite.testCase("validator rejects invalid UID state and incoherent content", [] {
    Protocol::Message m{};m.version=Protocol::VERSION;m.sessionId=1;m.sequence=1;
    m.type=Protocol::Type::RFID_PRESENTED;m.uidLen=3;
    CHECK(Protocol::validate(m)==Protocol::ValidationError::CONTENT);
    m.uidLen=4;m.doorState=static_cast<Protocol::DoorState>(9);
    CHECK(Protocol::validate(m)==Protocol::ValidationError::DOOR_STATE);
    m.doorState=Protocol::DoorState::UNKNOWN;m.type=Protocol::Type::REED_STATE;m.uidLen=0;
    CHECK(Protocol::validate(m)==Protocol::ValidationError::CONTENT);
    m.type=Protocol::Type::ACK;m.correlationSession=0;m.correlationSequence=0;
    CHECK(Protocol::validate(m)==Protocol::ValidationError::CONTENT);
  });
  return suite.result();
}
