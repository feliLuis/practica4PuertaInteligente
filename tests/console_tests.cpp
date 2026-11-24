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
    input("door closed\nuid 3961D706\n");
#if SIMULATION_MODE
    CHECK(controller.lockState() == LockState::UNLOCKED);
    CHECK(Serial.output.find("EVT-SIM") != std::string::npos);
    input("uid 9C318B06\nuid 3961D706\n");
    CHECK(controller.lockState() == LockState::UNLOCKED);
    CHECK(Serial.output.find("credentialId=keyfob-1") != std::string::npos);
    input("lock\nuid 9C318B06\n");
    CHECK(controller.lockState() == LockState::UNLOCKED);
    input("mode blocked\nuid 3961D706\nuid 9C318B06\n");
    CHECK(controller.lockState() == LockState::UNLOCKED);
    CHECK(Serial.output.find("result=rejected reason=blocked_mode") != std::string::npos);
#else
    CHECK(controller.lockState() == LockState::UNKNOWN);
    CHECK(!controller.sensorIsFresh());
#endif
    CHECK(Fake::writes == 0);
    input("calibrate 0\ncalibrate 90\n");
#if CALIBRATION_MODE
    CHECK(Fake::writes == 2 && Fake::lastAngle == 90);
#else
    CHECK(Fake::writes == 0);
#endif
    const int previousWrites = Fake::writes;
    input("servo 0\ncalibrate 45\ncalibrate abc\ncalibrate 0x\n");
    CHECK(Fake::writes == previousWrites);
#if SIMULATION_MODE
    input("auto-lock 10x\n"); CHECK(controller.autoLockSeconds() == 10);
    input("auto-lock 5\n"); CHECK(controller.autoLockSeconds() == 5);
    input("auto-lock 4294967296\n"); CHECK(controller.autoLockSeconds() == 5);
#endif
    input(std::string(100, 'x') + "calibrate 0\n");
    CHECK(Fake::writes == previousWrites);
    input("STATUS\r\n");
    CHECK(Serial.output.find("lockState=") != std::string::npos);
  });
  return s.result();
}
