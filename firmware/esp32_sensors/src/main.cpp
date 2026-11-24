#include <Arduino.h>
#include "config.h"
#include "DoorSensor.h"
#include "RfidReader.h"
#include "SerialLink.h"

DoorSensor doorSensor;
RfidReader rfidReader(Config::RC522_SS, Config::RC522_RST);
SerialLink serialLink;
uint32_t sequence = 0;
uint32_t lastHeartbeat = 0;

void sendMessage(Protocol::Type type, const uint8_t* uid = nullptr, uint8_t uidLen = 0) {
  if ((uidLen != 0 && uidLen != 4 && uidLen != 7 && uidLen != 10) ||
      (uidLen > 0 && uid == nullptr)) return;
  Protocol::Message message{};
  message.version = Protocol::VERSION;
  message.type = type;
  message.sequence = ++sequence;
  message.doorState = doorSensor.state();
  message.uidLen = uidLen;
  for (uint8_t i = 0; i < uidLen; ++i) message.uid[i] = uid[i];
  message.uptimeMs = millis();
  if (!serialLink.send(message)) Serial.println("[SIM-TX] The simulator rejected the message.");
}

void setup() {
  Serial.begin(Config::SERIAL_BAUD);
  serialLink.begin();
  rfidReader.begin();
  // Reed switch stabilization starts after the blocking RC522 initialization.
  doorSensor.begin();
  lastHeartbeat = millis();
  Serial.println("[BOOT] The Reed switch state is unknown until the reading is stable for 100 ms.");
  Serial.println("[BOOT] Simulated serial output only. No communication with ESP32 #1.");
  Serial.println(Config::SHOW_FULL_UID ? "[DIAG] Full UID output enabled on USB only."
                                            : "[NORMAL] UID hidden on USB.");
}

void loop() {
  // Read the switch before any potential waits inside the RC522 library.
  if (doorSensor.update()) sendMessage(Protocol::Type::REED_STATE);
  rfidReader.update();
  uint8_t uid[10];
  uint8_t length = 0;
  if (rfidReader.read(uid, sizeof(uid), length))
    sendMessage(Protocol::Type::RFID_PRESENTED, uid, length);

  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - lastHeartbeat) >= Config::HEARTBEAT_MS) {
    lastHeartbeat = now;
    sendMessage(Protocol::Type::HEARTBEAT);
  }
}
