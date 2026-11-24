#include "TestSupport.h"
// Build the production entry point with test doubles once per configuration.
#include "../firmware/esp32_control/src/main.cpp"

void input(const std::string& text) {
  for (char c : text) Serial.input.push_back(c);
  while (Serial.available()) loop();
}
int main() {
  Suite s;
  s.testCase("console isolates normal, simulation, and calibration modes", [] {
    setup();
    CHECK(Fake::writes == 0);
    input("puerta cerrada\nuid 3961D706\n");
#if SIMULATION_MODE
    CHECK(controller.lockState() == LockState::UNLOCKED);
    CHECK(Serial.output.find("EVT-SIM") != std::string::npos);
    input("uid 9C318B06\nuid 3961D706\n");
    CHECK(controller.lockState() == LockState::UNLOCKED);
    CHECK(Serial.output.find("credentialId=llavero-1") != std::string::npos);
    input("bloquear\nuid 9C318B06\n");
    CHECK(controller.lockState() == LockState::UNLOCKED);
    input("modo bloqueado\nuid 3961D706\nuid 9C318B06\n");
    CHECK(controller.lockState() == LockState::UNLOCKED);
    CHECK(Serial.output.find("resultado=rechazado motivo=modo_bloqueado") != std::string::npos);
#else
    CHECK(controller.lockState() == LockState::UNKNOWN);
    CHECK(!controller.sensorIsFresh());
#endif
    CHECK(Fake::writes == 0);
    input("calibrar 0\ncalibrar 90\n");
#if CALIBRATION_MODE
    CHECK(Fake::writes == 2 && Fake::lastAngle == 90);
#else
    CHECK(Fake::writes == 0);
#endif
    const int anteriores = Fake::writes;
    input("servo 0\ncalibrar 45\ncalibrar abc\ncalibrar 0x\n");
    CHECK(Fake::writes == anteriores);
#if SIMULATION_MODE
    input("autocierre 10x\n"); CHECK(controller.autoLockSeconds() == 10);
    input("autocierre 5\n"); CHECK(controller.autoLockSeconds() == 5);
    input("autocierre 4294967296\n"); CHECK(controller.autoLockSeconds() == 5);
#endif
    input(std::string(100, 'x') + "calibrar 0\n");
    CHECK(Fake::writes == anteriores);
    input("ESTADO\r\n");
    CHECK(Serial.output.find("cerradura_estimada=") != std::string::npos);
  });
  return s.result();
}
