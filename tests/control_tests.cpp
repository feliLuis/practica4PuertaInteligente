#include "TestSupport.h"
#include "LockController.h"
#include "LockServo.h"
#include "SerialMonitor.h"
#include "config.h"
#include <vector>
#include <cstring>

using Door = Protocol::DoorState;
const Source diagnostic = Source::DIAGNOSTIC;
struct Harness {
  LockController c{true};
  std::vector<Event> events;
  int movements = 0;
  bool issueCommand = true;
  Harness() {
    c.onEvent([this](const Event& e) { events.push_back(e); });
    c.onMovement([this](Action) { ++movements; return issueCommand; });
  }
  void close() { c.updateDoor(Door::CLOSED, diagnostic); }
  void open() { c.updateDoor(Door::OPEN, diagnostic); }
  Result unlock() { return c.processAction(Action::UNLOCK, diagnostic); }
  Result lock() { return c.processAction(Action::LOCK, diagnostic); }
  size_t count(EventType type) const {
    size_t total = 0;
    for (const auto& e : events) if (e.type == type) ++total;
    return total;
  }
};
bool accepted(Result r) { return r.result == EventResult::EXECUTED; }

int main() {
  Suite s;
  s.testCase("unknown startup state without movement", [] {
    Harness b; CHECK(b.c.lockState() == LockState::UNKNOWN);
    CHECK(b.c.doorState() == Door::UNKNOWN);
    CHECK(b.c.mode() == Mode::AUTOMATIC); CHECK(b.c.autoLockSeconds() == 10);
    CHECK(b.movements == 0 && !b.c.sensorIsFresh());
  });
  s.testCase("lock with a recent closed-door reading", [] {
    Harness b; b.close(); CHECK(accepted(b.lock()));
    CHECK(b.movements == 1 && b.c.lockState() == LockState::LOCKED);
  });
  s.testCase("reject locking with the door open", [] {
    Harness b; b.open(); CHECK(!accepted(b.lock())); CHECK(b.movements == 0);
  });
  s.testCase("reject locking with unknown or invalid door state", [] {
    Harness b; CHECK(!accepted(b.lock()));
    CHECK(!b.c.updateDoor(static_cast<Door>(255)));
    CHECK(!b.c.receiveHeartbeat()); CHECK(!b.c.sensorIsFresh());
    CHECK(!accepted(b.c.processAction(static_cast<Action>(255), diagnostic)));
    CHECK(b.movements == 0);
  });
  s.testCase("reject stale sensor data before the next update", [] {
    Harness b; b.close(); Fake::now = 30000;
    CHECK(b.c.doorState() == Door::UNKNOWN);
    CHECK(!accepted(b.lock())); CHECK(b.movements == 0);
  });
  s.testCase("card authorizes access and preserves the logical ID", [] {
    Harness b; const uint8_t uid[] = {0x39, 0x61, 0xD7, 0x06};
    CHECK(std::memcmp(uid, Config::CARD_UID, sizeof(uid)) == 0);
    CHECK(accepted(b.c.processUid(uid, sizeof(uid))));
    CHECK(b.c.lockState() == LockState::UNLOCKED);
    CHECK(b.count(EventType::ACCESS) == 1);
    CHECK(b.count(EventType::UNLOCK) == 1);
    CHECK(std::strcmp(b.events.back().credentialId, "card-1") == 0);
    CHECK(std::strcmp(b.events.back().reason, "authorized_credential") == 0);
  });
  s.testCase("key fob unlocks without a door reading and preserves the ID", [] {
    Harness b; const uint8_t uid[] = {0x9C, 0x31, 0x8B, 0x06};
    CHECK(std::memcmp(uid, Config::KEYFOB_UID, sizeof(uid)) == 0);
    CHECK(accepted(b.c.processUid(uid, sizeof(uid))));
    CHECK(b.c.lockState() == LockState::UNLOCKED);
    CHECK(std::strcmp(b.events.back().credentialId, "keyfob-1") == 0);
    CHECK(std::strcmp(b.events.back().reason, "authorized_credential") == 0);
    CHECK(b.movements == 1 && b.count(EventType::ACCESS) == 1);
    CHECK(b.count(EventType::UNLOCK) == 1 && b.count(EventType::LOCK) == 0);
  });
  s.testCase("credential list rejects lock actions even on replacement", [] {
    CredentialList credentials;
    CHECK(!credentials.add(Config::KEYFOB_UID, Config::KEYFOB_UID_LENGTH,
                            Action::LOCK, Config::KEYFOB_ID));
    Action action = Action::LOCK; const char* id = nullptr;
    CHECK(!credentials.find(Config::KEYFOB_UID, Config::KEYFOB_UID_LENGTH, action, id));
    CHECK(credentials.add(Config::KEYFOB_UID, Config::KEYFOB_UID_LENGTH,
                           Action::UNLOCK, Config::KEYFOB_ID));
    CHECK(!credentials.add(Config::KEYFOB_UID, Config::KEYFOB_UID_LENGTH,
                            Action::LOCK, "other-id"));
    CHECK(!credentials.add(Config::KEYFOB_UID, Config::KEYFOB_UID_LENGTH,
                            static_cast<Action>(255), Config::KEYFOB_ID));
    CHECK(credentials.find(Config::KEYFOB_UID, Config::KEYFOB_UID_LENGTH, action, id));
    CHECK(action == Action::UNLOCK && std::strcmp(id, "keyfob-1") == 0);
  });
  s.testCase("both credentials unlock to 90 in manual and automatic modes without toggling", [] {
    const uint8_t* uids[] = {Config::CARD_UID, Config::KEYFOB_UID};
    for (Mode mode : {Mode::MANUAL, Mode::AUTOMATIC}) {
      for (const uint8_t* uid : uids) {
        Harness b; LockServo servo(Config::PIN_SERVO, Config::SERVO_DETACH_MS);
        CHECK(servo.begin()); b.c.setMode(mode, diagnostic); b.close();
        b.c.onMovement([&](Action action) {
          ++b.movements;
          return action == Action::UNLOCK ? servo.unlock() : servo.lock();
        });
        CHECK(accepted(b.lock()) && Fake::lastAngle == 0);
        const int previousWrites = Fake::writes;
        CHECK(accepted(b.c.processUid(uid, 4)));
        CHECK(Fake::lastAngle == 90 && Fake::writes == previousWrites + 1);
        CHECK(b.c.lockState() == LockState::UNLOCKED);
        for (const uint8_t* repeatedUid : uids) CHECK(accepted(b.c.processUid(repeatedUid, 4)));
        CHECK(Fake::writes == previousWrites + 1 && b.movements == 2);
        CHECK(b.c.lockState() == LockState::UNLOCKED);
        CHECK(b.count(EventType::UNLOCK) == 1 && b.count(EventType::LOCK) == 1);
        CHECK(b.count(EventType::ACCESS) == 3);
      }
    }
  });
  s.testCase("blocked mode rejects both credentials from any state without movement", [] {
    const uint8_t* uids[] = {Config::CARD_UID, Config::KEYFOB_UID};
    const char* ids[] = {"card-1", "keyfob-1"};
    for (int initialStateIndex = 0; initialStateIndex < 3; ++initialStateIndex) {
      Harness b; b.close();
      if (initialStateIndex == 1) b.lock();
      if (initialStateIndex == 2) b.unlock();
      const auto state = b.c.lockState(); const int previousMovements = b.movements;
      const auto lockEvents = b.count(EventType::LOCK);
      const auto unlockEvents = b.count(EventType::UNLOCK);
      b.c.setMode(Mode::BLOCKED, diagnostic);
      for (int i = 0; i < 2; ++i) {
        const auto r = b.c.processUid(uids[i], 4);
        CHECK(!accepted(r) && std::strcmp(r.reason, "blocked_mode") == 0);
        CHECK(std::strcmp(b.events.back().credentialId, ids[i]) == 0);
        CHECK(b.events.back().result == EventResult::REJECTED);
      }
      CHECK(b.movements == previousMovements && b.c.lockState() == state);
      CHECK(b.count(EventType::ACCESS) == 2 && !b.c.autoLockPending());
      CHECK(b.count(EventType::LOCK) == lockEvents);
      CHECK(b.count(EventType::UNLOCK) == unlockEvents);
    }
  });
  s.testCase("repeating or alternating credentials neither moves the lock nor extends auto-lock", [] {
    for (const uint8_t* initialUid : {Config::CARD_UID, Config::KEYFOB_UID}) {
      Fake::now = 0; Harness b; b.close();
      CHECK(accepted(b.c.processUid(initialUid, 4)));
      Fake::now = 9000;
      for (const uint8_t* uid : {Config::CARD_UID, Config::KEYFOB_UID, initialUid}) {
        const auto r = b.c.processUid(uid, 4);
        CHECK(accepted(r) && std::strcmp(r.reason, "no_change") == 0);
      }
      CHECK(b.movements == 1 && b.count(EventType::UNLOCK) == 1);
      CHECK(b.count(EventType::ACCESS) == 4 && b.count(EventType::LOCK) == 0);
      Fake::now = 9999; b.c.update(); CHECK(b.movements == 1);
      Fake::now = 10000; b.c.update();
      CHECK(b.movements == 2 && b.c.lockState() == LockState::LOCKED);
    }
  });
  s.testCase("unknown UID emits one access event without movement", [] {
    Harness b; const uint8_t uid[] = {1, 2, 3, 4};
    CHECK(!accepted(b.c.processUid(uid, 4)));
    CHECK(!accepted(b.c.processUid(nullptr, 4)));
    CHECK(b.movements == 0 && b.events.size() == 2);
    CHECK(b.count(EventType::ACCESS) == 2 && b.events[0].credentialId[0] == '\0');
  });
  s.testCase("auto-lock when the door starts closed", [] {
    Harness b; b.close(); b.unlock(); CHECK(b.c.autoLockPending());
    Fake::now = 9999; b.c.update(); CHECK(b.movements == 1);
    Fake::now = 10000; b.c.update(); CHECK(b.movements == 2);
    CHECK(b.c.lockState() == LockState::LOCKED);
  });
  s.testCase("auto-lock waits for an open door to close", [] {
    Harness b; b.open(); b.unlock(); Fake::now = 15000; b.c.update();
    CHECK(b.movements == 1 && !b.c.autoLockPending());
    b.close(); Fake::now += 9999; b.c.update(); CHECK(b.movements == 1);
    ++Fake::now; b.c.update(); CHECK(b.movements == 2);
  });
  s.testCase("reopening cancels and restarts the full delay", [] {
    Harness b; b.close(); b.unlock(); Fake::now = 7000; b.open();
    Fake::now = 8000; b.close(); Fake::now = 10000; b.c.update();
    CHECK(b.movements == 1); Fake::now = 17999; b.c.update();
    CHECK(b.movements == 1); ++Fake::now; b.c.update();
    CHECK(b.movements == 2);
  });
  s.testCase("offline cancels and a valid recovery restarts the full delay", [] {
    Harness b; b.c.setAutoLock(40, diagnostic); b.close(); b.unlock();
    Fake::now = 30000; b.c.update(); b.c.update();
    CHECK(!b.c.autoLockPending() && b.c.doorState() == Door::UNKNOWN);
    CHECK(b.count(EventType::SENSOR) == 2); // initial online event and subsequent offline event
    CHECK(!b.c.receiveHeartbeat() && !b.c.sensorIsFresh());
    Fake::now = 31000; CHECK(b.c.receiveHeartbeat(Door::CLOSED));
    b.c.receiveHeartbeat(Door::CLOSED); CHECK(b.count(EventType::SENSOR) == 3);
    Fake::now = 51000; b.c.receiveHeartbeat(Door::CLOSED);
    Fake::now = 70999; b.c.update(); CHECK(b.movements == 1);
    ++Fake::now; b.c.update(); CHECK(b.movements == 2);
  });
  s.testCase("invalid message does not refresh sensor data", [] {
    Harness b; b.close(); Fake::now = 29000;
    CHECK(!b.c.updateDoor(static_cast<Door>(99)));
    CHECK(!b.c.receiveHeartbeat()); Fake::now = 30000;
    CHECK(!accepted(b.lock()));
  });
  s.testCase("manual mode skips auto-lock and automatic mode evaluates immediately", [] {
    Harness b; b.c.setMode(Mode::MANUAL, diagnostic); b.close(); b.unlock();
    Fake::now = 11000; b.c.update(); CHECK(b.movements == 1);
    b.c.setMode(Mode::AUTOMATIC, diagnostic);
    Fake::now = 20999; b.c.update(); CHECK(b.movements == 1);
    ++Fake::now; b.c.update(); CHECK(b.movements == 2);
  });
  s.testCase("blocked mode cancels auto-lock without forcing movement and rejects unlock", [] {
    Harness b; b.close(); b.unlock(); b.open();
    b.c.setMode(Mode::BLOCKED, diagnostic);
    CHECK(!accepted(b.unlock())); CHECK(!b.c.autoLockPending());
    Fake::now = 10000; b.c.update(); CHECK(b.movements == 1);
    CHECK(b.c.lockState() == LockState::UNLOCKED);
    CHECK(!accepted(b.c.setMode(static_cast<Mode>(255), diagnostic)));
    CHECK(b.c.mode() == Mode::BLOCKED);
  });
  s.testCase("repeating unlock neither moves the lock nor restarts the timer", [] {
    Harness b; b.close(); b.unlock(); Fake::now = 9000; b.unlock();
    b.c.receiveHeartbeat(Door::CLOSED); Fake::now = 10000; b.c.update();
    CHECK(b.movements == 2);
    b.lock(); CHECK(b.movements == 2 && b.count(EventType::LOCK) == 1);
  });
  s.testCase("changing a valid delay restarts the full delay", [] {
    Harness b;
    CHECK(!accepted(b.c.setAutoLock(4, diagnostic)));
    CHECK(!accepted(b.c.setAutoLock(121, diagnostic)));
    CHECK(!accepted(b.c.setAutoLock(UINT32_MAX, diagnostic)));
    CHECK(accepted(b.c.setAutoLock(120, diagnostic)));
    b.close(); b.unlock(); Fake::now = 9000;
    CHECK(accepted(b.c.setAutoLock(5, diagnostic)));
    Fake::now = 13999; b.c.update(); CHECK(b.movements == 1);
    ++Fake::now; b.c.update(); CHECK(b.movements == 2);
  });
  s.testCase("servo failure preserves state and does not report movement", [] {
    Harness b; b.close(); b.issueCommand = false;
    CHECK(!accepted(b.lock()));
    CHECK(b.c.lockState() == LockState::UNKNOWN);
    CHECK(b.count(EventType::LOCK) == 0);
    CHECK(std::strcmp(b.events.back().reason, "servo_command_failed") == 0);
    b.c.onMovement(nullptr); CHECK(!accepted(b.unlock()));
  });
  s.testCase("auto-lock failure does not retry each loop", [] {
    Harness b; b.close(); b.unlock(); b.issueCommand = false;
    Fake::now = 10000; b.c.update(); b.c.update(); b.c.update();
    CHECK(b.movements == 2 && !b.c.autoLockPending());
    CHECK(b.c.lockState() == LockState::UNLOCKED);
  });
  s.testCase("movement callback precedes state update and event", [] {
    LockController c;
    c.onMovement([&](Action) {
      CHECK(c.lockState() == LockState::UNKNOWN); return true;
    });
    CHECK(accepted(c.processAction(Action::UNLOCK, diagnostic)));
    CHECK(c.lockState() == LockState::UNLOCKED);
  });
  s.testCase("events are copies carrying a sequence and simulation flag", [] {
    Harness b; b.close(); b.unlock(); const auto copy = b.events;
    b.open(); b.c.setMode(Mode::MANUAL, diagnostic);
    for (size_t i = 0; i < b.events.size(); ++i) {
      CHECK(b.events[i].sequence == i + 1 && b.events[i].simulated);
    }
    CHECK(copy[0].doorState == Door::CLOSED && copy[0].mode == Mode::AUTOMATIC);
    CHECK(copy[0].source == diagnostic && std::strcmp(copy[0].reason, "sensor_online") == 0);
  });
  s.testCase("millis wraparound during auto-lock and sensor timeout", [] {
    Fake::now = UINT32_MAX - 5000; Harness b; b.close(); b.unlock();
    Fake::now += 9999; b.c.update(); CHECK(b.movements == 1);
    ++Fake::now; b.c.update(); CHECK(b.movements == 2);
    Fake::now += 20000; b.c.update(); CHECK(!b.c.sensorIsFresh());
  });
  s.testCase("servo stays detached at startup and validates positions", [] {
    LockServo servo(13, 700); CHECK(servo.begin());
    CHECK(Fake::writes == 0); CHECK(!servo.moveTo(45));
    Fake::attachFailure = true; CHECK(!servo.lock() && Fake::writes == 0);
    Fake::attachFailure = false; CHECK(servo.lock() && Fake::lastAngle == 0);
    Fake::now = 700; servo.update(); Fake::attachFailure = true;
    CHECK(!servo.unlock()); LockServo invalidServo(35, 700);
    CHECK(!invalidServo.begin() && !invalidServo.unlock());
  });
  s.testCase("servo detaches safely across millis wraparound", [] {
    LockServo servo(13, 700); servo.begin(); Fake::now = UINT32_MAX - 300;
    CHECK(servo.unlock()); Fake::now += 699; servo.update();
    Fake::attachFailure = true; CHECK(servo.lock()); // still attached
    Fake::now += 700; servo.update(); CHECK(!servo.lock());
  });
  s.testCase("incremental parser discards overflow and validates complete integers", [] {
    SerialMonitor m;
    for (char c : std::string("  STATUS  ")) CHECK(m.receive(c) == SerialMonitor::Result::INCOMPLETE);
    CHECK(m.receive('\r') == SerialMonitor::Result::READY);
    CHECK(std::strcmp(m.line(), "status") == 0);
    CHECK(m.receive('\n') == SerialMonitor::Result::INCOMPLETE);
    for (int i = 0; i < 100; ++i) m.receive('a');
    CHECK(m.receive('\n') == SerialMonitor::Result::INVALID);
    m.receive('\0'); m.receive('x'); CHECK(m.receive('\n') == SerialMonitor::Result::INVALID);
    uint32_t value = 0; CHECK(SerialMonitor::parseUnsigned("120", value) && value == 120);
    CHECK(!SerialMonitor::parseUnsigned("10x", value)); CHECK(!SerialMonitor::parseUnsigned("-5", value));
    CHECK(!SerialMonitor::parseUnsigned("", value)); CHECK(!SerialMonitor::parseUnsigned("4294967296", value));
    CHECK(SerialMonitor::parseUnsigned("4294967295", value) && value == UINT32_MAX);
  });
  return s.result();
}
