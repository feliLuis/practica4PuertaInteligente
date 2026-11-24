#include <Arduino.h>
#include "config.h"
#include "Types.h"
#include "LockController.h"
#include "SerialMonitor.h"

#ifndef SIMULATION_MODE
#define SIMULATION_MODE 0
#endif
#ifndef CALIBRATION_MODE
#define CALIBRATION_MODE 0
#endif
#if SIMULATION_MODE && CALIBRATION_MODE
#error "Simulation and calibration are mutually exclusive."
#endif

#if !SIMULATION_MODE
#include "LockServo.h"
LockServo servo(Config::PIN_SERVO, Config::SERVO_DETACH_MS);
#endif
LockController controller(SIMULATION_MODE != 0);
SerialMonitor monitor;

const char* typeName(EventType type) {
  switch (type) {
    case EventType::ACCESS: return "access";
    case EventType::UNLOCK: return "unlock";
    case EventType::LOCK: return "lock";
    case EventType::DOOR: return "door";
    case EventType::SENSOR: return "sensor";
    case EventType::MODE: return "mode";
    case EventType::AUTO_LOCK: return "auto_lock";
    case EventType::REQUEST: return "request";
  }
  return "unknown";
}

const char* sourceName(Source source) {
  switch (source) {
    case Source::VOICE: return "voice";
    case Source::RFID: return "rfid";
    case Source::EDGE: return "edge";
    case Source::CLOUD: return "cloud";
    case Source::DIAGNOSTIC: return "diagnostic";
  }
  return "unknown";
}

const char* lockStateName(LockState state) {
  switch (state) {
    case LockState::LOCKED: return "locked";
    case LockState::UNLOCKED: return "unlocked";
    default: return "unknown";
  }
}

const char* modeName(Mode mode) {
  switch (mode) {
    case Mode::MANUAL: return "manual";
    case Mode::AUTOMATIC: return "automatic";
    case Mode::BLOCKED: return "blocked";
  }
  return "unknown";
}

void onEvent(const Event& event) {
  Serial.printf("[EVT%s] sequence=%lu source=%s type=%s result=%s reason=%s "
                "doorState=%s lockState=%s mode=%s credentialId=%s\n",
    event.simulated ? "-SIM" : "", static_cast<unsigned long>(event.sequence),
    sourceName(event.source), typeName(event.type),
    event.result == EventResult::EXECUTED ? "executed" : "rejected", event.reason,
    Protocol::name(event.doorState), lockStateName(event.lockState),
    modeName(event.mode), event.credentialId);
}

void printStatus() {
  Serial.printf("lockState=%s doorState=%s mode=%s autoLockSeconds=%lu "
                "sensorStatus=%s autoLockTimer=%s\n",
    lockStateName(controller.lockState()), Protocol::name(controller.doorState()),
    modeName(controller.mode()), static_cast<unsigned long>(controller.autoLockSeconds()),
    controller.sensorIsFresh() ? "online" : "offline",
    controller.autoLockPending() ? "active" : "inactive");
}

void printHelp() {
  Serial.println("Commands: help | status");
#if SIMULATION_MODE
  Serial.println("[SIM WITHOUT PWM] door open|closed | heartbeat | uid HEX");
  Serial.println("unlock | lock | mode manual|automatic|blocked | auto-lock N");
  Serial.println("A heartbeat alone does not refresh the Reed switch. Use door open or door closed to supply a reading.");
#elif CALIBRATION_MODE
  Serial.println("[PHYSICAL CALIBRATION] calibrate 0 | calibrate 90");
  Serial.println("Verify power and travel before calibrating. Keep calibration disabled during operation.");
#else
  Serial.println("[NORMAL] Simulation and calibration are disabled.");
#endif
}

void processCommand(const char* line) {
  if (strcmp(line, "help") == 0) printHelp();
  else if (strcmp(line, "status") == 0) printStatus();
#if SIMULATION_MODE
  else if (strcmp(line, "door open") == 0)
    controller.updateDoor(Protocol::DoorState::OPEN, Source::DIAGNOSTIC);
  else if (strcmp(line, "door closed") == 0)
    controller.updateDoor(Protocol::DoorState::CLOSED, Source::DIAGNOSTIC);
  else if (strcmp(line, "heartbeat") == 0) {
    controller.receiveHeartbeat();
    Serial.println("[SIM] A heartbeat without a reading does not refresh the Reed switch.");
  } else if (strcmp(line, "unlock") == 0)
    controller.processAction(Action::UNLOCK, Source::DIAGNOSTIC);
  else if (strcmp(line, "lock") == 0)
    controller.processAction(Action::LOCK, Source::DIAGNOSTIC);
  else if (strcmp(line, "mode manual") == 0)
    controller.setMode(Mode::MANUAL, Source::DIAGNOSTIC);
  else if (strcmp(line, "mode automatic") == 0)
    controller.setMode(Mode::AUTOMATIC, Source::DIAGNOSTIC);
  else if (strcmp(line, "mode blocked") == 0)
    controller.setMode(Mode::BLOCKED, Source::DIAGNOSTIC);
  else if (strncmp(line, "auto-lock ", 10) == 0) {
    uint32_t seconds;
    if (SerialMonitor::parseUnsigned(line + 10, seconds))
      controller.setAutoLock(seconds, Source::DIAGNOSTIC);
    else Serial.println("[RX] Invalid decimal integer.");
  } else if (strncmp(line, "uid ", 4) == 0) {
    uint8_t uid[10];
    uint8_t length = 0;
    if (Protocol::hexToUid(line + 4, uid, sizeof(uid), length) &&
        (length == 4 || length == 7 || length == 10))
      controller.processUid(uid, length, Source::DIAGNOSTIC);
    else Serial.println("[RX] Invalid UID. Expected 4, 7, or 10 hexadecimal bytes.");
  }
#elif CALIBRATION_MODE
  else if (strcmp(line, "calibrate 0") == 0 || strcmp(line, "calibrate 90") == 0) {
    const bool issued = strcmp(line, "calibrate 0") == 0 ? servo.lock() : servo.unlock();
    Serial.println(issued ? "[CAL] PWM command issued. Physical position is unconfirmed."
                           : "[CAL] Failed to issue PWM command.");
  }
#endif
  else Serial.println("[RX] Unknown command or command disabled in this build.");
}

void setup() {
  Serial.begin(Config::SERIAL_BAUD);
  controller.onEvent(onEvent);
#if SIMULATION_MODE
  controller.onMovement([](Action) {
    Serial.println("[SIM] Movement command simulated without PWM.");
    return true;
  });
#elif CALIBRATION_MODE
  if (!servo.begin()) Serial.println("[BOOT] Servo configuration failed.");
#else
  if (!servo.begin()) Serial.println("[BOOT] Servo configuration failed.");
  controller.onMovement([](Action action) {
    return action == Action::UNLOCK ? servo.unlock() : servo.lock();
  });
#endif
  Serial.println("[BOOT] Door and lock states are unknown. No movement at startup.");
  Serial.println("[BOOT] Board-to-board communication and AWS integration are unavailable.");
  printHelp();
}

void loop() {
  // Limit input processed per loop so continuous input cannot starve timers.
  for (uint8_t i = 0; i < 64 && Serial.available(); ++i) {
    const auto result = monitor.receive(static_cast<char>(Serial.read()));
    if (result == SerialMonitor::Result::READY) processCommand(monitor.line());
    else if (result == SerialMonitor::Result::INVALID)
      Serial.println("[RX] Discarded line because it was too long or contained invalid characters.");
  }
  controller.update();
#if !SIMULATION_MODE
  servo.update();
#endif
}
