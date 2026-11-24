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
    case EventType::ACCESS: return "acceso";
    case EventType::UNLOCK: return "desbloqueo";
    case EventType::LOCK: return "bloqueo";
    case EventType::DOOR: return "puerta";
    case EventType::SENSOR: return "sensor";
    case EventType::MODE: return "modo";
    case EventType::AUTO_LOCK: return "autocierre";
    case EventType::REQUEST: return "solicitud";
  }
  return "desconocido";
}

const char* sourceName(Source source) {
  switch (source) {
    case Source::VOICE: return "voz";
    case Source::RFID: return "rfid";
    case Source::EDGE: return "edge";
    case Source::CLOUD: return "nube";
    case Source::DIAGNOSTIC: return "diagnostico";
  }
  return "desconocido";
}

const char* lockStateName(LockState state) {
  switch (state) {
    case LockState::LOCKED: return "bloqueada";
    case LockState::UNLOCKED: return "desbloqueada";
    default: return "desconocida";
  }
}

const char* modeName(Mode mode) {
  switch (mode) {
    case Mode::MANUAL: return "manual";
    case Mode::AUTOMATIC: return "automatico";
    case Mode::LOCKED: return "bloqueado";
  }
  return "desconocido";
}

void onEvent(const Event& event) {
  Serial.printf("[EVT%s] seq=%lu origen=%s tipo=%s resultado=%s motivo=%s "
                "puerta=%s cerradura_estimada=%s modo=%s credentialId=%s\n",
    event.simulated ? "-SIM" : "", static_cast<unsigned long>(event.seq),
    sourceName(event.source), typeName(event.type),
    event.executed ? "ejecutado" : "rechazado", event.reason,
    Protocol::name(event.doorState), lockStateName(event.lockState),
    modeName(event.mode), event.credentialId);
}

void printStatus() {
  Serial.printf("cerradura_estimada=%s puerta=%s modo=%s autocierre_s=%lu "
                "sensor=%s temporizador=%s\n",
    lockStateName(controller.lockState()), Protocol::name(controller.doorState()),
    modeName(controller.mode()), static_cast<unsigned long>(controller.autoLockSeconds()),
    controller.sensorIsFresh() ? "online" : "offline",
    controller.autoLockPending() ? "activo" : "inactivo");
}

void printHelp() {
  Serial.println("Commands: ayuda | estado");
#if SIMULATION_MODE
  Serial.println("[SIM WITHOUT PWM] puerta abierta|cerrada | latido | uid HEX");
  Serial.println("desbloquear | bloquear | modo manual|automatico|bloqueado | autocierre N");
  Serial.println("A heartbeat alone does not refresh the Reed switch. Use puerta abierta or puerta cerrada to supply a reading.");
#elif CALIBRATION_MODE
  Serial.println("[PHYSICAL CALIBRATION] calibrar 0 | calibrar 90");
  Serial.println("Verify power and travel before calibrating. Keep calibration disabled during operation.");
#else
  Serial.println("[NORMAL] Simulation and calibration are disabled.");
#endif
}

void processCommand(const char* line) {
  if (strcmp(line, "ayuda") == 0) printHelp();
  else if (strcmp(line, "estado") == 0) printStatus();
#if SIMULATION_MODE
  else if (strcmp(line, "puerta abierta") == 0)
    controller.updateDoor(Protocol::DoorState::OPEN, Source::DIAGNOSTIC);
  else if (strcmp(line, "puerta cerrada") == 0)
    controller.updateDoor(Protocol::DoorState::CLOSED, Source::DIAGNOSTIC);
  else if (strcmp(line, "latido") == 0) {
    controller.receiveHeartbeat();
    Serial.println("[SIM] A heartbeat without a reading does not refresh the Reed switch.");
  } else if (strcmp(line, "desbloquear") == 0)
    controller.processAction(Action::UNLOCK, Source::DIAGNOSTIC);
  else if (strcmp(line, "bloquear") == 0)
    controller.processAction(Action::LOCK, Source::DIAGNOSTIC);
  else if (strcmp(line, "modo manual") == 0)
    controller.setMode(Mode::MANUAL, Source::DIAGNOSTIC);
  else if (strcmp(line, "modo automatico") == 0)
    controller.setMode(Mode::AUTOMATIC, Source::DIAGNOSTIC);
  else if (strcmp(line, "modo bloqueado") == 0)
    controller.setMode(Mode::LOCKED, Source::DIAGNOSTIC);
  else if (strncmp(line, "autocierre ", 11) == 0) {
    uint32_t seconds;
    if (SerialMonitor::parseUnsigned(line + 11, seconds))
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
  else if (strcmp(line, "calibrar 0") == 0 || strcmp(line, "calibrar 90") == 0) {
    const bool issued = strcmp(line, "calibrar 0") == 0 ? servo.lock() : servo.unlock();
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
