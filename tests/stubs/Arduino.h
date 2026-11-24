#pragma once
// Test doubles avoid accessing GPIO, USB ports, and physical devices.
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <deque>

using byte = uint8_t;
constexpr int LOW = 0;
constexpr int HIGH = 1;
constexpr int INPUT_PULLUP = 2;
namespace Fake {
extern uint32_t now;
extern int level[40];
extern int mode[40];
extern bool attachFailure;
extern int writes;
extern int lastAngle;
extern byte rfidVersion;
extern bool cardPresent;
extern bool rfidReadOk;
extern byte uid[10];
extern byte uidLen;
extern int rfidInitializations;
extern int spi[4];
void reset();
}
inline uint32_t millis() { return Fake::now; }
inline int digitalRead(uint8_t pin) { return Fake::level[pin]; }
inline void pinMode(uint8_t pin, int mode) { Fake::mode[pin] = mode; }

struct FakeSerial {
  std::string output;
  std::deque<char> input;
  void begin(uint32_t) {}
  int available() const { return static_cast<int>(input.size()); }
  int read() { const char c = input.front(); input.pop_front(); return c; }
  void println(const char* text) { output += text; output += '\n'; }
  void printf(const char* format, ...) {
    char buffer[1024];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    output += buffer;
  }
};
extern FakeSerial Serial;
